/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>

static u8 board_rev = 1;
static int __init set_board_rev(char *s)
{
	int temp;
	int rc = kstrtouint(s, 0, &temp);
	if (rc < 0)
		return rc;
	board_rev = (temp >> 16) & 0xFF;
	
	// Workaround for wolverine rev 4 override.
	// Another bootloader bug...
	if (board_rev == 0)
		board_rev = temp & 0xFF;
		
	return 0;
}
__setup("androidboot.binfo.rev=", set_board_rev);

static struct gpiomux_setting gpiomux_out_low_2ma = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting gpiomux_out_high_2ma = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting gpiomux_in_pulldown = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpiomux_in_pullnone = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpiomux_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	/*
	 * Please keep I2C GPIOs drive-strength at minimum (2ma). It is a
	 * workaround for HW issue of glitches caused by rapid GPIO current-
	 * change.
	 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpiomux_in_pullup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_2_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc3_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc3_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc3_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpiomux_fct3_bt_pullup = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gpiomux_fct3_bt_pulldown = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpiomux_fct1_bt_pcm = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpiomux_fct4 = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
};

struct bbry_msm_gpiomux_config
{
	u8 min_rev;
	u8 max_rev;
	struct msm_gpiomux_config config;
};

static struct bbry_msm_gpiomux_config msm_gpio_common[] =
{
	{ 1, 0xFF, { 1,   { &gpiomux_in_pullup, NULL } } }, /* keypad irq */
	// { 1, 0xFF, { 2,   { &gpiomux_i2c_config, NULL } } }, /* i2c_1 sda */
	// { 1, 0xFF, { 3,   { &gpiomux_i2c_config, NULL } } }, /* i2c_1 scl */
	{ 1, 0xFF, { 6,   { &gpiomux_i2c_config, NULL } } }, /* touchscreen sda */
	{ 1, 0xFF, { 7,   { &gpiomux_i2c_config, NULL } } }, /* touchscreen scl */
	{ 1, 0xFF, { 8,   { &gpiomux_out_high_2ma, NULL } } },   /* display enable */
	{ 1, 0xFF, { 10,  { &gpiomux_i2c_config, NULL } } }, /* stled110 sda */
	{ 1, 0xFF, { 11,  { &gpiomux_i2c_config, NULL } } }, /* stled110 scl */
	//{ 1, 0xFF, { 10,  { &gpiomux_in_pullnone, NULL } } }, /* stled110 sda */
	//{ 1, 0xFF, { 11,  { &gpiomux_in_pullnone, NULL } } }, /* stled110 scl */
	{ 1, 0xFF, { 13,  { &gpiomux_out_low_2ma, NULL } } }, /* bt_ext_wake */
	{ 1, 0xFF, { 16,  { &gpiomux_out_high_2ma, NULL } } }, /* anx7808 power down */
	{ 1, 0xFF, { 18,  { &gpiomux_in_pulldown, NULL } } }, /* bt_host_wake */
	{ 1, 0xFF, { 25,  { &gpiomux_out_high_2ma, NULL } } }, /* stmvl6180 reset */
	{ 1, 0xFF, { 29,  { &gpiomux_i2c_config, NULL } } }, /* anx sda */
	{ 1, 0xFF, { 30,  { &gpiomux_i2c_config, NULL } } }, /* anx scl */
	{ 1, 0xFF, { 31,  { &hdmi_active_1_cfg, &hdmi_suspend_cfg } } },
	{ 1, 0xFF, { 32,  { &hdmi_active_1_cfg, &hdmi_suspend_cfg } } },
	{ 1, 0xFF, { 33,  { &hdmi_active_1_cfg, &hdmi_suspend_cfg } } },
	{ 1, 0xFF, { 34,  { &hdmi_active_2_cfg, &hdmi_suspend_cfg } } },
	{ 1, 0xFF, { 35,  { &sdc3_cmd_data_0_3_actv_cfg, &sdc3_suspend_cfg } } }, /* DAT3 */
	{ 1, 0xFF, { 36,  { &sdc3_cmd_data_0_3_actv_cfg, &sdc3_suspend_cfg } } }, /* DAT2 */
	{ 1, 0xFF, { 37,  { &sdc3_cmd_data_0_3_actv_cfg, &sdc3_suspend_cfg } } }, /* DAT1 */
	{ 1, 0xFF, { 38,  { &sdc3_cmd_data_0_3_actv_cfg, &sdc3_suspend_cfg } } }, /* DAT0 */
	{ 1, 0xFF, { 39,  { &sdc3_cmd_data_0_3_actv_cfg, &sdc3_suspend_cfg } } }, /* CMD */
	{ 1, 0xFF, { 40,  { &sdc3_clk_actv_cfg, &sdc3_suspend_cfg } } },          /* CLK */
	{ 1, 0xFF, { 41,  { &gpiomux_fct3_bt_pullup, &gpiomux_in_pulldown } } },   /* bt TX */
	{ 1, 0xFF, { 42,  { &gpiomux_fct3_bt_pulldown, &gpiomux_in_pulldown } } }, /* bt RX */
	{ 1, 0xFF, { 43,  { &gpiomux_fct3_bt_pulldown, &gpiomux_in_pulldown } } }, /* bt CTS */
	{ 1, 0xFF, { 44,  { &gpiomux_fct3_bt_pullup, &gpiomux_in_pulldown } } },   /* bt RFR */
	{ 1, 0xFF, { 46,  { &gpiomux_in_pulldown, NULL } } },  /* display resx */
	// { 1, 0xFF, { 47,  { &gpiomux_out_high_2ma, NULL } } }, /* radio killswitch */
	{ 1, 0xFF, { 48,  { &gpiomux_out_low_2ma, NULL } } },
	{ 1, 0xFF, { 49,  { &gpiomux_out_high_2ma, NULL } } }, /* display reset */
	{ 1, 0xFF, { 51,  { &gpiomux_fct4, NULL } } }, /* keypad sda */
	{ 1, 0xFF, { 52,  { &gpiomux_fct4, NULL } } }, /* keypad scl */
	{ 1, 0xFF, { 54,  { &gpiomux_in_pulldown, NULL } } }, /* anx7808 cable detect */
	{ 1, 0xFF, { 55,  { &gpiomux_out_high_2ma, NULL } } }, /* anx7808 reset */
	{ 1, 0xFF, { 56,  { &gpiomux_out_high_2ma, NULL } } }, /* nfc wake up */
	{ 1, 0xFF, { 57,  { &gpiomux_out_low_2ma, NULL } } }, /* wlan pwr */
	{ 1, 0xFF, { 62,  { &gpiomux_in_pulldown, NULL } } }, /* anx7808 interrupt */
	{ 1, 0xFF, { 64,  { &gpiomux_in_pullup, NULL } } },        /* display irq */
	{ 1, 0xFF, { 67,  { &gpiomux_in_pullnone, NULL } } }, /* sensor hub interrupt */
	{ 1, 0xFF, { 66,  { &gpiomux_in_pullnone, NULL } } }, /* l3gd20_gyr irq */
	{ 1, 0xFF, { 68,  { &gpiomux_in_pullup, NULL } } }, /* hall sensor */
	{ 1, 0xFF, { 74,  { &gpiomux_in_pullnone, NULL } } }, /* stmvl6180 irq */
	{ 1, 0xFF, { 77,  { &gpiomux_in_pullup, NULL } } }, /* wlan wake up */
	{ 1, 0xFF, { 78,  { &gpiomux_out_low_2ma, NULL } } }, /* anx7808 3.3v control */
	{ 1, 0xFF, { 79,  { &gpiomux_fct1_bt_pcm, NULL } } }, /* bluetooth pcm audio */
	{ 1, 0xFF, { 80,  { &gpiomux_fct1_bt_pcm, NULL } } }, /* bluetooth pcm audio */
	{ 1, 0xFF, { 81,  { &gpiomux_fct1_bt_pcm, NULL } } }, /* bluetooth pcm audio */
	{ 1, 0xFF, { 82,  { &gpiomux_fct1_bt_pcm, NULL } } }, /* bluetooth pcm audio */
	{ 1, 0xFF, { 83,  { &gpiomux_i2c_config, NULL } } }, /* nfc sda */
	{ 1, 0xFF, { 84,  { &gpiomux_i2c_config, NULL } } }, /* nfc scl */
	{ 1, 0xFF, { 87,  { &gpiomux_i2c_config, NULL } } }, /* sensors sda */
	{ 1, 0xFF, { 88,  { &gpiomux_i2c_config, NULL } } }, /* sensors scl */
	// { 1, 0xFF, { 89,  { &gpiomux_out_high_2ma, NULL } } }, /* bt rf kill v5 +*/
	{ 1, 0xFF, { 91,  { &gpiomux_in_pullup, NULL } } },
	{ 1, 0xFF, { 100, { &gpiomux_in_pullnone, NULL } } },
	{ 1, 0xFF, { 103, { &gpiomux_out_low_2ma, NULL } } }, /* 5v_boost_ext enable */
	// { 1, 0xFF, { 108, { &gpiomux_out_high_2ma, NULL } } }, /* 4g killswitch */
	{ 1, 0xFF, { 132, { &gpiomux_in_pulldown, NULL } } },
	{ 1, 0xFF, { 145, { &gpiomux_out_low_2ma, NULL } } } /* keypad reset */
};

