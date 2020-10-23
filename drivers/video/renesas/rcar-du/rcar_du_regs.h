/*
 * rcar_du_regs.h  --  R-Car Display Unit Registers Definitions
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2015 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#ifndef __RCAR_DU_REGS_H__
#define __RCAR_DU_REGS_H__

#define RCAR_DU_BASE_REG				0xFEB00000

/* CRTC offsets */
#define DU0_REG_OFFSET					0x00000
#define DU1_REG_OFFSET					0x30000
#define DU2_REG_OFFSET					0x40000
#define DU3_REG_OFFSET					0x70000

#define DU_GROUP0_REG_OFFSET			0x00000 //bits shared by the DU0 and DU1
#define DU_GROUP1_REG_OFFSET			0x40000 //bits shared by the DU2 and DU3

/* -----------------------------------------------------------------------------
 * Display Control Registers
 */
/* Display Unit System Control Register Configuration */
#define DSYSR				0x00000
#define DSYSR_DRES			(1 << 9) //Display Reset
#define DSYSR_DEN			(1 << 8) //Display Enable
#define DSYSR_TVM_MASTER	(0 << 6) //TV Synchronized Mode
#define DSYSR_TVM_SWITCH	(1 << 6)
#define DSYSR_TVM_MASK		(3 << 6)
#define DSYSR_SCM_INT_NONE	(0 << 4) //Scan Mode
#define DSYSR_SCM_MASK		(3 << 4)

/* Display unit mode register */
#define DSMR				0x00004
#define DSMR_DIPM_DISP		(0 << 25)
#define DSMR_CSPM			(1 << 24)
#define DSMR_VSL			(1 << 18)
#define DSMR_HSL			(1 << 17)
#define DSMR_ODEV			(1 << 8)

/* DU status register, R-only */
#define DSSR				0x00008
#define DSSR_VBK			(1 << 11) //Vertical Blanking Flag

/* DU status register clear register */
#define DSRCR				0x0000c
#define DSRCR_VBCL			(1 << 11) //Vertical blanking flag clear
#define DSRCR_MASK			0x0000cbff

/* DU interrupt enable register */
#define DIER				0x00010
#define DIER_VBE			(1 << 11) //Vertical blanking flag interrupt enable

/* Display Unit Extensional Function Control Register */
#define DEFR				0x00020
#define DEFR_CODE			(0x7773 << 16)
#define DEFR_DEFE			(1 << 0) //Enable function

#define DEFR5				0x000e0
#define DEFR5_CODE			(0x66 << 24)
#define DEFR5_DEFE5			(1 << 0) //Enable function

#define DEFR6				0x000e8
#define DEFR6_CODE			(0x7778 << 16)
#define DEFR6_ODPM12_DISP	(2 << 10)
#define DEFR6_ODPM02_DISP	(2 << 8)

#define DEFR10				0x20038
#define DEFR10_CODE			(0x7795 << 16)
#define DEFR10_DEFE10		(1 << 0)

/* Display Unit Input Dot Clock Select Register */
#define DIDSR				0x20028
#define DIDSR_CODE			(0x7790 << 16)

/* Display Unit PLL Control Register */
#define DPLLCR				0x20044
#define DPLLCR_CODE			(0x95 << 24)
#define DPLLCR_PLCS1		(1 << 23)
#define DPLLCR_PLCS0		(1 << 21)
#define DPLLCR_PLCS0_H3ES1X_WA	(1 << 20)
#define DPLLCR_CLKE			(1 << 18)
#define DPLLCR_FDPLL(n)		((n) << 12)
#define DPLLCR_N(n)			((n) << 5)
#define DPLLCR_M(n)			((n) << 3)
#define DPLLCR_STBY			(1 << 2)
#define DPLLCR_INCS_DOTCLKIN0	(0 << 0)
#define DPLLCR_INCS_DOTCLKIN1	(1 << 1)

/* -----------------------------------------------------------------------------
 * Display Timing Generation Registers
 */
#define HDSR				0x00040 //Horizontal Display Start
#define HDER				0x00044 //Horizontal Display End
#define VDSR				0x00048 //Vertical Display Start
#define VDER				0x0004c //Vertical Display End
#define HCR					0x00050 //Horizontal Cycle
#define HSWR				0x00054 //Horizontal Sync Width
#define VCR					0x00058 //Vertical Cycle
#define VSPR				0x0005c //Vertical Sync Point
#define DESR				0x00078 //DE Signal Start
#define DEWR				0x0007c //DE Signal Width

