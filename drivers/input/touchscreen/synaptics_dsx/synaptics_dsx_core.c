/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <synaptics_dsx.h>
#include "synaptics_dsx_core.h"
#include <linux/input/mt.h>

#include "synaptics_dsx_core.h"
#include "synaptics_dsx_fw_update.h"

#define INPUT_PHYS_NAME "synaptics_dsx/touch_input"
// #define REPORT_2D_Z
#define REPORT_2D_W

#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define EXP_FN_WORK_DELAY_MS 1000 /* ms */
#define MAX_F11_TOUCH_WIDTH 15
#define MAX_Z 255

#define CHECK_STATUS_TIMEOUT_MS 100

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18

#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CONFIGURED (1 << 7)

static int synaptics_rmi4_f12_set_enables(struct synaptics_rmi4_data *rmi4_data,
		unsigned short ctrl28);

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data);

static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_0_5 {
	union {
		struct {
			/* query 0 */
			unsigned char f11_query0_b0__2:3;
			unsigned char has_query_9:1;
			unsigned char has_query_11:1;
			unsigned char has_query_12:1;
			unsigned char has_query_27:1;
			unsigned char has_query_28:1;

			/* query 1 */
			unsigned char num_of_fingers:3;
			unsigned char has_rel:1;
			unsigned char has_abs:1;
			unsigned char has_gestures:1;
			unsigned char has_sensitibity_adjust:1;
			unsigned char f11_query1_b7:1;

			/* query 2 */
			unsigned char num_of_x_electrodes;

			/* query 3 */
			unsigned char num_of_y_electrodes;

			/* query 4 */
			unsigned char max_electrodes:7;
			unsigned char f11_query4_b7:1;

			/* query 5 */
			unsigned char abs_data_size:2;
			unsigned char has_anchored_finger:1;
			unsigned char has_adj_hyst:1;
			unsigned char has_dribble:1;
			unsigned char has_bending_correction:1;
			unsigned char has_large_object_suppression:1;
			unsigned char has_jitter_filter:1;
		} __packed;
		unsigned char data[6];
	};
};

struct synaptics_rmi4_f11_query_7_8 {
	union {
		struct {
			/* query 7 */
			unsigned char has_single_tap:1;
			unsigned char has_tap_and_hold:1;
			unsigned char has_double_tap:1;
			unsigned char has_early_tap:1;
			unsigned char has_flick:1;
			unsigned char has_press:1;
			unsigned char has_pinch:1;
			unsigned char has_chiral_scroll:1;

			/* query 8 */
			unsigned char has_palm_detect:1;
			unsigned char has_rotate:1;
			unsigned char has_touch_shapes:1;
			unsigned char has_scroll_zones:1;
			unsigned char individual_scroll_zones:1;
			unsigned char has_multi_finger_scroll:1;
			unsigned char has_multi_finger_scroll_edge_motion:1;
			unsigned char has_multi_finger_scroll_inertia:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f11_query_9 {
	union {
		struct {
			unsigned char has_pen:1;
			unsigned char has_proximity:1;
			unsigned char has_large_object_sensitivity:1;
			unsigned char has_suppress_on_large_object_detect:1;
			unsigned char has_two_pen_thresholds:1;
			unsigned char has_contact_geometry:1;
			unsigned char has_pen_hover_discrimination:1;
			unsigned char has_pen_hover_and_edge_filters:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_12 {
	union {
		struct {
			unsigned char has_small_object_detection:1;
			unsigned char has_small_object_detection_tuning:1;
			unsigned char has_8bit_w:1;
			unsigned char has_2d_adjustable_mapping:1;
			unsigned char has_general_information_2:1;
			unsigned char has_physical_properties:1;
			unsigned char has_finger_limit:1;
			unsigned char has_linear_cofficient_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_27 {
	union {
		struct {
			unsigned char f11_query27_b0:1;
			unsigned char has_pen_position_correction:1;
			unsigned char has_pen_jitter_filter_coefficient:1;
			unsigned char has_group_decomposition:1;
			unsigned char has_wakeup_gesture:1;
			unsigned char has_small_finger_correction:1;
			unsigned char has_data_37:1;
			unsigned char f11_query27_b7:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_ctrl_6_9 {
	union {
		struct {
			unsigned char sensor_max_x_pos_7_0;
			unsigned char sensor_max_x_pos_11_8:4;
			unsigned char f11_ctrl7_b4__7:4;
			unsigned char sensor_max_y_pos_7_0;
			unsigned char sensor_max_y_pos_11_8:4;
			unsigned char f11_ctrl9_b4__7:4;
		} __packed;
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f11_data_1_5 {
	union {
		struct {
			unsigned char x_position_11_4;
			unsigned char y_position_11_4;
			unsigned char x_position_3_0:4;
			unsigned char y_position_3_0:4;
			unsigned char wx:4;
			unsigned char wy:4;
			unsigned char z;
		} __packed;
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl32_is_present:1;
				unsigned char ctrl33_is_present:1;
				unsigned char ctrl34_is_present:1;
				unsigned char ctrl35_is_present:1;
				unsigned char ctrl36_is_present:1;
				unsigned char ctrl37_is_present:1;
				unsigned char ctrl38_is_present:1;
				unsigned char ctrl39_is_present:1;
			} __packed;
		};
		unsigned char data[6];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __packed;
		};
		unsigned char data[3];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
			unsigned char sensor_flags;
		};
		unsigned char data[15];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char obj_type_enable;
			unsigned char max_reported_objects;
		};
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
#ifdef REPORT_2D_Z
	unsigned char z;
#endif
#ifdef REPORT_2D_W
	unsigned char wx;
	unsigned char wy;
#endif
};

static struct device_attribute attrs[] = {
	__ATTR(reset, S_IWUGO,
			synaptics_rmi4_show_error,
			synaptics_rmi4_f01_reset_store),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUGO,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(suspend, S_IWUGO,
			synaptics_rmi4_show_error,
			synaptics_rmi4_suspend_store),
};

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			(rmi4_data->rmi4_mod_info.product_info[0]),
			(rmi4_data->rmi4_mod_info.product_info[1]));
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->firmware_id);
}

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
			device_status.flash_prog);
}

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		synaptics_rmi4_suspend(dev);
	else if (input == 0)
		synaptics_rmi4_resume(dev);
	else
		return -EINVAL;

	return count;
}

static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char finger_status_reg[3];
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
#ifdef REPORT_2D_Z
	int z;
#endif
#ifdef REPORT_2D_W
	int wx;
	int wy;
#endif
	int temp;
	struct synaptics_rmi4_f11_data_1_5 data;

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0)
		return retval;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
				& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, finger_status != 0);

