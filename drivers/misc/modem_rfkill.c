/*
 * Copyright (C) 2024 Balázs Triszka <info@balika011.hu>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

static struct rfkill *modem_rfk;
static int modem_gpio_1 = -1;
static int modem_gpio_2 = -1;

static int modem_set_block(void *data, bool blocked)
{
	if (!blocked) {
		gpio_direction_output(modem_gpio_1, 1);
		gpio_direction_output(modem_gpio_2, 1);
	} else {
		gpio_direction_output(modem_gpio_1, 0);
		gpio_direction_output(modem_gpio_2, 0);
	}

	return 0;
}

static struct rfkill_ops modem_rfkill_ops = {
	.set_block = modem_set_block,
};

static int modem_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device_node *node = pdev->dev.of_node;

	if (modem_gpio_1 >= 0 || modem_gpio_2 >= 0) {
		pr_err("GPIO already setup");
		return -EBUSY;
	}

	modem_gpio_1 = of_get_named_gpio(node, "gpio_1", 0);
	if (modem_gpio_1 < 0) {
		pr_err("modem_gpio_1 is not available");
		rc = -ENOTSUPP;
		goto err_end;
	}

	rc = gpio_request(modem_gpio_1, "modem_gpio_1");
	if (rc) {
		pr_err("GPIO req error no=%d", rc);
		gpio_free(modem_gpio_1);
		rc = gpio_request(modem_gpio_1, "modem_gpio_1");
		if(rc) {
			pr_err("GPIO req error no=%d", rc);
			goto err_gpio_reset_1;
		}
	}
	gpio_direction_output(modem_gpio_1, 1);

	modem_gpio_2 = of_get_named_gpio(node, "gpio_2", 0);
	if (modem_gpio_2 < 0) {
		pr_err("modem_gpio_2 is not available");
		rc = -ENOTSUPP;
		goto err_gpio_reset_1;
	}

	rc = gpio_request(modem_gpio_2, "modem_gpio_2");
	if (rc) {
		pr_err("GPIO req error no=%d", rc);
		gpio_free(modem_gpio_2);
		rc = gpio_request(modem_gpio_2, "modem_gpio_2");
		if(rc) {
			pr_err("GPIO req error no=%d", rc);
			goto err_gpio_reset_2;
		}
	}
	gpio_direction_output(modem_gpio_2, 1);

	modem_rfk = rfkill_alloc("modem_rfkill", &pdev->dev, RFKILL_TYPE_MODEM,
			&modem_rfkill_ops, NULL);
	if (!modem_rfk) {
		pr_err("rfkill alloc failed.\n");
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_set_states(modem_rfk, false, false);

	rc = rfkill_register(modem_rfk);
	if (rc)
		goto err_rfkill_reg;

	return 0;


err_rfkill_reg:
	rfkill_destroy(modem_rfk);
err_rfkill_alloc:
err_gpio_reset_2:
	gpio_free(modem_gpio_2);
err_gpio_reset_1:
	gpio_free(modem_gpio_1);
err_end:
	pr_err("modem_rfkill_probe error!\n");
	return rc;
}

static int modem_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(modem_rfk);
	rfkill_destroy(modem_rfk);

	if (modem_gpio_2 >= 0)
		gpio_free(modem_gpio_2);
	modem_gpio_2 = -1;

	if (modem_gpio_1 >= 0)
		gpio_free(modem_gpio_1);
	modem_gpio_1 = -1;

	return 0;
}
struct modem_rfkill_platform_data {
	int gpio_reset;
};

static struct of_device_id modem_rfkill_match_table[] = {
        { .compatible = "modem_rfkill", },
        {}
};


static struct modem_rfkill_platform_data modem_rfkill_platform;

static struct platform_device modem_rfkill_device = {
	.name = "modem_rfkill",
	.id   = -1,
	.dev = {
		.platform_data = &modem_rfkill_platform,
	},
};

static struct platform_driver modem_rfkill_driver = {
	.probe = modem_rfkill_probe,
	.remove = modem_rfkill_remove,
	.driver = {
		.name = "modem_rfkill",
		.owner = THIS_MODULE,
		.of_match_table = modem_rfkill_match_table,
	},
};

static int __init modem_rfkill_init(void)
{
	int ret;

	ret = platform_driver_register(&modem_rfkill_driver);
	if (ret)
		pr_err("Fail to register rfkill platform driver\n");

	return platform_device_register(&modem_rfkill_device);;
}

static void __exit modem_rfkill_exit(void)
{
	platform_device_unregister(&modem_rfkill_device);
	platform_driver_unregister(&modem_rfkill_driver);
}

device_initcall(modem_rfkill_init);
module_exit(modem_rfkill_exit);

MODULE_DESCRIPTION("modem rfkill");
MODULE_AUTHOR("Balázs Triszka <info@balika011.hu>");
MODULE_LICENSE("GPL");
