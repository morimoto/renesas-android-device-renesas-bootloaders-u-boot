/*
 * DesignWare High-Definition Multimedia Interface (HDMI) driver
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2018 Renesas Electronics Corporation
 * Copyright (C) 2013-2015 Mentor Graphics Inc.
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2010, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <common.h>
#include <linux/io.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <rcar-logo.h>
#include "dw-hdmi.h"

static struct dw_hdmi *G_HDMI;

static const u16 csc_coeff_default[3][4] = {
	{ 0x2000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x2000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x2000, 0x0000 }
};

struct drm_display_mode {
	unsigned int type;

	int clock;		/* in kHz */
	int hdisplay;
	int hsync_start;
	int hsync_end;
	int htotal;
	int hskew;
	int vdisplay;
	int vsync_start;
	int vsync_end;
	int vtotal;
	int vscan;
	unsigned int flags;
	int width_mm;
	int height_mm;

	int crtc_clock;
	int crtc_hdisplay;
	int crtc_hblank_start;
	int crtc_hblank_end;
	int crtc_hsync_start;
	int crtc_hsync_end;
	int crtc_htotal;
	int crtc_hskew;
	int crtc_vdisplay;
	int crtc_vblank_start;
	int crtc_vblank_end;
	int crtc_vsync_start;
	int crtc_vsync_end;
	int crtc_vtotal;

	int *private;
	int private_flags;
	int vrefresh;
	int hsync;
};

enum drm_connector_force {
	DRM_FORCE_UNSPECIFIED,
	DRM_FORCE_OFF,
	DRM_FORCE_ON,
	DRM_FORCE_ON_DIGITAL,
};

struct hdmi_vmode {
	bool mdataenablepolarity;

	unsigned int mpixelclock;
	unsigned int mpixelrepetitioninput;
	unsigned int mpixelrepetitionoutput;
};

struct hdmi_data_info {
	unsigned int hdcp_enable;
	struct hdmi_vmode video_mode;
};

struct dw_hdmi_phy_data {
	enum dw_hdmi_phy_type type;
	const char *name;
	unsigned int gen;
	bool has_svsret;
	int (*configure)(struct dw_hdmi *hdmi,
			 unsigned long mpixelclock);
};

struct dw_hdmi {
	unsigned int version;
	struct hdmi_data_info hdmi_data;
	struct {
		const struct dw_hdmi_phy_ops *ops;
		const char *name;
		void *data;
		bool enabled;
	} phy;

	struct drm_display_mode previous_mode;

	enum drm_connector_force force;	/* mutex-protected force state */
	bool disabled;			/* DRM has disabled our bridge */
	bool bridge_is_on;		/* indicates the bridge is on */
	bool rxsense;			/* rxsense state */
	u8 phy_mask;			/* desired phy int mask settings */
	u8 mc_clkdis;			/* clock disable register */
};

#define HDMI_IH_PHY_STAT0_RX_SENSE \
	(HDMI_IH_PHY_STAT0_RX_SENSE0 | HDMI_IH_PHY_STAT0_RX_SENSE1 | \
	 HDMI_IH_PHY_STAT0_RX_SENSE2 | HDMI_IH_PHY_STAT0_RX_SENSE3)

#define HDMI_PHY_RX_SENSE \
	(HDMI_PHY_RX_SENSE0 | HDMI_PHY_RX_SENSE1 | \
	 HDMI_PHY_RX_SENSE2 | HDMI_PHY_RX_SENSE3)

static inline void hdmi_writeb(u8 val, int offset)
{
	writeb(val, (ulong)(RCAR_HDMI0_BASE_REG + offset));
}

static inline u8 hdmi_readb(int offset)
{
	return readb((ulong)(RCAR_HDMI0_BASE_REG + offset));
}

static void hdmi_modb(u8 data, u8 mask, unsigned offset)
{
	u8 reg = readb((ulong)(RCAR_HDMI0_BASE_REG + offset));
	reg &= ~mask;
	writeb(reg | data, (ulong)(RCAR_HDMI0_BASE_REG + offset));
}

static void hdmi_mask_writeb(u8 data, unsigned int reg,
			     u8 shift, u8 mask)
{
	hdmi_modb(data << shift, mask, reg);
}

static void hdmi_video_sample(void)
{
	int color_format = 0;
	u8 val;

	color_format = 0x01; //RGB888_1X24

	val = HDMI_TX_INVID0_INTERNAL_DE_GENERATOR_DISABLE |
		((color_format << HDMI_TX_INVID0_VIDEO_MAPPING_OFFSET) &
		HDMI_TX_INVID0_VIDEO_MAPPING_MASK);
	hdmi_writeb(val, HDMI_TX_INVID0);

	/* Enable TX stuffing: When DE is inactive, fix the output data to 0 */
	val = HDMI_TX_INSTUFFING_BDBDATA_STUFFING_ENABLE |
		HDMI_TX_INSTUFFING_RCRDATA_STUFFING_ENABLE |
		HDMI_TX_INSTUFFING_GYDATA_STUFFING_ENABLE;
	hdmi_writeb(val, HDMI_TX_INSTUFFING);
	hdmi_writeb(0x0, HDMI_TX_GYDATA0);
	hdmi_writeb(0x0, HDMI_TX_GYDATA1);
	hdmi_writeb(0x0, HDMI_TX_RCRDATA0);
	hdmi_writeb(0x0, HDMI_TX_RCRDATA1);
	hdmi_writeb(0x0, HDMI_TX_BCBDATA0);
	hdmi_writeb(0x0, HDMI_TX_BCBDATA1);
}