/* -----------------------------------------------------------------------------
 * Display Attribute Registers
 */
#define DOOR				0x00090 //Display Off Mode Output
#define DOOR_RGB(r, g, b)	(((r) << 18) | ((g) << 10) | ((b) << 2))
#define BPOR				0x00098 //Background Plane Output
#define BPOR_RGB(r, g, b)	(((r) << 18) | ((g) << 10) | ((b) << 2))

/* -----------------------------------------------------------------------------
 * Display Plane Registers
 */
#define PLANE1_OFFSET		0x00000
#define PLANE3_OFFSET		0x00200
/* Plane p Mode Register m */
#define PnMR				0x00100
#define PnMR_SPIM_TP		(0 << 12) /* Transparent Color */
#define PnMR_SPIM_ALP		(1 << 12) /* Alpha Blending */
#define PnMR_SPIM_EOR		(2 << 12) /* EOR */
#define PnMR_SPIM_TP_OFF	(1 << 14) /* No Transparent Color */
#define PnMR_DDDF_8BPP		(0 << 0) /* 8bit */ //Display Data Format
#define PnMR_DDDF_16BPP		(1 << 0) /* 16bit or 32bit */
#define PnMR_DDDF_ARGB		(2 << 0) /* ARGB */
#define PnMR_DDDF_YC		(3 << 0) /* YC */
#define PnMR_DDDF_MASK		(3 << 0)

#define PnDSXR				0x00110 //Plane p Display Size X Register m
#define PnDSYR				0x00114 //Plane p Display Size Y Register m
#define PnDPXR				0x00118 //Plane p Display Position X Register m
#define PnDPYR				0x0011c //Plane p Display Position Y Register m

/* Plane p Display Data Control 4 Register */
#define PnDDCR4				0x00190
#define PnDDCR4_CODE		(0x7766 << 16)
#define PnDDCR4_SDFS_RGB	(0 << 4)
#define PnDDCR4_SDFS_YC		(5 << 4)
#define PnDDCR4_SDFS_MASK	(7 << 4)
#define PnDDCR4_EDF_NONE	(0 << 0)
#define PnDDCR4_EDF_ARGB8888	(1 << 0)
#define PnDDCR4_EDF_RGB888	(2 << 0)
#define PnDDCR4_EDF_RGB666	(3 << 0)
#define PnDDCR4_EDF_MASK	(7 << 0)

/* -----------------------------------------------------------------------------
 * External Synchronization Control Registers
 */
#define ESCR				0x31000
#define ESCR_DCLKSEL_DCLKIN	(0 << 20)
#define ESCR_DCLKSEL_CLKS	(1 << 20)
#define ESCR_FRQSEL_MASK	(0x3f << 0)

/* Output Signal Timing Adjustment Register */
#define OTAR				0x31004

/* -----------------------------------------------------------------------------
 * Dual Display Output Control Registers
 */
/* Display Unit Output Route Control Register */
#define DORCR				0x11000
#define DORCR_PG2T			(1 << 30)
#define DORCR_DK2S			(1 << 28)
#define DORCR_PG2D_DS1		(0 << 24)
#define DORCR_PG2D_DS2		(1 << 24)
#define DORCR_PG2D_FIX0		(2 << 24)
#define DORCR_PG2D_DOOR		(3 << 24)
#define DORCR_PG2D_MASK		(3 << 24)
#define DORCR_DR1D			(1 << 21)
#define DORCR_PG1D_DS1		(0 << 16)
#define DORCR_PG1D_DS2		(1 << 16)
#define DORCR_PG1D_FIX0		(2 << 16)
#define DORCR_PG1D_DOOR		(3 << 16)
#define DORCR_PG1D_MASK		(3 << 16)
#define DORCR_RGPV			(1 << 4)
#define DORCR_DPRS			(1 << 0)

/* Display Unit Plane Timing Select Register */
#define DPTSR				0x11004
#define DPTSR_PnDK(n)		(1 << ((n) + 16))
#define DPTSR_PnTS(n)		(1 << (n))

/* Display Superimpose Priority Register */
#define DS1PR0				0x11024

#endif /* __RCAR_DU_REGS_H__ */