		if (finger_status) {
			data_offset = data_addr +
					num_of_finger_status_regs +
					(finger * sizeof(data.data));
			retval = synaptics_rmi4_reg_read(rmi4_data,
					data_offset,
					data.data,
					sizeof(data.data));
			if (retval < 0) {
				mutex_unlock(&rmi4_data->rmi4_report_mutex);
				return retval;
			}

			x = (data.x_position_11_4 << 4) | data.x_position_3_0;
			y = (data.y_position_11_4 << 4) | data.y_position_3_0;
#ifdef REPORT_2D_Z
			z = data.z;
#endif
#ifdef REPORT_2D_W
			wx = data.wx;
			wy = data.wy;
#endif

			if (rmi4_data->hw_if->board_data->swap_axes) {
				temp = x;
				x = y;
				y = temp;
#ifdef REPORT_2D_W
				temp = wx;
				wx = wy;
				wy = temp;
#endif
			}

			if (rmi4_data->hw_if->board_data->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->hw_if->board_data->y_flip)
				y = rmi4_data->sensor_max_y - y;
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifdef REPORT_2D_Z
			/* report 0xf9 (MAX_Z-6) for edge and 0x00 - 0xf6 (MAX_Z-9) for the reset */
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_PRESSURE, z);
#endif

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);

			input_sync(rmi4_data->input_dev);

			touch_count++;
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);

		dev_dbg(rmi4_data->pdev->dev.parent, "%s: Touch UP\n", __func__);
		input_sync(rmi4_data->input_dev);
	}


	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return touch_count;
}


static int synaptics_rmi4_cancel_touch(
	struct synaptics_rmi4_data *rmi4_data,
	unsigned char   finger)
{

	input_mt_slot(rmi4_data->input_dev, finger);
	input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 1);
#ifdef REPORT_2D_Z
	/* report 0xfd for cancel */
	input_report_abs(rmi4_data->input_dev, ABS_MT_PRESSURE, MAX_Z-2);
#endif
	input_sync(rmi4_data->input_dev);
	return 0;
}

static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned short data_addr;
	int x;
	int y;
#ifdef REPORT_2D_Z
	int z;
#endif
#ifdef REPORT_2D_W
	int wx;
	int wy;
#endif
	int temp;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;

	fingers_to_process = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	/* Determine the total number of fingers to process */
	if (extra_data->data15_size) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data15_offset,
				extra_data->data15_data,
				extra_data->data15_size);
		if (retval < 0)
			return retval;

		/* Start checking from the highest bit */
		temp = extra_data->data15_size - 1; /* Highest byte */
		finger = (fingers_to_process - 1) % 8; /* Highest bit */
		do {
			if (extra_data->data15_data[temp] & (1 << finger))
				break;

			if (finger) {
				finger--;
			} else {
				temp--; /* Move to the next lower byte */
				finger = 7;
			}

			fingers_to_process--;
		} while (fingers_to_process);

		dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of fingers to process = %d\n",
			__func__, fingers_to_process);
	}

	if (!fingers_to_process) {
		if (false != rmi4_data->ignore_touch) {
			dev_info(rmi4_data->pdev->dev.parent,
				"%s: clear ignore_touch\n", __func__);
			rmi4_data->ignore_touch = false;
		}
		synaptics_rmi4_free_fingers(rmi4_data);
		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr + extra_data->data1_offset,
			(unsigned char *)fhandler->data,
			fingers_to_process * size_of_2d_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: failed to read finger data, return %d\n",
			__func__, retval);
		return retval;
	}

	data = (struct synaptics_rmi4_f12_finger_data *)fhandler->data;

	/* if we are currently ignoring touch, check if all fingers are released */
	touch_count = 0;
	for (finger = 0; finger < fingers_to_process; finger++) {
		struct synaptics_rmi4_f12_finger_data *finger_data = data + finger;
		finger_status = finger_data->object_type_and_status;
		
		if (finger_status != F12_NO_OBJECT_STATUS)
			touch_count++;
	}

	if (rmi4_data->ignore_touch) {
		if (touch_count > 0) {
			/* simply ignore the touches while ignore_touch is set */
			dev_dbg(rmi4_data->pdev->dev.parent, "%s: ignore_touch=%d, ignoring touches\n",	__func__, rmi4_data->ignore_touch);
			return 0;
		}

		dev_info(rmi4_data->pdev->dev.parent, "%s: reset ignore_touch to false\n", __func__);
		rmi4_data->ignore_touch = false;
	}

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	touch_count = 0;
	for (finger = 0; finger < fingers_to_process; finger++) {
		struct synaptics_rmi4_f12_finger_data *finger_data = data + finger;
		finger_status = finger_data->object_type_and_status;

		dev_dbg(rmi4_data->pdev->dev.parent, "%s: finger=%d finger_status=%x\n",
				__func__, finger, finger_status);

		switch (finger_status) {
		case F12_FINGER_STATUS:
		case F12_GLOVED_FINGER_STATUS:
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 1);

			x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
			y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
#ifdef REPORT_2D_Z
			z = finger_data->z;
#endif
#ifdef REPORT_2D_W
			wx = finger_data->wx;
			wy = finger_data->wy;
#endif

			if (rmi4_data->hw_if->board_data->swap_axes) {
				temp = x;
				x = y;
				y = temp;
#ifdef REPORT_2D_W
				temp = wx;
				wx = wy;
				wy = temp;
#endif
			}

			if (rmi4_data->hw_if->board_data->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->hw_if->board_data->y_flip)
				y = rmi4_data->sensor_max_y - y;

			dev_dbg(rmi4_data->pdev->dev.parent, "%s: finger=%d x=%d y=%d ma=%d mi=%d\n",
					__func__, finger, x, y, max(wx, wy), min(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifdef REPORT_2D_Z
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_PRESSURE, z);
#endif

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);

			input_sync(rmi4_data->input_dev);

			touch_count++;
			break;
		case F12_PALM_STATUS:
			synaptics_rmi4_cancel_touch(rmi4_data, finger);
			touch_count++;
			rmi4_data->ignore_touch = true;
			dev_info(rmi4_data->pdev->dev.parent,
				"Large object detected\n");
			break;
		default:
			input_mt_slot(rmi4_data->input_dev, finger);
			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 0);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
			dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Finger release f=%d, I=%d,S=%d\n",
				__func__, fingers_to_process, finger, finger_status);
			break;
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);

		dev_dbg(rmi4_data->pdev->dev.parent, "%s: Touch UP\n", __func__);
		input_sync(rmi4_data->input_dev);
	}


	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return touch_count;
}

static int synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval = 0;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x reporting\n",
			__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
	case SYNAPTICS_RMI4_F11:
		retval = synaptics_rmi4_f11_abs_report(rmi4_data,
				fhandler);
		if (retval < 0)
			return retval;

		if (retval)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F12:
		retval = synaptics_rmi4_f12_abs_report(rmi4_data,
				fhandler);
		if (retval < 0)
			return retval;

		if (retval)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	default:
		break;
	}

	return retval;
}

int mdss_fb_send_panel_dead(int fb_num);