static void dw_hdmi_update_csc_coeffs(struct dw_hdmi *hdmi)
{
	const u16 (*csc_coeff)[3][4] = &csc_coeff_default;
	unsigned i;
	u32 csc_scale = 1;

	/* The CSC registers are sequential, alternating MSB then LSB */
	for (i = 0; i < ARRAY_SIZE(csc_coeff_default[0]); i++) {
		u16 coeff_a = (*csc_coeff)[0][i];
		u16 coeff_b = (*csc_coeff)[1][i];
		u16 coeff_c = (*csc_coeff)[2][i];

		hdmi_writeb(coeff_a & 0xff, HDMI_CSC_COEF_A1_LSB + i * 2);
		hdmi_writeb(coeff_a >> 8, HDMI_CSC_COEF_A1_MSB + i * 2);
		hdmi_writeb(coeff_b & 0xff, HDMI_CSC_COEF_B1_LSB + i * 2);
		hdmi_writeb(coeff_b >> 8, HDMI_CSC_COEF_B1_MSB + i * 2);
		hdmi_writeb(coeff_c & 0xff, HDMI_CSC_COEF_C1_LSB + i * 2);
		hdmi_writeb(coeff_c >> 8, HDMI_CSC_COEF_C1_MSB + i * 2);
	}

	hdmi_modb(csc_scale, HDMI_CSC_SCALE_CSCSCALE_MASK,
		  HDMI_CSC_SCALE);
}

static void hdmi_video_csc(struct dw_hdmi *hdmi)
{
	int interpolation = HDMI_CSC_CFG_INTMODE_DISABLE;
	int decimation = 0;
	//unsigned int color_depth = HDMI_CSC_SCALE_CSC_COLORDE_PTH_24BPP;
	/* Configure the CSC registers */
	hdmi_writeb(interpolation | decimation, HDMI_CSC_CFG);
	dw_hdmi_update_csc_coeffs(hdmi);
}

/*
 * HDMI video packetizer is used to packetize the data.
 * for example, if input is YCC422 mode or repeater is used,
 * data should be repacked this module can be bypassed.
 */
static void hdmi_video_packetize(struct dw_hdmi *hdmi)
{
	//unsigned int color_depth = 4;//RGB888_1X24
	//unsigned int remap_size = HDMI_VP_REMAP_YCC422_16bit;
	unsigned int output_select = HDMI_VP_CONF_OUTPUT_SELECTOR_PP;
	u8 vp_conf;

	output_select = HDMI_VP_CONF_OUTPUT_SELECTOR_BYPASS;

	/* data from packetizer block */
	vp_conf = HDMI_VP_CONF_PR_EN_DISABLE |
		  HDMI_VP_CONF_BYPASS_SELECT_VID_PACKETIZER;

	hdmi_modb(vp_conf,
		  HDMI_VP_CONF_BYPASS_SELECT_MASK, HDMI_VP_CONF);

	if (output_select == HDMI_VP_CONF_OUTPUT_SELECTOR_PP) {
		vp_conf = HDMI_VP_CONF_BYPASS_EN_DISABLE |
			  HDMI_VP_CONF_PP_EN_ENABLE |
			  HDMI_VP_CONF_YCC422_EN_DISABLE;
	} else if (output_select == HDMI_VP_CONF_OUTPUT_SELECTOR_YCC422) {
		vp_conf = HDMI_VP_CONF_BYPASS_EN_DISABLE |
			  HDMI_VP_CONF_PP_EN_DISABLE |
			  HDMI_VP_CONF_YCC422_EN_ENABLE;
	} else if (output_select == HDMI_VP_CONF_OUTPUT_SELECTOR_BYPASS) {
		vp_conf = HDMI_VP_CONF_BYPASS_EN_ENABLE |
			  HDMI_VP_CONF_PP_EN_DISABLE |
			  HDMI_VP_CONF_YCC422_EN_DISABLE;
	} else {
		return;
	}

	hdmi_modb(vp_conf,
		  HDMI_VP_CONF_BYPASS_EN_MASK |
		  HDMI_VP_CONF_YCC422_EN_MASK, HDMI_VP_CONF);

	hdmi_writeb(HDMI_VP_STUFF_YCC422_STUFFING_STUFFING_MODE,
		    HDMI_VP_STUFF);

	hdmi_modb(output_select, HDMI_VP_CONF_OUTPUT_SELECTOR_MASK,
		  HDMI_VP_CONF);
}

/* -----------------------------------------------------------------------------
 * Synopsys PHY Handling
 */
static bool hdmi_phy_wait_i2c_done(int msec)
{
	u32 val;

	while ((val = hdmi_readb(HDMI_IH_I2CMPHY_STAT0) & 0x3) == 0) {
		if (msec-- == 0)
			return false;
		udelay(1000);
	}
	hdmi_writeb(val, HDMI_IH_I2CMPHY_STAT0);

	return true;
}

static void dw_hdmi_phy_i2c_write(unsigned short data,
			   unsigned char addr)
{
	hdmi_writeb(HDMI_IH_I2CMPHY_STAT0_I2CMPHYDONE |
		    HDMI_IH_I2CMPHY_STAT0_I2CMPHYERROR,
		    HDMI_IH_I2CMPHY_STAT0);

	hdmi_writeb(addr, HDMI_PHY_I2CM_ADDRESS_ADDR);
	hdmi_writeb((unsigned char)(data >> 8),
		    HDMI_PHY_I2CM_DATAO_1_ADDR);
	hdmi_writeb((unsigned char)(data >> 0),
		    HDMI_PHY_I2CM_DATAO_0_ADDR);
	hdmi_writeb(HDMI_PHY_I2CM_OPERATION_ADDR_WRITE,
		    HDMI_PHY_I2CM_OPERATION_ADDR);
	hdmi_phy_wait_i2c_done(1000);
}

