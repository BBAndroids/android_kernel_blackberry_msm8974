/*
 * Copyright (C) 2014 BlackBerry Limited
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "slimport_tx_drv.h"
#include "slimport_tx_reg.h"
#include "slimport_private.h"
#ifdef CONFIG_SLIMPORT_CEC
#include "cec_lib/slimport_tx_cec.h"
#endif

static unchar bytebuf[MAX_BUF_CNT];


static unchar dpcd_addrh;
static unchar dpcd_addrm;
static unchar dpcd_addrl;

/* EDID access break */
unchar bedid_break;
#ifdef CEC_PHYCISAL_ADDRESS_INSERT
unchar bedid_dtd_beginaddr;
#endif
static unchar bedid_checksum;
unchar bedid_extblock[128] = {0};
unchar bedid_firstblock[128] = {0};


static ulong pclk;
static ulong m_val, n_val;
enum SP_LINK_BW sp_tx_bw;
bool sp_tx_link_config_done;	/*link config done flag*/
bool sp_tx_hw_lt_done;	/*hardware linktraining done indicator*/
bool sp_tx_hw_lt_enable;	/*hardware linktraining enable*/
bool sp_tx_test_lt;
static unchar sp_tx_test_bw;
static bool sp_tx_test_edid;
static unchar sp_tx_ds_vid_stb_cntr;
unchar slimport_link_bw;

/* for HDCP */
static unchar sp_tx_hdcp_auth_pass;
/*HDCP auth fail count flag*/
static unchar sp_tx_hdcp_auth_fail_counter;
static unchar sp_tx_hdcp_auth_fail_counter1;

static unchar sp_tx_hdcp_capable_chk;
unchar sp_tx_hw_hdcp_en;
static unchar sp_tx_hdcp_auth_done;

enum RX_CBL_TYPE sp_tx_rx_type;
enum RX_CBL_TYPE sp_tx_rx_type_backup;
unchar sp_tx_pd_mode;


static struct AudiInfoframe sp_tx_audioinfoframe;
static struct Packet_AVI sp_tx_packet_avi;
static struct Packet_SPD sp_tx_packet_spd;
static struct Packet_MPEG sp_tx_packet_mpeg;
enum SP_TX_System_State sp_tx_system_state;
enum SP_TX_System_State sp_tx_system_state_bak;


/* ***************************************************************** */

/* GLOBAL VARIABLES DEFINITION FOR HDMI START */

/* ***************************************************************** */

static unchar g_hdmi_dvi_status;
static unchar g_cur_pix_clk;
static unchar g_video_stable_cntr;
static unchar g_audio_stable_cntr;
static unchar g_sync_expire_cntr;
static unchar g_hdcp_err_cnt;

static ulong g_cur_h_res;
static ulong g_cur_v_res;
static unchar g_video_muted;
static unchar g_audio_muted;
static unchar g_cts_got;
static unchar g_audio_got;

static enum HDMI_RX_System_State hdmi_system_state;


#ifdef CONFIG_SLIMPORT_FAST_CHARGE
/*for ccharging status*/
enum CHARGING_STATUS downstream_charging_status;
static bool bcharging__status_capable;
#endif

/*****************************************************************
//GLOBAL VARIABLES DEFINITION FOR HDMI END
*****************************************************************/


void sp_tx_variable_init(void)
{

	sp_tx_hdcp_auth_fail_counter = 0;
	sp_tx_hdcp_auth_fail_counter1 = 0;
	sp_tx_hdcp_auth_pass = 0;
	sp_tx_hw_hdcp_en = 0;
	sp_tx_hdcp_capable_chk = 0;
	sp_tx_hdcp_auth_done = 0;
	sp_tx_pd_mode = 1;
	sp_tx_rx_type = RX_NULL;
	sp_tx_rx_type_backup = RX_NULL;
	sp_tx_hw_lt_done = 0;
	sp_tx_hw_lt_enable = 0;
	sp_tx_link_config_done = 0;
	sp_tx_test_lt = 0;
	sp_tx_test_bw = 0;
	sp_tx_test_edid = 0;
	sp_tx_ds_vid_stb_cntr = 0;

	bedid_checksum = 0;
	bedid_break = 0;
	sp_tx_bw = BW_54G;
	slimport_link_bw = 0;
	hdcp_en = 0;
#ifdef CONFIG_SLIMPORT_FAST_CHARGE
	downstream_charging_status = NO_CHARGING;
	bcharging__status_capable = 0;
#endif

#ifdef CONFIG_SLIMPORT_CEC
	cec_status_set(CEC_NOT_READY);
#endif
}

static void sp_tx_api_m_gen_clk_select(unchar bspreading)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_M_CALCU_CTRL, &c);
	if (bspreading) {
		c |= M_GEN_CLK_SEL;
		sp_write_reg(TX_P0, SP_TX_M_CALCU_CTRL, c);
	} else {
		c &= ~M_GEN_CLK_SEL;
		sp_write_reg(TX_P0, SP_TX_M_CALCU_CTRL, c);
	}
}

static void sp_tx_link_phy_initialization(void)
{
	if (is_phy_tuning_set()) {
		DEV_DBG("PHY init with custom tuning");
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, phy_tuning[0]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, phy_tuning[1]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, phy_tuning[2]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, phy_tuning[3]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, phy_tuning[4]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, phy_tuning[5]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, phy_tuning[6]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, phy_tuning[7]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, phy_tuning[8]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, phy_tuning[9]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG10, phy_tuning[10]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG11, phy_tuning[11]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG12, phy_tuning[12]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG13, phy_tuning[13]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, phy_tuning[14]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, phy_tuning[15]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, phy_tuning[16]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, phy_tuning[17]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, phy_tuning[18]);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, phy_tuning[19]);
	} else {
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, 0x19);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, 0x1b);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, 0x22);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, 0x23);

		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, 0x09);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, 0x16);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, 0x1F);

		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, 0x26);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, 0x28);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, 0x2F);

		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, 0x10);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, 0x1F);

		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, 0x36);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, 0x3c);
		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, 0x18);

		sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, 0x3F);
	}
}

void sp_tx_initialization(void)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_EXTRA_ADDR_REG, &c);
	c |= I2C_EXTRA_ADDR & (~I2C_STRETCH_DISABLE);
	sp_write_reg(TX_P0, SP_TX_EXTRA_ADDR_REG, c);

	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL , &c);
	c |= LINK_POLLING;
	c &= ~AUTO_START;
	c &= ~AUTO_EN;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL, c);

	sp_read_reg(TX_P0, SP_TX_LINK_DEBUG_REG , &c);
	c |= M_VID_DEBUG;
	sp_write_reg(TX_P0, SP_TX_LINK_DEBUG_REG, c);

	sp_read_reg(TX_P0, SP_TX_DEBUG_REG1, &c);
	c |= FORCE_HPD | FORCE_PLL_LOCK;
	c &= ~POLLING_EN;
	sp_write_reg(TX_P0, SP_TX_DEBUG_REG1, c);

	sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, &c);
	c |= AUX_TERM_50OHM;
	sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, c);

	sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, &c);
	c &= ~P5V_PROTECT_PD;
	c &= ~SHORT_PROTECT_PD;
	sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, c);

	sp_read_reg(TX_P2, SP_TX_ANALOG_DEBUG_REG2, &c);
	c |= POWERON_TIME_1P5MS;
	sp_write_reg(TX_P2, SP_TX_ANALOG_DEBUG_REG2, c);

	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, &c);
	c |= BKSV_SRM_PASS;
	c |= KSVLIST_VLD;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);

	sp_write_reg(TX_P2, SP_TX_ANALOG_CTRL, 0xC5);
	sp_write_reg(TX_P0, I2C_GEN_10US_TIMER0, 0x0e);
	sp_write_reg(TX_P0, I2C_GEN_10US_TIMER1, 0x01);

	c = AUTO_POLLING_DISABLE;
	sp_write_reg(TX_P0, SP_TX_DP_POLLING_CTRL_REG, c);
	/*Short the link check timer for HDCP CTS item1a-07*/
	sp_write_reg(TX_P0, SP_TX_LINK_CHK_TIMER, 0x1d);

	sp_read_reg(TX_P0, SP_TX_MISC_CTRL_REG, &c);
	c |= EQ_TRAINING_LOOP;
	sp_write_reg(TX_P0, SP_TX_MISC_CTRL_REG, c);

	sp_write_reg(TX_P2, SP_COMMON_INT_MASK1, 0X00);
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK2, 0X00);
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK3, 0X00);
	sp_write_reg(TX_P2, SP_COMMON_INT_MASK4, 0X00);
	sp_write_reg(TX_P2, SP_INT_MASK, 0X90);
	sp_write_reg(TX_P2, SP_TX_INT_CTRL_REG, 0X01);
	sp_write_reg(TX_P0, 0x20, 0xa2);
	sp_write_reg(TX_P0, 0x21, 0x7e);
	sp_read_reg(TX_P0, 0x1f, &c);
	c |= 0x04;
	sp_write_reg(TX_P0, 0x1f, c);

	sp_tx_link_phy_initialization();
	sp_tx_api_m_gen_clk_select(1);
	dpcd_addrh = 0x00;
	dpcd_addrm = 0x00;
	dpcd_addrl = 0x00;

}

void sp_tx_power_down(enum SP_TX_POWER_BLOCK sp_tx_pd_block)
{
	unchar c;

	sp_read_reg(TX_P2, SP_POWERD_CTRL_REG, &c);
	if (sp_tx_pd_block == SP_TX_PWR_REG) {
		if (c&REGISTER_PD)
			return;
		else
			c |= REGISTER_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_HDCP) {
		if (c&HDCP_PD)
			return;
		else
			c |= HDCP_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_AUDIO) {
		if (c&AUDIO_PD)
			return;
		else
			c |= AUDIO_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_VIDEO) {
		if (c&VIDEO_PD)
			return;
		else
			c |= VIDEO_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_LINK) {
		if (c&LINK_PD)
			return;
		else
			c |= LINK_PD;

	} else if (sp_tx_pd_block == SP_TX_PWR_TOTAL) {
		if (c&TOTAL_PD)
			return;
		else
			c |= TOTAL_PD;
	}


	sp_write_reg(TX_P2, SP_POWERD_CTRL_REG, c);

	 DEV_DBG("sp_tx_power_down: %x", sp_tx_pd_block);
}

void sp_tx_power_on(enum SP_TX_POWER_BLOCK sp_tx_pd_block)
{
	unchar c;

	sp_read_reg(TX_P2, SP_POWERD_CTRL_REG, &c);
	if (sp_tx_pd_block == SP_TX_PWR_REG) {
		if ((c&REGISTER_PD) == 0)
			return;
		else
			c &= ~REGISTER_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_HDCP) {
		if ((c&HDCP_PD) == 0)
			return;
		else
			c &= ~HDCP_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_AUDIO) {
		if ((c&AUDIO_PD) == 0)
			return;
		else
			c &= ~AUDIO_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_VIDEO) {
		if ((c&VIDEO_PD) == 0)
			return;
		else
			c &= ~VIDEO_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_LINK) {
		if ((c&LINK_PD) == 0)
			return;
		else
			c &= ~LINK_PD;
	} else if (sp_tx_pd_block == SP_TX_PWR_TOTAL) {
		if ((c&TOTAL_PD) == 0)
			return;
		else
			c &= ~TOTAL_PD;
	}


	sp_write_reg(TX_P2, SP_POWERD_CTRL_REG, c);
	DEV_DBG("sp_tx_power_on");

}

#define SOURCE_AUX_OK 1
#define SOURCE_AUX_ERR 0
#define SOURCE_REG_OK 1
#define SOURCE_REG_ERR 0

#define SINK_DEV_SEL  0x005f0
#define SINK_ACC_OFS  0x005f1
#define SINK_ACC_REG  0x005f2

bool source_aux_read_7730dpcd(long addr, unchar cCount, unchar *pBuf)
{
	unchar c;
	unchar addr_l;
	unchar addr_m;
	unchar addr_h;
	addr_l = (unchar)addr;
	addr_m = (unchar)(addr >> 8);
	addr_h = (unchar)(addr >> 16);
	c = 0;
	while (1) {
		if (sp_tx_aux_dpcdread_bytes(addr_h, addr_m,
					addr_l, cCount, pBuf) == AUX_OK)
			return SOURCE_AUX_OK;
		c++;
		if (c > 3)
			return SOURCE_AUX_ERR;
	}
}

bool source_aux_write_7730dpcd(long addr, unchar cCount, unchar *pBuf)
{
	unchar c;
	unchar addr_l;
	unchar addr_m;
	unchar addr_h;
	addr_l = (unchar)addr;
	addr_m = (unchar)(addr >> 8);
	addr_h = (unchar)(addr >> 16);
	c = 0;
	while (1) {
		if (sp_tx_aux_dpcdwrite_bytes(addr_h, addr_m,
						addr_l, cCount, pBuf) == AUX_OK)
			return SOURCE_AUX_OK;
		c++;
		if (c > 3)
			return SOURCE_AUX_ERR;
	}
}

bool i2c_master_read_reg(unchar Sink_device_sel, unchar offset, unchar *Buf)
{
	unchar sbytebuf[2] = {0};
	long a0, a1;
	a0 = SINK_DEV_SEL;
	a1 = SINK_ACC_REG;
	sbytebuf[0] = Sink_device_sel;
	sbytebuf[1] = offset;

	if (source_aux_write_7730dpcd(a0, 2, sbytebuf) == SOURCE_AUX_OK) {
		if (source_aux_read_7730dpcd(a1, 1, Buf) == SOURCE_AUX_OK)
			return SOURCE_REG_OK;
	}
	return SOURCE_REG_ERR;
}

bool i2c_master_write_reg(unchar Sink_device_sel, unchar offset, unchar value)
{
	unchar sbytebuf[3] = {0};
	long a0;
	a0 = SINK_DEV_SEL;
	sbytebuf[0] = Sink_device_sel;
	sbytebuf[1] = offset;
	sbytebuf[2] = value;

	if (source_aux_write_7730dpcd(a0, 3, sbytebuf) == SOURCE_AUX_OK)
		return SOURCE_REG_OK;
	else
		return SOURCE_REG_ERR;
}

void sp_tx_rst_aux(void)
{
	unchar c, c1;

	sp_read_reg(TX_P0, SP_TX_DEBUG_REG1, &c1);
	c = c1;
	c1 &= ~HPD_POLLING_EN;
	c1 &= ~POLLING_EN;
	sp_write_reg(TX_P0, SP_TX_DEBUG_REG1, c1);

	sp_read_reg(TX_P2, SP_TX_RST_CTRL2_REG, &c1);
	c1 |= AUX_RST;
	sp_write_reg(TX_P2, SP_TX_RST_CTRL2_REG, c1);
	c1 &= ~AUX_RST;
	sp_write_reg(TX_P2, SP_TX_RST_CTRL2_REG, c1);

	/* enable polling after reset AUX-ANX.Fei-2011.9.19 */
	sp_write_reg(TX_P0, SP_TX_DEBUG_REG1, c);
}

static unchar sp_tx_wait_aux_finished(void)
{
	unchar c;
	unchar ccnt;
	ccnt = 0;

	sp_read_reg(TX_P0, SP_TX_AUX_STATUS, &c);

	while (c & AUX_BUSY) {
		ccnt++;
		sp_read_reg(TX_P0, SP_TX_AUX_STATUS, &c);
		if (ccnt > 100) {
			DEV_ERR("AUX Operaton timed out.\n");
			break;
		}
	}

	if (c & 0x0F) {
		DEV_ERR("aux operation failed %.2x\n", (uint)c);
		return 0;
	} else
		return 1;
}

unchar sp_tx_aux_dpcdread_bytes(unchar addrh, unchar addrm,
				unchar addrl, unchar c_count, unchar *p_buf)
{
	unchar c, i, c1;
	unchar bOK;

	sp_write_reg(TX_P0, SP_TX_BUF_DATA_COUNT_REG, 0x80);
	c = ((c_count - 1) << 4) | 0x09;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, c);

	if (dpcd_addrl != addrl) {
		dpcd_addrl = addrl;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_7_0_REG, addrl);
	}


	if (dpcd_addrm != addrm) {
		dpcd_addrm = addrm;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_15_8_REG, addrm);
	}

	if (dpcd_addrh != addrh) {
		dpcd_addrh = addrh;
		sp_read_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, &c);
		c = (c & 0xf0) | addrh;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, c);
	}


	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	c |= AUX_OP_EN;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	msleep(2);
	bOK = sp_tx_wait_aux_finished();

	if (!bOK) {
		DEV_ERR("aux read failed\n");
		/*add by span 20130217.*/
		sp_read_reg(TX_P2, SP_TX_INT_STATUS1, &c);
		sp_read_reg(TX_P0, SP_TX_DEBUG_REG1, &c1);
		/*if polling is enabled, wait polling error interrupt*/
		if (c1 & POLLING_EN) {
			if (c & POLLING_ERR)
				sp_tx_rst_aux();
		} else
			sp_tx_rst_aux();
		return AUX_ERR;
	}


	for (i = 0; i < c_count; i++) {
		sp_read_reg(TX_P0, SP_TX_BUF_DATA_0_REG + i, &c);

		*(p_buf + i) = c;

		if (i >= MAX_BUF_CNT)
			break;
	}

	return AUX_OK;
}