static int synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	unsigned char *intr = &data[1];
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;
	struct device *dev = rmi4_data->pdev->dev.parent;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			data,
			rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to read interrupt status\n",
				__func__);
		return retval;
	}

	status.data[0] = data[0];
	if (status.unconfigured && !status.flash_prog) {
		dev_info(dev, "%s: Reconfiguring controller\n", __func__);
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(dev,
					"%s: Failed to reinit device\n",
					__func__);
			return retval;
		}
	}

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask &
						intr[fhandler->intr_reg_num]) {
					retval = synaptics_rmi4_report_touch(rmi4_data, fhandler);
					if (retval < 0) {
						dev_err(dev,
						"%s: Failed to report "
						"touch\n", __func__);
						return retval;
					}

					switch (fhandler->fn_number) {
					case SYNAPTICS_RMI4_F01:
						if (status.status_code == STATUS_RESET_OCCURRED) {
							return -EAGAIN;
						} else if (status.status_code != STATUS_NO_ERROR) {
							dev_err(dev,
								"%s: Status error %d\n",
								__func__, status.status_code);
							return -EAGAIN;
						}
						break;
					default:
						break;
					}
				}
			}
		}
	}

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry(exp_fhandler, &rmi4_data->exp_data.list, link) {
			if (!exp_fhandler->insert &&
					!exp_fhandler->remove &&
					(exp_fhandler->exp_fn->attn != NULL))
				retval = exp_fhandler->exp_fn->attn(rmi4_data, intr[0]);
			if (retval)
				break;
		}
	}
	mutex_unlock(&rmi4_data->exp_data.mutex);

	return retval;
}

static void synaptics_rmi4_diable_irq_nosync(
	int irq, struct synaptics_rmi4_data *rmi4_data, bool disable)
{
	if (disable) {
		disable_irq_nosync(irq);
	} else {
		enable_irq(irq);
	}
}

static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;

	synaptics_rmi4_diable_irq_nosync(irq, rmi4_data, true);
	queue_work(rmi4_data->workqueue, &rmi4_data->irq_work);

	return IRQ_HANDLED;
}

static void synaptics_rmi4_irq_work(struct work_struct *work)
{
	struct synaptics_rmi4_data *rmi4_data =
			container_of(work, struct synaptics_rmi4_data,
					irq_work);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	mutex_lock(&rmi4_data->rmi4_irq_mutex);
	if (true != rmi4_data->irq_enabled) {
		/* IRQ is disabled. Must be a race conditon */
		dev_dbg(rmi4_data->pdev->dev.parent,
			"IRQ is disabled. exit.\n");
		mutex_unlock(&rmi4_data->rmi4_irq_mutex);
		goto exit;
	}
	mutex_unlock(&rmi4_data->rmi4_irq_mutex);

	if (gpio_get_value(bdata->irq_gpio) != bdata->irq_on_state) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s:No interrupt (irq_gpio = %d), abandon\n",
				__func__,
				gpio_get_value(bdata->irq_gpio));
		synaptics_rmi4_diable_irq_nosync(rmi4_data->irq, rmi4_data, false);
		goto exit;
	}

	synaptics_rmi4_sensor_report(rmi4_data);
	synaptics_rmi4_diable_irq_nosync(rmi4_data->irq, rmi4_data, false);

exit:
	return;
}

static int synaptics_rmi4_int_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char ii;
	unsigned char zero = 0x00;
	unsigned char *intr_mask;
	unsigned short intr_addr;

	intr_mask = rmi4_data->intr_mask;

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (intr_mask[ii] != 0x00) {
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			if (enable) {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&(intr_mask[ii]),
						sizeof(intr_mask[ii]));
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to write interrupt mask\n",
						__func__);
					return retval;
				}
			} else {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&zero,
						sizeof(zero));
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to zero interrupt mask\n",
						__func__);
					return retval;
				}
			}
		}
	}

	if (retval > 0)
		retval = 0;
	return retval;
}

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable, bool attn_only)
{
	int retval = 0;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (attn_only) {
		retval = synaptics_rmi4_int_enable(rmi4_data, enable);
		return retval;
	}

	mutex_lock(&rmi4_data->rmi4_irq_mutex);
	if (enable) {
		if (rmi4_data->irq_enabled) {
			mutex_unlock(&rmi4_data->rmi4_irq_mutex);
			return retval;
		}

		retval = synaptics_rmi4_int_enable(rmi4_data, false);
		if (retval < 0) {
			mutex_unlock(&rmi4_data->rmi4_irq_mutex);
			return retval;
		}

		/* Process and clear interrupts */
		synaptics_rmi4_sensor_report(rmi4_data);

		retval = request_irq(rmi4_data->irq,
				synaptics_rmi4_irq, bdata->irq_flags,
				PLATFORM_DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create irq thread(%d)\n",
					__func__, retval);
			mutex_unlock(&rmi4_data->rmi4_irq_mutex);
			return retval;
		}

		retval = synaptics_rmi4_int_enable(rmi4_data, true);
		if (retval < 0) {
			mutex_unlock(&rmi4_data->rmi4_irq_mutex);
			return retval;
		}

		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;
		}
	}
	mutex_unlock(&rmi4_data->rmi4_irq_mutex);

	return retval;
}

static void synaptics_rmi4_set_intr_mask(struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	unsigned char ii;
	unsigned char intr_offset;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	return;
}

static int synaptics_rmi4_f01_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->data = NULL;
	fhandler->extra = NULL;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	rmi4_data->f01_query_base_addr = fd->query_base_addr;
	rmi4_data->f01_ctrl_base_addr = fd->ctrl_base_addr;
	rmi4_data->f01_data_base_addr = fd->data_base_addr;
	rmi4_data->f01_cmd_base_addr = fd->cmd_base_addr;

	return 0;
}

