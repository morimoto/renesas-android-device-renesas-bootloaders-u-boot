/* SPDX-License-Identifier: GPL-2.0 */
/*
 * vsp1_regs.h  --  R-Car VSP1 Registers Definitions
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#ifndef __VSP1_REGS_H__
#define __VSP1_REGS_H__

#define RCAR_VSPD1_BASE_REG			0xFEA28000

/* -----------------------------------------------------------------------------
 * General Control Registers
 */
/* VSP2 Start Registers */
#define VI6_CMD(n)					(0x0000 + (n) * 4)
#define VI6_CMD_UPDHDR				(1 << 4)
#define VI6_CMD_STRCMD				(1 << 0)

/* Dynamic Clock Stop Control Register */
#define VI6_CLK_DCSWT			0x0018
#define VI6_CLK_DCSWT_CSTPW_SHIFT	8
#define VI6_CLK_DCSWT_CSTRW_SHIFT	0

/* WPFn Software Reset Register */
#define VI6_SRESET					0x0028
#define VI6_SRESET_SRTS(n)			(1 << (n))

/* Operating Status Register */
#define VI6_STATUS					0x0038
#define VI6_STATUS_FLD_STD(n)		(1 << ((n) + 28)) //Top or bottom field
#define VI6_STATUS_SYS_ACT(n)		(1 << ((n) + 8)) //WPFn operating status

/* WPFn Interrupt Enable Registers */
#define VI6_WPF_IRQ_ENB(n)			(0x0048 + (n) * 12)
#define VI6_WFP_IRQ_ENB_UNDE		(1 << 16) //Underrun in case of DU connection
#define VI6_WFP_IRQ_ENB_DFEE		(1 << 1)  //Display List Frame End
#define VI6_WFP_IRQ_ENB_FREE		(1 << 0)  //Frame End

/* WPFn Interrupt Status Registers */
#define VI6_WPF_IRQ_STA(n)			(0x004c + (n) * 12)
#define VI6_WFP_IRQ_STA_UND			(1 << 16)
#define VI6_WFP_IRQ_STA_DFE			(1 << 1)
#define VI6_WFP_IRQ_STA_FRE			(1 << 0)

/* Display Interrupt Enable Register */
#define VI6_DISP_IRQ_ENB(n)			(0x0078 + (n) * 60)
#define VI6_DISP_IRQ_ENB_DSTE		(1 << 8)
#define VI6_DISP_IRQ_ENB_MAEE		(1 << 5)
#define VI6_DISP_IRQ_ENB_LNEE(n)	(1 << (n))

/* Display Interrupt Status Register */
#define VI6_DISP_IRQ_STA(n)			(0x007c + (n) * 60)
#define VI6_DISP_IRQ_STA_DST		(1 << 8)
#define VI6_DISP_IRQ_STA_MAE		(1 << 5)
#define VI6_DISP_IRQ_STA_LNE(n)		(1 << (n))

/* -----------------------------------------------------------------------------
 * Display List Control Registers
 */
#define VI6_DL_CTRL					0x0100 //0x01001111 ++
#define VI6_DL_CTRL_AR_WAIT_MASK	(0xffff << 16)
#define VI6_DL_CTRL_AR_WAIT_SHIFT	16
#define VI6_DL_CTRL_DC2				(1 << 12)
#define VI6_DL_CTRL_DC1				(1 << 8)
#define VI6_DL_CTRL_DC0				(1 << 4)
#define VI6_DL_CTRL_CFM0			(1 << 2)
#define VI6_DL_CTRL_NH0				(1 << 1)
#define VI6_DL_CTRL_DLE				(1 << 0)

/* Display List-n Header Address Register */
#define VI6_DL_HDR_ADDR(n)			(0x0104 + (n) * 4)

/* Display List Data Swapping Register */
#define VI6_DL_SWAP(n)				(0x0114 + (n) * 56)
#define VI6_DL_SWAP_IND				(1 << 31)
#define VI6_DL_SWAP_LWS				(1 << 2)
#define VI6_DL_SWAP_WDS				(1 << 1)
#define VI6_DL_SWAP_BTS				(1 << 0)