static void dw_hdmi_phy_enable_svsret(u8 enable)
{
	hdmi_mask_writeb(enable, HDMI_PHY_CONF0,
			 HDMI_PHY_CONF0_SVSRET_OFFSET,
			 HDMI_PHY_CONF0_SVSRET_MASK);
}

static void dw_hdmi_phy_gen2_pddq(u8 enable)
{
	hdmi_mask_writeb(enable, HDMI_PHY_CONF0,
			 HDMI_PHY_CONF0_GEN2_PDDQ_OFFSET,
			 HDMI_PHY_CONF0_GEN2_PDDQ_MASK);
}

static void dw_hdmi_phy_gen2_txpwron(u8 enable)
{
	hdmi_mask_writeb(enable, HDMI_PHY_CONF0,
			 HDMI_PHY_CONF0_GEN2_TXPWRON_OFFSET,
			 HDMI_PHY_CONF0_GEN2_TXPWRON_MASK);
}

static void dw_hdmi_phy_sel_data_en_pol(u8 enable)
{
	hdmi_mask_writeb(enable, HDMI_PHY_CONF0,
			 HDMI_PHY_CONF0_SELDATAENPOL_OFFSET,
			 HDMI_PHY_CONF0_SELDATAENPOL_MASK);
}

static void dw_hdmi_phy_sel_interface_control(u8 enable)
{
	hdmi_mask_writeb(enable, HDMI_PHY_CONF0,
			 HDMI_PHY_CONF0_SELDIPIF_OFFSET,
			 HDMI_PHY_CONF0_SELDIPIF_MASK);
}

static void dw_hdmi_phy_reset(void)
{
	/* PHY reset. The reset signal is active high on Gen2 PHYs. */
	hdmi_writeb(HDMI_MC_PHYRSTZ_PHYRSTZ, HDMI_MC_PHYRSTZ);
	hdmi_writeb(0, HDMI_MC_PHYRSTZ);
}

static void dw_hdmi_phy_i2c_set_addr(u8 address)
{
	hdmi_writeb(address, HDMI_PHY_I2CM_SLAVE_ADDR);
}

static void dw_hdmi_phy_power_off(void)
{
	unsigned int i = 0;
	u16 val = 0;

	dw_hdmi_phy_gen2_txpwron(0);

	/*
	 * Wait for TX_PHY_LOCK to be deasserted to indicate that the PHY went
	 * to low power mode.
	 */
	for (i = 0; i < 5; ++i) {
		val = hdmi_readb(HDMI_PHY_STAT0);
		if (!(val & HDMI_PHY_TX_PHY_LOCK))
			break;

		udelay(2000);
	}

	if (val & HDMI_PHY_TX_PHY_LOCK)
		printf("%s: PHY failed to power down\n", __func__);

	dw_hdmi_phy_gen2_pddq(1);
}