unchar sp_tx_aux_dpcdwrite_bytes(unchar addrh, unchar addrm,
			unchar addrl, unchar c_count, unchar *p_buf)
{
	unchar c, i;
	unchar bOK;


	c =  ((c_count - 1) << 4) | 0x08;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, c);

	if (dpcd_addrl != addrl) {
		dpcd_addrl = addrl;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_7_0_REG, addrl);
	}

	if (dpcd_addrm != addrm) {
		dpcd_addrm = addrm;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_15_8_REG, addrm);
	}

	if (dpcd_addrh != addrh) {
		dpcd_addrh = addrh;
		sp_read_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, &c);
		c = (c & 0xf0) | addrh;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, c);
	}

	for (i = 0; i < c_count; i++) {
		c = *p_buf;
		p_buf++;
		sp_write_reg(TX_P0, SP_TX_BUF_DATA_0_REG + i, c);

		if (i >= MAX_BUF_CNT)
			break;
	}

	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	c |= 0x01;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);

	bOK = sp_tx_wait_aux_finished();

	if (bOK)
		return AUX_OK;
	else
		return AUX_ERR;
}

static void sp_tx_aux_dpcdwrite_byte(unchar addrh, unchar addrm,
	unchar addrl, unchar data1)
{

	unchar c;


	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x08);

	if (dpcd_addrl != addrl) {
		dpcd_addrl = addrl;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_7_0_REG, addrl);
	}

	if (dpcd_addrm != addrm) {
		dpcd_addrm = addrm;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_15_8_REG, addrm);
	}

	if (dpcd_addrh != addrh) {
		dpcd_addrh = addrh;
		sp_read_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, &c);
		c = (c & 0xf0) | addrh;
		sp_write_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, c);
	}

	sp_write_reg(TX_P0, SP_TX_BUF_DATA_0_REG, data1);

	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	c |= AUX_OP_EN;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);

	sp_tx_wait_aux_finished();

	return;
}


void sp_tx_set_colorspace(void)
{
	unchar c;
	unchar color_space;

	sp_read_reg(RX_P1, HDMI_RX_AVI_DATA00_REG, &color_space);
	color_space &= 0x60;
	sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &c);
	c = (c & 0xfc) | color_space >> 5;
	sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, c);
	switch (sp_tx_rx_type) {
	case RX_VGA_9832:
	case RX_VGA_GEN:
	case RX_DP:
		sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &color_space);
		if ((color_space & 0x03) == 0x01) {
			sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
			c |= RANGE_Y2R;
			c |= CSPACE_Y2R;
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);

			sp_read_reg(RX_P1, (HDMI_RX_AVI_DATA00_REG + 3), &c);
			if ((c == 0x04) || (c == 0x05) || (c == 0x10) ||
				(c == 0x13) || (c == 0x14) || (c == 0x1F) ||
				(c == 0x20) || (c == 0x21) || (c == 0x22) ||
				(c == 0x27) || (c == 0x28) || (c == 0x29) ||
				(c == 0x2E) || (c == 0x2F) || (c == 0x3C) ||
				(c == 0x3D) || (c == 0x3E) || (c == 0x3F) ||
				(c == 0x40)) {
				sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
				c |= CSC_STD_SEL;
				sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);
			} else {
				sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
				c &= ~CSC_STD_SEL;
				sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);
			}

			sp_read_reg(TX_P2, SP_TX_VID_CTRL6_REG, &c);
			c |= VIDEO_PROCESS_EN;
			c |= UP_SAMPLE;
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, c);
		} else if ((color_space & 0x03) == 0x02) {
			sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
			c |= RANGE_Y2R;
			c |= CSPACE_Y2R;
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);

			sp_read_reg(RX_P1, (HDMI_RX_AVI_DATA00_REG + 3), &c);
			if ((c == 0x04) || (c == 0x05) || (c == 0x10) ||
			(c == 0x13) || (c == 0x14) || (c == 0x1F) ||
			(c == 0x20) || (c == 0x21) || (c == 0x22)) {
				sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
				c |= CSC_STD_SEL;
				sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);
			} else {
				sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
				c &= ~CSC_STD_SEL;
				sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);
			}

			sp_read_reg(TX_P2, SP_TX_VID_CTRL6_REG, &c);
			c |= VIDEO_PROCESS_EN;
			c &= ~UP_SAMPLE;
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, c);
		} else if ((color_space & 0x03) == 0x00) {
			sp_read_reg(TX_P2, SP_TX_VID_CTRL5_REG, &c);
			c &= ~RANGE_Y2R;
			c &= ~CSPACE_Y2R;
			c &= ~CSC_STD_SEL;
			sp_write_reg(TX_P2, SP_TX_VID_CTRL5_REG, c);

			sp_read_reg(TX_P2, SP_TX_VID_CTRL6_REG, &c);
			c &= ~VIDEO_PROCESS_EN;
			c &= ~UP_SAMPLE;
			sp_write_reg(TX_P2, SP_TX_VID_CTRL6_REG, c);
		}
		break;
	case RX_HDMI:
	default:
		break;
	}
}

static void sp_tx_vsi_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 10; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		sp_tx_packet_mpeg.MPEG_data[i] = c;
	}
}


static void sp_tx_mpeg_setup(void)
{
	unchar c;
	int i;

	for (i = 0; i < 10; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		sp_tx_packet_mpeg.MPEG_data[i] = c;
	}
}




static void sp_tx_get_int_status(enum INTStatus IntIndex, unchar *c_status)
{
	unchar c;

	sp_read_reg(TX_P2, SP_COMMON_INT_STATUS1 + IntIndex, &c);
	sp_write_reg(TX_P2, SP_COMMON_INT_STATUS1 + IntIndex, c);

	*c_status = c;
}

static unchar sp_tx_get_pll_lock_status(void)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_DEBUG_REG1, &c);
	if (c & DEBUG_PLL_LOCK)
		return 1;
	else
		return 0;

}



static void sp_tx_lvttl_bit_mapping(void)
{

	enum HDMI_color_depth hdmi_input_color_depth = Hdmi_legacy;
	unchar c, c1;
	unchar vid_bit, value;

	sp_read_reg(RX_P0, HDMI_RX_VIDEO_STATUS_REG1, &c1);
	c1 &= COLOR_DEPTH;
	if (c1 == 0x00)
		hdmi_input_color_depth = Hdmi_legacy;
	else if (c1 == 0x40)
		hdmi_input_color_depth = Hdmi_24bit;
	else if (c1 == 0x50)
		hdmi_input_color_depth = Hdmi_30bit;
	else if (c1 == 0x60)
		hdmi_input_color_depth = Hdmi_36bit;
	else
		DEV_ERR("HDMI input color depth is not supported .\n");

	switch (hdmi_input_color_depth) {
	case Hdmi_legacy:
	case Hdmi_24bit:
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c = c & ~IN_BIT_SEl;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);

		sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f) | IN_BPC_8BIT;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, c);

		for (c = 0; c < 24; c++) {
			vid_bit = SP_TX_VID_BIT_CTRL0_REG + c;
			value = c;
			sp_write_reg(TX_P2, vid_bit, value);
		}


		break;
	case Hdmi_30bit:
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c = c & ~IN_BIT_SEl;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);

		sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f) | IN_BPC_10BIT;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, c);

		for (c = 0; c < 10; c++) {
			vid_bit = SP_TX_VID_BIT_CTRL0_REG + c;
			value = 0x02 + c;
			sp_write_reg(TX_P2, vid_bit, value);
		}

		for (c = 0; c < 10; c++) {
			vid_bit = SP_TX_VID_BIT_CTRL10_REG + c;
			value = 0x0E + c;
			sp_write_reg(TX_P2, vid_bit, value);
		}

		for (c = 0; c < 10; c++) {
			vid_bit = SP_TX_VID_BIT_CTRL20_REG + c;
			value = 0x1A + c;
			sp_write_reg(TX_P2, vid_bit, value);
		}

		break;
	case Hdmi_36bit:
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c = c & ~IN_BIT_SEl;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);

		sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &c);
		c = ((c & 0x8f) | IN_BPC_12BIT);
		sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, c);

		for (c = 0; c < 36; c++) {
			vid_bit = SP_TX_VID_BIT_CTRL0_REG + c;
			value = c;
			sp_write_reg(TX_P2, vid_bit, value);
		}
		break;
	default:
		break;
	}

	if (sp_tx_test_edid) {
		/*set color depth to 18-bit for link cts*/
		sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f);
		sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, c);
		sp_tx_test_edid = 0;
		DEV_DBG("***color space is set to 18bit***");
	}
	sp_read_reg(RX_P1, HDMI_RX_AVI_DATA00_REG, &c);
	if (c & 0x60) {
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET1, 0x80);
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET2, 0x00);
		sp_write_reg(TX_P0, SP_TX_VID_BLANK_SET3, 0x80);
	}

}


void sp_tx_enable_video_input(unchar enable)
{
	unchar c;

	if (enable) {
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c = (c & 0xf7) | VIDEO_EN;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);
		DEV_DBG("Slimport Video is enabled!\n");

	} else {
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c &= ~VIDEO_EN;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);
		DEV_DBG("Slimport Video is disabled!\n");

	}
}

static void sp_tx_enhancemode_set(void)
{
	unchar c;
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_LANE_COUNT, 1, &c);

	if (c & ENHANCED_FRAME_CAP) {

		sp_read_reg(TX_P0, SP_TX_SYS_CTRL4_REG, &c);
		c |= ENHANCED_MODE;
		sp_write_reg(TX_P0, SP_TX_SYS_CTRL4_REG, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_LANE_COUNT_SET, 1, &c);
		c |= ENHANCED_FRAME_EN;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01,
			DPCD_LANE_COUNT_SET, c);

		DEV_DBG("Enhance mode enabled\n");
	} else {

		sp_read_reg(TX_P0, SP_TX_SYS_CTRL4_REG, &c);
		c &= ~ENHANCED_MODE;
		sp_write_reg(TX_P0, SP_TX_SYS_CTRL4_REG, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_LANE_COUNT_SET, 1, &c);
		c &= ~ENHANCED_FRAME_EN;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01,
			DPCD_LANE_COUNT_SET, c);

		DEV_DBG("Enhance mode disabled\n");
	}
}

static void sp_tx_hdcp_reauth(void)
{
	unchar c;
	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, &c);
	c |= RE_AUTH;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);
	c &= ~RE_AUTH;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);
}

static void sp_tx_clean_hdcp_status(void)
{

	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, 0x00);
	sp_tx_hdcp_reauth();
}

static void sp_tx_hdcp_encryption_disable(void)
{
	unchar c;
	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, &c);
	c &= ~ENC_EN;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);
}

static void sp_tx_hdcp_encryption_enable(void)
{
	unchar c;
	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, &c);
	c |= ENC_EN;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);
}

static void sp_tx_hw_hdcp_enable(void)
{
	unchar c;
	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, &c);
	c &= ~ENC_EN;
	c &= ~HARD_AUTH_EN;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);
	sp_read_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, &c);
	c |= HARD_AUTH_EN;
	c |= BKSV_SRM_PASS;
	c |= KSVLIST_VLD;
	c |= ENC_EN;
	sp_write_reg(TX_P0, SP_TX_HDCP_CTRL0_REG, c);
	sp_write_reg(TX_P0, SP_TX_WAIT_R0_TIME, 0xb0);
	sp_write_reg(TX_P0, SP_TX_WAIT_KSVR_TIME, 0xc8);

	/*sp_write_reg(TX_P2, SP_COMMON_INT_MASK2, 0xfc);*/
	DEV_DBG("Hardware HDCP is enabled.\n");

}

void sp_tx_clean_hdcp(void)
{

	sp_tx_hdcp_auth_fail_counter = 0;
	sp_tx_hdcp_auth_pass = 0;
	sp_tx_hw_hdcp_en = 0;
	sp_tx_hdcp_capable_chk = 0;
	sp_tx_hdcp_auth_done = 0;
	sp_tx_clean_hdcp_status();
	DEV_DBG("HDCP Clean!\n");
}

static void sp_tx_pclk_calc(unchar *hbr_rbr)
{
	ulong str_clk = 0;
	unchar c;
	unchar link_bw_current = *hbr_rbr;


	switch (link_bw_current) {

	case 0x14:
		str_clk = 540;
		break;
	case 0x0a:
		str_clk = 270;
		break;
	case 0x06:
		str_clk = 162;
		break;
	default:
		break;

	}

	sp_read_reg(TX_P0, M_VID_2, &c);
	m_val = c * 0x10000;
	sp_read_reg(TX_P0, M_VID_1, &c);
	m_val = m_val + c * 0x100;
	sp_read_reg(TX_P0, M_VID_0, &c);
	m_val = m_val + c;

	sp_read_reg(TX_P0, N_VID_2, &c);
	n_val = c * 0x10000;
	sp_read_reg(TX_P0, N_VID_1, &c);
	n_val = n_val + c * 0x100;
	sp_read_reg(TX_P0, N_VID_0, &c);
	n_val = n_val + c;

	str_clk = str_clk * m_val;
	pclk = str_clk;
	pclk = pclk / n_val;
}

void sp_tx_show_infomation(void)
{
	unchar c, c1;
	uint h_res, h_act, v_res, v_act;
	uint h_fp, h_sw, h_bp, v_fp, v_sw, v_bp;
	ulong fresh_rate;

	DEV_DBG("\n*******SP Video Information*******\n");
	DEV_DBG("sp_tx_rx_type %x\n", (uint)sp_tx_rx_type);

	sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c);
	if (c == 0x06) {
		DEV_DBG("BW = 1.62G\n");
		sp_tx_pclk_calc(&c);
	} else if (c == 0x0a) {
		DEV_DBG("BW = 2.7G\n");
		sp_tx_pclk_calc(&c);
	} else if (c == 0x14) {
		DEV_DBG("BW = 5.4G\n");
		sp_tx_pclk_calc(&c);
	}
#ifdef SSC_EN
	DEV_DBG("   SSC On");
#else
	DEV_DBG("   SSC Off");
#endif

	DEV_DBG("   M = %lu, N = %lu, PCLK = %ld MHz\n", m_val, n_val, pclk);

	sp_read_reg(TX_P2, SP_TX_TOTAL_LINE_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_TOTAL_LINE_STA_H, &c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;

	sp_read_reg(TX_P2, SP_TX_ACT_LINE_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_ACT_LINE_STA_H, &c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;

	sp_read_reg(TX_P2, SP_TX_TOTAL_PIXEL_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_TOTAL_PIXEL_STA_H, &c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;

	sp_read_reg(TX_P2, SP_TX_ACT_PIXEL_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_ACT_PIXEL_STA_H, &c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	sp_read_reg(TX_P2, SP_TX_H_F_PORCH_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_H_F_PORCH_STA_H, &c1);

	h_fp = c1;
	h_fp = h_fp << 8;
	h_fp = h_fp + c;

	sp_read_reg(TX_P2, SP_TX_H_SYNC_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_H_SYNC_STA_H, &c1);

	h_sw = c1;
	h_sw = h_sw << 8;
	h_sw = h_sw + c;

	sp_read_reg(TX_P2, SP_TX_H_B_PORCH_STA_L, &c);
	sp_read_reg(TX_P2, SP_TX_H_B_PORCH_STA_H, &c1);

	h_bp = c1;
	h_bp = h_bp << 8;
	h_bp = h_bp + c;

	sp_read_reg(TX_P2, SP_TX_V_F_PORCH_STA, &c);
	v_fp = c;

	sp_read_reg(TX_P2, SP_TX_V_SYNC_STA, &c);
	v_sw = c;

	sp_read_reg(TX_P2, SP_TX_V_B_PORCH_STA, &c);
	v_bp = c;
	DEV_DBG("   Total resolution is %d * %d\n", h_res, v_res);
	DEV_DBG("   HF=%d, HSW=%d, HBP=%d\n", h_fp, h_sw, h_bp);
	DEV_DBG("   VF=%d, VSW=%d, VBP=%d\n", v_fp, v_sw, v_bp);
	DEV_DBG("   Active resolution is %d * %d ", h_act, v_act);

	fresh_rate = pclk * 1000;
	fresh_rate = fresh_rate / h_res;
	fresh_rate = fresh_rate * 1000;
	fresh_rate = fresh_rate / v_res;
	DEV_DBG(" @ %ldHz\n", fresh_rate);

	sp_read_reg(TX_P0, SP_TX_VID_CTRL, &c);

	if ((c & 0x06) == 0x00)
		DEV_DBG("   ColorSpace: RGB,");
	else if ((c & 0x06) == 0x02)
		DEV_DBG("   ColorSpace: YCbCr422,");
	else if ((c & 0x06) == 0x04)
		DEV_DBG("   ColorSpace: YCbCr444,");

	sp_read_reg(TX_P0, SP_TX_VID_CTRL, &c);

	if ((c & 0xe0) == 0x00)
		DEV_DBG("  6 BPC");
	else if ((c & 0xe0) == 0x20)
		DEV_DBG("  8 BPC");
	else if ((c & 0xe0) == 0x40)
		DEV_DBG("  10 BPC");
	else if ((c & 0xe0) == 0x60)
		DEV_DBG("  12 BPC");

	if (sp_tx_rx_type == RX_HDMI) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x23, 1, bytebuf);
		DEV_DBG("ANX7730 BC current FW Ver %.2x\n",
		    (uint)(bytebuf[0] & 0x7f));
	}

	DEV_DBG("\n***************************\n");
}

static void sp_tx_aux_wr(unchar offset)
{
	unchar c, cnt;
	cnt = 0;

	sp_write_reg(TX_P0, SP_TX_BUF_DATA_0_REG, offset);
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x04);
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, 0x01);
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	while (c & AUX_OP_EN) {
		msleep(2);
		cnt++;

		if (cnt == 10) {
			DEV_ERR("write break\n");
			cnt = 0;
			bedid_break = 1;
			break;
		}

		sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	}
}

static void sp_tx_aux_rd(unchar len_cmd)
{
	unchar c, cnt;
	cnt = 0;

	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, len_cmd);
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, 0x01);
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);

	while (c & AUX_OP_EN) {
		msleep(2);
		cnt++;

		if (cnt == 10) {
			DEV_ERR("read break\n");
			sp_tx_rst_aux();
			bedid_break = 1;
			break;
		}

		sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	}
}