/* Extended Display List Control Register */
#define VI6_DL_EXT_CTRL(n)			(0x011c + (n) * 36)
#define VI6_DL_EXT_CTRL_NWE			(1 << 16)
#define VI6_DL_EXT_CTRL_POLINT_MASK	(0x3f << 8)
#define VI6_DL_EXT_CTRL_POLINT_SHIFT	8
#define VI6_DL_EXT_CTRL_DLPRI		(1 << 5)
#define VI6_DL_EXT_CTRL_EXPRI		(1 << 4)
#define VI6_DL_EXT_CTRL_EXT			(1 << 0)

/* -----------------------------------------------------------------------------
 * RPF Control Registers
 */
#define VI6_RPF_OFFSET				0x100

/* Basic Read Size Registers */
#define VI6_RPF_SRC_BSIZE			0x0300
#define VI6_RPF_SRC_BSIZE_BHSIZE_MASK	(0x1fff << 16)
#define VI6_RPF_SRC_BSIZE_BHSIZE_SHIFT	16
#define VI6_RPF_SRC_BSIZE_BVSIZE_MASK	(0x1fff << 0)
#define VI6_RPF_SRC_BSIZE_BVSIZE_SHIFT	0
/* Extended Read Size Registers */
#define VI6_RPF_SRC_ESIZE			0x0304
#define VI6_RPF_SRC_ESIZE_EHSIZE_MASK	(0x1fff << 16)
#define VI6_RPF_SRC_ESIZE_EHSIZE_SHIFT	16
#define VI6_RPF_SRC_ESIZE_EVSIZE_MASK	(0x1fff << 0)
#define VI6_RPF_SRC_ESIZE_EVSIZE_SHIFT	0

/* Input Format Registers */
#define VI6_RPF_INFMT				0x0308
#define VI6_RPF_INFMT_VIR			(1 << 28)
#define VI6_RPF_INFMT_CIPM			(1 << 16)
#define VI6_RPF_INFMT_RDFMT_SHIFT	0

/* Data Swapping Registers */
#define VI6_RPF_DSWAP				0x030c
#define VI6_RPF_DSWAP_P_LLS			(1 << 3)
#define VI6_RPF_DSWAP_P_LWS			(1 << 2)

/* Display Location Registers */
#define VI6_RPF_LOC					0x0310
#define VI6_RPF_LOC_HCOORD_MASK		(0x1fff << 16)
#define VI6_RPF_LOC_HCOORD_SHIFT	16
#define VI6_RPF_LOC_VCOORD_MASK		(0x1fff << 0)
#define VI6_RPF_LOC_VCOORD_SHIFT	0

/* α Plane Selection Control Registers */
#define VI6_RPF_ALPH_SEL			0x0314
#define VI6_RPF_ALPH_SEL_ASEL_FIXED	(4 << 28)

/* Virtual Plane Color Information Registers */
#define VI6_RPF_VRTCOL_SET			0x0318
#define VI6_RPF_VRTCOL_SET_LAYA_SHIFT	24
#define VI6_RPF_VRTCOL_SET_LAYR_SHIFT	16
#define VI6_RPF_VRTCOL_SET_LAYG_SHIFT	8
#define VI6_RPF_VRTCOL_SET_LAYB_SHIFT	0

/* Mask Control Registers */
#define VI6_RPF_MSK_CTRL			0x031c

/* Color Keying Control Registers */
#define VI6_RPF_CKEY_CTRL			0x0328

/* Source Picture Memory Stride Setting Registers */
#define VI6_RPF_SRCM_PSTRIDE		0x0334
#define VI6_RPF_SRCM_PSTRIDE_Y_SHIFT	16
#define VI6_RPF_SRCM_PSTRIDE_C_SHIFT	0
/* Source α Memory Stride Setting Registers */
#define VI6_RPF_SRCM_ASTRIDE		0x0338
#define VI6_RPF_SRCM_PSTRIDE_A_SHIFT	0

/* Source Y/RGB Address Registers (read by RPFn) */
#define VI6_RPF_SRCM_ADDR_Y			0x033c
#define VI6_RPF_SRCM_ADDR_C0		0x0340
#define VI6_RPF_SRCM_ADDR_C1		0x0344
#define VI6_RPF_SRCM_ADDR_AI		0x0348

