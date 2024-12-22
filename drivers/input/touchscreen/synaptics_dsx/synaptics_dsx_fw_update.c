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
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define FW_IMAGE_FOLDER "synaptics/"
#define FW_IMAGE_NAME "%s.img"
#define FORCE_UPDATE false
#define DO_LOCKDOWN false

#define MAX_IMAGE_NAME_LEN 256
#define MAX_FIRMWARE_ID_LEN 10

#define IMAGE_AREA_OFFSET 0x100

#define BOOTLOADER_ID_OFFSET 0
#define BLOCK_NUMBER_OFFSET 0

#define V5_PROPERTIES_OFFSET 2
#define V5_BLOCK_SIZE_OFFSET 3
#define V5_BLOCK_COUNT_OFFSET 5
#define V5_BLOCK_DATA_OFFSET 2

#define V6_PROPERTIES_OFFSET 1
#define V6_BLOCK_SIZE_OFFSET 2
#define V6_BLOCK_COUNT_OFFSET 3
#define V6_BLOCK_DATA_OFFSET 1
#define V6_FLASH_COMMAND_OFFSET 2
#define V6_FLASH_STATUS_OFFSET 3

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)

#define INT_DISABLE_WAIT_MS 20
#define ENTER_FLASH_PROG_WAIT_MS 20
enum bl_version {
	V5 = 5,
	V6 = 6,
};

enum flash_area {
	NONE = 0,
	UI_FIRMWARE,
	CONFIG_AREA,
};

enum update_mode {
	NORMAL = 1,
	FORCE = 2,
};

enum config_area {
	UI_CONFIG_AREA = 0,
	PERM_CONFIG_AREA,
	BL_CONFIG_AREA,
	DISP_CONFIG_AREA,
};

enum flash_command {
	CMD_IDLE = 0x0,
	CMD_WRITE_FW_BLOCK = 0x2,
	CMD_ERASE_ALL = 0x3,
	CMD_WRITE_LOCKDOWN_BLOCK = 0x4,
	CMD_READ_CONFIG_BLOCK = 0x5,
	CMD_WRITE_CONFIG_BLOCK = 0x6,
	CMD_ERASE_CONFIG = 0x7,
	CMD_ERASE_BL_CONFIG = 0x9,
	CMD_ERASE_DISP_CONFIG = 0xa,
	CMD_ENABLE_FLASH_PROG = 0xf,
};