unchar sp_tx_chip_located(void)
{
	unchar c1, c2;

	sp_tx_hardware_poweron();
	sp_read_reg(TX_P2, SP_TX_DEV_IDL_REG, &c1);
	sp_read_reg(TX_P2, SP_TX_DEV_IDH_REG, &c2);
	if ((c1 == 0x08) && (c2 == 0x78)) {
		DEV_DBG("ANX7808 is found.\n");
		return 1;
	} else {
		sp_tx_hardware_powerdown();
		DEV_DBG("ANX7808 is not found.\n");
		return 0;
	}
}


void slimport_driver_version(void)
{
	static unchar ANX7808BA_FW_VER = 0x28;
	sp_write_reg(TX_P2, 0xe7, ANX7808BA_FW_VER);
}


void sp_tx_vbus_poweron(void)
{
	unchar c;
	int i;

	for (i = 0; i < 5; i++) {
		sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, &c);
		c &= ~P5V_PROTECT_PD;
		c &= ~SHORT_PROTECT_PD;
		sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, c);

		sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, &c);
		c &= ~V33_SWITCH_ON;
		sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, c);
		c |= V33_SWITCH_ON;
		sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, c);

		sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, &c);
		if (!(c & 0xc0)) {
			DEV_ERR("3.3V output enabled\n");
			return;
		} else {
			DEV_ERR("VBUS power can not be supplied\n");
		}
	}
}

void sp_tx_vbus_powerdown(void)
{
	unchar c;

	sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, &c);
	c &= ~V33_SWITCH_ON;
	sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL11, c);

	sp_read_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, &c);
	c |= P5V_PROTECT_PD | SHORT_PROTECT_PD;
	sp_write_reg(TX_P2, SP_TX_PLL_FILTER_CTRL6, c);
	DEV_NOTICE("3.3V output disabled\n");
}

static void sp_tx_spread_enable(unchar benable)
{
	unchar c;

	sp_read_reg(TX_P0, SSC_CTRL_REG1, &c);

	if (benable) {
		c |= SPREAD_AMP;
		sp_write_reg(TX_P0, SSC_CTRL_REG1, c);

		sp_read_reg(TX_P2, SP_TX_RST_CTRL2_REG, &c);
		c |= SSC_RST;
		sp_write_reg(TX_P2, SP_TX_RST_CTRL2_REG, c);
		c &= ~SSC_RST;
		sp_write_reg(TX_P2, SP_TX_RST_CTRL2_REG, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, 1, &c);
		c |= SPREAD_AMPLITUDE;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, c);

	} else {

		c &= ~SPREAD_AMP;
		sp_write_reg(TX_P0, SSC_CTRL_REG1, c);

		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, 1, &c);
		c &= ~SPREAD_AMPLITUDE;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, c);
	}
}

static void sp_tx_config_ssc(enum SP_LINK_BW linkbw)
{
	unchar c;

	sp_write_reg(TX_P0, SSC_CTRL_REG1, 0x00);
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_DOWNSPREAD, 1, &c);

#ifndef SSC_1
/*	DEV_DBG("*** Config SSC 0.4% ***");*/

	if (linkbw == BW_54G) {
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0xc0);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL3, 0x75);

	} else if (linkbw == BW_27G) {
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0x5f);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL3, 0x75);
	} else {
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0x9e);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);
	}

#else
/*	DEV_DBG("*** Config SSC 1% ***");*/

	if (linkbw == BW_54G) {
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0xdd);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL2, 0x01);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL3, 0x76);
	} else if (linkbw == BW_27G) {
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0xef);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL3, 0x76);
	} else {
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0x8e);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL2, 0x01);
		sp_write_reg(TX_P0, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);
	}

#endif

	sp_tx_spread_enable(1);
}

static void sp_tx_audioinfoframe_setup(void)
{
	int i;
	unchar c;
	sp_read_reg(RX_P1, HDMI_RX_AUDIO_TYPE_REG, &c);
	sp_tx_audioinfoframe.type = c;
	sp_read_reg(RX_P1, HDMI_RX_AUDIO_VER_REG, &c);
	sp_tx_audioinfoframe.version = c;
	sp_read_reg(RX_P1, HDMI_RX_AUDIO_LEN_REG, &c);
	sp_tx_audioinfoframe.length = c;
	for (i = 0; i < 11; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_AUDIO_DATA00_REG + i), &c);
		sp_tx_audioinfoframe.pb_byte[i] = c;
	}
}

static void sp_tx_enable_audio_output(unchar benable)
{
	unchar c, c1;
	int count;
	unchar pBuf[3] = {0x01, 0xd1, 0x02};

	sp_read_reg(TX_P0, SP_TX_AUD_CTRL, &c);

	if (benable) {
		sp_read_reg(TX_P0, SP_TX_AUD_CTRL, &c);
		if (c&AUD_EN) {
			c &= ~AUD_EN;
			sp_write_reg(TX_P0, SP_TX_AUD_CTRL, c);
		}
		msleep(20);
		if (sp_tx_rx_type == RX_HDMI) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x23, 1, &c1);
			if (c1 < 0x94) {
				count = 0;
				while (1) {
					if (AUX_OK ==
						sp_tx_aux_dpcdwrite_bytes
						(0x00, 0x05, 0xf0, 3, pBuf))
						break;
					count++;
					if (count > 3) {
						DEV_ERR("dpcd write error\n");
						break;
					}
				}
			}
		}

		c |= AUD_EN;
		sp_write_reg(TX_P0, SP_TX_AUD_CTRL, c);

		sp_tx_audioinfoframe_setup();
		sp_tx_config_packets(AUDIF_PACKETS);
	} else {
		c &= ~AUD_EN;
		sp_write_reg(TX_P0, SP_TX_AUD_CTRL, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AUD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
	}
}

static void sp_tx_get_link_bw(unchar *bwtype)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c);

	*bwtype = c;
}

static void sp_tx_config_audio(void)
{
	unchar c, g_bw;
	int i;
	ulong m_aud, ls_clk = 0;
	ulong aud_freq = 0;
	DEV_NOTICE("##Config audio ##");
	sp_tx_power_on(SP_TX_PWR_AUDIO);
	sp_read_reg(RX_P0, 0xCA, &c);

	switch (c & 0x0f) {
	case 0x00:
		aud_freq = 44.1;
		break;
	case 0x02:
		aud_freq = 48;
		break;
	case 0x03:
		aud_freq = 32;
		break;
	case 0x08:
		aud_freq = 88.2;
		break;
	case 0x0a:
		aud_freq = 96;
		break;
	case 0x0c:
		aud_freq = 176.4;
		break;
	case 0x0e:
		aud_freq = 192;
		break;
	default:
		break;
	}

	sp_tx_get_link_bw(&g_bw);

	switch (g_bw) {
	case BW_162G:
		ls_clk = 162000;
		break;
	case BW_27G:
		ls_clk = 270000;
		break;
	case BW_54G:
		ls_clk = 540000;
		break;
	default:
		break;
	}

	DEV_DBG("AUD_Freq = %ld , LS_CLK = %ld\n", aud_freq, ls_clk);

	m_aud = ((512 * aud_freq) / ls_clk) * 32768;
	m_aud = m_aud + 0x05;
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL4, (m_aud & 0xff));
	m_aud = m_aud >> 8;
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL5, (m_aud & 0xff));
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL6, 0x00);

	sp_read_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL0, &c);
	c &= ~AUD_INTERFACE_DISABLE;
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL0, c);

	sp_read_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL2, &c);
	c |= M_AUD_ADJUST_ST;
	sp_write_reg(TX_P1, SP_TX_AUD_INTERFACE_CTRL2, c);

	/* configure layout and channel number */
	sp_read_reg(RX_P0, HDMI_RX_HDMI_STATUS_REG, &c);

	if (c & HDMI_AUD_LAYOUT) {
		sp_read_reg(TX_P2, SP_TX_AUD_CH_NUM_REG5, &c);
		c |= CH_NUM_8 | AUD_LAYOUT;
		sp_write_reg(TX_P2, SP_TX_AUD_CH_NUM_REG5, c);
	} else {
		sp_read_reg(TX_P2, SP_TX_AUD_CH_NUM_REG5, &c);
		c &= ~CH_NUM_8;
		c &= ~AUD_LAYOUT;
		sp_write_reg(TX_P2, SP_TX_AUD_CH_NUM_REG5, c);
	}

	/* transfer audio chaneel status from HDMI Rx to Slinmport Tx */
	for (i = 0; i < 5; i++) {
		sp_read_reg(RX_P0, (HDMI_RX_AUD_IN_CH_STATUS1_REG + i), &c);
		sp_write_reg(TX_P2, (SP_TX_AUD_CH_STATUS_REG1 + i), c);
	}

	/* enable audio */
	sp_tx_enable_audio_output(1);
}


static void sp_tx_get_rx_bw(unchar b_max, unchar *c_bw)
{
	if (b_max)
		sp_tx_aux_dpcdread_bytes(0x00, 0x00,
				DPCD_MAX_LINK_RATE, 1, c_bw);
	else
		sp_tx_aux_dpcdread_bytes(0x00, 0x01,
				DPCD_LINK_BW_SET, 1, c_bw);
}

static void sp_tx_edid_read_initial(void)
{
	unchar c;
	dpcd_addrl = 0x50;
	dpcd_addrm = 0x00;
	dpcd_addrh = 0x00;

	sp_write_reg(TX_P0, SP_TX_AUX_ADDR_7_0_REG, 0x50);
	sp_write_reg(TX_P0, SP_TX_AUX_ADDR_15_8_REG, 0);
	sp_read_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, &c);
	c &= 0xf0;
	sp_write_reg(TX_P0, SP_TX_AUX_ADDR_19_16_REG, c);
}

static unchar sp_tx_aux_edidread_byte(unchar offset)
{

	unchar c, i, edid[16], data_cnt, cnt;
	unchar bReturn = 0;

#ifdef CEC_PHYCISAL_ADDRESS_INSERT
	unchar vsdbdata[4];
    uint VSDBaddr;
#endif

	cnt = 0;

	sp_tx_aux_wr(offset);
	sp_tx_aux_rd(0xf5);

	if ((offset == 0x00) || (offset == 0x80))
		bedid_checksum = 0;

	data_cnt = 0;
	while (data_cnt < 16) {
		sp_read_reg(TX_P0, SP_TX_BUF_DATA_COUNT_REG, &c);
		c = c & 0x1f;
		if (c != 0) {
			for (i = 0; i < c; i++) {
				sp_read_reg(TX_P0,
						SP_TX_BUF_DATA_0_REG + i,
						&edid[i + data_cnt]);
				bedid_checksum = bedid_checksum
					+ edid[i + data_cnt];
			}
		} else {
			sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x01);
			c = ADDR_ONLY_BIT | AUX_OP_EN;
			sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
			sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
			while (c & 0x01) {
				msleep(1);
				cnt++;

				if (cnt == 10) {
					DEV_ERR("read break");
					sp_tx_rst_aux();
					bedid_break = 1;
					bReturn = 0x01;
				}

				sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
			}

			bReturn = 0x02;
			return bReturn;
		}

		data_cnt = data_cnt + c;
		if (data_cnt < 16) {
			sp_tx_rst_aux();
			c = 0x05 | ((0x0f - data_cnt) << 4);
			sp_tx_aux_rd(c);
		}
	}

	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x01);
	c = ADDR_ONLY_BIT | AUX_OP_EN;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);

	while (c & AUX_OP_EN)
		sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);



	if (offset < 0x80) {

		for (i = 0; i < 16; i++)
			bedid_firstblock[offset + i] = edid[i];
	} else if (offset >= 0x80) {

		for (i = 0; i < 16; i++)
			bedid_extblock[offset - 0x80 + i] = edid[i];
#ifdef CEC_PHYCISAL_ADDRESS_INSERT
	if (offset == 0x80) {
		if (edid[0] != 0x02)
			return 0x03;
	}
	for (i = 0; i < 16; i++)
		bedid_extblock[offset-0x80+i] = edid[i];


	if (offset == 0x80)
		bedid_dtd_beginaddr = edid[2];
#endif
	}

	if (offset == 0x70) {
		bedid_checksum = bedid_checksum&0xff;
		bedid_checksum = bedid_checksum - edid[15];
		bedid_checksum = ~bedid_checksum + 1;
		if (bedid_checksum != edid[15]) {
			DEV_ERR("Bad EDID check sum1!\n");
			DEV_ERR("Expected value: %.2x\n",
				(uint)bedid_checksum);
			return 0x02;
		} else
			DEV_DBG("Good EDID check sum1!\n");
	} else if (offset == 0xf0) {
		bedid_checksum = bedid_checksum&0xff;
		bedid_checksum = bedid_checksum - edid[15];
		bedid_checksum = ~bedid_checksum + 1;
		if (bedid_checksum != edid[15]) {
			DEV_ERR("Bad EDID check sum2!\n");
			DEV_ERR("Expected value: %.2x\n",
				(uint)bedid_checksum);
			return 0x02;
		} else
			DEV_DBG("Good EDID check sum2!\n");
	}

	if (offset == 0xf0) {


#ifdef CEC_PHYCISAL_ADDRESS_INSERT

		for (VSDBaddr = 0x04; VSDBaddr < bedid_dtd_beginaddr;) {

			vsdbdata[0] = bedid_extblock[VSDBaddr];
			/*Tag code = 0x03,VSDB block*/
			if ((vsdbdata[0]&0xe0) == 0x60) {
				vsdbdata[1] = bedid_extblock[VSDBaddr+1];
				vsdbdata[2] = bedid_extblock[VSDBaddr+2];
				vsdbdata[3] = bedid_extblock[VSDBaddr+3];

				if ((vsdbdata[1] == 0x03)
					&& (vsdbdata[2] == 0x0c)
					&& (vsdbdata[3] == 0x00)) {
					/*bedid_ds_hdmi = 1;
					physical address followed.*/

					downstream_cec_phy_add_set(
					bedid_extblock[VSDBaddr+4],
					bedid_extblock[VSDBaddr+5]);
					return 0;
				} else {
					/*invalid physical address*/
					downstream_cec_phy_add_set(0xff, 0xff);
					/*bedid_ds_hdmi = 0;*/
					return 0x03;
				}

			}                else {
				VSDBaddr = VSDBaddr+(vsdbdata[0]&0x1f);
				VSDBaddr = VSDBaddr + 0x01;
			}

			if (VSDBaddr > bedid_dtd_beginaddr)
				return 0x03;
		}
#endif

	}


#ifdef EDID_DEBUG_PRINT

		for (i = 0; i < 16; i++) {
			if ((i & 0x0f) == 0)
				DEV_DBG("\n edid: [%.2x]  %.2x  ",
				    (uint)offset, (uint)edid[i]);
			else
				DEV_DBG("%.2x  ", (uint)edid[i]);

			if ((i & 0x0f) == 0x0f)
				DEV_DBG("\n");
		}

#endif

	return bReturn;
}

static void sp_tx_parse_segments_edid(unchar segment, unchar offset)
{
	unchar c, cnt;
	int i;

	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x04);
	dpcd_addrl = 0x30;
	sp_write_reg(TX_P0, SP_TX_AUX_ADDR_7_0_REG, 0x30);
	c = ADDR_ONLY_BIT | AUX_OP_EN;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	sp_tx_wait_aux_finished();
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG, &c);
	sp_write_reg(TX_P0, SP_TX_BUF_DATA_0_REG, segment);

	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x04);
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	c &= ~ADDR_ONLY_BIT;
	c |= AUX_OP_EN;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	cnt = 0;
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	while (c & AUX_OP_EN) {
		msleep(1);
		cnt++;
		if (cnt == 10) {
			DEV_ERR("write break");
			sp_tx_rst_aux();
			cnt = 0;
			bedid_break = 1;
			return;
		}

		sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);

	}

	dpcd_addrl = 0x50;
	sp_write_reg(TX_P0, SP_TX_AUX_ADDR_7_0_REG, 0x50);
	//c = ADDR_ONLY_BIT | AUX_OP_EN;
	//sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	//sp_tx_wait_aux_finished();

	sp_tx_aux_wr(offset);
	//c = ADDR_ONLY_BIT | AUX_OP_EN;
	//sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	//sp_tx_wait_aux_finished();

	sp_tx_aux_rd(0xf5);
	cnt = 0;
	for (i = 0; i < 16; i++) {
		sp_read_reg(TX_P0, SP_TX_BUF_DATA_COUNT_REG, &c);
		while ((c & 0x1f) == 0) {
			msleep(2);
			cnt++;
			sp_read_reg(TX_P0,
					SP_TX_BUF_DATA_COUNT_REG, &c);

			if (cnt == 10) {
				DEV_ERR("read break");
				sp_tx_rst_aux();
				bedid_break = 1;
				return;
			}
		}

		sp_read_reg(TX_P0, SP_TX_BUF_DATA_0_REG + i, &c);
	}

	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x01);
	c = ADDR_ONLY_BIT | AUX_OP_EN;
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);

	while (c & AUX_OP_EN)
		sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
}

static unchar sp_tx_get_edid_block(void)
{
	unchar c;
	sp_tx_aux_wr(0x00);
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, SP_TX_BUF_DATA_0_REG, &c);

	sp_tx_aux_wr(0x7e);
	sp_tx_aux_rd(0x01);
	sp_read_reg(TX_P0, SP_TX_BUF_DATA_0_REG, &c);
	DEV_DBG("EDID Block = %d\n", (int)(c + 1));

	if (c > 3)
		c = 1;

	return c;
}

