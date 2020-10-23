/*
 * rcar_du_drv.c  --  R-Car Display Unit driver
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <common.h>
#include <linux/io.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <rcar-logo.h>
#include "rcar_du_regs.h"
#include "../vsp2/vsp1.h"
#include "../vsp2/vsp1_drm.h"

static inline u32 rcar_du_read(u32 reg)
{
	return ioread32((void __iomem *)(unsigned long)(RCAR_DU_BASE_REG + reg));
}

static inline void rcar_du_write(u32 reg, u32 data)
{
	iowrite32(data, (void __iomem *)(unsigned long)(RCAR_DU_BASE_REG + reg));
}

static u32 rcar_du_crtc_read(u32 reg)
{
	return rcar_du_read(DU1_REG_OFFSET + reg);
}

static void rcar_du_crtc_write(u32 reg, u32 data)
{
	rcar_du_write(DU1_REG_OFFSET + reg, data);
}

static void rcar_du_crtc_clr(u32 reg, u32 clr)
{
	rcar_du_write(DU1_REG_OFFSET + reg,
		      rcar_du_read(DU1_REG_OFFSET + reg) & ~clr);
}

static void rcar_du_crtc_set(u32 reg, u32 set)
{
	rcar_du_write(DU1_REG_OFFSET + reg,
		      rcar_du_read(DU1_REG_OFFSET + reg) | set);
}

static void rcar_du_crtc_clr_set(u32 reg, u32 clr, u32 set)
{
	u32 value = rcar_du_read(DU1_REG_OFFSET + reg);
	rcar_du_write(DU1_REG_OFFSET + reg, (value & ~clr) | set);
}

static void rcar_du_plane_write(u32 reg, u32 data)
{
	rcar_du_write(PLANE3_OFFSET + reg, data);
}

static u32 rcar_du_group_read(u32 reg)
{
	return rcar_du_read(DU_GROUP0_REG_OFFSET + reg);
}

static void rcar_du_group_write(u32 reg, u32 data)
{
	rcar_du_write(DU_GROUP0_REG_OFFSET + reg, data);
}

static void rcar_du_group_setup_pins(void)
{
	u32 defr6 = DEFR6_CODE;
	defr6 |= DEFR6_ODPM02_DISP;
	defr6 |= DEFR6_ODPM12_DISP;
	rcar_du_group_write(DEFR6, defr6);//DEFR6=77780A00
}

static void rcar_du_group_setup_didsr(void)
{
	u32 didsr = DIDSR_CODE | 0;
	/* DU1 input dot clock source is the DU_DOTCLKIN1 pin */
	rcar_du_group_write(DIDSR, didsr); //DIDSR=77900000
}

static void rcar_du_group_setup(void)
{
	/* Enable extended features */
	rcar_du_group_write(DEFR, DEFR_CODE | DEFR_DEFE);
	rcar_du_group_write(DEFR5, DEFR5_CODE | DEFR5_DEFE5);
	rcar_du_group_setup_pins();
	rcar_du_group_setup_didsr();
	/* The VSP1 data format is RGB */
	rcar_du_group_write(DEFR10, DEFR10_CODE | DEFR10_DEFE10);
	/*
	 * Use DS1PR and DS2PR to configure planes priorities and connects the
	 * superposition 0 to DU0 pins. DU1 pins will be configured dynamically.
	 */
	rcar_du_group_write(DORCR, DORCR_PG1D_DS1 | DORCR_DPRS);
	/* Apply planes to CRTCs association (for Plane 3)*/
	rcar_du_group_write(DPTSR, (0x04 << 16) | 0x04);//DPTSR=40004
}

static void rcar_du_group_set_routing(void)
{
	u32 dorcr = rcar_du_group_read(DORCR);
	dorcr &= ~(DORCR_PG2T | DORCR_DK2S | DORCR_PG2D_MASK);
	dorcr |= DORCR_PG2T | DORCR_DK2S | DORCR_PG2D_DS2 | DORCR_DPRS;
	rcar_du_group_write(DORCR, dorcr);//DORCR=51000001
}