struct pdt_properties {
	union {
		struct {
			unsigned char reserved_1:6;
			unsigned char has_bsr:1;
			unsigned char reserved_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_status {
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

struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_flash_properties {
	union {
		struct {
			unsigned char reg_map:1;
			unsigned char unlocked:1;
			unsigned char has_config_id:1;
			unsigned char has_perm_config:1;
			unsigned char has_bl_config:1;
			unsigned char has_disp_config:1;
			unsigned char has_ctrl1:1;
		} __packed;
		unsigned char data[1];
	};
};

struct register_offset {
	unsigned char properties;
	unsigned char blk_size;
	unsigned char blk_count;
	unsigned char blk_data;
	unsigned char flash_cmd;
	unsigned char flash_status;
};

struct block_count {
	unsigned short ui_firmware;
	unsigned short ui_config;
	unsigned short disp_config;
	unsigned short perm_config;
	unsigned short bl_config;
};

struct image_header {
	/* 0x00 - 0x0f */
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char options_firmware_id:1;
	unsigned char options_bootloader:1;
	unsigned char options_reserved:6;
	unsigned char header_version;
	unsigned char firmware_size[4];
	unsigned char config_size[4];
	/* 0x10 - 0x1f */
	unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE];
	unsigned char package_id[2];
	unsigned char package_id_revision[2];
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	/* 0x20 - 0x2f */
	unsigned char bootloader_addr[4];
	unsigned char bootloader_size[4];
	unsigned char ui_addr[4];
	unsigned char ui_size[4];
	/* 0x30 - 0x3f */
	unsigned char ds_id[16];
	/* 0x40 - 0x4f */
	unsigned char dsp_cfg_addr[4];
	unsigned char dsp_cfg_size[4];
	unsigned char reserved_48_4f[8];
	/* 0x50 - 0x53 */
	unsigned char firmware_id[4];
};

struct block_data {
	unsigned int size;
	const unsigned char *data;
};

struct image_metadata {
	bool contains_firmware_id;
	bool contains_bootloader;
	bool contains_disp_config;
	unsigned int image_size;
	unsigned int firmware_id;
	unsigned int checksum;
	unsigned int bootloader_size;
	unsigned int disp_config_offset;
	unsigned char bl_version;
	unsigned char *image_name;
	const unsigned char *image;
	struct block_data ui_firmware;
	struct block_data ui_config;
	struct block_data disp_config;
};

struct synaptics_rmi4_fwu_handle {
	enum bl_version bl_version;
	bool initialized;
	bool program_enabled;
	bool force_update;
	bool in_flash_prog_mode;
	unsigned int data_pos;
	unsigned char *read_config_buf;
	unsigned char intr_mask;
	unsigned char command;
	unsigned char bootloader_id[2];
	unsigned char flash_status;
	unsigned short block_size;
	unsigned short config_size;
	unsigned short config_area;
	unsigned short config_block_count;
	const unsigned char *config_data;
	struct workqueue_struct *fwu_workqueue;
	struct work_struct fwu_work;
	struct image_metadata img;
	struct register_offset off;
	struct block_count bcount;
	struct f34_flash_properties flash_properties;
	struct synaptics_rmi4_fn_desc f34_fd;
	struct synaptics_rmi4_data *rmi4_data;
};

static int fwu_do_reflash(struct synaptics_rmi4_fwu_handle *fwu);

DECLARE_COMPLETION(fwu_remove_complete);

static unsigned int le_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
			(unsigned int)ptr[1] * 0x100 +
			(unsigned int)ptr[2] * 0x10000 +
			(unsigned int)ptr[3] * 0x1000000;
}

static unsigned int be_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[3] +
			(unsigned int)ptr[2] * 0x100 +
			(unsigned int)ptr[1] * 0x10000 +
			(unsigned int)ptr[0] * 0x1000000;
}

static void fwu_parse_image_header(struct synaptics_rmi4_fwu_handle *fwu)
{
	const unsigned char *image = fwu->img.image;
	struct image_header *header = (struct image_header *)image;

	fwu->img.bl_version = 0;
	fwu->img.ui_firmware.data = NULL;
	fwu->img.ui_config.data = NULL;
	fwu->img.disp_config.data = NULL;
	fwu->img.ui_firmware.size = 0;
	fwu->img.ui_config.size = 0;
	fwu->img.disp_config.size = 0;
	fwu->img.contains_firmware_id = false;
	fwu->img.contains_bootloader = false;
	fwu->img.contains_disp_config = false;

	fwu->img.checksum = le_to_uint(header->checksum);

	fwu->img.bl_version = header->header_version;

	fwu->img.ui_firmware.size = le_to_uint(header->firmware_size);

	fwu->img.ui_config.size = le_to_uint(header->config_size);

	fwu->img.contains_firmware_id = header->options_firmware_id;
	if (fwu->img.contains_firmware_id)
		fwu->img.firmware_id = le_to_uint(header->firmware_id);

	fwu->img.contains_bootloader = header->options_bootloader;
	if (fwu->img.contains_bootloader)
		fwu->img.bootloader_size = le_to_uint(header->bootloader_size);

	if (fwu->img.ui_firmware.size)
		fwu->img.ui_firmware.data = image + IMAGE_AREA_OFFSET;

	if (fwu->img.ui_config.size)
		fwu->img.ui_config.data = image + IMAGE_AREA_OFFSET +
				fwu->img.ui_firmware.size;

	if (fwu->img.contains_bootloader) {
		if (fwu->img.ui_firmware.size)
			fwu->img.ui_firmware.data += fwu->img.bootloader_size;
		if (fwu->img.ui_config.size)
			fwu->img.ui_config.data += fwu->img.bootloader_size;
	}

	if ((fwu->img.bl_version == V5) && fwu->img.contains_bootloader) {
		fwu->img.contains_disp_config = true;
		fwu->img.disp_config_offset = le_to_uint(header->dsp_cfg_addr);
		fwu->img.disp_config.size = le_to_uint(header->dsp_cfg_size);
		fwu->img.disp_config.data = image + fwu->img.disp_config_offset;
	}
}

static int fwu_read_f01_device_status(struct synaptics_rmi4_fwu_handle *fwu, struct f01_device_status *status)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			status->data,
			sizeof(status->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read F01 device status\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_read_f34_queries(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	unsigned char count;
	unsigned char base;
	unsigned char buf[10];
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	base = fwu->f34_fd.query_base_addr;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			base + BOOTLOADER_ID_OFFSET,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read bootloader ID\n",
				__func__);
		return retval;
	}