static void sp_tx_addronly_set(unchar b_set)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_AUX_CTRL_REG2, &c);
	if (b_set) {
		c |= ADDR_ONLY_BIT;
		sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	} else {
		c &= ~ADDR_ONLY_BIT;
		sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, c);
	}
}

static void sp_tx_load_packet(enum PACKETS_TYPE type)
{
	int i;
	unchar c;

	switch (type) {
	case AVI_PACKETS:
		sp_write_reg(TX_P2, SP_TX_AVI_TYPE, 0x82);
		sp_write_reg(TX_P2, SP_TX_AVI_VER, 0x02);
		sp_write_reg(TX_P2, SP_TX_AVI_LEN, 0x0d);

		for (i = 0; i < 13; i++) {
			sp_write_reg(TX_P2, SP_TX_AVI_DB0 + i,
					sp_tx_packet_avi.AVI_data[i]);
		}

		break;

	case SPD_PACKETS:
		sp_write_reg(TX_P2, SP_TX_SPD_TYPE, 0x83);
		sp_write_reg(TX_P2, SP_TX_SPD_VER, 0x01);
		sp_write_reg(TX_P2, SP_TX_SPD_LEN, 0x19);

		for (i = 0; i < 25; i++) {
			sp_write_reg(TX_P2, SP_TX_SPD_DB0 + i,
					sp_tx_packet_spd.SPD_data[i]);
		}

		break;

	case VSI_PACKETS:
		sp_write_reg(TX_P2, SP_TX_MPEG_TYPE, 0x81);
		sp_write_reg(TX_P2, SP_TX_MPEG_VER, 0x01);
		sp_read_reg(RX_P1, HDMI_RX_MPEG_LEN_REG, &c);
		sp_write_reg(TX_P2, SP_TX_MPEG_LEN, c);

		for (i = 0; i < 10; i++) {
			sp_write_reg(TX_P2, SP_TX_MPEG_DB0 + i,
					sp_tx_packet_mpeg.MPEG_data[i]);
		}

		break;
	case MPEG_PACKETS:
		sp_write_reg(TX_P2, SP_TX_MPEG_TYPE, 0x85);
		sp_write_reg(TX_P2, SP_TX_MPEG_VER, 0x01);
		sp_write_reg(TX_P2, SP_TX_MPEG_LEN, 0x0d);

		for (i = 0; i < 10; i++) {
			sp_write_reg(TX_P2, SP_TX_MPEG_DB0 + i,
					sp_tx_packet_mpeg.MPEG_data[i]);
		}

		break;
	case AUDIF_PACKETS:
		sp_write_reg(TX_P2, SP_TX_AUD_TYPE, 0x84);
		sp_write_reg(TX_P2, SP_TX_AUD_VER, 0x01);
		sp_write_reg(TX_P2, SP_TX_AUD_LEN, 0x0a);
		for (i = 0; i < 10; i++) {
			sp_write_reg(TX_P2, SP_TX_AUD_DB0 + i,
					sp_tx_audioinfoframe.pb_byte[i]);
		}

		break;

	default:
		break;
	}
}


void sp_tx_config_packets(enum PACKETS_TYPE b_type)
{
	unchar c;

	switch (b_type) {
	case AVI_PACKETS:

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AVI_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(AVI_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AVI_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AVI_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	case SPD_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~SPD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(SPD_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= SPD_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= SPD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	case VSI_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_tx_load_packet(VSI_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;
	case MPEG_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);


		sp_tx_load_packet(MPEG_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_UD;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		break;
	case AUDIF_PACKETS:
		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AUD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);


		sp_tx_load_packet(AUDIF_PACKETS);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AUD_IF_UP;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AUD_IF_EN;
		sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	default:
		break;
	}
}


void sp_tx_avi_setup(void)
{
	unchar c;
	int i;
	for (i = 0; i < 13; i++) {
		sp_read_reg(RX_P1, (HDMI_RX_AVI_DATA00_REG + i), &c);
		sp_tx_packet_avi.AVI_data[i] = c;
	}
	switch (sp_tx_rx_type) {
	case RX_VGA_9832:
	case RX_VGA_GEN:
	case RX_DP:
		sp_tx_packet_avi.AVI_data[0] &= ~0x60;
		break;
	case RX_HDMI:
		break;
	default:
		break;
	}
}


static unchar sp_tx_bw_lc_sel(void)
{
	unchar over_bw = 0;
	uint pixel_clk = 0;
	enum HDMI_color_depth hdmi_input_color_depth = Hdmi_legacy;
	unchar c;

	DEV_DBG("input pclk = %d\n", (uint)pclk);

	sp_read_reg(RX_P0, HDMI_RX_VIDEO_STATUS_REG1, &c);
	c &= COLOR_DEPTH;

	if (c == 0x00)
		hdmi_input_color_depth = Hdmi_legacy;
	else if (c == 0x40)
		hdmi_input_color_depth = Hdmi_24bit;
	else if (c == 0x50)
		hdmi_input_color_depth = Hdmi_30bit;
	else if (c == 0x60)
		hdmi_input_color_depth = Hdmi_36bit;
	else
		pr_warn("HDMI input color depth is not supported .\n");

	switch (hdmi_input_color_depth) {
	case Hdmi_legacy:
	case Hdmi_24bit:
		pixel_clk = pclk;
		break;
	case Hdmi_30bit:
		pixel_clk = pclk * 5 / 4;
		break;
	case Hdmi_36bit:
		pixel_clk = pclk * 3 / 2;
		break;
	default:
		break;
	}

	if (pixel_clk <= 53) {
		sp_tx_bw = BW_162G;
		over_bw = 0;
	} else if ((53 < pixel_clk) && (pixel_clk <= 89)) {
		if (sp_tx_bw >= BW_27G) {
			sp_tx_bw = BW_27G;
			over_bw = 0;
		} else
			over_bw = 1;
	} else if ((89 < pixel_clk) && (pixel_clk <= 180)) {
		if (sp_tx_bw >= BW_54G) {
			sp_tx_bw = BW_54G;
			over_bw = 0;
		} else
			over_bw = 1;
	} else
		over_bw = 1;

	if (over_bw)
		DEV_ERR("over bw!\n");
	else
		DEV_NOTICE("The optimized BW =%.2x\n", sp_tx_bw);

	return over_bw;
}

unchar sp_tx_hw_link_training(void)
{

	unchar c;

	if (!sp_tx_hw_lt_enable) {

		DEV_NOTICE("Hardware link training");
		if (!sp_tx_get_pll_lock_status()) {
			sp_read_reg(TX_P0, SP_TX_PLL_CTRL_REG, &c);
			c |= PLL_RST;
			sp_write_reg(TX_P0, SP_TX_PLL_CTRL_REG, c);
			c &= ~PLL_RST;
			sp_write_reg(TX_P0, SP_TX_PLL_CTRL_REG, c);
			DEV_ERR("PLL not lock!");
			return 1;
		}
		sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, sp_tx_bw);
		sp_tx_enhancemode_set();

		//Set sink into D0 power state.
		sp_tx_aux_dpcdwrite_byte(0x00, 0x06, 0x00, 0x01);//for both DP 1.1 and DP 1.2

		sp_read_reg(TX_P2, SP_INT_MASK, &c);
		sp_write_reg(TX_P2, SP_INT_MASK, c|0X20);
		sp_write_reg(TX_P0, SP_TX_LT_CTRL_REG, SP_TX_LT_EN);

		sp_tx_hw_lt_enable = 1;

	}

	if (sp_tx_hw_lt_done) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x02, 1, bytebuf);


		if ((bytebuf[0]&0x07) != 0x07) {
			sp_tx_hw_lt_enable = 0;
			sp_tx_hw_lt_done = 0;
			return 1;
		} else {
			sp_tx_hw_lt_done = 1;

			/* if there is link error, adjust pre-emphsis to check error again.
			If there is no error,keep the setting */
			if (!sp_tx_test_lt) {
				if (sp_tx_link_err_check()) {
					sp_read_reg(TX_P0, SP_TX_LT_SET_REG, &c);
					if(!(c&MAX_PRE_REACH)){
						sp_write_reg(TX_P0, SP_TX_LT_SET_REG, (c+0x08));//increase one pre-level

					//if error still exist, return to the link traing value
					if (sp_tx_link_err_check())
						sp_write_reg(TX_P0, SP_TX_LT_SET_REG,c);
					}
				}

				sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c);
				if (c != sp_tx_bw) {
					sp_tx_hw_lt_done = 0;
					sp_tx_hw_lt_enable = 0;
					return 1;
				}
			}
			sp_tx_set_sys_state(STATE_CONFIG_OUTPUT);
			return 0;
		}
	}

	return 1;
}

uint sp_tx_link_err_check(void)
{
	uint errl = 0, errh = 0;

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x10, 2, bytebuf);
	msleep(5);
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x10, 2, bytebuf);
	errh = bytebuf[1];

	if (errh & 0x80) {
		errl = bytebuf[0];
		errh = (errh & 0x7f) << 8;
		errl = errh + errl;
	}

	DEV_ERR(" Err of Lane = %d\n", errl);
	return errl;
}




static void hdmi_rx_get_video_info(void)
{
    unchar ch, cl;
    uint n;

	sp_read_reg(RX_P0, HDMI_RX_HTOTAL_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_HTOTAL_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	g_cur_h_res = n;
	sp_read_reg(RX_P0, HDMI_RX_VTOTAL_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_VTOTAL_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	g_cur_v_res = n;

	sp_read_reg(RX_P0, HDMI_RX_VID_PCLK_CNTR_REG, &cl);
	g_cur_pix_clk = cl;
	sp_read_reg(RX_P0, HDMI_RX_HDMI_STATUS_REG, &cl);
	g_hdmi_dvi_status = ((cl & HDMI_MODE) == HDMI_MODE);
}

static void hdmi_rx_unmute_video(void)
{
	unchar c;

	sp_read_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c &= ~VID_MUTE;
	sp_write_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_video_muted = 0;
	DEV_DBG("Unmute Video.\n");
}
void hdmi_rx_show_video_info(void)
{
	unchar c, c1;
	unchar cl, ch;
	ulong n;
	ulong h_res, v_res;

	sp_read_reg(RX_P0, HDMI_RX_HACT_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_HACT_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	h_res = n;

	sp_read_reg(RX_P0, HDMI_RX_VACT_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_VACT_HIGH, &ch);
	n = ch;
	n = (n << 8) + cl;
	v_res = n;

	DEV_DBG("");
	DEV_DBG("*****************HDMI_RX Info*******************");
	DEV_DBG("HDMI_RX Is Normally Play Back.\n");
	sp_read_reg(RX_P0, HDMI_RX_HDMI_STATUS_REG, &c);

	if (c & HDMI_MODE)
		DEV_DBG("HDMI_RX Mode = HDMI Mode.\n");
	else
		DEV_DBG("HDMI_RX Mode = DVI Mode.\n");

	sp_read_reg(RX_P0, HDMI_RX_VIDEO_STATUS_REG1, &c);
	if (c & VIDEO_TYPE)
		v_res += v_res;


	DEV_DBG("HDMI_RX Video Resolution = %ld * %ld ", h_res, v_res);
	sp_read_reg(RX_P0, HDMI_RX_VIDEO_STATUS_REG1, &c);

	if (c & VIDEO_TYPE)
		DEV_DBG("    Interlace Video.");
	else
		DEV_DBG("    Progressive Video.");

	sp_read_reg(RX_P0, HDMI_RX_SYS_CTRL1_REG, &c);

	if ((c & 0x30) == 0x00)
		DEV_DBG("Input Pixel Clock = Not Repeated.\n");
	else if ((c & 0x30) == 0x10)
		DEV_DBG("Input Pixel Clock = 2x Video Clock. Repeated.\n");
	else if ((c & 0x30) == 0x30)
		DEV_DBG("Input Pixel Clock = 4x Vvideo Clock. Repeated.\n");

	if ((c & 0xc0) == 0x00)
		DEV_DBG("Output Video Clock = Not Divided.\n");
	else if ((c & 0xc0) == 0x40)
		DEV_DBG("Output Video Clock = Divided By 2.\n");
	else if ((c & 0xc0) == 0xc0)
		DEV_DBG("Output Video Clock = Divided By 4.\n");

	if (c & 0x02)
		DEV_DBG("Output Video Using Rising Edge To Latch Data.\n");
	else
		DEV_DBG("Output Video Using Falling Edge To Latch Data.\n");

	DEV_DBG("Input Video Color Depth = ");
	sp_read_reg(RX_P0, 0x70, &c1);
	c1 &= 0xf0;

	if (c1 == 0x00)
		DEV_DBG("Legacy Mode.\n");
	else if (c1 == 0x40)
		DEV_DBG("24 Bit Mode.\n");
	else if (c1 == 0x50)
		DEV_DBG("30 Bit Mode.\n");
	else if (c1 == 0x60)
		DEV_DBG("36 Bit Mode.\n");
	else if (c1 == 0x70)
		DEV_DBG("48 Bit Mode.\n");

	DEV_DBG("Input Video Color Space = ");
	sp_read_reg(RX_P1, HDMI_RX_AVI_DATA00_REG, &c);
	c &= 0x60;

	if (c == 0x20)
		DEV_DBG("YCbCr4:2:2 .\n");
	else if (c == 0x40)
		DEV_DBG("YCbCr4:4:4 .\n");
	else if (c == 0x00)
		DEV_DBG("RGB.\n");
	else
		DEV_DBG("Unknow 0x44 = 0x%.2x\n", (int)c);

	sp_read_reg(RX_P1, HDMI_RX_HDCP_STATUS_REG, &c);

	if (c & AUTH_EN)
		DEV_DBG("Authentication is attempted.");
	else
		DEV_DBG("Authentication is not attempted.");

	for (cl = 0; cl < 20; cl++) {
		sp_read_reg(RX_P1, HDMI_RX_HDCP_STATUS_REG, &c);

		if (c & DECRYPT_EN)
			break;
		else
			msleep(10);
	}

	if (cl < 20)
		DEV_DBG("Decryption is active.");
	else
		DEV_DBG("Decryption is not active.");


	DEV_DBG("********************************************************");
	DEV_DBG("");
}
static void hdmi_rx_set_sys_state(enum HDMI_RX_System_State ss)
{
	if (hdmi_system_state != ss) {
		DEV_NOTICE("");
		hdmi_system_state = ss;

		switch (ss) {
		case HDMI_CLOCK_DET:
			DEV_NOTICE("HDMI_RX:  HDMI_CLOCK_DET");
			break;
		case HDMI_SYNC_DET:
			DEV_NOTICE("HDMI_RX:  HDMI_SYNC_DET");
			break;
		case HDMI_VIDEO_CONFIG:
			DEV_NOTICE("HDMI_RX:  HDMI_VIDEO_CONFIG");
			break;
		case HDMI_AUDIO_CONFIG:
			DEV_NOTICE("HDMI_RX:  HDMI_AUDIO_CONFIG");
			break;
		case HDMI_PLAYBACK:
			DEV_NOTICE("HDMI_RX:  HDMI_PLAYBACK");
			break;
		default:
			break;
		}
	}
}
static void hdmi_rx_unmute_audio(void)
{
	unchar c;

	DEV_DBG("Unmute Audio.");
	sp_read_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c &= ~AUD_MUTE;
	sp_write_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_audio_muted = 0;
}
unchar sp_tx_lt_pre_config(void)
{
	unchar c;
	unchar link_bw = 0;

	if (!sp_tx_link_config_done) {
		sp_tx_get_rx_bw(1, &c);
		switch (c) {
		case 0x06:
			sp_tx_bw = BW_162G;
			break;
		case 0x0a:
			sp_tx_bw = BW_27G;
			break;
		case 0x14:
			sp_tx_bw = BW_54G;
			break;
		default:
			sp_tx_bw = BW_54G;
			break;
		}


		if ((sp_tx_bw != BW_27G) && (sp_tx_bw != BW_162G)
			&& (sp_tx_bw != BW_54G))
			return 1;

		sp_tx_power_on(SP_TX_PWR_VIDEO);
		sp_tx_video_mute(1);

		sp_tx_enable_video_input(1);
		sp_read_reg(TX_P0, SP_TX_SYS_CTRL2_REG, &c);
		sp_write_reg(TX_P0, SP_TX_SYS_CTRL2_REG, c);
		sp_read_reg(TX_P0, SP_TX_SYS_CTRL2_REG, &c);

		if (c & CHA_STA) {
			DEV_ERR("Stream clock not stable!\n");
			return 1;
		}

		sp_read_reg(TX_P0, SP_TX_SYS_CTRL3_REG, &c);
		sp_write_reg(TX_P0, SP_TX_SYS_CTRL3_REG, c);
		sp_read_reg(TX_P0, SP_TX_SYS_CTRL3_REG, &c);

		if (!(c & STRM_VALID)) {
			DEV_ERR("video stream not valid!\n");
			return 1;
		}



		hdmi_rx_get_video_info();
		hdmi_rx_unmute_video();
		sp_tx_lvttl_bit_mapping();
		sp_tx_power_down(SP_TX_PWR_AUDIO);

		if (g_hdmi_dvi_status) {
			DEV_ERR("HDMI mode: Video is stable.\n");
			if (sp_tx_rx_type == RX_HDMI)
				sp_tx_send_message(MSG_INPUT_HDMI);
			hdmi_rx_set_sys_state(HDMI_AUDIO_CONFIG);
		} else {
			DEV_ERR("DVI mode: Video is stable.\n");
			if (sp_tx_rx_type == RX_HDMI)
				sp_tx_send_message(MSG_INPUT_DVI);
			hdmi_rx_unmute_audio();
			hdmi_rx_set_sys_state(HDMI_PLAYBACK);
		}


		sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, 0x14);
		sp_tx_get_link_bw(&link_bw);
		sp_tx_pclk_calc(&link_bw);

		if (sp_tx_test_lt) {
			sp_tx_test_lt = 0;
			switch (sp_tx_test_bw) {
			case 0x06:
				sp_tx_bw = BW_162G;
				break;
			case 0x0a:
				sp_tx_bw = BW_27G;
				break;
			case 0x14:
				sp_tx_bw = BW_54G;
				break;
			default:
				sp_tx_bw = BW_NULL;
				break;
			}

			/*Link CTS 4.3.3.1, need to send the video timing
			640x480p@60Hz, 18-bit*/
			sp_read_reg(TX_P2, SP_TX_VID_CTRL2_REG, &c);
			c = (c & 0x8f);
			sp_write_reg(TX_P2, SP_TX_VID_CTRL2_REG, c);
		} else {
			/* Optimize the LT to get minimum power consumption */
			if (sp_tx_bw_lc_sel()) {
				DEV_ERR("****Over bandwidth****\n");
				return 1;
			}
		}

		/*Diable video before link training to enable idle pattern*/
		sp_tx_enable_video_input(0);
#ifdef SSC_EN
		sp_tx_config_ssc(sp_tx_bw);
#else
		sp_tx_spread_enable(0);
#endif



		sp_tx_link_config_done = 1;
	}

	return 0;
}

void sp_tx_video_mute(unchar enable)
{
	unchar c;

	if (enable) {
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c |= VIDEO_MUTE;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);
	} else {
		sp_read_reg(TX_P2, SP_TX_VID_CTRL1_REG, &c);
		c &= ~VIDEO_MUTE;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL1_REG, c);
	}

}