static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char offset;
	unsigned char fingers_supported;
	struct synaptics_rmi4_f11_extra_data *extra_data;
	struct synaptics_rmi4_f11_query_0_5 query_0_5;
	struct synaptics_rmi4_f11_query_7_8 query_7_8;
	struct synaptics_rmi4_f11_query_9 query_9;
	struct synaptics_rmi4_f11_query_12 query_12;
	struct synaptics_rmi4_f11_query_27 query_27;
	struct synaptics_rmi4_f11_ctrl_6_9 control_6_9;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Fail to alloc extra data\n",
				__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f11_extra_data *)fhandler->extra;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base,
			query_0_5.data,
			sizeof(query_0_5.data));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	if (query_0_5.num_of_fingers <= 4)
		fhandler->num_of_data_points = query_0_5.num_of_fingers + 1;
	else if (query_0_5.num_of_fingers == 5)
		fhandler->num_of_data_points = 10;

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + 6,
			control_6_9.data,
			sizeof(control_6_9.data));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x = control_6_9.sensor_max_x_pos_7_0 |
			(control_6_9.sensor_max_x_pos_11_8 << 8);
	rmi4_data->sensor_max_y = control_6_9.sensor_max_y_pos_7_0 |
			(control_6_9.sensor_max_y_pos_11_8 << 8);
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->max_touch_width = MAX_F11_TOUCH_WIDTH;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	fhandler->data = NULL;

	offset = sizeof(query_0_5.data);

	/* query 6 */
	if (query_0_5.has_rel)
		offset += 1;

	/* queries 7 8 */
	if (query_0_5.has_gestures) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_7_8.data,
				sizeof(query_7_8.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_7_8.data);
	}

	/* query 9 */
	if (query_0_5.has_query_9) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_9.data,
				sizeof(query_9.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_9.data);
	}

	/* query 10 */
	if (query_0_5.has_gestures && query_7_8.has_touch_shapes)
		offset += 1;

	/* query 11 */
	if (query_0_5.has_query_11)
		offset += 1;

	/* query 12 */
	if (query_0_5.has_query_12) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_12.data,
				sizeof(query_12.data));
		if (retval < 0)
			return retval;

		offset += sizeof(query_12.data);
	}

	/* query 13 */
	if (query_0_5.has_jitter_filter)
		offset += 1;

	/* query 14 */
	if (query_0_5.has_query_12 && query_12.has_general_information_2)
		offset += 1;

	/* queries 15 16 17 18 19 20 21 22 23 24 25 26*/
	if (query_0_5.has_query_12 && query_12.has_physical_properties)
		offset += 12;

	/* query 27 */
	if (query_0_5.has_query_27) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.query_base + offset,
				query_27.data,
				sizeof(query_27.data));
		if (retval < 0)
			return retval;

		rmi4_data->f11_wakeup_gesture = query_27.has_wakeup_gesture;
	}

	if (!rmi4_data->f11_wakeup_gesture)
		return retval;

	/* data 0 */
	fingers_supported = fhandler->num_of_data_points;
	offset = (fingers_supported + 3) / 4;

	/* data 1 2 3 4 5 */
	offset += 5 * fingers_supported;

	/* data 6 7 */
	if (query_0_5.has_rel)
		offset += 2 * fingers_supported;

	/* data 8 */
	if (query_0_5.has_gestures && query_7_8.data[0])
		offset += 1;

	/* data 9 */
	if (query_0_5.has_gestures && (query_7_8.data[0] || query_7_8.data[1]))
		offset += 1;

	/* data 10 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_pinch || query_7_8.has_flick))
		offset += 1;

	/* data 11 12 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_flick || query_7_8.has_rotate))
		offset += 2;

	/* data 13 */
	if (query_0_5.has_gestures && query_7_8.has_touch_shapes)
		offset += (fingers_supported + 3) / 4;

	/* data 14 15 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_scroll_zones ||
			query_7_8.has_multi_finger_scroll ||
			query_7_8.has_chiral_scroll))
		offset += 2;

	/* data 16 17 */
	if (query_0_5.has_gestures &&
			(query_7_8.has_scroll_zones &&
			query_7_8.individual_scroll_zones))
		offset += 2;

	/* data 18 19 20 21 22 23 24 25 26 27 */
	if (query_0_5.has_query_9 && query_9.has_contact_geometry)
		offset += 10 * fingers_supported;

	/* data 28 */
	if (query_0_5.has_bending_correction ||
			query_0_5.has_large_object_suppression)
		offset += 1;

	/* data 29 30 31 */
	if (query_0_5.has_query_9 && query_9.has_pen_hover_discrimination)
		offset += 3;

	/* data 32 */
	if (query_0_5.has_query_12 &&
			query_12.has_small_object_detection_tuning)
		offset += 1;

	/* data 33 34 */
	if (query_0_5.has_query_27 && query_27.f11_query27_b0)
		offset += 2;

	/* data 35 */
	if (query_0_5.has_query_12 && query_12.has_8bit_w)
		offset += fingers_supported;

	/* data 36 */
	if (query_0_5.has_bending_correction)
		offset += 1;

	/* data 37 */
	if (query_0_5.has_query_27 && query_27.has_data_37)
		offset += 1;

	/* data 38 */
	if (query_0_5.has_query_27 && query_27.has_wakeup_gesture)
		extra_data->data38_offset = offset;

	return retval;
}

static int synaptics_rmi4_f12_set_enables(struct synaptics_rmi4_data *rmi4_data,
		unsigned short ctrl28)
{
	int retval;

	if (ctrl28)
		rmi4_data->ctrl_28_address = ctrl28;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->ctrl_28_address,
			&rmi4_data->report_enable,
			sizeof(rmi4_data->report_enable));
	if (retval < 0)
		return retval;

	return retval;
}

static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char size_of_2d_data;
	unsigned char size_of_query8;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_11_offset;
	unsigned char ctrl_20_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_27_offset;
	unsigned char ctrl_28_offset;
	unsigned char ctrl_36_offset = 0;
	unsigned char num_of_fingers;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	unsigned char size_of_query_5;
	struct synaptics_rmi4_f12_query_5 query_5 = { { {0} } };
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_f12_ctrl_8 ctrl_8;
	struct synaptics_rmi4_f12_ctrl_23 ctrl_23;
	uint16_t pitch_of_rx_q4_12, pitch_of_tx_q4_12;

	unsigned char ctrl_20_data[3] = {1, 1, 0};

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Fail to alloc extra data\n",
				__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 4,
			&size_of_query_5,
			sizeof(size_of_query_5));
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 5,
			query_5.data,
			size_of_query_5);
	if (retval < 0)
		return retval;

	ctrl_8_offset = query_5.ctrl0_is_present +
			query_5.ctrl1_is_present +
			query_5.ctrl2_is_present +
			query_5.ctrl3_is_present +
			query_5.ctrl4_is_present +
			query_5.ctrl5_is_present +
			query_5.ctrl6_is_present +
			query_5.ctrl7_is_present;

	ctrl_11_offset = ctrl_8_offset +
			query_5.ctrl8_is_present +
			query_5.ctrl9_is_present +
			query_5.ctrl10_is_present;

	ctrl_20_offset = ctrl_8_offset +
			query_5.ctrl8_is_present +
			query_5.ctrl9_is_present +
			query_5.ctrl10_is_present +
			query_5.ctrl11_is_present +
			query_5.ctrl12_is_present +
			query_5.ctrl13_is_present +
			query_5.ctrl14_is_present +
			query_5.ctrl15_is_present +
			query_5.ctrl16_is_present +
			query_5.ctrl17_is_present +
			query_5.ctrl18_is_present +
			query_5.ctrl19_is_present;

	ctrl_23_offset = ctrl_20_offset +
			query_5.ctrl20_is_present +
			query_5.ctrl21_is_present +
			query_5.ctrl22_is_present;

	ctrl_27_offset = ctrl_23_offset +
			query_5.ctrl23_is_present +
			query_5.ctrl24_is_present +
			query_5.ctrl25_is_present +
			query_5.ctrl26_is_present;

	ctrl_28_offset = ctrl_23_offset +
			query_5.ctrl23_is_present +
			query_5.ctrl24_is_present +
			query_5.ctrl25_is_present +
			query_5.ctrl26_is_present +
			query_5.ctrl27_is_present;

	if ((size_of_query_5 > 5) && query_5.ctrl36_is_present) {
		ctrl_36_offset = ctrl_28_offset +
				query_5.ctrl28_is_present +
				query_5.ctrl29_is_present +
				query_5.ctrl30_is_present +
				query_5.ctrl31_is_present +
				query_5.ctrl32_is_present +
				query_5.ctrl33_is_present +
				query_5.ctrl34_is_present +
				query_5.ctrl35_is_present;
	}
	extra_data->ctrl36_offset = ctrl_36_offset;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_23_offset,
			ctrl_23.data,
			sizeof(ctrl_23.data));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	fhandler->num_of_data_points = min_t(unsigned char,
			ctrl_23.max_reported_objects,
			F12_FINGERS_TO_SUPPORT);

	num_of_fingers = fhandler->num_of_data_points;
	rmi4_data->num_of_fingers = num_of_fingers;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 7,
			&size_of_query8,
			sizeof(size_of_query8));
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 8,
			query_8.data,
			size_of_query8);
	if (retval < 0)
		return retval;

	/* Determine the presence of the Data0 register */
	extra_data->data1_offset = query_8.data0_is_present;

	if ((size_of_query8 >= 3) && (query_8.data13_is_present)) {
		extra_data->data13_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present +
				query_8.data4_is_present +
				query_8.data5_is_present +
				query_8.data6_is_present +
				query_8.data7_is_present +
				query_8.data8_is_present +
				query_8.data9_is_present +
				query_8.data10_is_present +
				query_8.data11_is_present +
				query_8.data12_is_present;
	}

	if ((size_of_query8 >= 3) && (query_8.data15_is_present)) {
		extra_data->data15_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present +
				query_8.data4_is_present +
				query_8.data5_is_present +
				query_8.data6_is_present +
				query_8.data7_is_present +
				query_8.data8_is_present +
				query_8.data9_is_present +
				query_8.data10_is_present +
				query_8.data11_is_present +
				query_8.data12_is_present +
				query_8.data13_is_present +
				query_8.data14_is_present;
		extra_data->data15_size = (num_of_fingers + 7) / 8;
	} else {
		extra_data->data15_size = 0;
	}

	rmi4_data->report_enable = RPT_DEFAULT;