static void rcar_du_group_start_stop(bool start)
{
	rcar_du_group_write(DSYSR,
		(rcar_du_group_read(DSYSR) & ~(DSYSR_DRES | DSYSR_DEN)) |
		(start ? DSYSR_DEN : DSYSR_DRES));
}

/* -----------------------------------------------------------------------------
 * Hardware Setup
 */

struct dpll_info {
	unsigned int output;
	unsigned int fdpll;
	unsigned int n;
	unsigned int m;
};

static void rcar_du_dpll_divider(struct dpll_info *dpll,
				 unsigned long input,
				 unsigned long target)
{
	unsigned long best_diff = (unsigned long)-1;
	unsigned long diff;
	unsigned int fdpll;
	unsigned int m;
	unsigned int n;
	bool clk_high = false;

	if (target > 148500000)
		clk_high = true;

	/*
	 *   fin                                 fvco        fout       fclkout
	 * in --> [1/M] --> |PD| -> [LPF] -> [VCO] -> [1/P] -+-> [1/FDPLL] -> out
	 *              +-> |  |                             |
	 *              |                                    |
	 *              +---------------- [1/N] <------------+
	 *
	 *	fclkout = fvco / P / FDPLL -- (1)
	 *
	 * fin/M = fvco/P/N
	 *
	 *	fvco = fin * P *  N / M -- (2)
	 *
	 * (1) + (2) indicates
	 *
	 *	fclkout = fin * N / M / FDPLL
	 *
	 * NOTES
	 *	N	: (n + 1)
	 *	M	: (m + 1)
	 *	FDPLL	: (fdpll + 1)
	 *	P	: 2
	 *	2kHz < fvco < 4096MHz
	 *
	 * To minimize the jitter,
	 * N : as large as possible
	 * M : as small as possible
	 */
	for (m = 0; m < 4; m++) {
		for (n = 119; n > 38; n--) {
			/*
			 * This code only runs on 64-bit architectures, the
			 * unsigned long type can thus be used for 64-bit
			 * computation. It will still compile without any
			 * warning on 32-bit architectures.
			 *
			 * To optimize calculations, use fout instead of fvco
			 * to verify the VCO frequency constraint.
			 */
			unsigned long fout = input * (n + 1) / (m + 1);

			if (fout < 1000 || fout > 2048 * 1000 * 1000U)
				continue;

			for (fdpll = 1; fdpll < 32; fdpll++) {
				unsigned long output;
				unsigned long wa_div;

				wa_div = 1;

				output = fout / (fdpll + 1) / wa_div;
				if (output >= 400 * 1000 * 1000)
					continue;

				if (clk_high && output < target)
					continue;

				diff = abs((long)output - (long)target);
				if (best_diff > diff) {
					best_diff = diff;
					dpll->n = n;
					dpll->m = m;
					dpll->fdpll = fdpll;
					dpll->output = output;
				}

				if (diff == 0)
					return;
			}
		}
	}
}