	if (fwu->bootloader_id[1] == '5') {
		fwu->bl_version = V5;
	} else if (fwu->bootloader_id[1] == '6') {
		fwu->bl_version = V6;
	} else {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Unrecognized bootloader version\n",
				__func__);
		return -EINVAL;
	}

	if (fwu->bl_version == V5) {
		fwu->off.properties = V5_PROPERTIES_OFFSET;
		fwu->off.blk_size = V5_BLOCK_SIZE_OFFSET;
		fwu->off.blk_count = V5_BLOCK_COUNT_OFFSET;
		fwu->off.blk_data = V5_BLOCK_DATA_OFFSET;
	} else if (fwu->bl_version == V6) {
		fwu->off.properties = V6_PROPERTIES_OFFSET;
		fwu->off.blk_size = V6_BLOCK_SIZE_OFFSET;
		fwu->off.blk_count = V6_BLOCK_COUNT_OFFSET;
		fwu->off.blk_data = V6_BLOCK_DATA_OFFSET;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			base + fwu->off.blk_size,
			buf,
			2);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read block size info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->block_size, &(buf[0]));

	if (fwu->bl_version == V5) {
		fwu->off.flash_cmd = fwu->off.blk_data + fwu->block_size;
		fwu->off.flash_status = fwu->off.flash_cmd;
	} else if (fwu->bl_version == V6) {
		fwu->off.flash_cmd = V6_FLASH_COMMAND_OFFSET;
		fwu->off.flash_status = V6_FLASH_STATUS_OFFSET;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			base + fwu->off.properties,
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read flash properties\n",
				__func__);
		return retval;
	}

	count = 4;

	if (fwu->flash_properties.has_perm_config)
		count += 2;

	if (fwu->flash_properties.has_bl_config)
		count += 2;

	if (fwu->flash_properties.has_disp_config)
		count += 2;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			base + fwu->off.blk_count,
			buf,
			count);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read block count info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->bcount.ui_firmware, &(buf[0]));
	batohs(&fwu->bcount.ui_config, &(buf[2]));

	count = 4;

	if (fwu->flash_properties.has_perm_config) {
		batohs(&fwu->bcount.perm_config, &(buf[count]));
		count += 2;
	}

	if (fwu->flash_properties.has_bl_config) {
		batohs(&fwu->bcount.bl_config, &(buf[count]));
		count += 2;
	}

	if (fwu->flash_properties.has_disp_config)
		batohs(&fwu->bcount.disp_config, &(buf[count]));

	return 0;
}

static int fwu_read_f34_flash_status(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	unsigned char status;
	unsigned char command;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->off.flash_status,
			&status,
			sizeof(status));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read flash status\n",
				__func__);
		return retval;
	}

	fwu->program_enabled = status >> 7;

	if (fwu->bl_version == V5)
		fwu->flash_status = (status >> 4) & MASK_3BIT;
	else if (fwu->bl_version == V6)
		fwu->flash_status = status & MASK_3BIT;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read flash command\n",
				__func__);
		return retval;
	}

	fwu->command = command & MASK_4BIT;

	return 0;
}