#ifdef REPORT_2D_Z
	rmi4_data->report_enable |= RPT_Z;
#endif
#ifdef REPORT_2D_W
	rmi4_data->report_enable |= (RPT_WX | RPT_WY);
#endif

	dev_dbg(rmi4_data->pdev->dev.parent,
		"%s: Writing to F12_CTRL_20\n",
		__func__);
	retval = synaptics_rmi4_reg_write(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_20_offset,
			ctrl_20_data,
			sizeof(ctrl_20_data));
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_f12_set_enables(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_28_offset);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_8_offset,
			ctrl_8.data,
			sizeof(ctrl_8.data));

	extra_data->ctrl8_offset = ctrl_8_offset;

	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x =
			((unsigned short)ctrl_8.max_x_coord_lsb << 0) |
			((unsigned short)ctrl_8.max_x_coord_msb << 8);
	rmi4_data->sensor_max_y =
			((unsigned short)ctrl_8.max_y_coord_lsb << 0) |
			((unsigned short)ctrl_8.max_y_coord_msb << 8);
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	/* pitch x and y  - in Q4.12 fixed-point format */
	pitch_of_rx_q4_12 = (((unsigned short)ctrl_8.rx_pitch_lsb << 0) |
				  ((unsigned short)ctrl_8.rx_pitch_msb << 8));
	pitch_of_tx_q4_12 = (((unsigned short)ctrl_8.tx_pitch_lsb << 0) |
				  ((unsigned short)ctrl_8.tx_pitch_msb << 8));

	rmi4_data->num_of_rx = ctrl_8.num_of_rx;
	rmi4_data->num_of_tx = ctrl_8.num_of_tx;

	/* pitch x and y  - decode Q4.12 fixed-point format */
	rmi4_data->max_touch_width = max((rmi4_data->num_of_rx * pitch_of_rx_q4_12)/4096,
					 (rmi4_data->num_of_tx * pitch_of_tx_q4_12)/4096);

	rmi4_data->f12_wakeup_gesture = query_5.ctrl27_is_present;
	if (rmi4_data->f12_wakeup_gesture) {
		extra_data->ctrl20_offset = ctrl_20_offset;
		extra_data->ctrl27_offset = ctrl_27_offset;
		extra_data->data4_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present;
	}

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	/* Allocate memory for finger data storage space */
	fhandler->data_size = num_of_fingers * size_of_2d_data;
	fhandler->data = kmalloc(fhandler->data_size, GFP_KERNEL);

	return retval;
}

static void synaptics_rmi4_empty_fn_list(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_fn *fhandler_temp;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry_safe(fhandler,
				fhandler_temp,
				&rmi->support_fn_list,
				link) {
			kfree(fhandler->extra);
			kfree(fhandler->data);
			list_del(&fhandler->link);
			kfree(fhandler);
		}
	}
	INIT_LIST_HEAD(&rmi->support_fn_list);

	return;
}

static int synaptics_rmi4_check_status(struct synaptics_rmi4_data *rmi4_data,
		bool *was_in_bl_mode)
{
	int retval;
	int timeout = CHECK_STATUS_TIMEOUT_MS;
	unsigned char intr_status;
	struct synaptics_rmi4_f01_device_status status;

	dev_dbg(rmi4_data->pdev->dev.parent, "%s: Checking status\n", __func__);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			status.data,
			sizeof(status.data));
	if (retval < 0)
		return retval;

	while (status.status_code == STATUS_CRC_IN_PROGRESS) {
		if (timeout > 0)
			msleep(20);
		else
			return -EBUSY;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
		if (retval < 0)
			return retval;

		timeout -= 20;
	}

	if (timeout != CHECK_STATUS_TIMEOUT_MS)
		*was_in_bl_mode = true;

	if (status.flash_prog == 1) {
		rmi4_data->flash_prog_mode = true;
		dev_info(rmi4_data->pdev->dev.parent,
			"%s: In flash prog mode, status = 0x%02x\n",
			__func__,
			status.status_code);
	} else {
		rmi4_data->flash_prog_mode = false;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1,
			&intr_status,
			sizeof(intr_status));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read interrupt status\n",
				__func__);
		return retval;
	}

	return 0;
}