void sp_tx_aux_polling_enable(bool benable)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_DEBUG_REG1, &c);
	if (benable)
		c |= POLLING_EN;
	else
		c &= ~POLLING_EN;

	sp_write_reg(TX_P0, SP_TX_DEBUG_REG1, c);
}


void sp_tx_send_message(enum SP_TX_SEND_MSG message)
{
	unchar c;

	switch (message) {
	case MSG_INPUT_HDMI:
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x26, 0x01);
		break;

	case MSG_INPUT_DVI:
		sp_tx_aux_dpcdwrite_byte(0x00, 0x05, 0x26, 0x00);
		break;

	case MSG_CLEAR_IRQ:
		sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x10, 1, &c);
		c |= 0x01;
		sp_tx_aux_dpcdwrite_byte(0x00, 0x04, 0x10, c);
		break;
	}

}


void sp_tx_disable_slimport_hdcp(void)
{
	sp_tx_hdcp_encryption_disable();
	msleep(100);
	sp_tx_clean_hdcp();
	sp_tx_video_mute(0);
	DEV_DBG("*****slimport HDCP is disabled********\n");
}

static void eeprom_reload(void)
{
	unchar sByteBuf[5];

	sByteBuf[0] = 0x61;
	sByteBuf[1] = 0x6e;
	sByteBuf[2] = 0x61;
	sByteBuf[3] = 0x6f;
	sByteBuf[4] = 0x6e;

	if (sp_tx_aux_dpcdwrite_bytes
		(0x00, 0x04, 0xf5, 5, sByteBuf) == AUX_ERR) {
		DEV_ERR("aux load initial error\n");
		return;
	}

	sByteBuf[0] = 0x04;
	sByteBuf[1] = 0x00;
	sByteBuf[2] = 0x00;
	sByteBuf[3] = 0x06;
	sByteBuf[3] = 0x40;


	if (sp_tx_aux_dpcdwrite_bytes
		(0x00, 0x04, 0xf0, 5, sByteBuf) == AUX_ERR) {
		DEV_ERR("reload error!\n");
	}

}


unchar sp_tx_get_cable_type(bool bdelay)
{
	unchar SINK_OUI[8] = { 0 };
	unchar ds_port_preset = 0;
	unchar ds_port_recoginze = 0;
#ifdef CHARGING_STATUS_REPORT_EN
	unchar c;
#endif
	int i;

	for (i = 0; i < 5; i++) {
		if (AUX_ERR == sp_tx_aux_dpcdread_bytes
			(0x00, 0x00, DPCD_DSPORT_PRESENT, 1, &ds_port_preset)) {
			/*time delay for VGA dongle mcu startup*/
			if (bdelay)
				msleep(500);
			DEV_DBG(" AUX access error");
			continue;
		}

		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x00, 0x0c, bytebuf);

		switch ((ds_port_preset & 0x06)>>1) {
		case 0x00:
			sp_tx_rx_type = RX_DP;

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
			bcharging__status_capable = 0;
#endif

			ds_port_recoginze = 1;
			DEV_NOTICE("DS is DP dongle.");
			break;
		case 0x01:
		case 0x03://others
			sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x00, 8,
				SINK_OUI);
			if (((SINK_OUI[0] == 0x00) &&
					(SINK_OUI[1] == 0x22) &&
					(SINK_OUI[2] == 0xb9) &&
					(SINK_OUI[3] == 0x61) &&
					(SINK_OUI[4] == 0x39) &&
					(SINK_OUI[5] == 0x38) &&
					(SINK_OUI[6] == 0x33))) {
				DEV_NOTICE("DS is VGA dongle.");
#ifdef CONFIG_SLIMPORT_FAST_CHARGE
				bcharging__status_capable = 0;
#endif

				sp_tx_rx_type = RX_VGA_9832;
			} else {
				if ((SINK_OUI[0] == 0x00) &&
					(SINK_OUI[1] == 0x22) &&
					(SINK_OUI[2] == 0xb9) &&
					(SINK_OUI[3] == 0x73) &&
					(SINK_OUI[4] == 0x69) &&
					(SINK_OUI[5] == 0x76) &&
					(SINK_OUI[6] == 0x61)) {//7732
					ds_port_recoginze = 1;
					sp_tx_rx_type = RX_VGA_GEN;

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
				bcharging__status_capable = 1;
#endif

					if (bdelay) {
						eeprom_reload();
						msleep(150);
					}
				} else {
#ifdef CONFIG_SLIMPORT_FAST_CHARGE
				bcharging__status_capable = 0;
#endif

					ds_port_recoginze = 1;
					sp_tx_rx_type = RX_VGA_GEN;
					DEV_NOTICE("DS is DP2VGA converter.\n");
				}
			}
			break;
		case 0x02:
			if (AUX_OK == sp_tx_aux_dpcdread_bytes
				(0x00, 0x04, 0x00, 8, SINK_OUI)) {
				if ((SINK_OUI[0] == 0xb9)
					&& (SINK_OUI[1] == 0x22)
					&& (SINK_OUI[2] == 0x00)
					&& (SINK_OUI[3] == 0x00)
					&& (SINK_OUI[4] == 0x00)
					&& (SINK_OUI[5] == 0x00)
					&& (SINK_OUI[6] == 0x00)) {
#ifdef CHARGING_STATUS_REPORT_EN
					sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x23, 1, &c);
					if((c&0x7f)>=0x15){
						bcharging__status_capable = 1;
						//pr_info("fast charge capable!!");
					}
					else
						bcharging__status_capable = 0;
#endif

					DEV_NOTICE("DS is HDMI dongle.");
					sp_tx_rx_type = RX_HDMI;
				} else {
					sp_tx_rx_type = RX_DP;
					DEV_NOTICE("DS is DP2HDMI converter.");
				}
				ds_port_recoginze = 1;
			} else
				DEV_NOTICE("dpcd read error!.\n");

			break;
		default:
#ifdef CHARGING_STATUS_REPORT_EN
			bcharging__status_capable = 0;
#endif

			DEV_ERR("DS can not recognized.");
			sp_tx_rx_type = RX_NULL;
			ds_port_recoginze = 0;
			break;
		}

		if (ds_port_recoginze)
			return 1;
	}

	return 0;
}

uint32_t sp_tx_get_chg_current(void)
{
	unchar val = 0;
	uint32_t chg_current = NORMAL_CHG_I_MA;
	int ret = 0;
	int i;

	/*
	 * Sometimes a normal charger is detected as a fast charger
	 * erroneously. To minimize detection error, read charger type
	 * 3 times and set it as fast charger only if all of reading
	 * show fast charger type.
	 */
	for (i = 0; i < 3; i++) {
		ret = sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x22, 1, &val);

		if (ret != AUX_OK) {
			DEV_ERR("failed to get charger type!\n");
			chg_current = NORMAL_CHG_I_MA;
			break;
		}

		if ((val & 0x01) == 0x00) {
			chg_current = FAST_CHG_I_MA;
		} else {
			chg_current = NORMAL_CHG_I_MA;
			break;
		}

		if (i < 2)
			msleep(10);
	}

	DEV_NOTICE("charging current = %d mA\n", chg_current);
	return chg_current;
}

bool sp_tx_get_hdmi_connection(void)
{
	unchar c;

	if (AUX_OK != sp_tx_aux_dpcdread_bytes
		(0x00, 0x05, 0x18, 1, &c)) {
		return FALSE;
	}
	if ((c & 0x41) == 0x41)
		return TRUE;
	else
		return FALSE;
}

bool sp_tx_get_vga_connection(void)
{
	unchar c;
	if (AUX_OK != sp_tx_aux_dpcdread_bytes
		(0x00, 0x02, DPCD_SINK_COUNT, 1, &c)) {
		DEV_ERR("aux error.\n");
		return FALSE;
	}

	if (c & 0x01)
		return TRUE;
	else
		return FALSE;
}

static bool sp_tx_get_ds_video_status(void)
{
	unchar c;
	sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x27, 1, &c);
	if (c & 0x01)
		return TRUE;
	else
		return FALSE;
}

bool sp_tx_get_dp_connection(void)
{
	unchar c;

	if (AUX_OK != sp_tx_aux_dpcdread_bytes
		(0x00, 0x02, DPCD_SINK_COUNT, 1, &c)) {
		DEV_ERR("aux error.\n");
		return FALSE;
	}
	if (c & 0x1f) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x04, 1, &c);
		if (c & 0x20)
			sp_tx_aux_dpcdwrite_byte(0x00, 0x06, 0x00, 0x20);
		return TRUE;
	} else {
		return FALSE;
	}
}

unchar sp_tx_get_downstream_type(void)
{
	unchar SINK_OUI[8] = { 0 };
	unchar ds_port_preset = 0;
	unchar ds_port_recoginze = 0;
	int i;

	for (i = 0; i < 5; i++) {
		if (AUX_ERR == sp_tx_aux_dpcdread_bytes
			(0x00, 0x00, DPCD_DSPORT_PRESENT, 1, &ds_port_preset)) {
			/*time delay for VGA dongle mcu startup*/
			DEV_ERR(" AUX access error\n");
			continue;
		}
		ds_port_preset = ds_port_preset >> 1;
		switch (ds_port_preset & 0x03) {
		case 0x00:
			sp_tx_rx_type = RX_DP;
			ds_port_recoginze = 1;

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
			bcharging__status_capable = 0;
#endif

			/*DEV_DBG("Downstream is DP dongle.\n");*/
			break;
		case 0x01:
		case 0x03://others
			sp_tx_aux_dpcdread_bytes(0x00, 0x04, 0x00, 8, SINK_OUI);
			if (((SINK_OUI[0] == 0x00) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0xb9) && (SINK_OUI[3] == 0x61)
			    && (SINK_OUI[4] == 0x39) && (SINK_OUI[5] == 0x38)
			    && (SINK_OUI[6] == 0x33))) {
				sp_tx_rx_type = RX_VGA_9832;
				DEV_DBG("Downstream is 9832VGA dongle.\n");
			}else if((SINK_OUI[0] == 0x00) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0xb9) && (SINK_OUI[3] == 0x73)
			    && (SINK_OUI[4] == 0x69) && (SINK_OUI[5] == 0x76)
			    && (SINK_OUI[6] == 0x61)){//7732

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
				bcharging__status_capable = true;
				//pr_info("fast charge capable!!!!");
#endif

				sp_tx_rx_type = RX_VGA_GEN;

			}else {
#ifdef CONFIG_SLIMPORT_FAST_CHARGE
				bcharging__status_capable = 0;
#endif

				sp_tx_rx_type = RX_VGA_GEN;
				/*DEV_DBG("Downstream is general DP2VGA converter.\n");*/
			}
			ds_port_recoginze = 1;
			break;
		case 0x02:
			if (AUX_OK == sp_tx_aux_dpcdread_bytes
				(0x00, 0x04, 0x00, 8, SINK_OUI)) {
				if ((SINK_OUI[0] == 0xb9)
					&& (SINK_OUI[1] == 0x22)
					&& (SINK_OUI[2] == 0x00)
					&& (SINK_OUI[3] == 0x00)
					&& (SINK_OUI[4] == 0x00)
					&& (SINK_OUI[5] == 0x00)
					&& (SINK_OUI[6] == 0x00)) {
					sp_tx_rx_type = RX_HDMI;
					/*DEV_DBG("Downstream is 7730 HDMI dongle.\n");*/

				} else {
					sp_tx_rx_type = RX_DP;
					/*DEV_DBG("sink is general DP2HDMI converter.\n");*/
				}
				ds_port_recoginze = 1;
			} else
				DEV_DBG("dpcd read error!.\n");

			break;
		default:
			sp_tx_rx_type = RX_NULL;
			ds_port_recoginze = 0;
			DEV_DBG("Downstream can not recognized.\n");
			break;
		}

		if (ds_port_recoginze)
			return 1;
	}

	return 0;
}

unchar sp_tx_get_downstream_connection(enum RX_CBL_TYPE cabletype)
{
	unchar ret;

	switch (cabletype) {
	case RX_HDMI:
		if (sp_tx_get_hdmi_connection())
			ret = 1;
		else
			ret = 0;
		break;
	case RX_DP:
		if (sp_tx_get_dp_connection())
			ret = 1;
		else
			ret = 0;

		break;
	case RX_VGA_GEN:
		if (sp_tx_get_vga_connection())
			ret = 1;
		else
			ret = 0;
		break;
	case RX_VGA_9832:
		if (sp_tx_get_vga_connection())
			ret = 1;
		else
			ret = 0;
		break;
	case RX_NULL:
	default:
		ret = 0;

		break;
	}
	return ret;

}

void sp_tx_edid_read(void)
{
	unchar i, j, edid_block = 0, segment = 0, offset = 0;
	unchar c;

	memset(bedid_extblock, 0, sizeof(bedid_extblock));
	memset(bedid_firstblock, 0, sizeof(bedid_firstblock));

	/*Add bandwidth check to support low
	resolution for VGA and myDP monitor*/
	sp_tx_get_rx_bw(1, &c);
	slimport_link_bw = c;

	sp_tx_edid_read_initial();

	bedid_break = 0;

	sp_tx_addronly_set(1);
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG, 0x04);
	sp_write_reg(TX_P0, SP_TX_AUX_CTRL_REG2, 0x03);
	sp_tx_wait_aux_finished();

	edid_block = sp_tx_get_edid_block();

	if (edid_block < 2) {
		edid_block = 8 * (edid_block + 1);
		for (i = 0; i < edid_block; i++) {
			if (!bedid_break)
				sp_tx_aux_edidread_byte(i * 16);
		}

		sp_tx_addronly_set(0);
	} else {
		for (i = 0; i < 16; i++) {
			if (!bedid_break)
				sp_tx_aux_edidread_byte(i * 16);
		}

		sp_tx_addronly_set(0);

		if (!bedid_break) {
			edid_block = (edid_block + 1);

			for (i = 0; i < ((edid_block - 1) / 2); i++) {
				DEV_DBG("EXT 256 EDID block");
				segment = i + 1;

				for (j = 0; j < 16; j++) {
					sp_tx_parse_segments_edid(segment,
							offset);
					offset = offset + 0x10;
				}
			}

			if (edid_block % 2) {
				DEV_DBG("Last block");
				segment = segment + 1;

				for (j = 0; j < 8; j++) {
					sp_tx_parse_segments_edid(segment,
							offset);
					offset = offset + 0x10;
				}
			}
		}
	}

	sp_tx_addronly_set(0);
	sp_tx_rst_aux();

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x18, 1, bytebuf);
	if (bytebuf[0] & 0x04) {

		DEV_DBG("check sum = %.2x\n", (uint)bedid_checksum);
		bytebuf[0] = bedid_checksum;
		sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x61, 1, bytebuf);

		bytebuf[0] = 0x04;
		sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1, bytebuf);
		DEV_DBG("Test EDID done\n");

	}

	/*Release the HPD after the EEPROM loaddown*/
	for (i = 0; i < 10; i++) {
		sp_read_reg(TX_P0, SP_TX_HDCP_KEY_STATUS, &c);
		if ((c&0x07) == 0x05) {
			sp_write_reg(RX_P0, HDMI_RX_INT_MASK1_REG, 0xe2);
			return;
		} else
			msleep(2);
	}
}

static void sp_tx_pll_changed_int_handler(void)
{
	if (sp_tx_system_state > STATE_PARSE_EDID) {
		if (!sp_tx_get_pll_lock_status()) {
			DEV_ERR("PLL:_______________PLL not lock!");
			sp_tx_clean_hdcp();
			sp_tx_set_sys_state(STATE_LINK_TRAINING);
			sp_tx_link_config_done = 0;
			sp_tx_hw_lt_done = 0;
			sp_tx_hw_lt_enable = 0;
		}
	}
}