/* Multiple Alpha Control */
#define VI6_RPF_MULT_ALPHA			0x036c
#define VI6_RPF_MULT_ALPHA_RATIO_SHIFT	0

/* -----------------------------------------------------------------------------
 * WPF Control Registers
 */
#define VI6_WPF_OFFSET				0x100

/* WPFn-Source-RPF Registers */
#define VI6_WPF_SRCRPF				0x1000
#define VI6_WPF_SRCRPF_VIRACT_DIS	(0 << 28)
#define VI6_WPF_SRCRPF_VIRACT_SUB	(1 << 28)
#define VI6_WPF_SRCRPF_VIRACT_MST	(2 << 28)
#define VI6_WPF_SRCRPF_RPF_ACT_DIS(n)	(0 << ((n) * 2))
#define VI6_WPF_SRCRPF_RPF_ACT_SUB(n)	(1 << ((n) * 2))
#define VI6_WPF_SRCRPF_RPF_ACT_MST(n)	(2 << ((n) * 2))

/* WPFn Horizontal Input Size Clipping Registers */
#define VI6_WPF_HSZCLIP				0x1004
/* WPFn Vertical Input Size Clipping Registers */
#define VI6_WPF_VSZCLIP				0x1008
#define VI6_WPF_SZCLIP_EN			(1 << 28)
#define VI6_WPF_SZCLIP_OFST_MASK	(0xff << 16)
#define VI6_WPF_SZCLIP_OFST_SHIFT	16
#define VI6_WPF_SZCLIP_SIZE_MASK	(0xfff << 0)
#define VI6_WPF_SZCLIP_SIZE_SHIFT	0

/* WPFn Output Format Registers */
#define VI6_WPF_OUTFMT				0x100c
#define VI6_WPF_OUTFMT_PDV_SHIFT	24

/* WPF0 LIF Write Back Control Registers */
#define VI6_WPF_WRBCK_CTRL			0x1034

/* -----------------------------------------------------------------------------
 * DPR Control Registers
 */

/* RPFn Routing Register */
#define VI6_DPR_RPF_ROUTE(n)		(0x2000 + (n) * 4)

/* WPFn Timing Control Register */
#define VI6_DPR_WPF_FPORCH(n)		(0x2014 + (n) * 4)
#define VI6_DPR_WPF_FPORCH_FP_WPFN	(5 << 8)

#define VI6_DPR_SRU_ROUTE			0x2024
#define VI6_DPR_UDS_ROUTE(n)		(0x2028 + (n) * 4)
#define VI6_DPR_LUT_ROUTE			0x203c
#define VI6_DPR_CLU_ROUTE			0x2040
#define VI6_DPR_HST_ROUTE			0x2044
#define VI6_DPR_HSI_ROUTE			0x2048
#define VI6_DPR_BRU_ROUTE			0x204c
#define VI6_DPR_ILV_BRS_ROUTE		0x2050

#define VI6_DPR_ROUTE_BRSSEL		(1 << 28)
#define VI6_DPR_ROUTE_FXA_MASK		(0xff << 16)
#define VI6_DPR_ROUTE_FXA_SHIFT		16
#define VI6_DPR_ROUTE_FP_MASK		(0x3f << 8)
#define VI6_DPR_ROUTE_FP_SHIFT		8
#define VI6_DPR_ROUTE_RT_MASK		(0x3f << 0)
#define VI6_DPR_ROUTE_RT_SHIFT		0

#define VI6_DPR_HGO_SMPPT			0x2054
#define VI6_DPR_HGT_SMPPT			0x2058
#define VI6_DPR_SMPPT_TGW_MASK		(7 << 8)
#define VI6_DPR_SMPPT_TGW_SHIFT		8
#define VI6_DPR_SMPPT_PT_MASK		(0x3f << 0)
#define VI6_DPR_SMPPT_PT_SHIFT		0

#define VI6_DPR_UIF_ROUTE(n)		(0x2074 + (n) * 4)