static int synaptics_rmi4_set_configured(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set configured\n",
				__func__);
		return retval;
	}

	rmi4_data->no_sleep_setting = device_ctrl & NO_SLEEP_ON;
	device_ctrl |= CONFIGURED;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set configured\n",
				__func__);
	}

	return retval;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
		struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kmalloc(sizeof(**fhandler), GFP_KERNEL);
	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base =
			(rmi_fd->data_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.ctrl_base =
			(rmi_fd->ctrl_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.cmd_base =
			(rmi_fd->cmd_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.query_base =
			(rmi_fd->query_base_addr |
			(page_number << 8));

	return 0;
}

static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char page_number;
	unsigned char intr_count;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned short pdt_entry_addr;
	bool f01found;
	bool was_in_bl_mode;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

rescan_pdt:
	f01found = false;
	was_in_bl_mode = false;
	intr_count = 0;
	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			pdt_entry_addr &= ~(MASK_8BIT << 8);

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				break;
			}

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: F%02x found (page %d)\n",
					__func__, rmi_fd.fn_number,
					page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				if (rmi_fd.intr_src_count == 0)
					break;

				f01found = true;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f01_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;

				retval = synaptics_rmi4_check_status(rmi4_data,
						&was_in_bl_mode);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to check status\n",
							__func__);
					return retval;
				}

				if (was_in_bl_mode) {
					kfree(fhandler);
					fhandler = NULL;
					goto rescan_pdt;
				}

				if (rmi4_data->flash_prog_mode)
					goto flash_prog_mode;

				break;
			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f11_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F12:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f12_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
			}
		}
	}

	if (!f01found) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to find F01\n",
				__func__);
		return -EINVAL;
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of interrupt registers = %d\n",
			__func__, rmi4_data->num_of_intr_regs);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr,
			f01_query,
			sizeof(f01_query));
	if (retval < 0)
		return retval;

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;

	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	rmi->product_info[0] = f01_query[2] & MASK_7BIT;
	rmi->product_info[1] = f01_query[3] & MASK_7BIT;
	rmi->date_code[0] = f01_query[4] & MASK_5BIT;
	rmi->date_code[1] = f01_query[5] & MASK_4BIT;
	rmi->date_code[2] = f01_query[6] & MASK_5BIT;
	rmi->tester_id = ((f01_query[7] & MASK_7BIT) << 8) |
			(f01_query[8] & MASK_7BIT);
	rmi->serial_number = ((f01_query[9] & MASK_7BIT) << 8) |
			(f01_query[10] & MASK_7BIT);
	memcpy(rmi->product_id_string, &f01_query[11], 10);

	if (rmi->manufacturer_id != 1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Non-Synaptics device found, manufacturer ID = %d\n",
				__func__, rmi->manufacturer_id);
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_BUID_ID_OFFSET,
			rmi->build_id,
			sizeof(rmi->build_id));
	if (retval < 0)
		return retval;

	rmi4_data->firmware_id = (unsigned int)rmi->build_id[0] +
			(unsigned int)rmi->build_id[1] * 0x100 +
			(unsigned int)rmi->build_id[2] * 0x10000;

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
						fhandler->intr_mask;
			}
		}
	}

	retval = synaptics_rmi4_set_configured(rmi4_data);
	if (retval < 0)
		return retval;

	return 0;
}

static int synaptics_rmi4_gpio_setup(int gpio, bool config, int dir, int state)
{
	int retval = 0;
	unsigned char buf[16];

	if (config) {
		snprintf(buf, PAGE_SIZE, "dsx_gpio_%u\n", gpio);

		retval = gpio_request(gpio, buf);
		if (retval) {
			pr_err("%s: Failed to get gpio %d (code: %d)",
					__func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval) {
			pr_err("%s: Failed to set gpio %d direction",
					__func__, gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return retval;
}

static void synaptics_rmi4_set_params(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	dev_dbg(rmi4_data->pdev->dev.parent,"%s: x=%d y=%d ma=%d mi=%d nf=%d\n",
				__func__, rmi4_data->sensor_max_x, rmi4_data->sensor_max_y,
				rmi4_data->max_touch_width, rmi4_data->max_touch_width,
				rmi4_data->num_of_fingers);

	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_X, 0,
			rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_Y, 0,
			rmi4_data->sensor_max_y, 0, 0);
#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			rmi4_data->max_touch_width, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MINOR, 0,
			rmi4_data->max_touch_width, 0, 0);
#endif
#ifdef REPORT_2D_Z
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_PRESSURE, 0,
			MAX_Z, 0, 0);
#endif

	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers);

	return;
}

static int synaptics_rmi4_set_input_dev(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int temp;
	struct synaptics_dsx_board_data *bdata =
				rmi4_data->hw_if->board_data;

	rmi4_data->input_dev = input_allocate_device();
	if (rmi4_data->input_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate input device\n",
				__func__);
		retval = -ENOMEM;
		goto err_input_device;
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	rmi4_data->input_dev->name = rmi4_data->hw_if->board_data->input_dev_name;
	rmi4_data->input_dev->phys = INPUT_PHYS_NAME;
	rmi4_data->input_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	rmi4_data->input_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	rmi4_data->input_dev->dev.parent = rmi4_data->pdev->dev.parent;
	input_set_drvdata(rmi4_data->input_dev, rmi4_data);

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);
	set_bit(EV_KEY, rmi4_data->input_dev->evbit);
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, rmi4_data->input_dev->keybit);
	if (bdata->touchpad)
		set_bit(INPUT_PROP_POINTER, rmi4_data->input_dev->propbit);
	else
		set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);

	if (bdata->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	synaptics_rmi4_set_params(rmi4_data);

	retval = input_register_device(rmi4_data->input_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register input device\n",
				__func__);
		goto err_register_input;
	}

	return 0;

err_register_input:
err_query_device:
	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_free_device(rmi4_data->input_dev);

err_input_device:
	return retval;
}

static int synaptics_rmi4_set_gpio(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	retval = synaptics_rmi4_gpio_setup(
			bdata->irq_gpio,
			true, 0, 0);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to configure attention GPIO\n",
				__func__);
		goto err_gpio_irq;
	}

	if (bdata->power_gpio >= 0) {
		retval = synaptics_rmi4_gpio_setup(
				bdata->power_gpio,
				true, 1, !bdata->power_on_state);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to configure power GPIO\n",
					__func__);
			goto err_gpio_power;
		}
	}

	if (bdata->reset_gpio >= 0) {
		retval = synaptics_rmi4_gpio_setup(
				bdata->reset_gpio,
				true, 1, !bdata->reset_on_state);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to configure reset GPIO\n",
					__func__);
			goto err_gpio_reset;
		}
	}

	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
		msleep(bdata->reset_delay_ms);
	}

	return 0;

err_gpio_reset:
	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0);

err_gpio_power:
	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0);

err_gpio_irq:
	return retval;
}

static int synaptics_rmi4_get_reg(struct synaptics_rmi4_data *rmi4_data,
		bool get)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		rmi4_data->pwr_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->pwr_reg_name);
		if (IS_ERR(rmi4_data->pwr_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get power regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		rmi4_data->bus_reg = regulator_get(rmi4_data->pdev->dev.parent,
				bdata->bus_reg_name);
		if (IS_ERR(rmi4_data->bus_reg)) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to get bus pullup regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->bus_reg);
			goto regulator_put;
		}
	}

	return 0;

regulator_put:
	if (rmi4_data->pwr_reg) {
		regulator_put(rmi4_data->pwr_reg);
		rmi4_data->pwr_reg = NULL;
	}

	if (rmi4_data->bus_reg) {
		regulator_put(rmi4_data->bus_reg);
		rmi4_data->bus_reg = NULL;
	}

	return retval;
}

static int synaptics_rmi4_enable_reg(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (rmi4_data->bus_reg) {
		retval = regulator_enable(rmi4_data->bus_reg);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable bus pullup regulator\n",
					__func__);
			goto exit;
		}
	}

	if (rmi4_data->pwr_reg) {
		retval = regulator_enable(rmi4_data->pwr_reg);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable power regulator\n",
					__func__);
			goto disable_bus_reg;
		}
		msleep(bdata->power_delay_ms);
	}

	return 0;

disable_pwr_reg:
	if (rmi4_data->pwr_reg)
		regulator_disable(rmi4_data->pwr_reg);

disable_bus_reg:
	if (rmi4_data->bus_reg)
		regulator_disable(rmi4_data->bus_reg);

