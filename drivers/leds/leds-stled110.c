#include <linux/module.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>

/*
registers:
0x01 - charge pump state
0x03 - max current
0x04 - fault status
0x0A - * 100 + 4400 - boost voltage
*/

struct stled110_data {
	struct i2c_client 	*client;
	int					bl_en;
	int					ldo_en;
	struct led_classdev	cdev;
	struct work_struct	work;
	struct mutex		lock;
};

static int backlight_rev = -1;
static int __init set_backlight_rev(char *s)
{
	if(strcmp(s, "samantaV2") == 0)
		backlight_rev = 2;
	if(strcmp(s, "samanta") == 0 || strcmp(s, "samantaV1") == 0)
		backlight_rev = 1;

	return 1;
}
__setup("backlight=", set_backlight_rev);

static int stled110_power_on(struct stled110_data *stled)
{
	struct i2c_client *client = stled->client;
	
	if (i2c_smbus_write_byte_data(client, 0x02, 0x7F) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x02\n");
		goto err;
	}
	
	if (i2c_smbus_write_byte_data(client, 0x03, 0x80) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x03\n");
		goto err;
	}
	
	if (i2c_smbus_write_byte_data(client, 0x08, 0x00) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x08\n");
		goto err;
	}
	
	if (i2c_smbus_write_byte_data(client, 0x1b, 0x01) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x1b\n");
		goto err;
	}
	
	if (i2c_smbus_write_byte_data(client, 0x01, 0x31) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x01\n");
		goto err;
	}
	
	if (i2c_smbus_write_byte_data(client, 0x17, 0x10) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x17\n");
		goto err;
	}
	
	return 0;
	
err:
	return -EIO;
}

static int stled110_power_off(struct stled110_data *stled)
{
	struct i2c_client *client = stled->client;
	
	uint8_t R01 = i2c_smbus_read_byte_data(client, 0x01);
	if (R01 < 0) {
		dev_err(&client->dev, "Failed to read register R01, use powered-on default (0x2F/0x27)\n");
		R01 = 0x31;
	}
	
	R01 &= ~0x20;
	if (i2c_smbus_write_byte_data(client, 0x01, R01) < 0) {
		dev_err(&client->dev, "Failed to i2c write reg 0x01\n");
		goto err;
	}
	
	if ((R01 & 4) != 0)
	{
		R01 &= ~4;
		if (i2c_smbus_write_byte_data(client, 0x01, R01) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x01\n");
			goto err;
		}
	}
	
	usleep(1000);
	
	if ((R01 & 2) != 0)
	{
		R01 &= ~2;
		if (i2c_smbus_write_byte_data(client, 0x01, R01) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x01\n");
			goto err;
		}
	}
	
	if ((R01 & 1) != 0)
	{
		usleep(100000);
		R01 &= ~1;
		if (i2c_smbus_write_byte_data(client, 0x01, R01) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x01\n");
			goto err;
		}
	}
	
	return 0;
	
err:
	return R01;
}

static void stled110_set_work(struct work_struct *work)
{
	struct stled110_data *stled = container_of(work, struct stled110_data, work);
	struct i2c_client *client = stled->client;
	int brightness = stled->cdev.brightness;
	int retry;
	
	mutex_lock(&stled->lock);
	
	for (retry = 0; retry < 15; retry++) {
		// 0ms
		if (i2c_smbus_write_byte_data(client, 0x0F, 0x00) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0xf\n");
			goto out;
		}

		if (i2c_smbus_write_byte_data(client, 0x10, brightness & 0xFF) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x10\n");
			goto out;
		}

		if (i2c_smbus_write_byte_data(client, 0x11, (brightness >> 8) & 0xF) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x11\n");
			goto out;
		}

		if (i2c_smbus_write_byte_data(client, 0x12, 0x01) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x12\n");
			goto out;
		}

		usleep(1000);
		
		dev_info(&client->dev, "I: %d vs %d\n", brightness, i2c_smbus_read_byte_data(client, 0x14) | i2c_smbus_read_byte_data(client, 0x15) << 8);
		
		if (i2c_smbus_read_byte_data(client, 0x12) == 0) {
			dev_info(&client->dev, "Succesfully set backlight!\n");
			goto out;
		}

		dev_err(&client->dev, "Failed to set, retry...\n");
	}

	dev_err(&client->dev, "Failed to set, ran out of retries\n");

out:
	mutex_unlock(&stled->lock);
	return;
}

static void stled110_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct stled110_data *stled = container_of(led_cdev, struct stled110_data, cdev);
	struct i2c_client *client = stled->client;

	dev_info(&client->dev, "%s: %d\n", __func__, value);

	if (value < LED_OFF) {
		dev_err(&client->dev, "Invalid brightness value\n");
		return;
	}

	if (value > stled->cdev.max_brightness)
		value = stled->cdev.max_brightness;

	stled->cdev.brightness = value;
	schedule_work(&stled->work);
}

int stled110_resume(struct i2c_client *client)
{
	struct stled110_data *stled = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s\n", __func__);

	if (stled110_power_on(stled) < 0) {
		dev_err(&client->dev, "Power on failed! Unable to recover, turning power off!\n");
		return -EIO;
	}

	stled110_set(&stled->cdev, stled->cdev.max_brightness);
	return 0;
}

