/*
 * Copyright (C) 2024 Balázs Trizska <info@balika011.hu>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sensor_extended.h"
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/sensors.h>
#include <linux/of.h>
#include <linux/of_i2c.h>

#define SENSOR_MUX_DEV_NAME "sensor_mux"
#define SENSOR_HUB_ADDR_GESTURE 0x03
#define SENSOR_HUB_ADDR_MUX 0x08
#define STM_VL6180_ADDR 0x29

struct sensor_mux_priv {
	struct i2c_adapter	*parent;
	struct i2c_adapter	adapter;
	struct i2c_algorithm	algo;
	__u16			reg;
	void			*complete;
	int			irq;
};

static irqreturn_t sensor_mux_interrupt(int irq, void *privid)
{
	struct sensor_mux_priv *priv = privid;
	disable_irq_nosync(priv->irq);
	if (priv->complete)
		complete(priv->complete);
	return IRQ_HANDLED;
}

int sensor_mux_prepare_read(struct sensor_mux_priv *priv, int reg_size, struct i2c_msg *msg)
{
	int rc;
	struct i2c_adapter *parent = priv->parent;
	struct i2c_msg hub_msg = {
		.addr = 3,
		.flags = 0,
		.len = msg->len == 1 ? 3 : (msg->len < 128 ? 4 : 5),
	};
	hub_msg.buf = kzalloc(hub_msg.len, GFP_KERNEL);
	
	hub_msg.buf[0] = msg->addr;
	if (reg_size == 2)
		hub_msg.buf[1] = (priv->reg >> 8) | 0x40;
	else
		hub_msg.buf[1] = 0;
	hub_msg.buf[1] = ~((unsigned int)~(hub_msg.buf[1] << 25) >> 25);
	hub_msg.buf[2] = priv->reg;
	
	if (msg->len == 1)
		hub_msg.buf[1] |= 0x20;
	else if (msg->len < 128)
		hub_msg.buf[3] = msg->len;
	else {
		hub_msg.buf[3] = ~((~(msg->len << 25)) >> 25);
		hub_msg.buf[4] = msg->len >> 7;
	}

	rc = parent->algo->master_xfer(parent, &hub_msg, 1);
	if (rc < 0) {
		pr_err("%s: Error doing prep_read: %d\n", __func__, rc);
	}

	kfree(hub_msg.buf);

	return rc;
}

int sensor_mux_read_ready(struct sensor_mux_priv *priv, int *ready)
{
	int rc;
	struct i2c_adapter *parent = priv->parent;
	__u8 hub_status_buf[] = { 0, 0, 2 };
	struct i2c_msg hub_status_msgs[] = {
		{
			.addr = 3,
			.flags = 0,
			.len = 3,
			.buf = hub_status_buf,
		},
		{
			.addr = 3,
			.flags = I2C_M_RD,
			.len = 2,
			.buf = hub_status_buf,
		}
	};
	
	rc = parent->algo->master_xfer(parent, hub_status_msgs, 2);
	
	*ready = hub_status_buf[0] >> 7;
	
	return rc;
}

static int sensor_mux_master_xfer(struct i2c_adapter *adapter,
			       struct i2c_msg msgs[], int num)
{
	struct sensor_mux_priv *priv = adapter->algo_data;
	struct i2c_adapter *parent = priv->parent;
	int rc = 0, i, j;
	DECLARE_COMPLETION_ONSTACK(complete);

	for (i = 0; i < num; i++) {
		struct i2c_msg *msg = &msgs[i];
		
		// STM VL6180 uses 2 bytes long reg addresses.
		// Is there a better way to detect this?
		int reg_size = msg->addr == STM_VL6180_ADDR ? 2 : 1;
		
		// the sensor hub doesn't need to be multiplexed
		if (msg->addr == SENSOR_HUB_ADDR_GESTURE || msg->addr == SENSOR_HUB_ADDR_MUX) {
			rc = parent->algo->master_xfer(parent, msg, 1);
			if (rc < 0)
				break;
			continue;
		}
		
		if ((msg->flags & I2C_M_RD) == 0) {
			struct i2c_msg hub_msg = {
				.addr = 3,
				.flags = msg->flags,
				.len = msg->len + 3 - reg_size,
			};

			if (msg->len == reg_size) {
				if (reg_size == 2) {
					priv->reg = (msg->buf[0] << 8) | msgs->buf[1];
				} else
					priv->reg = msg->buf[0];
				continue;
			}

			hub_msg.buf = kzalloc(hub_msg.len, GFP_KERNEL);
			
			hub_msg.buf[0] = msg->addr;
			if (reg_size == 2) { 
				hub_msg.buf[1] = msg->buf[0];
				hub_msg.buf[1] |= 0x40u;
			} else {
				hub_msg.buf[1] = 0;
			}
			memcpy(&hub_msg.buf[2], &msg->buf[reg_size - 1], msg->len - reg_size + 1);

			rc = parent->algo->master_xfer(parent, &hub_msg, 1);
			if (rc < 0)
				break;
			
			kfree(hub_msg.buf);
		} else {
			priv->complete = &complete;
			rc = sensor_mux_prepare_read(priv, reg_size, msg);
			if (rc < 0)
				break;

			msg->addr = 3;
			for (j = 0; j <= 10; j++) {
				int ready;

				enable_irq(priv->irq);
				if(!wait_for_completion_timeout(&complete, msecs_to_jiffies(100))) {
					pr_err("%s: irq timed out\n", __func__);
					continue;
				}
				
				rc = sensor_mux_read_ready(priv, &ready);
				if (rc < 0) {
					pr_err("%s: Error reading status: %d\n", __func__, rc);
					break;
				}
				
				if (ready) {
					rc = parent->algo->master_xfer(parent, msg, 1);
					break;
				}
			}
			priv->complete = NULL;
			if (j == 11) {
				pr_err("%s: ran out of retries\n", __func__);
				rc = -5;
			}
			if (rc < 0)
				break;
		}
	}
	
	if (rc < 0)
		return rc;
	
	return num;
}

/* Return the parent's functionality */
static u32 sensor_mux_functionality(struct i2c_adapter *adapter)
{
	struct sensor_mux_priv *priv = adapter->algo_data;
	struct i2c_adapter *parent = priv->parent;

	return parent->algo->functionality(parent);
}