exit:
	return retval;
}

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (ii = 0; ii < rmi4_data->num_of_fingers; ii++) {
		input_mt_slot(rmi4_data->input_dev, ii);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, 0);
	}

	input_report_key(rmi4_data->input_dev,
			BTN_TOUCH, 0);
	input_report_key(rmi4_data->input_dev,
			BTN_TOOL_FINGER, 0);
	input_sync(rmi4_data->input_dev);

	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	rmi4_data->fingers_on_2d = false;

	return 0;
}

static int synaptics_rmi4_sw_reset(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char command = 0x01;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0)
		return retval;

	msleep(rmi4_data->hw_if->board_data->reset_delay_ms);

	if (rmi4_data->hw_if->ui_hw_init) {
		retval = rmi4_data->hw_if->ui_hw_init(rmi4_data);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	synaptics_rmi4_free_fingers(rmi4_data);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F12) {
				retval = synaptics_rmi4_f12_set_enables(rmi4_data, 0);
				if (retval < 0)
					goto exit;
				break;
			}
		}
	}

	retval = synaptics_rmi4_int_enable(rmi4_data, true);
	if (retval < 0)
		goto exit;

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry(exp_fhandler, &rmi4_data->exp_data.list, link)
			if (exp_fhandler->exp_fn->reinit != NULL)
				exp_fhandler->exp_fn->reinit(rmi4_data);
	}
	mutex_unlock(&rmi4_data->exp_data.mutex);

	if (!rmi4_data->hw_if->board_data->touchpad) {
		mdss_fb_send_panel_dead(0);
	}

	retval = synaptics_rmi4_set_configured(rmi4_data);
exit:
	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
	return retval;
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int temp;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	const struct synaptics_dsx_board_data *bdata =
				rmi4_data->hw_if->board_data;

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

	retval = synaptics_rmi4_sw_reset(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command\n",
				__func__);
		mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
		return retval;
	}

	synaptics_rmi4_free_fingers(rmi4_data);

	synaptics_rmi4_empty_fn_list(rmi4_data);

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to query device\n",
				__func__);
		mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
		return retval;
	}

	if (bdata->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	if (bdata->max_y_for_2d >= 0)
		rmi4_data->sensor_max_y = bdata->max_y_for_2d;

	synaptics_rmi4_set_params(rmi4_data);

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry(exp_fhandler, &rmi4_data->exp_data.list, link)
			if (exp_fhandler->exp_fn->reset != NULL)
				exp_fhandler->exp_fn->reset(rmi4_data);
	}
	mutex_unlock(&rmi4_data->exp_data.mutex);

	synaptics_rmi4_irq_enable(rmi4_data, true, false);

	if (!rmi4_data->hw_if->board_data->touchpad) {
		mdss_fb_send_panel_dead(0);
	}

	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));

	return 0;
}

static void synaptics_rmi4_exp_fn_work(struct work_struct *work)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler_temp;
	struct synaptics_rmi4_exp_fn_data *exp_data =
		container_of((struct delayed_work *) work,
		struct synaptics_rmi4_exp_fn_data, work);
	struct synaptics_rmi4_data *rmi4_data = exp_data->rmi4_data;

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry_safe(exp_fhandler,
				exp_fhandler_temp,
				&rmi4_data->exp_data.list,
				link) {
			if ((exp_fhandler->exp_fn->init != NULL) &&
					exp_fhandler->insert) {
				exp_fhandler->exp_fn->init(rmi4_data);
				exp_fhandler->insert = false;
			} else if ((exp_fhandler->exp_fn->remove != NULL) &&
					exp_fhandler->remove) {
				exp_fhandler->exp_fn->remove(rmi4_data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&rmi4_data->exp_data.mutex);

	return;
}

void synaptics_rmi4_new_function(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_exp_fn *exp_fn,
		bool insert)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	if (!rmi4_data->exp_data.initialized) {
		mutex_init(&rmi4_data->exp_data.mutex);
		INIT_LIST_HEAD(&rmi4_data->exp_data.list);
		rmi4_data->exp_data.initialized = true;
	}

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
		if (!exp_fhandler) {
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for expansion function\n",
				__func__);
			mutex_unlock(&rmi4_data->exp_data.mutex);
			return;
		}
		exp_fhandler->exp_fn = exp_fn;
		exp_fhandler->insert = true;
		exp_fhandler->remove = false;
		list_add_tail(&exp_fhandler->link, &rmi4_data->exp_data.list);
	} else if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry(exp_fhandler, &rmi4_data->exp_data.list, link) {
			if (exp_fhandler->exp_fn->fn_type == exp_fn->fn_type) {
				exp_fhandler->insert = false;
				exp_fhandler->remove = true;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&rmi4_data->exp_data.mutex);

	if (rmi4_data->exp_data.queue_work) {
		queue_delayed_work(rmi4_data->exp_data.workqueue,
				&rmi4_data->exp_data.work,
				msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
	}

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_new_function);

static int synaptics_rmi4_probe(struct platform_device *pdev)
{
	int retval;
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data;
	const struct synaptics_dsx_hw_interface *hw_if;
	const struct synaptics_dsx_board_data *bdata;

	hw_if = pdev->dev.platform_data;
	if (!hw_if) {
		dev_err(&pdev->dev,
				"%s: No hardware interface found\n",
				__func__);
		return -EINVAL;
	}

	bdata = hw_if->board_data;
	if (!bdata) {
		dev_err(&pdev->dev,
				"%s: No board data found\n",
				__func__);
		return -EINVAL;
	}

	rmi4_data = kzalloc(sizeof(*rmi4_data), GFP_KERNEL);
	if (!rmi4_data) {
		dev_err(&pdev->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
	}

	rmi4_data->pdev = pdev;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->hw_if = hw_if;
	rmi4_data->suspend = false;
	rmi4_data->irq_enabled = false;
	rmi4_data->fingers_on_2d = false;

	rmi4_data->reset_device = synaptics_rmi4_reset_device;
	rmi4_data->irq_enable = synaptics_rmi4_irq_enable;

	mutex_init(&(rmi4_data->rmi4_reset_mutex));
	mutex_init(&(rmi4_data->rmi4_report_mutex));
	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->rmi4_power_mutex));
	mutex_init(&(rmi4_data->rmi4_irq_mutex));

	platform_set_drvdata(pdev, rmi4_data);

	retval = synaptics_rmi4_get_reg(rmi4_data, true);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to get regulators\n",
				__func__);
		goto err_get_reg;
	}

	if (rmi4_data->pwr_reg) {
		if (regulator_is_enabled(rmi4_data->pwr_reg))
		{
			dev_dbg(&pdev->dev,
				"%s: pwr regulator already on, delete reset gpio\n",
				__func__);
			rmi4_data->hw_if->board_data->reset_gpio = -1;
		}
	}

	retval = synaptics_rmi4_enable_reg(rmi4_data, true);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to enable regulators\n",
				__func__);
		goto err_enable_reg;
	}

	retval = synaptics_rmi4_set_gpio(rmi4_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to set up GPIO's\n",
				__func__);
		goto err_set_gpio;
	}

	if (hw_if->ui_hw_init) {
		retval = hw_if->ui_hw_init(rmi4_data);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to initialize hardware interface\n",
					__func__);
			goto err_ui_hw_init;
		}
	}

	retval = synaptics_rmi4_set_input_dev(rmi4_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to set up input device\n",
				__func__);
		goto err_set_input_dev;
	}

	if (!rmi4_data->exp_data.initialized) {
		mutex_init(&rmi4_data->exp_data.mutex);
		INIT_LIST_HEAD(&rmi4_data->exp_data.list);
		rmi4_data->exp_data.initialized = true;
	}

	rmi4_data->irq = gpio_to_irq(bdata->irq_gpio);

	rmi4_data->workqueue = alloc_workqueue("background_workqueue",
				(WQ_HIGHPRI | WQ_UNBOUND | WQ_MEM_RECLAIM), 1);

	INIT_WORK(&rmi4_data->irq_work,
			synaptics_rmi4_irq_work);

	retval = synaptics_rmi4_irq_enable(rmi4_data, true, false);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to enable attention interrupt\n",
				__func__);
		goto err_enable_irq;
	}

	/* register a handler to listen about interested input events */
	rmi4_data->ignore_touch = false;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = device_create_file(&rmi4_data->input_dev->dev,
				&attrs[attr_count]);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto err_sysfs;
		}
	}

	rmi4_data->exp_data.workqueue = create_singlethread_workqueue("dsx_exp_workqueue");
	INIT_DELAYED_WORK(&rmi4_data->exp_data.work, synaptics_rmi4_exp_fn_work);
	rmi4_data->exp_data.rmi4_data = rmi4_data;
	rmi4_data->exp_data.queue_work = true;
	queue_delayed_work(rmi4_data->exp_data.workqueue,
			&rmi4_data->exp_data.work,
			0);
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_FW_UPDATE
	synaptics_rmi4_fw_update_module_init(rmi4_data);