static int fwu_write_f34_command(struct synaptics_rmi4_fwu_handle *fwu, unsigned char cmd)
{
	int retval;
	unsigned char command = cmd & MASK_4BIT;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	fwu->command = cmd;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write command 0x%02x\n",
				__func__, command);
		return retval;
	}

	return 0;
}

static int fwu_wait_for_idle(struct synaptics_rmi4_fwu_handle *fwu, int timeout_ms)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	while(time_before(jiffies, timeout)) {
		fwu_read_f34_flash_status(fwu);

		if ((fwu->command == 0x00) && (fwu->flash_status == 0x00))
			return 0;
	}

	dev_err(rmi4_data->pdev->dev.parent,
			"%s: Timed out waiting for idle status\n",
			__func__);

	return -ETIMEDOUT;
}

static enum flash_area fwu_go_nogo(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	enum flash_area flash_area = NONE;
	unsigned char index = 0;
	unsigned char config_id[4];
	unsigned int device_config_id;
	unsigned int image_config_id;
	unsigned int device_fw_id;
	unsigned long image_fw_id;
	char *strptr;
	char *firmware_id;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (fwu->force_update) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* Update both UI and config if device is in bootloader mode */
	if (fwu->in_flash_prog_mode) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* Get device firmware ID */
	device_fw_id = rmi4_data->firmware_id;
	dev_info(rmi4_data->pdev->dev.parent,
			"%s: Device firmware ID = %d\n",
			__func__, device_fw_id);

	/* Get image firmware ID */
	if (fwu->img.contains_firmware_id) {
		image_fw_id = fwu->img.firmware_id;
	} else {
		strptr = strstr(fwu->img.image_name, "PR");
		if (!strptr) {
			dev_err(rmi4_data->pdev->dev.parent,
			"%s: No valid PR number found in image file name(%s)\n",
			__func__, fwu->img.image_name);
			flash_area = NONE;
			goto exit;
		}

		strptr += 2;
		firmware_id = kzalloc(MAX_FIRMWARE_ID_LEN, GFP_KERNEL);
		if (!firmware_id) {
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Fail to alloc firmware id\n",
				__func__);
			goto exit;
		}
		while (strptr[index] >= '0' && strptr[index] <= '9') {
			firmware_id[index] = strptr[index];
			index++;
		}

		retval = sstrtoul(firmware_id, 10, &image_fw_id);
		kfree(firmware_id);
		if (retval) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to obtain image firmware ID\n",
					__func__);
			flash_area = NONE;
			goto exit;
		}
	}
	dev_info(rmi4_data->pdev->dev.parent,
			"%s: Image firmware ID = %d\n",
			__func__, (unsigned int)image_fw_id);

	if (image_fw_id > device_fw_id) {
		flash_area = UI_FIRMWARE;
		goto exit;
	} else if (image_fw_id < device_fw_id) {
		dev_info(rmi4_data->pdev->dev.parent,
				"%s: Image firmware ID older than device firmware ID\n",
				__func__);
		flash_area = NONE;
		goto exit;
	}

	/* Get device config ID */
	retval = synaptics_rmi4_reg_read(rmi4_data,
				fwu->f34_fd.ctrl_base_addr,
				config_id,
				sizeof(config_id));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read device config ID\n",
				__func__);
		flash_area = NONE;
		goto exit;
	}
	device_config_id = be_to_uint(config_id);
	dev_info(rmi4_data->pdev->dev.parent,
			"%s: Device config ID = 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,
			config_id[0],
			config_id[1],
			config_id[2],
			config_id[3]);

	/* Get image config ID */
	image_config_id = be_to_uint(fwu->img.ui_config.data);
	dev_info(rmi4_data->pdev->dev.parent,
			"%s: Image config ID = 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,
			fwu->img.ui_config.data[0],
			fwu->img.ui_config.data[1],
			fwu->img.ui_config.data[2],
			fwu->img.ui_config.data[3]);

	if (image_config_id > device_config_id) {
		flash_area = CONFIG_AREA;
		goto exit;
	}

	flash_area = NONE;

