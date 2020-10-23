/*
 * rcar-clk.c  --  CPG and MSSR Driver for R-Car video support
 *
 * Copyright (C) 2020 GlobalLogic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <common.h>
#include <clk.h>
#include <time.h>
#include <wait_bit.h>
#include <rcar-logo.h>

#define RCAR_CPG_MSSR_BASE_REG		0xe6150000

/* HDMI-IF clock frequency control register */
#define HDMICKCR					0xE6150250
/* Write protect registers */
#define CPGWPR						0xE6150900
#define CPGWPCR						0xE6150904

/* Module Clocks IDs */
#define HDMI0_MOD_CLK_ID			729
#define FCP1_MOD_CLK_ID				602
#define VSP1_MOD_CLK_ID				622
#define DU1_MOD_CLK_ID				723

/* HDMI-IF clock stop bit */
#define CPG_CKSTP_BIT				BIT(8)

/*
 * System Module Stop Control Register offsets
 */
static const u16 smstpcr[] = {
	0x130, 0x134, 0x138, 0x13C, 0x140, 0x144, 0x148, 0x14C,
	0x990, 0x994, 0x998, 0x99C,
};
#define	SMSTPCR(i)	smstpcr[i]

/*
 * Module Stop Status Register offsets
 */
static const u16 mstpsr[] = {
	0x030, 0x038, 0x040, 0x048, 0x04C, 0x03C, 0x1C0, 0x1C4,
	0x9A0, 0x9A4, 0x9A8, 0x9AC,
};
#define	MSTPSR(i)	mstpsr[i]

static int renesas_clk_endisable(unsigned long id,
		void __iomem *base, bool enable)
{
	const unsigned long clkid = id & 0xffff;
	const unsigned int reg = clkid / 100;
	const unsigned int bit = clkid % 100;
	const u32 bitmask = BIT(bit);

	if (enable) {
		clrbits_le32(base + SMSTPCR(reg), bitmask);
		return wait_for_bit_le32(base + MSTPSR(reg),
				    bitmask, 0, 100, 0);
	} else {
		setbits_le32(base + SMSTPCR(reg), bitmask);
		return 0;
	}
}

/* Enable clocks for HDMI, FCP, VSP, DU modules */
int rcar_clks_enable(void)
{
	int ret = 0;

	/* Unlock CPG access */
	writel(0xA5A5FFFF, CPGWPR);
	writel(0x5A5A0000, CPGWPCR);

	/* Supply clock */
	u32 hdmi_clk_reg = readl(HDMICKCR);
	hdmi_clk_reg &= ~CPG_CKSTP_BIT;
	writel(hdmi_clk_reg, HDMICKCR);

	/* DEF_MOD("hdmi0", 729,	R8A7795_CLK_HDMI) */
	ret = renesas_clk_endisable(HDMI0_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), true);
	if (ret) {
		printf("Can't enable HDMI0 clk: %d\n", ret);
		return -1;
	}

	/* DEF_MOD("fcpvd1", 602,	R8A7795_CLK_S0D2) */
	ret = renesas_clk_endisable(FCP1_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), true);
	if (ret) {
		printf("Can't enable FCP1 clk: %d\n", ret);
		return -1;
	}

	/* DEF_MOD("vspd1", 622,	R8A7795_CLK_S0D2), 400 MHz */
	ret = renesas_clk_endisable(VSP1_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), true);
	if (ret) {
		printf("Can't enable VSPD1 clk: %d\n", ret);
		return -1;
	}

	/* DEF_MOD("du1", 723,	R8A7795_CLK_S2D1) */
	ret = renesas_clk_endisable(DU1_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), true);
	if (ret) {
		printf("Can't enable DU1 clk: %d\n", ret);
		return -1;
	}

	return 0;
}

/* Disable clocks for HDMI, FCP, VSP, DU */
void rcar_clks_disable(void)
{
	/* Unlock CPG access */
	writel(0xA5A5FFFF, CPGWPR);
	writel(0x5A5A0000, CPGWPCR);

	/* Stop clock */
	u32 hdmi_clk_reg = readl(HDMICKCR);
	hdmi_clk_reg |= CPG_CKSTP_BIT;
	writel(hdmi_clk_reg, HDMICKCR);

	renesas_clk_endisable(HDMI0_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), false);
	renesas_clk_endisable(FCP1_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), false);
	renesas_clk_endisable(VSP1_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), false);
	renesas_clk_endisable(DU1_MOD_CLK_ID,
			(void*)(RCAR_CPG_MSSR_BASE_REG), false);
}