#endif

	return retval;

err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	cancel_delayed_work_sync(&rmi4_data->exp_data.work);
	flush_workqueue(rmi4_data->exp_data.workqueue);
	destroy_workqueue(rmi4_data->exp_data.workqueue);

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

err_enable_irq:
	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

err_set_input_dev:
	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0);

	if (bdata->reset_gpio >= 0) {
		synaptics_rmi4_gpio_setup(bdata->reset_gpio, false, 0, 0);
	}

	if (bdata->power_gpio >= 0) {
		dev_dbg(&pdev->dev,
				"%s: Disabling power gpio(%d)\n",
				__func__, !bdata->power_on_state);
		gpio_set_value(bdata->power_gpio, !bdata->power_on_state);
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0);
	}

err_ui_hw_init:
err_set_gpio:
	synaptics_rmi4_enable_reg(rmi4_data, false);

err_enable_reg:
	synaptics_rmi4_get_reg(rmi4_data, false);

err_get_reg:
	kfree(rmi4_data);

	return retval;
}

static int synaptics_rmi4_remove(struct platform_device *pdev)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = platform_get_drvdata(pdev);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	cancel_delayed_work_sync(&rmi4_data->exp_data.work);
	flush_workqueue(rmi4_data->exp_data.workqueue);
	destroy_workqueue(rmi4_data->exp_data.workqueue);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0);

	if (bdata->reset_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->reset_gpio, false, 0, 0);

	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0);

	synaptics_rmi4_enable_reg(rmi4_data, false);
	synaptics_rmi4_get_reg(rmi4_data, false);

	kfree(rmi4_data);

	return 0;
}

#ifdef CONFIG_PM
static int synaptics_rmi4_sensor_sleep(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	dev_dbg(rmi4_data->pdev->dev.parent, "%s\n", __func__);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return retval;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | NO_SLEEP_OFF | SENSOR_SLEEP);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return retval;
	}

	rmi4_data->sensor_sleep = true;

	return retval;
}

static int synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;
	unsigned char no_sleep_setting = rmi4_data->no_sleep_setting;
	dev_info(rmi4_data->pdev->dev.parent,
		"%s: Waking sensor\n",
		__func__);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return retval;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | no_sleep_setting | NORMAL_OPERATION);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return retval;
	}

	rmi4_data->sensor_sleep = false;

	return 0;
}

static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	mutex_lock(&rmi4_data->input_dev->mutex);

	if (rmi4_data->suspend)
		goto exit;

	if (rmi4_data->stay_awake)
		goto exit;

	if (!rmi4_data->suspend) {
		synaptics_rmi4_irq_enable(rmi4_data, false, false);
		synaptics_rmi4_sensor_sleep(rmi4_data);
		synaptics_rmi4_free_fingers(rmi4_data);
	}

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry(exp_fhandler, &rmi4_data->exp_data.list, link)
			if (exp_fhandler->exp_fn->suspend != NULL)
				exp_fhandler->exp_fn->suspend(rmi4_data);
	}
	mutex_unlock(&rmi4_data->exp_data.mutex);

	if (rmi4_data->pwr_reg)
		regulator_disable(rmi4_data->pwr_reg);

	rmi4_data->suspend = true;
	dev_info(dev, "Synaptic suspend complete\n");
exit:
	mutex_unlock(&rmi4_data->input_dev->mutex);
	return 0;
}

static int synaptics_rmi4_resume(struct device *dev)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	mutex_lock(&rmi4_data->input_dev->mutex);

	if (!rmi4_data->suspend)
		goto exit;

	if (rmi4_data->stay_awake)
		goto exit;

	if (rmi4_data->pwr_reg) {
		retval = regulator_enable(rmi4_data->pwr_reg);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to enable regulator\n",
				__func__);
			goto exit;
		}

		msleep(bdata->power_delay_ms);
		rmi4_data->current_page = MASK_8BIT;
		if (rmi4_data->hw_if->ui_hw_init)
			rmi4_data->hw_if->ui_hw_init(rmi4_data);
	}

	synaptics_rmi4_sensor_wake(rmi4_data);
	synaptics_rmi4_irq_enable(rmi4_data, true, false);

	mutex_lock(&rmi4_data->exp_data.mutex);
	if (!list_empty(&rmi4_data->exp_data.list)) {
		list_for_each_entry(exp_fhandler, &rmi4_data->exp_data.list, link)
			if (exp_fhandler->exp_fn->resume != NULL)
				exp_fhandler->exp_fn->resume(rmi4_data);
	}
	mutex_unlock(&rmi4_data->exp_data.mutex);

	rmi4_data->suspend = false;
	dev_info(dev, "Synaptic resume complete\n");
exit:
	mutex_unlock(&rmi4_data->input_dev->mutex);
	return 0;
}

static const struct dev_pm_ops synaptics_rmi4_dev_pm_ops = {
	.suspend = synaptics_rmi4_suspend,
	.resume  = synaptics_rmi4_resume,
};
#endif

static struct platform_driver synaptics_rmi4_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = synaptics_rmi4_remove,
};

static int __init synaptics_rmi4_init(void)
{
	int retval;

	retval = synaptics_rmi4_bus_init();
	if (retval)
		return retval;

	return platform_driver_register(&synaptics_rmi4_driver);
}

static void __exit synaptics_rmi4_exit(void)
{
	platform_driver_unregister(&synaptics_rmi4_driver);

	synaptics_rmi4_bus_exit();

	return;
}

fs_initcall_sync(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Touch Driver");
MODULE_LICENSE("GPL v2");