//Node values
#define VI6_DPR_NODE_RPF(n)			(n)
#define VI6_DPR_NODE_UIF(n)			(12 + (n))
#define VI6_DPR_NODE_SRU			16
#define VI6_DPR_NODE_UDS(n)			(17 + (n))
#define VI6_DPR_NODE_LUT			22
#define VI6_DPR_NODE_BRU_IN(n)		(((n) <= 3) ? 23 + (n) : 49)
#define VI6_DPR_NODE_BRU_OUT		27
#define VI6_DPR_NODE_CLU			29
#define VI6_DPR_NODE_HST			30
#define VI6_DPR_NODE_HSI			31
#define VI6_DPR_NODE_BRS_IN(n)		(38 + (n))
#define VI6_DPR_NODE_WPF(n)			(56 + (n))
#define VI6_DPR_NODE_UNUSED			63

/* -----------------------------------------------------------------------------
 * BRS and BRU Control Registers
 */

#define VI6_ROP_NOP					0
#define VI6_ROP_AND					1
#define VI6_ROP_AND_REV				2
#define VI6_ROP_COPY				3
#define VI6_ROP_AND_INV				4
#define VI6_ROP_CLEAR				5
#define VI6_ROP_XOR					6
#define VI6_ROP_OR					7
#define VI6_ROP_NOR					8
#define VI6_ROP_EQUIV				9
#define VI6_ROP_INVERT				10
#define VI6_ROP_OR_REV				11
#define VI6_ROP_COPY_INV			12
#define VI6_ROP_OR_INV				13
#define VI6_ROP_NAND				14
#define VI6_ROP_SET					15

#define VI6_BRU_BASE				0x2c00
#define VI6_BRS_BASE				0x3900

/* BRU Input Control Register */
#define VI6_BRU_INCTRL				0x0000
#define VI6_BRU_INCTRL_NRM			(1 << 28)

/* Size Register of BRU Input Virtual RPF */
#define VI6_BRU_VIRRPF_SIZE			0x0004
#define VI6_BRU_VIRRPF_SIZE_HSIZE_SHIFT	16
#define VI6_BRU_VIRRPF_SIZE_VSIZE_SHIFT	0

/* Display Location Register of BRU Input Virtual RPF */
#define VI6_BRU_VIRRPF_LOC			0x0008
#define VI6_BRU_VIRRPF_LOC_HCOORD_SHIFT	16
#define VI6_BRU_VIRRPF_LOC_VCOORD_SHIFT	0

/* Color Information Register of BRU Input Virtual RPF */
#define VI6_BRU_VIRRPF_COL			0x000c
#define VI6_BRU_VIRRPF_COL_A_SHIFT	24
#define VI6_BRU_VIRRPF_COL_RCR_SHIFT	16
#define VI6_BRU_VIRRPF_COL_GY_SHIFT	8
#define VI6_BRU_VIRRPF_COL_BCB_SHIFT	0

/* BRU Control Registers */
#define VI6_BRU_CTRL(n)				(0x0010 + (n) * 8 + ((n) <= 3 ? 0 : 4))
#define VI6_BRU_CTRL_RBC			(1 << 31)
#define VI6_BRU_CTRL_DSTSEL_BRUIN(n)	(((n) <= 3 ? (n) : (n)+1) << 20)
#define VI6_BRU_CTRL_DSTSEL_VRPF	(4 << 20)
#define VI6_BRU_CTRL_DSTSEL_MASK	(7 << 20)
#define VI6_BRU_CTRL_SRCSEL_BRUIN(n)	(((n) <= 3 ? (n) : (n)+1) << 16)
#define VI6_BRU_CTRL_SRCSEL_VRPF	(4 << 16)
#define VI6_BRU_CTRL_SRCSEL_MASK	(7 << 16)
#define VI6_BRU_CTRL_CROP(rop)		((rop) << 4)
#define VI6_BRU_CTRL_CROP_MASK		(0xf << 4)
#define VI6_BRU_CTRL_AROP(rop)		((rop) << 0)
#define VI6_BRU_CTRL_AROP_MASK		(0xf << 0)