static struct bbry_msm_gpiomux_config msm_gpio_wichita[] =
{
	//{ 1, 0xFF, { 94,  { &gpiomux_out_high_2ma, NULL } } }, /* stled110 ldo_en */
	// { 1, 0xFF, { 50,  { &gpiomux_out_low_2ma, NULL } } }, /* bt_rfkill v4 -*/
	{ 1, 0xFF, { 96,  { &gpiomux_out_low_2ma, NULL } } }, /* speaker amp enable */
	//{ 1, 0xFF, { 109, { &gpiomux_out_high_2ma, NULL } } }, /* stled110 bl_en */
};

static struct bbry_msm_gpiomux_config msm_gpio_wolverine[] =
{
	// { 1, 4,    { 0,   { &gpiomux_out_high_2ma, NULL } } }, /* stled110 ldo_en */
	// { 1, 0xFF, { 9,   { &gpiomux_out_high_2ma, NULL } } }, /* stled110 bl_en */
	{ 1, 3,    { 45,  { &gpiomux_in_pullnone, NULL } } }, { 4, 0xFF, { 45, { &gpiomux_in_pulldown, NULL } } },
	// { 1, 4,    { 50,  { &gpiomux_out_low_2ma, NULL } } }, /* bt_rfkill v4 -*/
	{ 2, 0xFF, { 86,  { &gpiomux_in_pullnone, NULL } } },
	// { 5, 0xFF, { 89,  { &gpiomux_out_low_2ma, NULL } } }, /* bt_rfkill v5 +*/
	{ 2, 0xFF, { 90,  { &gpiomux_out_high_2ma, NULL } } },
	{ 2, 0xFF, { 95,  { &gpiomux_in_pullnone, NULL } } },
	{ 2, 0xFF, { 102, { &gpiomux_out_low_2ma, NULL } } }, /* usb_vbus_en enable */
	{ 2, 0xFF, { 144, { &gpiomux_in_pullnone, NULL } } }
};