static void sp_tx_auth_done_int_handler(void)
{
	unchar c;

	sp_read_reg(TX_P0, SP_TX_HDCP_STATUS, &c);

	if (c & SP_TX_HDCP_AUTH_PASS) {
		sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x2A, 2, bytebuf);
		c = bytebuf[1];
		if ((c & 0x08) || (bytebuf[0]&0x80)) {
			DEV_ERR("max cascade/devs exceeded!\n");
			sp_tx_hdcp_encryption_disable();
			sp_tx_hdcp_auth_pass = 1;
			sp_tx_hdcp_auth_fail_counter = 0;
			sp_tx_hdcp_auth_fail_counter1 = 0;

		} else {
			DEV_NOTICE("Authentication pass in Auth_Done");
			sp_tx_hdcp_auth_pass = 1;
			sp_tx_hdcp_auth_fail_counter = 0;
			sp_tx_hdcp_auth_fail_counter1 = 0;
		}

	} else {
		DEV_ERR("Authentication failed in AUTH_done");
		sp_tx_hdcp_auth_pass = 0;
		sp_tx_hdcp_auth_fail_counter++;
		sp_tx_hdcp_auth_fail_counter1++;

		if (sp_tx_rx_type == RX_HDMI) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);
			if ((c & 0x01) != 0x01) {
				DEV_NOTICE("Downstream HDMI is unpluged!\n");
				if ((sp_tx_system_state >
					 STATE_CABLE_PLUG)
					&& (!sp_tx_pd_mode)) {
					sp_tx_clean_hdcp();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_hw_lt_enable = 0;
					return;
				}
			}
		}

		if (sp_tx_hdcp_auth_fail_counter1 >= 4) {
			if ((sp_tx_system_state >
				 STATE_CABLE_PLUG)
				&& (!sp_tx_pd_mode)) {
				sp_tx_hdcp_auth_fail_counter1 = 0;
				sp_tx_clean_hdcp();
				sp_tx_power_down(SP_TX_PWR_REG);
				sp_tx_power_down(SP_TX_PWR_TOTAL);
				sp_tx_hardware_powerdown();
				sp_tx_set_sys_state(STATE_CABLE_PLUG);
				sp_tx_pd_mode = 1;
				sp_tx_link_config_done = 0;
				sp_tx_hw_lt_done = 0;
				sp_tx_hw_lt_enable = 0;
				return;
			}
		} else {
			sp_tx_video_mute(1);
			sp_tx_clean_hdcp();
			DEV_ERR("Re-authentication!\n");

			if (sp_tx_system_state > STATE_CONFIG_OUTPUT) {
				sp_tx_set_sys_state
					(STATE_HDCP_AUTH);
				return;
			}
		}
	}

	sp_tx_hdcp_auth_done = 1;
}

static void sp_tx_link_chk_fail_int_handler(void)
{
	if (sp_tx_system_state >= STATE_HDCP_AUTH) {
		sp_tx_set_sys_state(STATE_HDCP_AUTH);
		sp_tx_clean_hdcp();
		DEV_ERR("IRQ:____________HDCP Sync lost!");
	}
}

static void sp_tx_lt_done_int_handler(void)
{
	unchar c, c1;

	if ((sp_tx_hw_lt_done) || (sp_tx_system_state != STATE_LINK_TRAINING))
		return;

	sp_read_reg(TX_P0, SP_TX_LT_CTRL_REG, &c);
	if (c & 0x70) {
		c = (c & 0x70) >> 4;
		DEV_ERR("HW LT failed in interrupt,");
		DEV_ERR("ERR code = %.2x\n", (uint) c);

		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_set_sys_state(STATE_LINK_TRAINING);
		msleep(50);
	} else {
		sp_tx_hw_lt_done = 1;
		sp_read_reg(TX_P0, SP_TX_LT_SET_REG, &c);
		sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c1);
		DEV_NOTICE("HW LT succeed,LANE0_SET = %.2x,", (uint) c);
		DEV_NOTICE("link_bw = %.2x\n", (uint) c1);
	}
}

static void sp_tx_link_change_int_handler(void)
{
	unchar lane0_1_status, sl_cr, al;

	if (sp_tx_system_state < STATE_CONFIG_OUTPUT)
		return;

	sp_tx_link_err_check();

	sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_LANE_ALIGN_UD, 1,
				 bytebuf);
	al = bytebuf[0];
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, DPCD_LANE0_1_STATUS, 1, bytebuf);
	lane0_1_status = bytebuf[0];


	if (((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
		sl_cr = 0;
	else
		sl_cr = 1;
	if (((al & 0x01) == 0) || (sl_cr == 0)) {
		if ((al & 0x01) == 0)
			DEV_ERR("Lane align not done\n");

		if (sl_cr == 0)
			DEV_ERR("Lane clock recovery not done\n");
		sp_tx_get_cable_type(0);
		if (sp_tx_rx_type_backup != sp_tx_rx_type) {
			sp_tx_vbus_powerdown();
			sp_tx_power_down(SP_TX_PWR_REG);
			sp_tx_power_down(SP_TX_PWR_TOTAL);
			sp_tx_hardware_powerdown();
			sp_tx_pd_mode = 1;
			sp_tx_link_config_done = 0;
			sp_tx_hw_lt_enable = 0;
			sp_tx_hw_lt_done = 0;
			sp_tx_rx_type = RX_NULL;
			sp_tx_rx_type_backup = RX_NULL;
			sp_tx_set_sys_state(STATE_CABLE_PLUG);
		} else {
			if (sp_tx_get_downstream_connection
				(sp_tx_rx_type)) {
				if ((sp_tx_system_state
					> STATE_LINK_TRAINING)
				&& sp_tx_link_config_done) {
					sp_tx_link_config_done = 0;
					sp_tx_set_sys_state
						(STATE_LINK_TRAINING);
					DEV_ERR("IRQ:_______re-LT request!");
				}
			} else
				sp_tx_set_sys_state(STATE_CABLE_PLUG);
		}
	}

}

static void sp_tx_polling_err_int_handler(void)
{
	/*unchar c;
	int i;*/


	if ((sp_tx_system_state < STATE_CABLE_PLUG) || sp_tx_pd_mode)
		return;
/*
	for (i = 0; i < 5; i++) {
		if (AUX_OK == sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x00, 1, &c))
			return;

		msleep(2);
	}
*/
	if (sp_tx_pd_mode == 0) {
		DEV_ERR("Cwire polling is corrupted,power down ANX7808.\n");
		sp_tx_clean_hdcp();
		sp_tx_vbus_powerdown();
		sp_tx_power_down(SP_TX_PWR_TOTAL);
		sp_tx_power_down(SP_TX_PWR_REG);
		sp_tx_hardware_powerdown();
		sp_tx_set_sys_state(STATE_CABLE_PLUG);
		sp_tx_pd_mode = 1;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_rx_type = RX_NULL;
		sp_tx_rx_type_backup = RX_NULL;
	}
}

static void sp_tx_irq_isr(void)
{
	unchar c, c1, lane0_1_status, sl_cr, al;
	unchar IRQ_Vector;
	unchar Int_vector[2];
	unchar test_vector;

	if (AUX_OK == sp_tx_aux_dpcdread_bytes
		(0x00, 0x02, DPCD_SERVICE_IRQ_VECTOR, 1, bytebuf)) {
		IRQ_Vector = bytebuf[0];
		sp_tx_aux_dpcdwrite_bytes
			(0x00, 0x02, DPCD_SERVICE_IRQ_VECTOR, 1, bytebuf);
	} else {
		return;
	}

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
	/*charging irq*/
	if(bcharging__status_capable){//dongle has fast charging detection capability
		if (IRQ_Vector & SINK_SPECIFIC_IRQ) {
			if (AUX_OK == sp_tx_aux_dpcdread_bytes
				(0x00, 0x05, 0x22, 1, &c1)) {
				if ((c1&0x01) == 0x00) {
					downstream_charging_status
						= FAST_CHARGING;
					DEV_DBG("fast charging!\n");
				} else {
					downstream_charging_status
						= NORMAL_CHARGING;
					DEV_DBG("normal charging!\n");
				}
			}
		}
	}
#endif
	/* HDCP IRQ */
	if (IRQ_Vector & CP_IRQ) {
		if (sp_tx_hdcp_auth_pass) {
			sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x29, 1, &c1);
			if (c1 & 0x04) {
				if (sp_tx_system_state >=
						STATE_HDCP_AUTH) {
					sp_tx_clean_hdcp();
					sp_tx_set_sys_state
						(STATE_HDCP_AUTH);
					DEV_ERR("IRQ:______HDCP Sync lost!\n");
				}
			}
		}
	}

	/* specific int */
	if ((IRQ_Vector & SINK_SPECIFIC_IRQ) && (sp_tx_rx_type == RX_HDMI)) {

		sp_tx_aux_dpcdread_bytes(0x00, 0x05, DPCD_SPECIFIC_INTERRUPT1,
					 2, Int_vector);
		sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, DPCD_SPECIFIC_INTERRUPT1,
					 2, Int_vector);


		/*do not check unplug event during hdcp process.*/
		if (sp_tx_system_state != STATE_HDCP_AUTH) {
			if ((Int_vector[0] & 0x02) == 0x02) {
				sp_tx_aux_dpcdread_bytes
					(0x00, 0x05, 0x18, 1, &c);
				if ((c & 0x01) != 0x01) {
					DEV_NOTICE("DS HDMI is unpluged!\n");

					if ((sp_tx_system_state
						> STATE_CABLE_PLUG)
						&& (!sp_tx_pd_mode)) {
						sp_tx_clean_hdcp();
						sp_tx_power_down(SP_TX_PWR_REG);
						sp_tx_power_down
							(SP_TX_PWR_TOTAL);
						sp_tx_hardware_powerdown();
						sp_tx_set_sys_state
							(STATE_CABLE_PLUG);
						sp_tx_pd_mode = 1;
						sp_tx_link_config_done = 0;
						sp_tx_hw_lt_done = 0;
						sp_tx_hw_lt_enable = 0;
					}
				}
			}
		}

		if (((Int_vector[0] & 0x04) == 0x04)
			&& (sp_tx_system_state > STATE_CONFIG_OUTPUT)) {

			DEV_ERR("Rx specific  IRQ: Link is down!\n");

			sp_tx_aux_dpcdread_bytes(0x00, 0x02,
					DPCD_LANE_ALIGN_UD,
					1, bytebuf);
			al = bytebuf[0];

			sp_tx_aux_dpcdread_bytes(0x00, 0x02,
					DPCD_LANE0_1_STATUS, 1,
					bytebuf);
			lane0_1_status = bytebuf[0];

			if (((lane0_1_status & 0x01) == 0)
					|| ((lane0_1_status & 0x04) == 0))
				sl_cr = 0;
			else
				sl_cr = 1;

			if (((al & 0x01) == 0) || (sl_cr == 0)) {
				if ((al & 0x01) == 0)
					DEV_ERR("Lane align not done\n");

				if (sl_cr == 0)
					DEV_ERR("Lane CR not done\n");

				sp_tx_get_cable_type(0);
				if (sp_tx_rx_type_backup != sp_tx_rx_type) {
					sp_tx_vbus_powerdown();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_rx_type = RX_NULL;
					sp_tx_rx_type_backup = RX_NULL;
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
				} else {
					//if (sp_tx_get_downstream_connection(sp_tx_rx_type)) {
						if ((sp_tx_system_state
							> STATE_LINK_TRAINING)
						&& sp_tx_link_config_done) {
							sp_tx_link_config_done = 0;
							sp_tx_clean_hdcp();
							sp_tx_set_sys_state
								(STATE_LINK_TRAINING);
							DEV_ERR("INT:\re-LT request!\n");
						}
					//}else
					//	sp_tx_set_sys_state(STATE_CABLE_PLUG);

				}
			}

			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);
			if (!(c & 0x40)) {
				if ((sp_tx_system_state > STATE_CABLE_PLUG)
						&& (!sp_tx_pd_mode)) {
					sp_tx_clean_hdcp();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_hw_lt_enable = 0;
					return;
				}
			}

		}

		if ((Int_vector[0] & 0x08) == 0x08) {
			DEV_DBG("Downstream HDCP is done!\n");

			if ((Int_vector[0] & 0x10) != 0x10)
				DEV_DBG("Downstream HDCP is passed!\n");
			else {
				if (sp_tx_system_state > STATE_CONFIG_OUTPUT) {
					sp_tx_video_mute(1);
					sp_tx_clean_hdcp();
					sp_tx_set_sys_state(STATE_HDCP_AUTH);
					DEV_ERR("Re-auth,ds HDCP failure!");
				}
			}
		}

		if ((Int_vector[0] & 0x20) == 0x20) {
			DEV_ERR(" Downstream HDCP link integrity check fail!");

			if (sp_tx_system_state > STATE_HDCP_AUTH) {
				sp_tx_set_sys_state(STATE_HDCP_AUTH);
				sp_tx_clean_hdcp();
				msleep(2000);
				DEV_ERR("IRQ:____________HDCP Sync lost!");
			}
		}

		if ((Int_vector[0] & 0x40) == 0x40) {
#ifdef CONFIG_SLIMPORT_CEC
			DEV_DBG("Receive CEC command from downstream done!\n");
			downstream_hdmi_cec_readmessage();
#endif
		}

		if ((Int_vector[0] & 0x80) == 0x80)
			DEV_DBG("CEC command transfer to downstream done!");

		if ((Int_vector[1] & 0x04) == 0x04) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x05, 0x18, 1, &c);

			if ((c & 0x40) == 0x40)
				DEV_NOTICE("DS HDMI termination detected!\n");
		}

		/* specific int */
	} else if ((IRQ_Vector & SINK_SPECIFIC_IRQ)
	&& (sp_tx_rx_type != RX_HDMI)) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x00, 1, &c);
		if (!(c & 0x01)) {
			if ((sp_tx_system_state > STATE_CABLE_PLUG)
				&& (!sp_tx_pd_mode)) {
				sp_tx_vbus_powerdown();
				sp_tx_power_down(SP_TX_PWR_TOTAL);
				sp_tx_power_down(SP_TX_PWR_REG);
				sp_tx_hardware_powerdown();
				sp_tx_clean_hdcp();
				sp_tx_pd_mode = 1;
				sp_tx_link_config_done = 0;
				sp_tx_hw_lt_enable = 0;
				sp_tx_hw_lt_done = 0;
				sp_tx_set_sys_state(STATE_CABLE_PLUG);
				return;
			}
		} else {
			if (sp_tx_rx_type == RX_VGA_9832)
				sp_tx_send_message(MSG_CLEAR_IRQ);
		}

		if (sp_tx_system_state > STATE_CONFIG_OUTPUT) {
			sp_tx_aux_dpcdread_bytes(0x00, 0x02,
					DPCD_LANE_ALIGN_UD, 1, bytebuf);
			al = bytebuf[0];
			sp_tx_aux_dpcdread_bytes(0x00, 0x02,
					DPCD_LANE0_1_STATUS, 1, bytebuf);
			lane0_1_status = bytebuf[0];

			if (((lane0_1_status & 0x01) == 0)
				|| ((lane0_1_status & 0x04) == 0))
				sl_cr = 0;
			else
				sl_cr = 1;
			if (((al & 0x01) == 0) || (sl_cr == 0)) {
				if ((al & 0x01) == 0)
					DEV_ERR("Lane align not done\n");

				if (sl_cr == 0)
					DEV_ERR("Lane CR not done\n");

				sp_tx_get_cable_type(0);
				if (sp_tx_rx_type_backup != sp_tx_rx_type) {
					sp_tx_vbus_powerdown();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_rx_type = RX_NULL;
					sp_tx_rx_type_backup = RX_NULL;
					sp_tx_set_sys_state
						(STATE_CABLE_PLUG);
				} else{
					if (sp_tx_get_downstream_connection
						(sp_tx_rx_type)) {
						if ((sp_tx_system_state
							> STATE_LINK_TRAINING)
						&& sp_tx_link_config_done) {
							sp_tx_link_config_done = 0;
							sp_tx_clean_hdcp();
							sp_tx_set_sys_state
								(STATE_LINK_TRAINING);
							DEV_ERR("IRQ:\re-LT request.\n");
						}
					} else{
							sp_tx_clean_hdcp();
							sp_tx_vbus_powerdown();
							sp_tx_power_down(SP_TX_PWR_REG);
							sp_tx_power_down(SP_TX_PWR_TOTAL);
							sp_tx_hardware_powerdown();
							sp_tx_pd_mode = 1;
							sp_tx_link_config_done = 0;
							sp_tx_hw_lt_enable = 0;
							sp_tx_hw_lt_done = 0;
							sp_tx_rx_type = RX_NULL;
							sp_tx_rx_type_backup = RX_NULL;
							sp_tx_set_sys_state(STATE_CABLE_PLUG);
						}
				}
			}
		}
	}

	/* AUTOMATED TEST IRQ */
	if (IRQ_Vector & TEST_IRQ) {
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x18, 1, bytebuf);
		test_vector = bytebuf[0];
		if (test_vector & 0x01) {
			sp_tx_test_lt = 1;

			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x19, 1, bytebuf);
			sp_tx_test_bw = bytebuf[0];
			DEV_DBG(" test_bw = %.2x\n", (uint)sp_tx_test_bw);

			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, bytebuf);
			bytebuf[0] = bytebuf[0] | TEST_ACK;
			sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1,
					bytebuf);

			DEV_DBG("Set TEST_ACK!\n");
			if (sp_tx_system_state > STATE_LINK_TRAINING)
				sp_tx_set_sys_state(STATE_LINK_TRAINING);
			DEV_DBG("IRQ:test-LT request!\n");
		}

		if (test_vector & 0x04) {
			if (sp_tx_system_state > STATE_PARSE_EDID)
				sp_tx_set_sys_state(STATE_PARSE_EDID);
			sp_tx_test_edid = 1;
			DEV_DBG("Test EDID Requested!\n");
		}
		/*phy test pattern*/
		if (test_vector & 0x08) {
			sp_tx_phy_auto_test();

			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, bytebuf);
			bytebuf[0] = bytebuf[0] | 0x01;
			sp_tx_aux_dpcdwrite_bytes(0x00, 0x02, 0x60, 1,
					bytebuf);
		}
	}
}

