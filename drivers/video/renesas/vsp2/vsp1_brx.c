// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_brx.c  --  R-Car VSP1 Blend ROP Unit (BRU and BRS)
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include "vsp1_brx.h"
#include "vsp1.h"
#include "vsp1_dl.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"

static inline void vsp1_brx_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	vsp1_dl_body_write(dlb, VI6_BRU_BASE + reg, data);
}

static void brx_configure_stream(struct vsp1_dl_body *dlb)
{
	/*
	 * Disable dithering and enable color data normalization unless the
	 * format at the pipeline output is premultiplied.
	 */
	vsp1_brx_write(dlb, VI6_BRU_INCTRL, VI6_BRU_INCTRL_NRM);

	/*
	 * Set the background position to cover the whole output image and
	 * configure its color.
	 */
	vsp1_brx_write(dlb, VI6_BRU_VIRRPF_SIZE,
		(CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH << VI6_BRU_VIRRPF_SIZE_HSIZE_SHIFT) |
		(CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT << VI6_BRU_VIRRPF_SIZE_VSIZE_SHIFT));
	vsp1_brx_write(dlb, VI6_BRU_VIRRPF_LOC,
		       (0 << VI6_BRU_VIRRPF_LOC_HCOORD_SHIFT) |
		       (0 << VI6_BRU_VIRRPF_LOC_VCOORD_SHIFT));

	vsp1_brx_write(dlb, VI6_BRU_VIRRPF_COL, 0 |
		       (0xFF << VI6_BRU_VIRRPF_COL_A_SHIFT));

	/*
	 * Route BRU input 1 as SRC input to the ROP unit and configure the ROP
	 * unit with a NOP operation to make BRU input 1 available as the
	 * Blend/ROP unit B SRC input. Only needed for BRU, the BRS has no ROP
	 * unit.
	 */
	vsp1_brx_write(dlb, VI6_BRU_ROP,
			   VI6_BRU_ROP_DSTSEL_BRUIN(1) |
			   VI6_BRU_ROP_CROP(VI6_ROP_NOP) |
			   VI6_BRU_ROP_AROP(VI6_ROP_NOP));

	bool premultiplied = false;
	u32 ctrl = 0;

	/*
	 * Configure all Blend/ROP units corresponding to an enabled BRx
	 * input for alpha blending. Blend/ROP units corresponding to
	 * disabled BRx inputs are used in ROP NOP mode to ignore the
	 * SRC input.
	 */
	ctrl |= VI6_BRU_CTRL_RBC;
	ctrl |= VI6_BRU_CTRL_CROP(VI6_ROP_NOP)
					 |  VI6_BRU_CTRL_AROP(VI6_ROP_NOP);

	/*
	 * Select the virtual RPF as the Blend/ROP unit A DST input to
	 * serve as a background color.
	 */
	ctrl |= VI6_BRU_CTRL_DSTSEL_VRPF;

	/*
	 * Route inputs 0 to 3 as SRC inputs to Blend/ROP units A to D
	 * in that order. In the BRU the Blend/ROP unit B SRC is
	 * hardwired to the ROP unit output, the corresponding register
	 * bits must be set to 0. The BRS has no ROP unit and doesn't
	 * need any special processing.
	 */
	ctrl |= VI6_BRU_CTRL_SRCSEL_BRUIN(0);

	vsp1_brx_write(dlb, VI6_BRU_CTRL(0), ctrl);

	/*
	 * Harcode the blending formula to
	 *
	 *	DSTc = DSTc * (1 - SRCa) + SRCc * SRCa
	 *	DSTa = DSTa * (1 - SRCa) + SRCa
	 *
	 * when the SRC input isn't premultiplied, and to
	 *
	 *	DSTc = DSTc * (1 - SRCa) + SRCc
	 *	DSTa = DSTa * (1 - SRCa) + SRCa
	 *
	 * otherwise.
	 */
	vsp1_brx_write(dlb, VI6_BRU_BLD(0),  //VI6_BRU_BLD(i)=0, =323400FF
			   VI6_BRU_BLD_CCMDX_255_SRC_A |
			   (premultiplied ? VI6_BRU_BLD_CCMDY_COEFY :
			   VI6_BRU_BLD_CCMDY_SRC_A) |
			   VI6_BRU_BLD_ACMDX_255_SRC_A |
			   VI6_BRU_BLD_ACMDY_COEFY |
			   (0xff << VI6_BRU_BLD_COEFY_SHIFT));
}

static const struct vsp1_entity_operations brx_entity_ops = {
	.configure_stream = brx_configure_stream,
};

struct vsp1_brx *vsp1_brx_create(struct vsp1_device *vsp1)
{
	static struct vsp1_brx brx;
	memset(&brx, 0, sizeof(struct vsp1_brx));
	brx.entity.ops = &brx_entity_ops;
	brx.entity.type = VSP1_ENTITY_BRU;
	vsp1_entity_init(vsp1, &brx.entity);
	return &brx;
}