static void rcar_du_crtc_set_display_timing(void)
{
	unsigned long mode_clock = 74250 * 1000;
	unsigned long clk;
	u32 value;
	u32 escr;
	u32 div;

	/*
	 * Compute the clock divisor and select the internal or external dot
	 * clock based on the requested frequency.
	 */
	clk = 399999984;//~400 MHz
	div = DIV_ROUND_CLOSEST(clk, mode_clock);
	div = clamp(div, 1U, 64U) - 1;
	escr = div | ESCR_DCLKSEL_CLKS;

	/* For extclock */
	struct dpll_info dpll = { 0 };
	unsigned long extclk = 33000000;
	unsigned long extrate;
	unsigned long rate;
	u32 extdiv;
	unsigned long target = mode_clock;

	rcar_du_dpll_divider(&dpll, extclk, target);
	extclk = dpll.output;

	extdiv = DIV_ROUND_CLOSEST(extclk, mode_clock);
	extdiv = clamp(extdiv, 1U, 64U) - 1;

	rate = clk / (div + 1);
	extrate = extclk / (extdiv + 1);

	if (abs((long)extrate - (long)mode_clock) <
		abs((long)rate - (long)mode_clock)) {

		u32 dpllcr = DPLLCR_CODE | DPLLCR_CLKE
			   | DPLLCR_FDPLL(dpll.fdpll)
			   | DPLLCR_N(dpll.n) | DPLLCR_M(dpll.m)
			   | DPLLCR_STBY;

			dpllcr |= DPLLCR_PLCS1
				   |  DPLLCR_INCS_DOTCLKIN1;

		/* The DU1 input dot clock source is the DPLL0.
		 * The DPLL0/DPLL1 clock is output
		 */
		rcar_du_group_write(DPLLCR, dpllcr); //DPLLCR=958576A6
	}
	/* The input dot clock source is the DCLKIN pin */
	escr = ESCR_DCLKSEL_DCLKIN | extdiv;

	rcar_du_group_write(ESCR, escr); //ESCR=0
	rcar_du_group_write(OTAR, 0);

	/* Signal polarities */
	value = DSMR_VSL | DSMR_HSL | DSMR_DIPM_DISP | DSMR_CSPM;
	rcar_du_crtc_write(DSMR, value);//DSMR=1060000

	/* Display timings */
	/*
	 * It is prohibited to set a value less than 1 to VDSR and HDSR
	 * register by the H/W specification.
	 */
	const int htotal = 1650;
	const int hsync_start = 1336;
	const int hdisplay = CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH;
	const int hsync_end = 1472;
	const int crtc_vtotal = 750;
	const int crtc_vsync_end = 726;
	const int crtc_vdisplay = CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT;
	const int crtc_vsync_start = 721;

	rcar_du_crtc_write(HDSR, htotal - hsync_start - 19);
	rcar_du_crtc_write(HDER, htotal - hsync_start + hdisplay - 19);
	rcar_du_crtc_write(HSWR, hsync_end - hsync_start - 1);
	rcar_du_crtc_write(HCR,  htotal - 1);
	rcar_du_crtc_write(VDSR, crtc_vtotal - crtc_vsync_end - 2);
	rcar_du_crtc_write(VDER, crtc_vtotal - crtc_vsync_end + crtc_vdisplay - 2);
	rcar_du_crtc_write(VSPR, crtc_vtotal - crtc_vsync_end + crtc_vsync_start - 1);
	rcar_du_crtc_write(VCR, crtc_vtotal - 1);
	rcar_du_crtc_write(DESR, htotal - hsync_start - 1);
	rcar_du_crtc_write(DEWR, hdisplay);
}

static void rcar_du_crtc_enable_vblank(void)
{
	rcar_du_crtc_write(DSRCR, DSRCR_VBCL);
	rcar_du_crtc_set(DIER, DIER_VBE);
}

static void rcar_du_crtc_disable_vblank(void)
{
	rcar_du_crtc_clr(DIER, DIER_VBE);
}

static void rcar_du_crtc_update_planes(void)
{
	/* Restart the group */
	rcar_du_group_start_stop(false);
	rcar_du_group_start_stop(true);
	/* If VSP+DU integration is enabled the plane assignment is fixed.
	 * Priority level 2 is not used.Plane 3 has priority level 1 in display
	 * generation by superimposition processor 1.
	 */
	rcar_du_group_write(DS1PR0, 0x03);
}