static int sensor_mux_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc;
	int int_pin, force_nr = 0;
	struct sensor_mux_priv *priv;

	dev_info(&client->dev, "sensor_mux probe\n");

	priv = kzalloc(sizeof(struct sensor_mux_priv), GFP_KERNEL);
	if (!priv)
		return -1;
	i2c_set_clientdata(client, priv);

	get_u32_property(&client->dev, &force_nr, "cell-index");
	get_u32_property(&client->dev, &int_pin, "interrupt");

	priv->irq = gpio_to_irq(int_pin);
	rc = request_irq(priv->irq, sensor_mux_interrupt,
			IRQF_TRIGGER_HIGH, "sensor_hub_interrupt", priv);
	if (rc) {
		dev_err(&client->dev, "request_irq failed\n");
		kfree(priv);
		return -1;
	}
	disable_irq(priv->irq);

	priv->parent = client->adapter;
	priv->algo.master_xfer = sensor_mux_master_xfer;
	priv->algo.functionality = sensor_mux_functionality;

	snprintf(priv->adapter.name, sizeof(priv->adapter.name), "i2c-sensor-mux");
	priv->adapter.owner = THIS_MODULE;
	priv->adapter.algo = &priv->algo;
	priv->adapter.algo_data = priv;
	priv->adapter.dev.parent = &priv->parent->dev;
	priv->adapter.dev.of_node = client->dev.of_node;
	priv->complete = NULL;

	if (force_nr) {
		priv->adapter.nr = force_nr;
		rc = i2c_add_numbered_adapter(&priv->adapter);
	} else {
		rc = i2c_add_adapter(&priv->adapter);
	}
	if (rc < 0) {
		dev_err(&client->dev, "failed to add mux-adapter (error=%d)\n", rc);
		kfree(priv);
		return -1;
	}
	
	of_i2c_register_devices(&priv->adapter);

	return 0;
}

static int sensor_mux_remove(struct i2c_client *client)
{
	struct sensor_mux_priv *priv = i2c_get_clientdata(client);
	int rc;

	dev_info(&client->dev, "sensor_mux remove\n");

	rc = i2c_del_adapter(&priv->adapter);
	if (rc < 0)
		return rc;
	free_irq(priv->irq, priv);
	kfree(priv);

	return 0;
}

static const struct i2c_device_id sensor_mux_id[]
	= { { SENSOR_MUX_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, sensor_mux_id);

static struct i2c_driver sensor_mux_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_MUX_DEV_NAME,
	},
	.probe = sensor_mux_probe,
	.remove = sensor_mux_remove,
	.id_table = sensor_mux_id,
};
static int __init sensor_mux_init(void)
{
	int rc;
	rc = i2c_add_driver(&sensor_mux_driver);
	return rc;
}

static void __exit sensor_mux_exit(void)
{
    i2c_del_driver(&sensor_mux_driver);
}

module_init(sensor_mux_init);
module_exit(sensor_mux_exit);
MODULE_DESCRIPTION("I2C Sensor MUX");
MODULE_AUTHOR("Balázs Triszka");
MODULE_LICENSE("GPL v2");