static int dw_hdmi_phy_power_on(void)
{
	unsigned int i;
	u8 val;

	dw_hdmi_phy_gen2_txpwron(1);
	dw_hdmi_phy_gen2_pddq(0);

	/* Wait for PHY PLL lock */
	for (i = 0; i < 5; ++i) {
		val = hdmi_readb(HDMI_PHY_STAT0) & HDMI_PHY_TX_PHY_LOCK;
		if (val)
			break;

		udelay(2000);
	}

	if (!val) {
		printf("%s: PHY PLL failed to lock\n", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

#define RCAR_HDMI_PHY_OPMODE_PLLCFG	0x06	/* Mode of operation and PLL dividers */
#define RCAR_HDMI_PHY_CKSYMTXCTRL	0x09	/* Clock Symbol and Transmitter Control Register */
#define RCAR_HDMI_PHY_VLEVCTRL		0x0e	/* Voltage Level Control Register */
#define RCAR_HDMI_PHY_PLLCURRGMPCTRL	0x10	/* PLL current and Gmp (conductance) */
#define RCAR_HDMI_PHY_PLLDIVCTRL	0x11	/* PLL dividers */
#define RCAR_HDMI_PHY_TXTERM		0x19	/* Transmission Termination Register */

struct rcar_hdmi_phy_params {
	unsigned long mpixelclock;
	u16 opmode_div;	/* Mode of operation and PLL dividers */
	u16 curr_gmp;	/* PLL current and Gmp (conductance) */
	u16 div;	/* PLL dividers */
};

struct rcar_hdmi_phy_params_2 {
	unsigned long mpixelclock;
	u16 clk;	/* Clock Symbol and Transmitter Control Register */
	u16 vol_level;	/* Voltage Level */
	u16 trans;	/* Transmission Termination Register */
};

static const struct rcar_hdmi_phy_params rcar_hdmi_phy_params[] = {
	{ 35500000,  0x0003, 0x0283, 0x0628 },
	{ 44900000,  0x0003, 0x0285, 0x0228 },
	{ 71000000,  0x0002, 0x1183, 0x0614 },
	{ 90000000,  0x0002, 0x1142, 0x0214 },
	{ 140250000, 0x0001, 0x20c0, 0x060a },
	{ 182750000, 0x0001, 0x2080, 0x020a },
	{ 281250000, 0x0000, 0x3040, 0x0605 },
	{ 297000000, 0x0000, 0x3041, 0x0205 },
	{ ~0UL,      0x0000, 0x0000, 0x0000 },
};

static const struct rcar_hdmi_phy_params_2 rcar_hdmi_phy_params_2[] = {
	{ 165000000,  0x8c88, 0x0180, 0x0007},
	{ 297000000,  0x83c8, 0x0180, 0x0004},
	{ ~0UL,       0x0000, 0x0000, 0x0000},
};

static int rcar_hdmi_phy_configure(struct dw_hdmi *hdmi, unsigned long mpixelclock)
{
	const struct rcar_hdmi_phy_params *params = rcar_hdmi_phy_params;
	const struct rcar_hdmi_phy_params_2 *params_2 = rcar_hdmi_phy_params_2;

	for (; params->mpixelclock != ~0UL; ++params) {
		if (mpixelclock <= params->mpixelclock)
			break;
	}

	if (params->mpixelclock == ~0UL)
		return -EINVAL;

	dw_hdmi_phy_i2c_write(params->opmode_div,
			      RCAR_HDMI_PHY_OPMODE_PLLCFG);
	dw_hdmi_phy_i2c_write(params->curr_gmp,
			      RCAR_HDMI_PHY_PLLCURRGMPCTRL);
	dw_hdmi_phy_i2c_write(params->div, RCAR_HDMI_PHY_PLLDIVCTRL);

	for (; params_2->mpixelclock != ~0UL; ++params_2) {
		if (mpixelclock <= params_2->mpixelclock)
			break;
	}

	if (params_2->mpixelclock == ~0UL)
		return -EINVAL;

	dw_hdmi_phy_i2c_write(params_2->clk, RCAR_HDMI_PHY_CKSYMTXCTRL);
	dw_hdmi_phy_i2c_write(params_2->vol_level,
			      RCAR_HDMI_PHY_VLEVCTRL);
	dw_hdmi_phy_i2c_write(params_2->trans, RCAR_HDMI_PHY_TXTERM);

	return 0;
}

static int hdmi_phy_configure(struct dw_hdmi *hdmi)
{
	const struct dw_hdmi_phy_data *phy = hdmi->phy.data;
	unsigned long mpixelclock = hdmi->hdmi_data.video_mode.mpixelclock;
	int ret;

	dw_hdmi_phy_power_off();

	/* Leave low power consumption mode by asserting SVSRET. */
	if (phy->has_svsret)
		dw_hdmi_phy_enable_svsret(1);

	dw_hdmi_phy_reset();

	dw_hdmi_phy_i2c_set_addr(HDMI_PHY_I2CM_SLAVE_ADDR_PHY_GEN2);

	/* Write to the PHY as configured by the platform */
	ret = rcar_hdmi_phy_configure(hdmi, mpixelclock);
	if (ret) {
		printf("%s: PHY configuration failed (clock %lu)\n", __func__, mpixelclock);
		return ret;
	}

	return dw_hdmi_phy_power_on();
}

static int dw_hdmi_phy_init(struct dw_hdmi *hdmi, void *data)
{
	int i, ret;
	/* HDMI Phy spec says to do the phy initialization sequence twice */
	for (i = 0; i < 2; i++) {
		dw_hdmi_phy_sel_data_en_pol(1);
		dw_hdmi_phy_sel_interface_control(0);

		ret = hdmi_phy_configure(hdmi);
		if (ret)
			return ret;
	}
	return 0;
}

static void dw_hdmi_phy_disable(struct dw_hdmi *hdmi, void *data)
{
	dw_hdmi_phy_power_off();
}

static int dw_hdmi_phy_read_hpd(struct dw_hdmi *hdmi,
					       void *data)
{
	int stat = hdmi_readb(HDMI_PHY_STAT0) & HDMI_PHY_HPD ?
		1 /*conn*/ : 2 /*disconn*/;
	return stat;
}

static void dw_hdmi_phy_update_hpd(struct dw_hdmi *hdmi, void *data,
			    bool force, bool disabled, bool rxsense)
{
	u8 old_mask = hdmi->phy_mask;

	if (force || disabled || !rxsense)
		hdmi->phy_mask |= HDMI_PHY_RX_SENSE;
	else
		hdmi->phy_mask &= ~HDMI_PHY_RX_SENSE;

	if (old_mask != hdmi->phy_mask)
		hdmi_writeb(hdmi->phy_mask, HDMI_PHY_MASK0);
}

static void dw_hdmi_phy_setup_hpd(struct dw_hdmi *hdmi, void *data)
{
	/*
	 * Configure the PHY RX SENSE and HPD interrupts polarities and clear
	 * any pending interrupt.
	 */
	hdmi_writeb(HDMI_PHY_HPD | HDMI_PHY_RX_SENSE, HDMI_PHY_POL0);
	hdmi_writeb(HDMI_IH_PHY_STAT0_HPD | HDMI_IH_PHY_STAT0_RX_SENSE,
		    HDMI_IH_PHY_STAT0);

	/* Enable cable hot plug irq. */
	hdmi_writeb(hdmi->phy_mask, HDMI_PHY_MASK0);

	/* Clear and unmute interrupts. */
	hdmi_writeb(HDMI_IH_PHY_STAT0_HPD | HDMI_IH_PHY_STAT0_RX_SENSE,
		    HDMI_IH_PHY_STAT0);

	hdmi_writeb((HDMI_IH_MUTE_PHY_STAT0_MASK &
		    ~(HDMI_IH_PHY_STAT0_HPD |
		    HDMI_IH_PHY_STAT0_RX_SENSE)),
		    HDMI_IH_MUTE_PHY_STAT0);
}

static const struct dw_hdmi_phy_ops dw_hdmi_synopsys_phy_ops = {
	.init = dw_hdmi_phy_init,
	.disable = dw_hdmi_phy_disable,
	.read_hpd = dw_hdmi_phy_read_hpd,
	.update_hpd = dw_hdmi_phy_update_hpd,
	.setup_hpd = dw_hdmi_phy_setup_hpd,
};

/* -----------------------------------------------------------------------------
 * HDMI TX Setup
 */
static void hdmi_tx_hdcp_config(struct dw_hdmi *hdmi)
{
	u8 de;

	if (hdmi->hdmi_data.video_mode.mdataenablepolarity)
		de = HDMI_A_VIDPOLCFG_DATAENPOL_ACTIVE_HIGH;
	else
		de = HDMI_A_VIDPOLCFG_DATAENPOL_ACTIVE_LOW;

	/* disable rx detect */
	hdmi_modb(HDMI_A_HDCPCFG0_RXDETECT_DISABLE,
		  HDMI_A_HDCPCFG0_RXDETECT_MASK, HDMI_A_HDCPCFG0);

	hdmi_modb(de, HDMI_A_VIDPOLCFG_DATAENPOL_MASK, HDMI_A_VIDPOLCFG);

	hdmi_modb(HDMI_A_HDCPCFG1_ENCRYPTIONDISABLE_DISABLE,
		  HDMI_A_HDCPCFG1_ENCRYPTIONDISABLE_MASK, HDMI_A_HDCPCFG1);
}

#define DRM_MODE_FLAG_PHSYNC			(1<<0)
#define DRM_MODE_FLAG_PVSYNC			(1<<2)
#define DRM_MODE_FLAG_INTERLACE			(1<<4)

static void hdmi_av_composer(struct dw_hdmi *hdmi,
			     const struct drm_display_mode *mode)
{
	u8 inv_val;
	struct hdmi_vmode *vmode = &hdmi->hdmi_data.video_mode;
	int hblank, vblank, h_de_hs, v_de_vs, hsync_len, vsync_len;
	unsigned int vdisplay;

	vmode->mpixelclock = mode->clock * 1000;

	/* Set up HDMI_FC_INVIDCONF */
	inv_val = (hdmi->hdmi_data.hdcp_enable ?
		HDMI_FC_INVIDCONF_HDCP_KEEPOUT_ACTIVE :
		HDMI_FC_INVIDCONF_HDCP_KEEPOUT_INACTIVE);

	inv_val |= mode->flags & DRM_MODE_FLAG_PVSYNC ?
		HDMI_FC_INVIDCONF_VSYNC_IN_POLARITY_ACTIVE_HIGH :
		HDMI_FC_INVIDCONF_VSYNC_IN_POLARITY_ACTIVE_LOW;

	inv_val |= mode->flags & DRM_MODE_FLAG_PHSYNC ?
		HDMI_FC_INVIDCONF_HSYNC_IN_POLARITY_ACTIVE_HIGH :
		HDMI_FC_INVIDCONF_HSYNC_IN_POLARITY_ACTIVE_LOW;

	inv_val |= (vmode->mdataenablepolarity ?
		HDMI_FC_INVIDCONF_DE_IN_POLARITY_ACTIVE_HIGH :
		HDMI_FC_INVIDCONF_DE_IN_POLARITY_ACTIVE_LOW);

	inv_val |= mode->flags & DRM_MODE_FLAG_INTERLACE ?
		HDMI_FC_INVIDCONF_R_V_BLANK_IN_OSC_ACTIVE_HIGH :
		HDMI_FC_INVIDCONF_R_V_BLANK_IN_OSC_ACTIVE_LOW;

	inv_val |= mode->flags & DRM_MODE_FLAG_INTERLACE ?
		HDMI_FC_INVIDCONF_IN_I_P_INTERLACED :
		HDMI_FC_INVIDCONF_IN_I_P_PROGRESSIVE;

	inv_val |= HDMI_FC_INVIDCONF_DVI_MODEZ_HDMI_MODE;

	hdmi_writeb(inv_val, HDMI_FC_INVIDCONF);

	vdisplay = mode->vdisplay;
	vblank = mode->vtotal - mode->vdisplay;
	v_de_vs = mode->vsync_start - mode->vdisplay;
	vsync_len = mode->vsync_end - mode->vsync_start;

	/*
	 * When we're setting an interlaced mode, we need
	 * to adjust the vertical timing to suit.
	 */
	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		vdisplay /= 2;
		vblank /= 2;
		v_de_vs /= 2;
		vsync_len /= 2;
	}

	/* Set up horizontal active pixel width */
	hdmi_writeb(mode->hdisplay >> 8, HDMI_FC_INHACTV1);
	hdmi_writeb(mode->hdisplay, HDMI_FC_INHACTV0);

	/* Set up vertical active lines */
	hdmi_writeb(vdisplay >> 8, HDMI_FC_INVACTV1);
	hdmi_writeb(vdisplay, HDMI_FC_INVACTV0);

	/* Set up horizontal blanking pixel region width */
	hblank = mode->htotal - mode->hdisplay;
	hdmi_writeb(hblank >> 8, HDMI_FC_INHBLANK1);
	hdmi_writeb(hblank, HDMI_FC_INHBLANK0);

	/* Set up vertical blanking pixel region width */
	hdmi_writeb(vblank, HDMI_FC_INVBLANK);

	/* Set up HSYNC active edge delay width (in pixel clks) */
	h_de_hs = mode->hsync_start - mode->hdisplay;
	hdmi_writeb(h_de_hs >> 8, HDMI_FC_HSYNCINDELAY1);
	hdmi_writeb(h_de_hs, HDMI_FC_HSYNCINDELAY0);

	/* Set up VSYNC active edge delay (in lines) */
	hdmi_writeb(v_de_vs, HDMI_FC_VSYNCINDELAY);

	/* Set up HSYNC active pulse width (in pixel clks) */
	hsync_len = mode->hsync_end - mode->hsync_start;
	hdmi_writeb(hsync_len >> 8, HDMI_FC_HSYNCINWIDTH1);
	hdmi_writeb(hsync_len, HDMI_FC_HSYNCINWIDTH0);

	/* Set up VSYNC active edge delay (in lines) */
	hdmi_writeb(vsync_len, HDMI_FC_VSYNCINWIDTH);
}

static void hdmi_vsync_in_width(struct dw_hdmi *hdmi,
				const struct drm_display_mode *mode)
{
	int vsync_len = mode->vsync_end - mode->vsync_start;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		vsync_len /= 2;

	/* Set up VSYNC active edge delay (in lines) */
	hdmi_writeb(vsync_len, HDMI_FC_VSYNCINWIDTH);
}

/* HDMI Initialization Step B.4 */
static void dw_hdmi_enable_video_path(struct dw_hdmi *hdmi)
{
	/* control period minimum duration */
	hdmi_writeb(12, HDMI_FC_CTRLDUR);
	hdmi_writeb(32, HDMI_FC_EXCTRLDUR);
	hdmi_writeb(1, HDMI_FC_EXCTRLSPAC);

	/* Set to fill TMDS data channels */
	hdmi_writeb(0x0B, HDMI_FC_CH0PREAM);
	hdmi_writeb(0x16, HDMI_FC_CH1PREAM);
	hdmi_writeb(0x21, HDMI_FC_CH2PREAM);

	/* Enable pixel clock and tmds data path */
	hdmi->mc_clkdis |= HDMI_MC_CLKDIS_HDCPCLK_DISABLE |
			   HDMI_MC_CLKDIS_CSCCLK_DISABLE |
			   HDMI_MC_CLKDIS_AUDCLK_DISABLE |
			   HDMI_MC_CLKDIS_PREPCLK_DISABLE |
			   HDMI_MC_CLKDIS_TMDSCLK_DISABLE;
	hdmi->mc_clkdis &= ~HDMI_MC_CLKDIS_PIXELCLK_DISABLE;
	hdmi_writeb(hdmi->mc_clkdis, HDMI_MC_CLKDIS);

	hdmi->mc_clkdis &= ~HDMI_MC_CLKDIS_TMDSCLK_DISABLE;
	hdmi_writeb(hdmi->mc_clkdis, HDMI_MC_CLKDIS);

	/* Disable color space conversion */
	hdmi_writeb(HDMI_MC_FLOWCTRL_FEED_THROUGH_OFF_CSC_BYPASS,
			    HDMI_MC_FLOWCTRL);
}

/* Workaround to clear the overflow condition */
static void dw_hdmi_clear_overflow(struct dw_hdmi *hdmi)
{
	unsigned int count;
	unsigned int i;
	u8 val;

	/*
	 * Under some circumstances the Frame Composer arithmetic unit can miss
	 * an FC register write due to being busy processing the previous one.
	 * The issue can be worked around by issuing a TMDS software reset and
	 * then write one of the FC registers several times.
	 *
	 * The number of iterations matters and depends on the HDMI TX revision
	 * (and possibly on the platform). So far only i.MX6Q (v1.30a) and
	 * i.MX6DL (v1.31a) have been identified as needing the workaround, with
	 * 4 and 1 iterations respectively.
	 * The Amlogic Meson GX SoCs (v2.01a) have been identified as needing
	 * the workaround with a single iteration.
	 */

	switch (hdmi->version) {
	case 0x130a:
		count = 4;
		break;
	case 0x131a:
	case 0x201a:
		count = 1;
		break;
	default:
		return;
	}

	/* TMDS software reset */
	hdmi_modb(0,
		  HDMI_MC_SWRSTZ_TMDSSWRST_MASK,
		  HDMI_MC_SWRSTZ);

	val = hdmi_readb(HDMI_FC_INVIDCONF);
	for (i = 0; i < count; i++)
		hdmi_writeb(val, HDMI_FC_INVIDCONF);
}

static int dw_hdmi_setup(struct dw_hdmi *hdmi, struct drm_display_mode *mode)
{
	int ret = 0;

	hdmi->hdmi_data.video_mode.mpixelrepetitionoutput = 0;
	hdmi->hdmi_data.video_mode.mpixelrepetitioninput = 0;
	hdmi->hdmi_data.hdcp_enable = 0;
	hdmi->hdmi_data.video_mode.mdataenablepolarity = true;

	/* HDMI Initialization Step B.1 */
	hdmi_av_composer(hdmi, mode);

	/* HDMI Initializateion Step B.2 */
	ret = hdmi->phy.ops->init(hdmi, hdmi->phy.data);
	if (ret)
		return ret;
	hdmi->phy.enabled = true;

	/* HDMI Initialization Step B.3 */
	dw_hdmi_enable_video_path(hdmi);

	/* Rewrite Vsync width */
	hdmi_vsync_in_width(hdmi, mode);

	/* HDMI Initialization Step F - Configure AVI InfoFrame */
	hdmi_video_packetize(hdmi);
	hdmi_video_csc(hdmi);
	hdmi_video_sample();
	hdmi_tx_hdcp_config(hdmi);

	dw_hdmi_clear_overflow(hdmi);
	return 0;
}

static void dw_hdmi_setup_i2c(struct dw_hdmi *hdmi)
{
	hdmi_writeb(HDMI_PHY_I2CM_INT_ADDR_DONE_POL,
		    HDMI_PHY_I2CM_INT_ADDR);

	hdmi_writeb(HDMI_PHY_I2CM_CTLINT_ADDR_NAC_POL |
		    HDMI_PHY_I2CM_CTLINT_ADDR_ARBITRATION_POL,
		    HDMI_PHY_I2CM_CTLINT_ADDR);
}

static void initialize_hdmi_rcar_ih_mutes(struct dw_hdmi *hdmi)
{
	u8 ih_mute;
	/*
	 * Boot up defaults are:
	 * HDMI_IH_MUTE   = 0x03 (disabled)
	 * HDMI_IH_MUTE_* = 0x00 (enabled)
	 *
	 * Disable top level interrupt bits in HDMI block
	 */
	ih_mute = hdmi_readb(HDMI_IH_MUTE) |
		  HDMI_IH_MUTE_MUTE_WAKEUP_INTERRUPT |
		  HDMI_IH_MUTE_MUTE_ALL_INTERRUPT;

	hdmi_writeb(ih_mute, HDMI_IH_MUTE);

	/* by default mask all interrupts */
	hdmi_writeb(0xff, HDMI_VP_MASK);
	hdmi_writeb(0xff, HDMI_FC_MASK0);
	hdmi_writeb(0xff, HDMI_FC_MASK1);
	hdmi_writeb(0x03, HDMI_FC_MASK2);
	hdmi_writeb(0xf3, HDMI_PHY_MASK0);
	hdmi_writeb(0x0c, HDMI_PHY_I2CM_INT_ADDR);
	hdmi_writeb(0xcc, HDMI_PHY_I2CM_CTLINT_ADDR);
	hdmi_writeb(0x0c, HDMI_AUD_INT);
	hdmi_writeb(0xdf, HDMI_A_APIINTMSK);
	hdmi_writeb(0x7f, HDMI_MISC_MASK);
	hdmi_writeb(0x44, HDMI_I2CM_INT);
	hdmi_writeb(0x44, HDMI_I2CM_CTLINT);

	/* Disable interrupts in the IH_MUTE_* registers */
	hdmi_writeb(0xdf, HDMI_IH_MUTE_FC_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_FC_STAT1);
	hdmi_writeb(0x03, HDMI_IH_MUTE_FC_STAT2);
	hdmi_writeb(0x1f, HDMI_IH_MUTE_AS_STAT0);
	hdmi_writeb(0x3f, HDMI_IH_MUTE_PHY_STAT0);
	hdmi_writeb(0x03, HDMI_IH_MUTE_I2CM_STAT0);
	hdmi_writeb(0x0f, HDMI_IH_MUTE_VP_STAT0);
	hdmi_writeb(0x03, HDMI_IH_MUTE_I2CMPHY_STAT0);

	/* Enable top level interrupt bits in HDMI block */
	ih_mute &= ~(HDMI_IH_MUTE_MUTE_WAKEUP_INTERRUPT |
		    HDMI_IH_MUTE_MUTE_ALL_INTERRUPT);
	hdmi_writeb(ih_mute, HDMI_IH_MUTE);
}

static void dw_hdmi_poweron(struct dw_hdmi *hdmi)
{
	hdmi->bridge_is_on = true;
	dw_hdmi_setup(hdmi, &hdmi->previous_mode);
}

static void dw_hdmi_poweroff(struct dw_hdmi *hdmi)
{
	if (hdmi->phy.enabled) {
		hdmi->phy.ops->disable(hdmi, hdmi->phy.data);
		hdmi->phy.enabled = false;
	}
	hdmi->bridge_is_on = false;
}

static void dw_hdmi_update_power(struct dw_hdmi *hdmi)
{
	int force = hdmi->force;

	if (hdmi->disabled) {
		force = DRM_FORCE_OFF;
	} else if (force == DRM_FORCE_UNSPECIFIED) {
		if (hdmi->rxsense)
			force = DRM_FORCE_ON;
		else
			force = DRM_FORCE_OFF;
	}

	if (force == DRM_FORCE_OFF) {
		if (hdmi->bridge_is_on)
			dw_hdmi_poweroff(hdmi);
	} else {
		if (!hdmi->bridge_is_on)
			dw_hdmi_poweron(hdmi);
	}
}

static void dw_hdmi_update_phy_mask(struct dw_hdmi *hdmi)
{
	if (hdmi->phy.ops->update_hpd)
		hdmi->phy.ops->update_hpd(hdmi, hdmi->phy.data,
					  hdmi->force, hdmi->disabled,
					  hdmi->rxsense);
}

int dw_hdmi_connector_detect(void)
{
	struct dw_hdmi *hdmi = G_HDMI;

	hdmi->force = DRM_FORCE_UNSPECIFIED;
	dw_hdmi_update_power(hdmi);
	dw_hdmi_update_phy_mask(hdmi);

	return hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data);
}

void dw_hdmi_bridge_disable(void)
{
	struct dw_hdmi *hdmi = G_HDMI;
	hdmi->disabled = true;
	dw_hdmi_update_power(hdmi);
	dw_hdmi_update_phy_mask(hdmi);
}

void dw_hdmi_bridge_enable(void)
{
	struct dw_hdmi *hdmi = G_HDMI;
	hdmi->disabled = false;
	dw_hdmi_update_power(hdmi);
	dw_hdmi_update_phy_mask(hdmi);
}

static const struct dw_hdmi_phy_data dw_hdmi_phys[] = {
	{
		.type = DW_HDMI_PHY_DWC_HDMI_TX_PHY,
		.name = "DWC HDMI TX PHY",
		.gen = 1,
	}, {
		.type = DW_HDMI_PHY_DWC_MHL_PHY_HEAC,
		.name = "DWC MHL PHY + HEAC PHY",
		.gen = 2,
		.has_svsret = true,
	}, {
		.type = DW_HDMI_PHY_DWC_MHL_PHY,
		.name = "DWC MHL PHY",
		.gen = 2,
		.has_svsret = true,
	}, {
		.type = DW_HDMI_PHY_DWC_HDMI_3D_TX_PHY_HEAC,
		.name = "DWC HDMI 3D TX PHY + HEAC PHY",
		.gen = 2,
	}, {
		.type = DW_HDMI_PHY_DWC_HDMI_3D_TX_PHY,
		.name = "DWC HDMI 3D TX PHY",
		.gen = 2,
	}, {
		.type = DW_HDMI_PHY_DWC_HDMI20_TX_PHY,
		.name = "DWC HDMI 2.0 TX PHY",
		.gen = 2,
		.has_svsret = true,
	}, {
		.type = DW_HDMI_PHY_VENDOR_PHY,
		.name = "Vendor PHY",
	}
};

static int dw_hdmi_detect_phy(struct dw_hdmi *hdmi)
{
	unsigned int i;
	u8 phy_type;

	phy_type = hdmi_readb(HDMI_CONFIG2_ID);//DW_HDMI_PHY_DWC_HDMI20_TX_PHY

	/* Synopsys PHYs are handled internally. */
	for (i = 0; i < ARRAY_SIZE(dw_hdmi_phys); ++i) {
		if (dw_hdmi_phys[i].type == phy_type) {
			hdmi->phy.ops = &dw_hdmi_synopsys_phy_ops;
			hdmi->phy.name = dw_hdmi_phys[i].name;
			hdmi->phy.data = (void *)&dw_hdmi_phys[i];
			return 0;
		}
	}
	return -ENODEV;
}

int dw_hdmi_probe(void)
{
	struct dw_hdmi *hdmi;
	int ret = 0;
	u8 prod_id0 = 0;
	u8 prod_id1 = 0;

	hdmi = kzalloc(sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->disabled = true;
	hdmi->rxsense = true;
	hdmi->phy_mask = (u8)~(BIT(3) | BIT(2) | HDMI_PHY_HPD |
				 HDMI_PHY_RX_SENSE);
	hdmi->mc_clkdis = 0x5f;
	hdmi->previous_mode.clock=74250;
	hdmi->previous_mode.crtc_clock=74250;
	hdmi->previous_mode.crtc_hblank_end=1650;
	hdmi->previous_mode.crtc_hblank_start=CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH;
	hdmi->previous_mode.crtc_hdisplay=CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH;
	hdmi->previous_mode.crtc_hskew=0;
	hdmi->previous_mode.crtc_hsync_end=1472;
	hdmi->previous_mode.crtc_hsync_start=1336;
	hdmi->previous_mode.crtc_htotal=1650;
	hdmi->previous_mode.crtc_vblank_end=750;
	hdmi->previous_mode.crtc_hblank_start=CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH;
	hdmi->previous_mode.crtc_vdisplay=CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT;
	hdmi->previous_mode.crtc_vsync_end=726;
	hdmi->previous_mode.crtc_vsync_start=721;
	hdmi->previous_mode.crtc_vtotal=750;
	hdmi->previous_mode.flags=5;
	hdmi->previous_mode.hdisplay=CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH;
	hdmi->previous_mode.height_mm=0;
	hdmi->previous_mode.hskew=0;
	hdmi->previous_mode.hsync=0;
	hdmi->previous_mode.hsync_end=1472;
	hdmi->previous_mode.hsync_start=1336;
	hdmi->previous_mode.htotal=1650;
	hdmi->previous_mode.type=64;
	hdmi->previous_mode.vdisplay=CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT;
	hdmi->previous_mode.vrefresh=60;
	hdmi->previous_mode.vscan=0;
	hdmi->previous_mode.vsync_end=726;
	hdmi->previous_mode.vsync_start=721;
	hdmi->previous_mode.vsync_start=750;
	hdmi->previous_mode.width_mm=0;

	/* Product and revision IDs */
	hdmi->version = (hdmi_readb(HDMI_DESIGN_ID) << 8)
		      | (hdmi_readb(HDMI_REVISION_ID) << 0);
	prod_id0 = hdmi_readb(HDMI_PRODUCT_ID0);
	prod_id1 = hdmi_readb(HDMI_PRODUCT_ID1);

	if (prod_id0 != HDMI_PRODUCT_ID0_HDMI_TX ||
	    (prod_id1 & ~HDMI_PRODUCT_ID1_HDCP) != HDMI_PRODUCT_ID1_HDMI_TX) {
		printf("%s: Unsupported HDMI controller (%04x:%02x:%02x)\n",
			__func__, hdmi->version, prod_id0, prod_id1);
		ret = -ENODEV;
		return ret;
	}

	ret = dw_hdmi_detect_phy(hdmi);
	if (ret < 0)
		return ret;

	debug("Detected HDMI TX controller v%x.%03x %s HDCP (%s)\n",
		 hdmi->version >> 12, hdmi->version & 0xfff,
		 prod_id1 & HDMI_PRODUCT_ID1_HDCP ? "with" : "without",
		 hdmi->phy.name);

	initialize_hdmi_rcar_ih_mutes(hdmi);

	dw_hdmi_setup_i2c(hdmi);

	if (hdmi->phy.ops->setup_hpd)
		hdmi->phy.ops->setup_hpd(hdmi, hdmi->phy.data);

	G_HDMI = hdmi;
	return 0;
}

void dw_hdmi_remove(void)
{
	/* Disable all interrupts */
	hdmi_writeb(0x3f, HDMI_IH_MUTE_PHY_STAT0);
}