static int rcar_du_crtc_setup(void)
{
	/* Disable all interrupts */
	rcar_du_crtc_write(DIER, 0);

	/* Set display off mode and background plane output to black */
	rcar_du_crtc_write(DOOR, DOOR_RGB(0, 0, 0));
	rcar_du_crtc_write(BPOR, BPOR_RGB(0, 0, 0));

	/* Configure display timings and output routing */
	rcar_du_crtc_set_display_timing();

	rcar_du_group_set_routing();

	/* Start with all planes disabled. */
	rcar_du_group_write(DS1PR0, 0);

	/* R-Car Display Unit Planes Setup */
	/* Setup format */
	rcar_du_plane_write(PnMR, PnMR_SPIM_TP_OFF | PnMR_DDDF_16BPP);
	rcar_du_plane_write(PnDDCR4,
		PnDDCR4_CODE | PnDDCR4_SDFS_RGB | PnDDCR4_EDF_ARGB8888);
	/* Destination position and size */
	rcar_du_plane_write(PnDSXR, CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH);
	rcar_du_plane_write(PnDSYR, CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT);
	rcar_du_plane_write(PnDPXR, 0);
	rcar_du_plane_write(PnDPYR, 0);

	/* Enable the VSP compositor. */
	return vsp1_du_setup_lif(true);
}

static void rcar_du_crtc_start(void)
{
	/*
	 * Select master sync mode. This enables display operation in master
	 * sync mode (with the HSYNC, VSYNC and CSYNC signals configured as outputs
	 * and actively driven). Non-interlaced mode.
	 */
	rcar_du_crtc_clr_set(DSYSR,
		DSYSR_TVM_MASK | DSYSR_SCM_MASK, DSYSR_TVM_MASTER | DSYSR_SCM_INT_NONE);

	rcar_du_group_start_stop(true);
}

static void rcar_du_crtc_disable_planes(void)
{
	u32 status = 0;
	u32 vblank_count = 0;
	u32 timeout = 0;

	/* Disable planes and calculate how many vertical blanking interrupts we
	 * have to wait for */
	rcar_du_group_write(DS1PR0, 0);
	status = rcar_du_crtc_read(DSSR);
	vblank_count = status & DSSR_VBK ? 2 : 1;

	for (timeout = 50; timeout > 0; --timeout) {

		status = rcar_du_crtc_read(DSSR);
		rcar_du_crtc_write(DSRCR, status & DSRCR_MASK);

		if (status & DSSR_VBK) {
			if (--vblank_count == 0)
				break;
		}
		udelay(2000);
	}

	if (!timeout) {
		printf("%s: Failed to disable planes\n", __func__);
	}
}

int rcar_du_probe(void)
{
	int ret = 0;
	rcar_du_group_setup();

	ret = rcar_du_crtc_setup();
	if (ret)
		return ret;

	rcar_du_group_write(DSYSR,
		(rcar_du_group_read(DSYSR) & ~(DSYSR_DRES | DSYSR_DEN)) | DSYSR_DEN);
	rcar_du_group_write(DSYSR,
		(rcar_du_group_read(DSYSR) & ~(DSYSR_DRES | DSYSR_DEN)) | DSYSR_DRES);
	return 0;
}

int rcar_du_start(void)
{
	int ret;

	rcar_du_crtc_update_planes();

	ret = vsp1_du_atomic_flush();
	if (ret)
		return ret;

	rcar_du_crtc_start();
	return 0;
}

void rcar_du_stop(void)
{
	/* Enable vblank */
	rcar_du_crtc_enable_vblank();

	/*
	 * Disable all planes and wait for the change to take effect.
	 */
	rcar_du_crtc_disable_planes();

	/* Disable vblank */
	rcar_du_crtc_disable_vblank();

	/* Disable the VSP compositor. */
	vsp1_du_setup_lif(false);

	/*
	 * Select switch sync mode. This stops display operation and configures
	 * the HSYNC and VSYNC signals as inputs.
	 */
	rcar_du_crtc_clr_set(DSYSR, DSYSR_TVM_MASK, DSYSR_TVM_SWITCH);

	rcar_du_group_start_stop(false);
}