static void sp_tx_sink_irq_int_handler(void)
{
	sp_tx_irq_isr();
}

void sp_tx_hdcp_process(void)
{
	unchar c;

	if ((hdcp_en == 0) &&
		((sp_tx_rx_type == RX_VGA_9832)
		|| (sp_tx_rx_type == RX_VGA_GEN))) {
		sp_tx_power_down(SP_TX_PWR_HDCP);
		sp_tx_video_mute(0);
		sp_tx_set_sys_state(STATE_PLAY_BACK);
		return;
	}

	if (!sp_tx_hdcp_capable_chk) {
		sp_tx_hdcp_capable_chk = 1;
		sp_tx_aux_dpcdread_bytes(0x06, 0x80, 0x28, 1, &c);
		if (!(c & 0x01)) {
			DEV_ERR("Sink is not capable HDCP\n");
			sp_tx_power_down(SP_TX_PWR_HDCP);
			sp_tx_video_mute(1);
			sp_tx_set_sys_state(STATE_PLAY_BACK);
			return;
		}
	}

	/*In case ANX730 video can not get ready*/
	if ((sp_tx_rx_type == RX_HDMI)
		&& (!sp_tx_hw_hdcp_en)) {
		if (!sp_tx_get_ds_video_status()) {
			if (sp_tx_ds_vid_stb_cntr ==
				SP_TX_DS_VID_STB_TH) {
				sp_tx_vbus_powerdown();
				sp_tx_power_down(SP_TX_PWR_REG);
				sp_tx_power_down(SP_TX_PWR_TOTAL);
				sp_tx_hardware_powerdown();
				sp_tx_pd_mode = 1;
				sp_tx_link_config_done = 0;
				sp_tx_hw_lt_enable = 0;
				sp_tx_hw_lt_done = 0;
				sp_tx_rx_type = RX_NULL;
				sp_tx_rx_type_backup = RX_NULL;
				sp_tx_ds_vid_stb_cntr = 0;
				sp_tx_set_sys_state(STATE_CABLE_PLUG);
			} else {
				sp_tx_ds_vid_stb_cntr++;
				msleep(5);
			}
			DEV_DBG("ds video not stable\n");
			return;
		} else {
			sp_tx_ds_vid_stb_cntr = 0;
		}
	}

	if (!sp_tx_hw_hdcp_en) {
		sp_tx_power_on(SP_TX_PWR_HDCP);
		sp_write_reg(TX_P2, SP_COMMON_INT_MASK2, 0X01);
		msleep(50);
		sp_tx_hw_hdcp_enable();
		sp_tx_hw_hdcp_en = 1;
	}

	if (sp_tx_hdcp_auth_done) {
		sp_tx_hdcp_auth_done = 0;

		if (sp_tx_hdcp_auth_pass) {

			sp_tx_hdcp_encryption_enable();
			sp_tx_video_mute(0);

			DEV_NOTICE("@@@@hdcp_auth_pass@@@@\n");

			if (sp_tx_rx_type == RX_HDMI) {
				sp_tx_aux_dpcdread_bytes
					(0x00, 0x05, 0x18, 1, &c);
				if ((c & 0x01) != 0x01) {
					DEV_NOTICE("DS HDMI is unpluged!\n");
					if ((sp_tx_system_state >
						 STATE_CABLE_PLUG)
						&& (!sp_tx_pd_mode)) {
						sp_tx_clean_hdcp();
						sp_tx_power_down
							(SP_TX_PWR_REG);
						sp_tx_power_down
							(SP_TX_PWR_TOTAL);
						sp_tx_hardware_powerdown();
						sp_tx_set_sys_state
							(STATE_CABLE_PLUG);
						sp_tx_pd_mode = 1;
						sp_tx_link_config_done = 0;
						sp_tx_hw_lt_done = 0;
						sp_tx_hw_lt_enable = 0;
						return;
					}
				}
			}


		} else {
			sp_tx_hdcp_encryption_disable();
			sp_tx_video_mute(1);
			DEV_NOTICE("****hdcp_auth_failed****\n");
			if (sp_tx_rx_type == RX_HDMI) {
				sp_tx_aux_dpcdread_bytes(0x00,
					0x05, 0x18, 1, &c);
				if ((c & 0x01) != 0x01) {
					DEV_NOTICE("DS HDMI is unpluged!\n");

					if ((sp_tx_system_state >
						 STATE_CABLE_PLUG)
						&& (!sp_tx_pd_mode)) {
						sp_tx_clean_hdcp();
						sp_tx_power_down
							(SP_TX_PWR_REG);
						sp_tx_power_down
							(SP_TX_PWR_TOTAL);
						sp_tx_hardware_powerdown();
						sp_tx_set_sys_state
							(STATE_CABLE_PLUG);
						sp_tx_pd_mode = 1;
						sp_tx_link_config_done = 0;
						sp_tx_hw_lt_done = 0;
						sp_tx_hw_lt_enable = 0;
					}
				}
				return;
			}

		}

		sp_tx_set_sys_state(STATE_PLAY_BACK);
		sp_tx_show_infomation();
	}
}

void sp_tx_set_sys_state(enum SP_TX_System_State ss)
{
	DEV_NOTICE("SP_TX To System State: ");
	sp_tx_system_state_bak = STATE_INIT;
	switch (ss) {
	case STATE_INIT:
		sp_tx_system_state = STATE_INIT;
		DEV_NOTICE("STATE_INIT");
		break;
	case STATE_CABLE_PLUG:
		sp_tx_system_state = STATE_CABLE_PLUG;
		DEV_NOTICE("STATE_CABLE_PLUG");
		break;
	case STATE_PARSE_EDID:
		sp_tx_system_state = STATE_PARSE_EDID;
		DEV_NOTICE("SP_TX_READ_PARSE_EDID");
		break;
	case STATE_CONFIG_HDMI:
		sp_tx_system_state = STATE_CONFIG_HDMI;
		DEV_NOTICE("STATE_CONFIG_HDMI");
		break;
	case STATE_CONFIG_OUTPUT:
		sp_tx_system_state = STATE_CONFIG_OUTPUT;
		DEV_NOTICE("STATE_CONFIG_OUTPUT");
		break;
	case STATE_LINK_TRAINING:
		sp_tx_system_state = STATE_LINK_TRAINING;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		DEV_NOTICE("STATE_LINK_TRAINING");
		break;
	case STATE_HDCP_AUTH:
		sp_tx_system_state = STATE_HDCP_AUTH;
		DEV_NOTICE("STATE_HDCP_AUTH");
		break;
	case STATE_PLAY_BACK:
		sp_tx_system_state = STATE_PLAY_BACK;
		DEV_NOTICE("STATE_PLAY_BACK");
		break;
	default:
		break;
	}
}

void sp_tx_int_irq_handler(void)
{
	unchar c1, c2, c3, c4, c5;

	sp_tx_get_int_status(COMMON_INT_1, &c1);
	sp_tx_get_int_status(COMMON_INT_2, &c2);
	sp_tx_get_int_status(COMMON_INT_3, &c3);
	sp_tx_get_int_status(COMMON_INT_4, &c4);
	sp_tx_get_int_status(SP_INT_STATUS, &c5);

	if (c1 & PLL_LOCK_CHG)
		sp_tx_pll_changed_int_handler();

	if (c2 & HDCP_AUTH_DONE)
		sp_tx_auth_done_int_handler();

	if (c3 & HDCP_LINK_CHK_FAIL)
		sp_tx_link_chk_fail_int_handler();

	if (c5 & DPCD_IRQ_REQUEST)
		sp_tx_sink_irq_int_handler();

	if (c5 & POLLING_ERR)
		sp_tx_polling_err_int_handler();

	if (c5 & TRAINING_Finish)
		sp_tx_lt_done_int_handler();

	if (c5 & LINK_CHANGE)
		sp_tx_link_change_int_handler();
}

#ifdef EYE_TEST
void sp_tx_eye_diagram_test(void)
{
	unchar c;
	int i;

	sp_write_reg(TX_P2, 0x05, 0x00);

	sp_read_reg(TX_P2, SP_TX_RST_CTRL_REG, &c);
	c |= SW_RST;
	sp_write_reg(TX_P2, SP_TX_RST_CTRL_REG, c);
	c &= ~SW_RST;
	sp_write_reg(TX_P2, SP_TX_RST_CTRL_REG, c);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, 0x19);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, 0x18);

	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, 0x16);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, 0x1b);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, 0x22);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, 0x23);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, 0x09);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, 0x16);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, 0x1F);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, 0x26);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, 0x28);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, 0x2F);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, 0x10);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, 0x1F);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, 0x36);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, 0x3c);
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, 0x3F);
	/* set link bandwidth 5.4G */
	sp_write_reg(TX_P0, 0xa0, 0x14);
	/* set swing 600mv3.5db */
	sp_write_reg(TX_P0, 0xa3, 0x0a);
	/* send link error measurement patterns */
	sp_write_reg(TX_P0, 0xa2, 0x14);
	sp_write_reg(TX_P0, 0xA9, 0x00);
	sp_write_reg(TX_P0, 0xAA, 0x01);

	sp_write_reg(TX_P2, SP_TX_ANALOG_CTRL, 0xC5);
	sp_write_reg(TX_P0, I2C_GEN_10US_TIMER0, 0x0E);
	sp_write_reg(TX_P0, I2C_GEN_10US_TIMER1, 0x01);

	for (i = 0; i < 256; i++) {
		sp_read_reg(0x72, i, &c);

		if ((i & 0x0f) == 0)
			printk(KERN_INFO "\n [%.2x]	%.2x  ",
			(uint)i, (uint)c);
		else
			printk(KERN_INFO "%.2x  ", (uint)c);

		if ((i & 0x0f) == 0x0f)
			printk(KERN_INFO "\n--------------------------------");
	}

	printk(KERN_INFO "\n");

	for (i = 0; i < 256; i++) {
		sp_read_reg(0x70, i, &c);

		if ((i & 0x0f) == 0)
			printk(KERN_INFO "\n [%.2x]	%.2x  ",
			(uint)i,
			(uint)c);
		else
			printk(KERN_INFO "%.2x  ", (uint)c);

		if ((i & 0x0f) == 0x0f)
			printk(KERN_INFO "\n--------------------------------");
	}

	printk(KERN_INFO "*******Eye Diagram Test Pattern is sent********\n");
}
#endif

/* ***************************************************************** */

/* Functions defination for HDMI Input */

/* ***************************************************************** */

void hdmi_rx_set_hpd(unchar enable)
{
	unchar c;

	if (enable) {
		/* set HPD high */
		sp_read_reg(TX_P2, SP_TX_VID_CTRL3_REG, &c);
		c |= HPD_OUT;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL3_REG, c);

		DEV_NOTICE("HPD high is issued\n");
	} else {
		/* set HPD low */
		sp_read_reg(TX_P2, SP_TX_VID_CTRL3_REG, &c);
		c &= ~HPD_OUT;
		sp_write_reg(TX_P2, SP_TX_VID_CTRL3_REG, c);

		DEV_NOTICE("HPD low is issued\n");
	}
}

void hdmi_rx_set_termination(unchar enable)
{
	unchar c;

	if (enable) {
		/* set termination high */

		sp_read_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG6, &c);
		c &= ~TERM_PD;
		sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG6, c);
		DEV_NOTICE("Termination high is issued\n");
	} else {
		/* set termination low */

		sp_read_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG6, &c);
		c |= TERM_PD;
		sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG6, c);
		DEV_NOTICE("Termination low is issued\n");
	}
}

static void hdmi_rx_restart_audio_chk(void)
{
		DEV_NOTICE("WAIT_AUDIO: hdmi_rx_restart_audio_chk.\n");
		g_cts_got = 0;
		g_audio_got = 0;

		/*when audio infofram change, reconfig the audio*/
		if (hdmi_system_state > HDMI_AUDIO_CONFIG)
			hdmi_rx_set_sys_state(HDMI_AUDIO_CONFIG);

}
static void hdmi_rx_mute_video(void)
{
	unchar c;

	DEV_DBG("Mute Video.");
	sp_read_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c |= VID_MUTE;
	sp_write_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_video_muted = 1;
}

static void hdmi_rx_mute_audio(void)
{
	unchar c;

	DEV_DBG("Mute Audio.");
	sp_read_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, &c);
	c |= AUD_MUTE;
	sp_write_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, c);
	g_audio_muted = 1;
}

static unchar hdmi_rx_is_video_change(void)
{
	unchar ch, cl;
	ulong n;
	sp_read_reg(RX_P0, HDMI_RX_HTOTAL_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_HTOTAL_HIGH, &ch);
	n = (ulong)ch;
	n = (n << 8) + (ulong)cl;

	if ((g_cur_h_res == 0) || (g_cur_v_res == 0)) {
		pr_err("video changed.\n");
		return 1;
	}

	if ((g_cur_h_res < (n - 10)) || (g_cur_h_res > (n + 10))) {
		DEV_ERR("H_Res changed.");
		DEV_ERR("Current H_Res = %ld\n", n);
		return 1;
	}

	sp_read_reg(RX_P0, HDMI_RX_VTOTAL_LOW, &cl);
	sp_read_reg(RX_P0, HDMI_RX_VTOTAL_HIGH, &ch);
	n = (ulong)ch;
	n = (n << 8) + (ulong)cl;

	if ((g_cur_v_res < (n - 10)) || (g_cur_v_res > (n + 10))) {
		DEV_ERR("V_Res changed.\n");
		DEV_ERR("Current V_Res = %ld\n", n);
		return 1;
	}

	sp_read_reg(RX_P0, HDMI_RX_HDMI_STATUS_REG, &cl);

	cl &= HDMI_MODE;

	if (g_hdmi_dvi_status != cl) {
		DEV_ERR("DVI to HDMI or HDMI to DVI Change.");
		return 1;
	}

	return 0;
}

static void hdmi_rx_show_audio_info(void)
{
	unchar c;

	DEV_DBG("Audio Fs = ");
	sp_read_reg(RX_P0, HDMI_RX_AUD_IN_CH_STATUS4_REG, &c);
	c &= 0x0f;

	switch (c) {
	case 0x00:
		DEV_DBG("44.1 KHz.");
		break;
	case 0x02:
		DEV_DBG("48 KHz.");
		break;
	case 0x03:
		DEV_DBG("32 KHz.");
		break;
	case 0x08:
		DEV_DBG("88.2 KHz.");
		break;
	case 0x0a:
		DEV_DBG("96 KHz.");
		break;
	case 0x0e:
		DEV_DBG("192 KHz.");
		break;
	default:
		break;
	}

	DEV_DBG("");
}

static void hdmi_rx_init_var(void)
{
	hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
	g_cur_h_res = 0;
	g_cur_v_res = 0;
	g_cur_pix_clk = 0;
	g_video_muted = 1;
	g_audio_muted = 1;
	g_audio_stable_cntr = 0;
	g_video_stable_cntr = 0;
	g_sync_expire_cntr = 0;
	g_hdcp_err_cnt = 0;
	g_hdmi_dvi_status = VID_DVI_MODE;
	g_cts_got = 0;
	g_audio_got = 0;

}

static void hdmi_rx_tmds_phy_initialization(void)
{
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG2, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG4, 0x28);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG5, 0xe3);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG7, 0x70);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG19, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG21, 0x04);
	sp_write_reg(RX_P0, HDMI_RX_TMDS_CTRL_REG22, 0x38);
}

void hdmi_rx_initialization(void)
{
	unchar c;

	hdmi_rx_init_var();

	c = AUD_MUTE | VID_MUTE;
	sp_write_reg(RX_P0, HDMI_RX_HDMI_MUTE_CTRL_REG, c);

	sp_read_reg(RX_P0, HDMI_RX_CHIP_CTRL_REG, &c);
	c |= MAN_HDMI5V_DET;
	c |= PLLLOCK_CKDT_EN;
	c |= DIGITAL_CKDT_EN;
	sp_write_reg(RX_P0, HDMI_RX_CHIP_CTRL_REG, c);

	sp_read_reg(RX_P0, HDMI_RX_SRST_REG, &c);
	c |= HDCP_MAN_RST;
	sp_write_reg(RX_P0, HDMI_RX_SRST_REG, c);
	sp_read_reg(RX_P0, HDMI_RX_SRST_REG, &c);
	c &= ~HDCP_MAN_RST;
	sp_write_reg(RX_P0, HDMI_RX_SRST_REG, c);

	sp_read_reg(RX_P0, HDMI_RX_SRST_REG, &c);
	c |= SW_MAN_RST;
	sp_write_reg(RX_P0, HDMI_RX_SRST_REG, c);
	c &= ~SW_MAN_RST;
	sp_write_reg(RX_P0, HDMI_RX_SRST_REG, c);

	sp_read_reg(RX_P0, HDMI_RX_SRST_REG, &c);
	c |= TMDS_RST;
	sp_write_reg(RX_P0, HDMI_RX_SRST_REG, c);

	c = AEC_EN06 | AEC_EN05 ;//Sync detect change , GP set mute
	sp_write_reg(RX_P0, HDMI_RX_AEC_EN0_REG, c);

	c = AEC_EN21;//audio mode change
	sp_write_reg(RX_P0, HDMI_RX_AEC_EN2_REG, c);

	//enable AAC, AEC
	sp_read_reg(RX_P0, HDMI_RX_AEC_CTRL_REG, &c);
	c |=  AVC_EN|AAC_OE|AAC_EN;
	sp_write_reg(RX_P0, HDMI_RX_AEC_CTRL_REG, c);

	sp_read_reg(RX_P0, HDMI_RX_SYS_PWDN1_REG, &c);
	c &= ~PWDN_CTRL;
	sp_write_reg(RX_P0, HDMI_RX_SYS_PWDN1_REG, c);

	sp_write_reg(RX_P0, HDMI_RX_INT_MASK1_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK2_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK3_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK4_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK5_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK6_REG, 0x00);
	sp_write_reg(RX_P0, HDMI_RX_INT_MASK7_REG, 0x00);

	/* Range limitation for RGB input */
	sp_read_reg(RX_P0, HDMI_RX_VID_DATA_RNG_CTRL_REG, &c);
	c |= R2Y_INPUT_LIMIT;
	sp_write_reg(RX_P0, HDMI_RX_VID_DATA_RNG_CTRL_REG, c);

	c = CEC_RST;
	sp_write_reg(RX_P0, HDMI_RX_CEC_CTRL_REG, c);
	c = CEC_SPEED_27M;
	sp_write_reg(RX_P0, HDMI_RX_CEC_SPEED_CTRL_REG, c);
	c = CEC_RX_EN;
	sp_write_reg(RX_P0, HDMI_RX_CEC_CTRL_REG, c);
	hdmi_rx_tmds_phy_initialization();
	hdmi_rx_set_hpd(0);
	hdmi_rx_set_termination(0);
	DEV_NOTICE("HDMI Rx is initialized...\n");
}