/* BRU Blend Control Registers */
#define VI6_BRU_BLD(n)				(0x0014 + (n) * 8 + ((n) <= 3 ? 0 : 4))
#define VI6_BRU_BLD_CBES			(1 << 31)
#define VI6_BRU_BLD_CCMDX_DST_A		(0 << 28)
#define VI6_BRU_BLD_CCMDX_255_DST_A	(1 << 28)
#define VI6_BRU_BLD_CCMDX_SRC_A		(2 << 28)
#define VI6_BRU_BLD_CCMDX_255_SRC_A	(3 << 28)
#define VI6_BRU_BLD_CCMDX_COEFX		(4 << 28)
#define VI6_BRU_BLD_CCMDX_MASK		(7 << 28)
#define VI6_BRU_BLD_CCMDY_DST_A		(0 << 24)
#define VI6_BRU_BLD_CCMDY_255_DST_A	(1 << 24)
#define VI6_BRU_BLD_CCMDY_SRC_A		(2 << 24)
#define VI6_BRU_BLD_CCMDY_255_SRC_A	(3 << 24)
#define VI6_BRU_BLD_CCMDY_COEFY		(4 << 24)
#define VI6_BRU_BLD_CCMDY_MASK		(7 << 24)
#define VI6_BRU_BLD_CCMDY_SHIFT		24
#define VI6_BRU_BLD_ABES			(1 << 23)
#define VI6_BRU_BLD_ACMDX_DST_A		(0 << 20)
#define VI6_BRU_BLD_ACMDX_255_DST_A	(1 << 20)
#define VI6_BRU_BLD_ACMDX_SRC_A		(2 << 20)
#define VI6_BRU_BLD_ACMDX_255_SRC_A	(3 << 20)
#define VI6_BRU_BLD_ACMDX_COEFX		(4 << 20)
#define VI6_BRU_BLD_ACMDX_MASK		(7 << 20)
#define VI6_BRU_BLD_ACMDY_DST_A		(0 << 16)
#define VI6_BRU_BLD_ACMDY_255_DST_A	(1 << 16)
#define VI6_BRU_BLD_ACMDY_SRC_A		(2 << 16)
#define VI6_BRU_BLD_ACMDY_255_SRC_A	(3 << 16)
#define VI6_BRU_BLD_ACMDY_COEFY		(4 << 16)
#define VI6_BRU_BLD_ACMDY_MASK		(7 << 16)
#define VI6_BRU_BLD_COEFX_MASK		(0xff << 8)
#define VI6_BRU_BLD_COEFX_SHIFT		8
#define VI6_BRU_BLD_COEFY_MASK		(0xff << 0)
#define VI6_BRU_BLD_COEFY_SHIFT		0

/* BRU Raster Operation Control Register */
#define VI6_BRU_ROP					0x0030
#define VI6_BRU_ROP_DSTSEL_BRUIN(n)	(((n) <= 3 ? (n) : (n)+1) << 20)
#define VI6_BRU_ROP_DSTSEL_VRPF		(4 << 20)
#define VI6_BRU_ROP_DSTSEL_MASK		(7 << 20)
#define VI6_BRU_ROP_CROP(rop)		((rop) << 4)
#define VI6_BRU_ROP_CROP_MASK		(0xf << 4)
#define VI6_BRU_ROP_AROP(rop)		((rop) << 0)
#define VI6_BRU_ROP_AROP_MASK		(0xf << 0)

/* -----------------------------------------------------------------------------
 * LIF Control Registers
 */
#define VI6_LIF_OFFSET				(-0x100)

/* LIF Control Register  */
#define VI6_LIF_CTRL				0x3b00
#define VI6_LIF_CTRL_OBTH_MASK		(0x7ff << 16)
#define VI6_LIF_CTRL_OBTH_SHIFT		16
#define VI6_LIF_CTRL_CFMT			(1 << 4)
#define VI6_LIF_CTRL_REQSEL			(1 << 1)
#define VI6_LIF_CTRL_LIF_EN			(1 << 0)

/* LIF Clock Stop Buffer Control Register */
#define VI6_LIF_CSBTH				0x3b04
#define VI6_LIF_CSBTH_HBTH_SHIFT	16
#define VI6_LIF_CSBTH_LBTH_SHIFT	0

/* -----------------------------------------------------------------------------
 * IP Version Registers
 */
#define VI6_IP_VERSION				0x3f00

/* -----------------------------------------------------------------------------
 * Formats
 */
#define VI6_FMT_ARGB_8888			0x13

#endif /* __VSP1_REGS_H__ */