exit:
	if (flash_area == NONE) {
		dev_info(rmi4_data->pdev->dev.parent,
				"%s: No need to do reflash\n",
				__func__);
	} else {
		dev_info(rmi4_data->pdev->dev.parent,
				"%s: Updating %s\n",
				__func__,
				flash_area == UI_FIRMWARE ?
				"UI firmware" :
				"config only");
	}

	return flash_area;
}

static int fwu_scan_pdt(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	unsigned char ii;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	bool f01found = false;
	bool f34found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				addr,
				(unsigned char *)&rmi_fd,
				sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {
			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Found F%02x\n",
					__func__, rmi_fd.fn_number);
			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				f01found = true;

				rmi4_data->f01_query_base_addr =
						rmi_fd.query_base_addr;
				rmi4_data->f01_ctrl_base_addr =
						rmi_fd.ctrl_base_addr;
				rmi4_data->f01_data_base_addr =
						rmi_fd.data_base_addr;
				rmi4_data->f01_cmd_base_addr =
						rmi_fd.cmd_base_addr;
				break;
			case SYNAPTICS_RMI4_F34:
				f34found = true;
				fwu->f34_fd.query_base_addr =
						rmi_fd.query_base_addr;
				fwu->f34_fd.ctrl_base_addr =
						rmi_fd.ctrl_base_addr;
				fwu->f34_fd.data_base_addr =
						rmi_fd.data_base_addr;

				fwu->intr_mask = 0;
				intr_src = rmi_fd.intr_src_count;
				intr_off = intr_count % 8;
				for (ii = intr_off;
						ii < ((intr_src & MASK_3BIT) +
						intr_off);
						ii++) {
					fwu->intr_mask |= 1 << ii;
				}
				break;
			}
		} else {
			break;
		}

		intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
	}

	if (!f01found || !f34found) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to find both F01 and F34\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

static int fwu_write_blocks(struct synaptics_rmi4_fwu_handle *fwu,
		unsigned char *block_ptr, unsigned short block_cnt,
		unsigned char command)
{
	int retval;
	unsigned char block_number[] = {0, 0};
	unsigned short blk;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	block_number[1] |= (fwu->config_area << 5);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fwu->f34_fd.data_base_addr + BLOCK_NUMBER_OFFSET,
			block_number,
			sizeof(block_number));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write to block number registers\n",
				__func__);
		return retval;
	}

	for (blk = 0; blk < block_cnt; blk++) {
		retval = synaptics_rmi4_reg_write(rmi4_data,
				fwu->f34_fd.data_base_addr + fwu->off.blk_data,
				block_ptr,
				fwu->block_size);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to write block data (block %d)\n",
					__func__, blk);
			return retval;
		}

		retval = fwu_write_f34_command(fwu, command);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to write command for block %d\n",
					__func__, blk);
			return retval;
		}

		retval = fwu_wait_for_idle(fwu, WRITE_WAIT_MS);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to wait for idle status (block %d)\n",
					__func__, blk);
			return retval;
		}

		block_ptr += fwu->block_size;
	}

	return 0;
}

static int fwu_write_firmware(struct synaptics_rmi4_fwu_handle *fwu)
{
	unsigned short firmware_block_count;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	firmware_block_count = fwu->img.ui_firmware.size / fwu->block_size;
	if (firmware_block_count != fwu->bcount.ui_firmware) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Firmware size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return fwu_write_blocks(fwu, (unsigned char *)fwu->img.ui_firmware.data,
			firmware_block_count, CMD_WRITE_FW_BLOCK);
}

static int fwu_write_configuration(struct synaptics_rmi4_fwu_handle *fwu)
{
	return fwu_write_blocks(fwu, (unsigned char *)fwu->config_data,
			fwu->config_block_count, CMD_WRITE_CONFIG_BLOCK);
}

static int fwu_write_ui_configuration(struct synaptics_rmi4_fwu_handle *fwu)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	fwu->config_area = UI_CONFIG_AREA;
	fwu->config_data = fwu->img.ui_config.data;
	fwu->config_size = fwu->img.ui_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;
	if (fwu->config_block_count != fwu->bcount.ui_config) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return fwu_write_configuration(fwu);
}