static void hdmi_rx_clk_det_int(void)
{

	DEV_NOTICE("*HDMI_RX Interrupt: Pixel Clock Change.\n");
	if (sp_tx_system_state > STATE_LINK_TRAINING) {
		hdmi_rx_mute_audio();
		hdmi_rx_mute_video();
		sp_tx_video_mute(1);
		sp_tx_enable_video_input(0);
		sp_tx_enable_audio_output(0);
		sp_tx_set_sys_state(STATE_LINK_TRAINING);

		if (hdmi_system_state > HDMI_CLOCK_DET)
			hdmi_rx_set_sys_state(HDMI_CLOCK_DET);
	}
}

static void hdmi_rx_sync_det_int(void)
{

	DEV_NOTICE("*HDMI_RX Interrupt: Sync Detect.");
}

static void hdmi_rx_hdmi_dvi_int(void)
{
	unchar c;

	DEV_NOTICE("*HDMI_RX Interrupt: HDMI-DVI Mode Change.");
	sp_read_reg(RX_P0, HDMI_RX_HDMI_STATUS_REG, &c);
	hdmi_rx_get_video_info();

	if ((c & HDMI_MODE) == HDMI_MODE) {
		DEV_NOTICE("hdmi_rx_hdmi_dvi_int: HDMI MODE.");

		if (hdmi_system_state == HDMI_PLAYBACK)
			hdmi_rx_restart_audio_chk();
	} else {
		hdmi_rx_unmute_audio();
	}
}



static void hdmi_rx_cts_rcv_int(void)
{
	unchar c;
	g_cts_got = 1;
	sp_read_reg(RX_P0, HDMI_RX_SYS_STATUS_REG, &c);

	if ((hdmi_system_state == HDMI_AUDIO_CONFIG)
		&& (c & TMDS_DE_DET)) {
		if (g_cts_got && g_audio_got) {
			if (g_audio_stable_cntr >= AUDIO_STABLE_TH) {
				hdmi_rx_unmute_audio();
				hdmi_rx_unmute_video();
				g_audio_stable_cntr = 0;
				hdmi_rx_show_audio_info();
				hdmi_rx_set_sys_state(HDMI_PLAYBACK);
				sp_tx_config_audio();
			} else {
				g_audio_stable_cntr++;
			}
		} else {
			g_audio_stable_cntr = 0;
		}
	}
}

static void hdmi_rx_audio_rcv_int(void)
{
	unchar c;
	g_audio_got = 1;

	sp_read_reg(RX_P0, HDMI_RX_SYS_STATUS_REG, &c);

	if ((hdmi_system_state == HDMI_AUDIO_CONFIG)
		&& (c & TMDS_DE_DET)) {
		if (g_cts_got && g_audio_got) {
			if (g_audio_stable_cntr >= AUDIO_STABLE_TH) {
				hdmi_rx_unmute_audio();
				hdmi_rx_unmute_video();
				g_audio_stable_cntr = 0;
				hdmi_rx_show_audio_info();
				hdmi_rx_set_sys_state(HDMI_PLAYBACK);
				sp_tx_config_audio();
			} else {
				g_audio_stable_cntr++;
			}
		} else {
			g_audio_stable_cntr = 0;
		}
	}
}

static void hdmi_rx_hdcp_error_int(void)
{
	g_audio_got = 0;
	g_cts_got = 0;

	if (g_hdcp_err_cnt >= 40) {
		g_hdcp_err_cnt = 0;
		DEV_ERR("Lots of hdcp error occured ...");
		hdmi_rx_mute_audio();
		hdmi_rx_mute_video();

		/* issue hotplug */
		hdmi_rx_set_hpd(0);
		msleep(10);
		hdmi_rx_set_hpd(1);

	} else {
		g_hdcp_err_cnt++;
	}
}

static void hdmi_rx_new_avi_int(void)
{

	DEV_NOTICE("*HDMI_RX Interrupt: New AVI Packet.");
	sp_tx_lvttl_bit_mapping();
	sp_tx_set_colorspace();
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
}

static void hdmi_rx_new_gcp_int(void)
{
	unchar c;
	sp_read_reg(RX_P1, HDMI_RX_GENERAL_CTRL, &c);
	if (c & SET_AVMUTE) {
		if (!g_video_muted)
			hdmi_rx_mute_video();
		if (!g_audio_muted)
			hdmi_rx_mute_audio();

	} else if (c & CLEAR_AVMUTE) {
		if (g_video_muted)
			hdmi_rx_unmute_video();
		if (g_audio_muted)
			hdmi_rx_unmute_audio();
	}
}

static void hdmi_rx_new_vsi_int(void)
{
	unchar c;
	unchar hdmi_video_format, vsi_header, v3d_structure;
	DEV_ERR("*HDMI_RX Interrupt: NEW VSI packet.\n");
	sp_read_reg(TX_P0, SP_TX_3D_VSC_CTRL, &c);
		if (!(c & INFO_FRAME_VSC_EN)) {
			sp_read_reg(RX_P1, HDMI_RX_MPEG_TYPE_REG, &vsi_header);
			sp_read_reg(RX_P1, HDMI_RX_MPEG_DATA03_REG,
				&hdmi_video_format);
			if ((vsi_header == 0x81) &&
				((hdmi_video_format & 0xe0) == 0x40)) {
				DEV_DBG("3D VSI packet is detected.\n");
				/*use mpeg packet as mail box
				to send vsi packet*/
				sp_tx_vsi_setup();
				sp_tx_config_packets(VSI_PACKETS);

				sp_read_reg(RX_P1, HDMI_RX_MPEG_DATA05_REG,
					&v3d_structure);
			switch (v3d_structure & 0xf0) {
			case 0x00:/*frame packing*/
				v3d_structure = 0x02;
				break;
			case 0x20:/*Line alternative*/
				v3d_structure = 0x03;
				break;
			case 0x30:/*Side-by-side(full)*/
				v3d_structure = 0x04;
				break;
			default:
				v3d_structure = 0x00;
				DEV_ERR("3D structure is not supported\n");
				break;
			}

				sp_write_reg(TX_P0, SP_TX_VSC_DB1,
					v3d_structure);
			sp_read_reg(TX_P0, SP_TX_3D_VSC_CTRL, &c);
			c |= INFO_FRAME_VSC_EN;
			sp_write_reg(TX_P0, SP_TX_3D_VSC_CTRL, c);

			sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
			c &= ~SPD_IF_EN;
			sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

			sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
			c |= SPD_IF_UD;
			sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);

			sp_read_reg(TX_P0, SP_TX_PKT_EN_REG, &c);
			c |= SPD_IF_EN;
			sp_write_reg(TX_P0, SP_TX_PKT_EN_REG, c);
		}
	}
}

static void hdmi_rx_no_vsi_int(void)
{
	unchar c;
	sp_read_reg(TX_P0, SP_TX_3D_VSC_CTRL, &c);
		if (c & INFO_FRAME_VSC_EN) {
			DEV_ERR("No new VSI is received.\n");
			c &= ~INFO_FRAME_VSC_EN;
			sp_write_reg(TX_P0, SP_TX_3D_VSC_CTRL, c);
			sp_tx_mpeg_setup();
			sp_tx_config_packets(MPEG_PACKETS);
		}
}

#ifdef CONFIG_SLIMPORT_CEC
static void hdmi_rx_cec_rcv_int(void)
{
	DEV_DBG("**HDMI RX received CEC***");
	if ((sp_tx_rx_type == RX_HDMI)
		&& (CEC_IS_READY == cec_status_get()))
		upstream_hdmi_cec_readmessage();

}
#endif
void sp_tx_config_hdmi_input(void)
{
	unchar avmute_status, sys_status;

	sp_read_reg(RX_P0, HDMI_RX_SYS_STATUS_REG, &sys_status);
	if ((sys_status&TMDS_DE_DET) && (sys_status&TMDS_CLOCK_DET)
		&& (hdmi_system_state == HDMI_CLOCK_DET))
		hdmi_rx_set_sys_state(HDMI_VIDEO_CONFIG);


	if (hdmi_rx_is_video_change()) {
		DEV_ERR("Video Changed , mute video and mute audio");
		g_video_stable_cntr = 0;

		if (!g_video_muted)
			hdmi_rx_mute_video();

		if (!g_audio_muted)
			hdmi_rx_mute_audio();

	} else if (g_video_stable_cntr < VIDEO_STABLE_TH) {
		g_video_stable_cntr++;
		DEV_NOTICE("WAIT_VIDEO: Wait for video stable cntr.");
	} else {
		sp_read_reg(RX_P0, HDMI_RX_HDMI_STATUS_REG, &avmute_status);
		if (!(avmute_status & MUTE_STAT)) {
			hdmi_rx_get_video_info();
			hdmi_rx_unmute_video();
			sp_tx_lvttl_bit_mapping();
			sp_tx_set_sys_state(STATE_LINK_TRAINING);
			hdmi_rx_show_video_info();
			sp_tx_power_down(SP_TX_PWR_AUDIO);

			if (g_hdmi_dvi_status) {
				DEV_NOTICE("HDMI mode: Video is stable.");
				sp_tx_send_message(MSG_INPUT_HDMI);
				hdmi_rx_set_sys_state(HDMI_AUDIO_CONFIG);
			} else {
				DEV_NOTICE("DVI mode: Video is stable.");
				sp_tx_send_message(MSG_INPUT_DVI);
				hdmi_rx_unmute_audio();
				hdmi_rx_set_sys_state(HDMI_PLAYBACK);
			}
		}
	}

	hdmi_rx_get_video_info();
}

void hdmi_rx_int_irq_handler(void)
{
	unchar c1, c2, c3, c4, c5, c6, c7;

	if ((hdmi_system_state < HDMI_CLOCK_DET)
			|| (sp_tx_system_state < STATE_CONFIG_HDMI))
		return;

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS1_REG, &c1);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS1_REG, c1);

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS2_REG, &c2);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS2_REG, c2);

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS3_REG, &c3);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS3_REG, c3);

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS4_REG, &c4);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS4_REG, c4);

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS5_REG, &c5);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS5_REG, c5);

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS6_REG, &c6);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS6_REG, c6);

	sp_read_reg(RX_P0, HDMI_RX_INT_STATUS7_REG, &c7);
	sp_write_reg(RX_P0, HDMI_RX_INT_STATUS7_REG, c7);

	if (c1 & CKDT_CHANGE)
		hdmi_rx_clk_det_int();

	if (c1 & SCDT_CHANGE)
		hdmi_rx_sync_det_int();

	if (c1 & HDMI_DVI)
		hdmi_rx_hdmi_dvi_int();


	if (c6 & NEW_AVI)
		hdmi_rx_new_avi_int();

	if (c7 & NEW_VS)
		hdmi_rx_new_vsi_int();

	if (c7 & NO_VSI)
		hdmi_rx_no_vsi_int();

#ifdef CONFIG_SLIMPORT_CEC
	if (c7 & CEC_RX_READY)
		hdmi_rx_cec_rcv_int();
#endif

	if ((c6 & NEW_AUD) || (c3 & AUD_MODE_CHANGE))
		hdmi_rx_restart_audio_chk();


	if (c6 & CTS_RCV)
		hdmi_rx_cts_rcv_int();


	if (c5 & AUDIO_RCV)
		hdmi_rx_audio_rcv_int();


	if (c2 & HDCP_ERR)
		hdmi_rx_hdcp_error_int();



	if (c6 & NEW_CP)
		hdmi_rx_new_gcp_int();
}

void sp_tx_phy_auto_test(void)
{
	unchar bSwing, bEmp;
	unchar c1;
	enum SP_LINK_BW link_bw;
	sp_tx_aux_dpcdread_bytes(0x0, 0x02, 0x19, 1, bytebuf);
	switch (bytebuf[0]) {
	case 0x06:
		sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, 0x06);
		DEV_DBG("test BW= 1.62Gbps\n");
		break;
	case 0x0a:
		sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, 0x0a);
		DEV_DBG("test BW= 2.7Gbps\n");
		break;
	case 0x14:
		sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, 0x14);
		DEV_DBG("test BW= 5.4Gbps\n");
		break;
	}
	/*DPCD 0x248 PHY_TEST_PATTERN*/
	sp_tx_aux_dpcdread_bytes(0x0, 0x02, 0x48, 1, bytebuf);
	switch (bytebuf[0]) {
	case 0:
		DEV_DBG("No test pattern selected\n");
		break;
	case 1:
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x04);
		DEV_DBG("D10.2 Pattern\n");
		break;
	case 2:
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x08);
		DEV_DBG("Symbol Error Measurement Count\n");
		break;
	case 3:
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x0c);
		DEV_DBG("PRBS7 Pattern\n");
		break;
	case 4:
		sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x50, 0xa, bytebuf);
		sp_write_reg(TX_P1, 0x80, bytebuf[0]);
		sp_write_reg(TX_P1, 0x81, bytebuf[1]);
		sp_write_reg(TX_P1, 0x82, bytebuf[2]);
		sp_write_reg(TX_P1, 0x83, bytebuf[3]);
		sp_write_reg(TX_P1, 0x84, bytebuf[4]);
		sp_write_reg(TX_P1, 0x85, bytebuf[5]);
		sp_write_reg(TX_P1, 0x86, bytebuf[6]);
		sp_write_reg(TX_P1, 0x87, bytebuf[7]);
		sp_write_reg(TX_P1, 0x88, bytebuf[8]);
		sp_write_reg(TX_P1, 0x89, bytebuf[9]);
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x30);
		DEV_DBG("80bit custom pattern transmitted\n");
		break;
	case 5:
		sp_write_reg(TX_P0, 0xA9, 0x00);
		sp_write_reg(TX_P0, 0xAA, 0x01);
		sp_write_reg(TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x14);
		DEV_DBG("HBR2 Compliance Eye Pattern\n");
		break;
	}
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, 0x03, 1, bytebuf);
	switch (bytebuf[0] & 0x01) {
	case 0:
		sp_tx_spread_enable(0);
		DEV_DBG("SSC OFF\n");
		break;
	case 1:
		sp_read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, &c1);
				switch (c1) {
				case 0x06:
					link_bw = BW_162G;
					break;
				case 0x0a:
					link_bw = BW_27G;
					break;
				case 0x14:
					link_bw = BW_54G;
					break;
				default:
					link_bw = BW_NULL;
					break;
				}
				sp_tx_config_ssc(link_bw);
		DEV_DBG("SSC ON\n");
		break;
	}
	/*get swing adjust request*/
	sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x06, 1, bytebuf);
	DEV_DBG("DPCD:0x00206 = %.2x\n", (uint)bytebuf[0]);
	c1 = bytebuf[0] & 0x03;
	switch (c1) {
	case 0x00:
		sp_read_reg(TX_P0, 0xA3, &bSwing);
		sp_write_reg(TX_P0, 0xA3, (bSwing&~0x03)|0x00);
		DEV_DBG("lane0,Swing200mv\n");
		break;
	case 0x01:
		sp_read_reg(TX_P0, 0xA3, &bSwing);
		sp_write_reg(TX_P0, 0xA3, (bSwing&~0x03)|0x01);
		DEV_DBG("lane0,Swing400mv\n");
		break;
	case 0x02:
		sp_read_reg(TX_P0, 0xA3, &bSwing);
		sp_write_reg(TX_P0, 0xA3, (bSwing&~0x03)|0x02);
		DEV_DBG("lane0,Swing600mv\n");
		break;
	case 0x03:
		sp_read_reg(TX_P0, 0xA3, &bSwing);
		sp_write_reg(TX_P0, 0xA3, (bSwing&~0x03)|0x03);
		DEV_DBG("lane0,Swing800mv\n");
		break;
	default:
		break;
	}
	/*get emphasis adjust request*/
	c1 = (bytebuf[0] & 0x0c);
	c1 = c1 >> 2;
	switch (c1) {
	case 0x00:
		sp_read_reg(TX_P0, 0xA3, &bEmp);
		sp_write_reg(TX_P0, 0xA3, (bEmp&~0x18)|0x00);
		DEV_DBG("lane0,emp 0db\n");
		break;
	case 0x01:
		sp_read_reg(TX_P0, 0xA3, &bEmp);
		sp_write_reg(TX_P0, 0xA3, (bEmp&~0x18)|0x08);
		DEV_DBG("lane0,emp 3.5db\n");
		break;
	case 0x02:
		sp_read_reg(TX_P0, 0xA3, &bEmp);
		sp_write_reg(TX_P0, 0xA3, (bEmp&~0x18)|0x10);
		DEV_DBG("lane0,emp 6db\n");
		break;
	default:
		break;
	}
}