static struct bbry_msm_gpiomux_config msm_gpio_oslo[] =
{
	{ 1, 0xFF, { 86,  { &gpiomux_in_pullnone, NULL } } },
	// { 1, 0xFF, { 45, { &gpiomux_out_high_2ma, NULL } } }, /* jack uart switch */
	{ 1, 0xFF, { 89,  { &gpiomux_out_low_2ma, NULL } } }, /* bt_rfkill */
	{ 1, 0xFF, { 90,  { &gpiomux_out_high_2ma, NULL } } },
	{ 1, 0xFF, { 95,  { &gpiomux_in_pullnone, NULL } } },
	{ 1, 0xFF, { 96,  { &gpiomux_out_high_2ma, NULL } } }, /* speaker amp enable */
	{ 1, 0xFF, { 102, { &gpiomux_out_low_2ma, NULL } } }, /* usb_vbus_en enable */
	{ 1, 0xFF, { 144, { &gpiomux_in_pullnone, NULL } } }
};

static struct bbry_msm_gpiomux_config msm_cam_configs[] =
{
	{ 1, 0xFF, { 14, { &cam_settings[3], &cam_settings[4]  } } }, /* CAM_RESET12 */
	{ 1, 0xFF, { 15, { &cam_settings[0], &cam_settings[1] } } }, /* CAM_MCLK0 */
	{ 1, 0xFF, { 17, { &cam_settings[0], &cam_settings[1] } } }, /* CAM_MCLK2 */
	{ 1, 0xFF, { 19, { &cam_settings[0],  &cam_settings[1]  } } }, /* CCI_I2C_SDA0 */
	{ 1, 0xFF, { 20, { &cam_settings[0],  &cam_settings[1]  } } }, /* CCI_I2C_SCL0 */
	{ 1, 0xFF, { 21, { &cam_settings[0],  &cam_settings[1]  } } }, /* CCI_I2C_SDA1 */
	{ 1, 0xFF, { 22, { &cam_settings[0],  &cam_settings[1]  } } }, /* CCI_I2C_SCL1 */
	{ 1, 0xFF, { 23, { &cam_settings[3], &cam_settings[4]  } } }, /* CAM_RESET10 */
	{ 1, 0xFF, { 24, { &cam_settings[0], &gpiomux_in_pullnone  } } },
	{ 1, 0xFF, { 27, { &cam_settings[3], &cam_settings[4]  } } }  /* CAM_LDO_EN */
};

