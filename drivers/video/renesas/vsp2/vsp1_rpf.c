// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_rpf.c  --  R-Car VSP1 Read Pixel Formatter
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <linux/compat.h>
#include "vsp1.h"
#include "vsp1_dl.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"

static inline void vsp1_rpf_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	vsp1_dl_body_write(dlb, reg + 0 * VI6_RPF_OFFSET, data);
}

static void rpf_configure_stream(struct vsp1_dl_body *dlb)
{
	u32 infmt = 0;

	/* Stride */
	vsp1_rpf_write(dlb, VI6_RPF_SRCM_PSTRIDE,
			5120 << VI6_RPF_SRCM_PSTRIDE_Y_SHIFT);

	/* Format */
	infmt = VI6_RPF_INFMT_CIPM
	      | (VI6_FMT_ARGB_8888 << VI6_RPF_INFMT_RDFMT_SHIFT);
	infmt &= ~VI6_RPF_INFMT_VIR;
	vsp1_rpf_write(dlb, VI6_RPF_INFMT, infmt);

	vsp1_rpf_write(dlb, VI6_RPF_DSWAP, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS);

	/* Output location */
	vsp1_rpf_write(dlb, VI6_RPF_LOC,
		       (0 << VI6_RPF_LOC_HCOORD_SHIFT) | /*left*/
		       (0 << VI6_RPF_LOC_VCOORD_SHIFT)); /*top*/

	/* The fixed α value (VI6_RPFn_VRTCOL_SET.LAYA value, VIR is disabled) */
	vsp1_rpf_write(dlb, VI6_RPF_ALPH_SEL, VI6_RPF_ALPH_SEL_ASEL_FIXED);

	vsp1_rpf_write(dlb, VI6_RPF_MSK_CTRL, 0);
	vsp1_rpf_write(dlb, VI6_RPF_CKEY_CTRL, 0);
}

static void rpf_configure_frame(struct vsp1_dl_body *dlb)
{
	vsp1_rpf_write(dlb, VI6_RPF_VRTCOL_SET,
		       255 << VI6_RPF_VRTCOL_SET_LAYA_SHIFT
			   | 0 << VI6_RPF_VRTCOL_SET_LAYR_SHIFT
			   | 0 << VI6_RPF_VRTCOL_SET_LAYG_SHIFT
			   | 0 << VI6_RPF_VRTCOL_SET_LAYB_SHIFT);

	vsp1_rpf_write(dlb, VI6_RPF_MULT_ALPHA, 0 | (255 << VI6_RPF_MULT_ALPHA_RATIO_SHIFT));
}

static void rpf_configure_partition(struct vsp1_dl_body *dlb)
{
	/* Source size and crop offsets */
	vsp1_rpf_write(dlb, VI6_RPF_SRC_BSIZE,
		       (CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH << VI6_RPF_SRC_BSIZE_BHSIZE_SHIFT) |
		       (CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT << VI6_RPF_SRC_BSIZE_BVSIZE_SHIFT));
	vsp1_rpf_write(dlb, VI6_RPF_SRC_ESIZE,
		       (CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH << VI6_RPF_SRC_ESIZE_EHSIZE_SHIFT) |
		       (CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT << VI6_RPF_SRC_ESIZE_EVSIZE_SHIFT));

	vsp1_dl_set_addr_auto_fld(dlb);
}

static const struct vsp1_entity_operations rpf_entity_ops = {
	.configure_stream = rpf_configure_stream,
	.configure_frame = rpf_configure_frame,
	.configure_partition = rpf_configure_partition,
};

struct vsp1_rwpf *vsp1_rpf_create(struct vsp1_device *vsp1)
{
	static struct vsp1_rwpf rpf;
	memset(&rpf, 0, sizeof(struct vsp1_rwpf));
	rpf.entity.ops = &rpf_entity_ops;
	rpf.entity.type = VSP1_ENTITY_RPF;
	vsp1_entity_init(vsp1, &rpf.entity);
	return &rpf;
}