static int fwu_write_disp_configuration(struct synaptics_rmi4_fwu_handle *fwu)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	fwu->config_area = DISP_CONFIG_AREA;
	fwu->config_data = fwu->img.disp_config.data;
	fwu->config_size = fwu->img.disp_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;
	if (fwu->config_block_count != fwu->bcount.disp_config) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return fwu_write_configuration(fwu);
}

static int fwu_write_bootloader_id(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->off.blk_data,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write bootloader ID\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_enter_flash_prog(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	struct f01_device_status f01_device_status;
	struct f01_device_control f01_device_control;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = rmi4_data->irq_enable(rmi4_data, false, true);
	if (retval < 0)
		return retval;

	msleep(INT_DISABLE_WAIT_MS);

	retval = fwu_read_f34_flash_status(fwu);
	if (retval < 0)
		return retval;

	if (fwu->program_enabled)
		return 0;

	retval = fwu_write_bootloader_id(fwu);
	if (retval < 0)
		return retval;

	retval = fwu_write_f34_command(fwu, CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(fwu, ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	if (!fwu->program_enabled) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Program enabled bit not set\n",
				__func__);
		return -EINVAL;
	}

	if (rmi4_data->hw_if->bl_hw_init) {
		retval = rmi4_data->hw_if->bl_hw_init(rmi4_data);
		if (retval < 0)
			return retval;
	}

	retval = fwu_scan_pdt(fwu);
	if (retval < 0)
		return retval;

	retval = fwu_read_f01_device_status(fwu, &f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Not in flash prog mode\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries(fwu);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read F01 device control\n",
				__func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write F01 device control\n",
				__func__);
		return retval;
	}

	msleep(ENTER_FLASH_PROG_WAIT_MS);

	return retval;
}

static int fwu_erase_config(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu_write_bootloader_id(fwu);
	if (retval < 0)
		return retval;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Bootloader ID written\n",
			__func__);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = fwu_write_f34_command(fwu, CMD_ERASE_CONFIG);
		break;
	case DISP_CONFIG_AREA:
		retval = fwu_write_f34_command(fwu, CMD_ERASE_DISP_CONFIG);
		break;
	}
	if (retval < 0)
		return retval;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(fwu, ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Idle status detected\n",
			__func__);

	return retval;
}

static int fwu_do_reflash(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu_write_bootloader_id(fwu);
	if (retval < 0)
		return retval;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(fwu, CMD_ERASE_ALL);
	if (retval < 0)
		return retval;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Erase all command written\n",
			__func__);

	retval = fwu_wait_for_idle(fwu, ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Idle status detected\n",
			__func__);

	if (fwu->img.ui_firmware.data) {
		retval = fwu_write_firmware(fwu);
		if (retval < 0)
			return retval;
		pr_notice("%s: Firmware programmed\n", __func__);
	}

	if (fwu->img.ui_config.data) {
		fwu->config_area = UI_CONFIG_AREA;
		retval = fwu_write_ui_configuration(fwu);
		if (retval < 0)
			return retval;
		pr_notice("%s: Configuration programmed\n", __func__);
	}

	if (fwu->flash_properties.has_disp_config &&
			fwu->img.contains_disp_config) {
		fwu->config_area = DISP_CONFIG_AREA;
		retval = fwu_erase_config(fwu);
		if (retval < 0)
			return retval;
		retval = fwu_write_disp_configuration(fwu);
		if (retval < 0)
			return retval;
		pr_notice("%s: Display configuration programmed\n", __func__);
	}

	return retval;
}