void bbry_init_install_gpio(struct bbry_msm_gpiomux_config *configs, int nconfigs, unsigned int rev)
{
	int i;

	for (i = 0; i < nconfigs; ++i)
	{
		if (configs[i].min_rev <= rev && configs[i].max_rev >= rev)
			msm_gpiomux_install(&configs[i].config, 1);
	}
}

enum hw_types
{
	WICHITA,
	WOLVERINE,
	OSLO
};

int get_hw_type(void)
{
	if (of_find_compatible_node(NULL, NULL, "qcom,msm8974-wichita"))
		return WICHITA;

	if (of_find_compatible_node(NULL, NULL, "qcom,msm8974-wolverine"))
		return WOLVERINE;

	if (of_find_compatible_node(NULL, NULL, "qcom,msm8974-oslo"))
		return OSLO;

	return -1;
}

void __init msm_8974_init_gpiomux(void)
{
	int rc, board_type;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}
	
	board_type = get_hw_type();

	pr_err("%s:%d board_type %x rev %x\n", __func__, __LINE__,
		board_type, board_rev);

	pr_err("%s:%d socinfo_get_version %x\n", __func__, __LINE__,
		socinfo_get_version());
	if (socinfo_get_version() >= 0x20000)
		msm_tlmm_misc_reg_write(TLMM_SPARE_REG, 0xf);

	bbry_init_install_gpio(msm_gpio_common, ARRAY_SIZE(msm_gpio_common), board_rev);

	if (board_type == WICHITA) {
		bbry_init_install_gpio(msm_gpio_wichita, ARRAY_SIZE(msm_gpio_wichita), board_rev);
	} else if (board_type == WOLVERINE) {
		bbry_init_install_gpio(msm_gpio_wolverine, ARRAY_SIZE(msm_gpio_wolverine), board_rev);
	} else if (board_type == OSLO) {
		bbry_init_install_gpio(msm_gpio_oslo, ARRAY_SIZE(msm_gpio_oslo), board_rev);
	}

	bbry_init_install_gpio(msm_cam_configs, ARRAY_SIZE(msm_cam_configs), board_rev);
}