int stled110_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct stled110_data *stled = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s\n", __func__);

	stled110_power_off(stled);
	
	return 0;
}

static int __devinit stled110_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct stled110_data *stled;
	int rc;

	dev_info(&client->dev, "%s: backlight_rev = %d\n", __func__, backlight_rev);
	if (backlight_rev < 0)
		return -EINVAL;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	stled = kzalloc(sizeof(*stled), GFP_KERNEL);
	if (!stled)
		return -ENOMEM;
	mutex_init(&stled->lock);
	stled->client = client;
	i2c_set_clientdata(client, stled);

	stled->bl_en = of_get_named_gpio_flags(np, "stled110,bl_en", 0, NULL);
	if (stled->bl_en < 0) {
		dev_err(&client->dev, "%s: Failed to get bl_en!\n", __func__);
		return -EINVAL;
	}

	stled->ldo_en = of_get_named_gpio_flags(np, "stled110,ldo_en", 0, NULL);
	if (stled->ldo_en < 0) {
		dev_err(&client->dev, "%s: Failed to get ldo_en!\n", __func__);
		return -EINVAL;
	}

#if 0
	if (backlight_rev == 1)
	{
		uint8_t cmd_buf[] = { 0x40, 0xAA };
		struct i2c_msg cmd = {
			.len = sizeof(cmd_buf),
			.buf = cmd_buf,
			.addr = client->addr,
			.flags = I2C_M_IGNORE_NAK
		};

		gpio_set_value(stled->bl_en, 0);
		gpio_set_value(stled->ldo_en, 0);

		usleep(10000);
		
		gpio_set_value(stled->ldo_en, 1);
		gpio_set_value(stled->bl_en, 1);

		usleep(1000);

		i2c_transfer(adapter, &cmd, 1);

		usleep(1000);

		if (i2c_smbus_write_byte_data(client, 0x47, 1) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x47\n");
			goto err;
		}

		usleep(1000);

		if (i2c_smbus_write_byte_data(client, 0x48, 0xFF) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x48\n");
			goto err;
		}

		usleep(1000);

		if (i2c_smbus_write_byte_data(client, 0x33, 0x12) < 0) {
			dev_err(&client->dev, "Failed to i2c write reg 0x33\n");
			goto err;
		}

		usleep(1000);
	} else {
		uint8_t cmd_buf[] = { 0x48, 0x1F };
		struct i2c_msg cmd = {
			.len = sizeof(cmd_buf),
			.buf = cmd_buf,
			.addr = client->addr,
			.flags = I2C_M_IGNORE_NAK
		};

		gpio_set_value(stled->bl_en, 0);
		gpio_set_value(stled->ldo_en, 0);

		usleep(1000);

		gpio_set_value(stled->bl_en, 1);
		gpio_set_value(stled->ldo_en, 1);

		usleep(1000);

		i2c_transfer(adapter, &cmd, 1);

		usleep(10000);
	}
	
	if (stled110_power_on(stled) < 0)
		goto err;
#else	
	gpio_set_value(stled->ldo_en, 1);
	gpio_set_value(stled->bl_en, 1);
#endif

	INIT_WORK(&stled->work, stled110_set_work);

	stled->cdev.name = of_get_property(np, "label", NULL) ? : "stled110";
	stled->cdev.brightness_set = stled110_set;
	stled->cdev.max_brightness = 4095;
	stled->cdev.default_trigger = of_get_property(np, "linux,default-trigger", NULL);
	rc = led_classdev_register(&client->dev, &stled->cdev);
	if (rc) {
		dev_err(&client->dev, "unable to register led, rc=%d\n", rc);
		goto err;
	}
	
	stled->cdev.brightness = stled->cdev.max_brightness;

	return 0;
	
err:
	return -EINVAL;
}

static int __devexit stled110_remove(struct i2c_client *client)
{
	struct stled110_data *stled = i2c_get_clientdata(client);
	pm_message_t mesg = {
		.event = 0
	};

	stled110_suspend(client, mesg);

	gpio_set_value(stled->bl_en, 0);
	gpio_set_value(stled->ldo_en, 0);

	led_classdev_unregister(&stled->cdev);
	cancel_work_sync(&stled->work);

	gpio_free(stled->bl_en);
	gpio_free(stled->ldo_en);

	kfree(stled);
	return 0;
}

static struct of_device_id stled110_match_table[] = {
	{ .compatible = "stled110" },
	{ },
};

static const struct i2c_device_id stled110_id[] = {
	{ "stled110" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, stled110_id);

static struct i2c_driver stled110_driver = {
	.driver   = {
		.name    = "leds-stled110",
		.owner   = THIS_MODULE,
		.of_match_table = stled110_match_table,
	},
	.probe    = stled110_probe,
	.remove   = __devexit_p(stled110_remove),
	.id_table = stled110_id,
	.resume   = stled110_resume,
	.suspend  = stled110_suspend,
};

static int __init stled110_leds_init(void)
{
	return i2c_add_driver(&stled110_driver);
}

static void __exit stled110_leds_exit(void)
{
	i2c_del_driver(&stled110_driver);
}

module_init(stled110_leds_init);
module_exit(stled110_leds_exit);

MODULE_AUTHOR("Balázs Triszka <info@balika011.hu>");
MODULE_DESCRIPTION("STLED110 driver");
MODULE_LICENSE("GPL v2");