static int fwu_start_reflash(struct synaptics_rmi4_fwu_handle *fwu)
{
	int retval = 0;
	enum flash_area flash_area;
	struct f01_device_status f01_device_status;
	const struct firmware *fw_entry = NULL;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (rmi4_data->sensor_sleep) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Sensor sleeping\n",
				__func__);
		return -ENODEV;
	}

	rmi4_data->stay_awake = true;

	pr_notice("%s: Start of reflash process\n", __func__);

	if (fwu->img.image == NULL) {
		snprintf(fwu->img.image_name, MAX_IMAGE_NAME_LEN, FW_IMAGE_FOLDER FW_IMAGE_NAME, rmi4_data->rmi4_mod_info.product_id_string);
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Requesting firmware image %s\n",
				__func__, fwu->img.image_name);

		retval = request_firmware(&fw_entry, fwu->img.image_name,
				rmi4_data->pdev->dev.parent);
		if (retval != 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Firmware image %s not available\n",
					__func__, fwu->img.image_name);
			retval = -EINVAL;
			goto exit;
		}

		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Firmware image size = %d\n",
				__func__, (int)fw_entry->size);

		fwu->img.image = fw_entry->data;
	}

	fwu_parse_image_header(fwu);

	if (fwu->bl_version != fwu->img.bl_version) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Bootloader version mismatch\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_read_f01_device_status(fwu, &f01_device_status);
	if (retval < 0)
		goto exit;

	if (f01_device_status.flash_prog) {
		dev_info(rmi4_data->pdev->dev.parent,
				"%s: In flash prog mode\n",
				__func__);
		fwu->in_flash_prog_mode = true;
	} else {
		fwu->in_flash_prog_mode = false;
	}

	flash_area = fwu_go_nogo(fwu);

	if (flash_area != NONE) {
		retval = fwu_enter_flash_prog(fwu);
		if (retval < 0)
			goto exit;
	}

	switch (flash_area) {
	case UI_FIRMWARE:
		retval = fwu_do_reflash(fwu);
		break;
	case CONFIG_AREA:
		fwu->config_area = UI_CONFIG_AREA;
		retval = fwu_erase_config(fwu);
		if (retval < 0)
			break;
		retval = fwu_write_ui_configuration(fwu);
		break;
	case NONE:
	default:
		goto exit2;
	}

	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do reflash\n",
				__func__);
	}

exit:
	rmi4_data->reset_device(rmi4_data);

exit2:
	if (fw_entry)
		release_firmware(fw_entry);

	pr_notice("%s: End of reflash process\n", __func__);

	rmi4_data->stay_awake = false;

	return retval;
}
static void fwu_startup_fw_update_work(struct work_struct *work)
{
	struct synaptics_rmi4_fwu_handle *fwu = container_of(work, struct synaptics_rmi4_fwu_handle, fwu_work);
	fwu_start_reflash(fwu);

	return;
}

int synaptics_rmi4_fw_update_module_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct pdt_properties pdt_props;
	struct synaptics_rmi4_fwu_handle *fwu;

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for fwu\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	fwu->img.image_name = kzalloc(MAX_IMAGE_NAME_LEN, GFP_KERNEL);
	if (!fwu->img.image_name) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for image name\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_fwu;
	}

	fwu->rmi4_data = rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			PDT_PROPS,
			pdt_props.data,
			sizeof(pdt_props.data));
	if (retval < 0) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Failed to read PDT properties, assuming 0x00\n",
				__func__);
	} else if (pdt_props.has_bsr) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Reflash for LTS not currently supported\n",
				__func__);
		retval = -ENODEV;
		goto exit_free_mem;
	}

	retval = fwu_scan_pdt(fwu);
	if (retval < 0)
		goto exit_free_mem;

	retval = fwu_read_f34_queries(fwu);
	if (retval < 0)
		goto exit_free_mem;

	fwu->force_update = FORCE_UPDATE;
	fwu->initialized = true;

	fwu->fwu_workqueue = create_singlethread_workqueue("fwu_workqueue");
	INIT_WORK(&fwu->fwu_work, fwu_startup_fw_update_work);
	queue_work(fwu->fwu_workqueue,
			&fwu->fwu_work);

	return 0;

exit_free_mem:
	kfree(fwu->img.image_name);

exit_free_fwu:
	kfree(fwu);
	fwu = NULL;

exit:
	return retval;
}

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX FW Update Module");
MODULE_LICENSE("GPL v2");
