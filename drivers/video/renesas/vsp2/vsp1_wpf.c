// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_wpf.c  --  R-Car VSP1 Write Pixel Formatter
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include "vsp1.h"
#include "vsp1_dl.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"

static inline void vsp1_wpf_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	vsp1_dl_body_write(dlb, reg + 0 * VI6_WPF_OFFSET, data);
}

static void vsp1_wpf_destroy(struct vsp1_entity *entity)
{
	struct vsp1_rwpf *wpf = entity_to_rwpf(entity);
	vsp1_dlm_destroy(wpf->dlm);
}

static void wpf_configure_stream(struct vsp1_dl_body *dlb)
{
	u32 srcrpf = VI6_WPF_SRCRPF_VIRACT_DIS;

	/* WPFn Internal Operation Timing Setting */
	vsp1_dl_body_write(dlb, VI6_DPR_WPF_FPORCH(0), VI6_DPR_WPF_FPORCH_FP_WPFN);
	/* Write Back disabled */
	vsp1_wpf_write(dlb, VI6_WPF_WRBCK_CTRL, 0);

	/*
	 * Sources. If the pipeline has a single input and BRx is not used,
	 * configure it as the master layer. Otherwise configure all
	 * inputs as sub-layers and select the virtual RPF as the master
	 * layer.
	 */
	srcrpf |= VI6_WPF_SRCRPF_VIRACT_MST;
	srcrpf |= VI6_WPF_SRCRPF_RPF_ACT_SUB(0); //VI6_WPF_SRCRPF_RPF_ACT_MST(0);
	vsp1_wpf_write(dlb, VI6_WPF_SRCRPF, srcrpf);

	/* Clear & Enable interrupts */
	vsp1_dl_body_write(dlb, VI6_WPF_IRQ_STA(0), 0);
	vsp1_dl_body_write(dlb, VI6_WPF_IRQ_ENB(0),
			   VI6_WFP_IRQ_ENB_DFEE | VI6_WFP_IRQ_ENB_FREE | VI6_WFP_IRQ_ENB_UNDE);
}

static void wpf_configure_frame(struct vsp1_dl_body *dlb)
{
	vsp1_wpf_write(dlb, VI6_WPF_OUTFMT, (255 << VI6_WPF_OUTFMT_PDV_SHIFT));
}

static void wpf_configure_partition(struct vsp1_dl_body *dlb)
{
	vsp1_wpf_write(dlb, VI6_WPF_HSZCLIP, VI6_WPF_SZCLIP_EN |
		       (0 << VI6_WPF_SZCLIP_OFST_SHIFT) |
		       (CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH << VI6_WPF_SZCLIP_SIZE_SHIFT));
	vsp1_wpf_write(dlb, VI6_WPF_VSZCLIP, VI6_WPF_SZCLIP_EN |
		       (0 << VI6_WPF_SZCLIP_OFST_SHIFT) |
		       (CONFIG_VIDEO_RENESAS_DISPLAY_HEIGHT << VI6_WPF_SZCLIP_SIZE_SHIFT));
}

static const struct vsp1_entity_operations wpf_entity_ops = {
	.destroy = vsp1_wpf_destroy,
	.configure_stream = wpf_configure_stream,
	.configure_frame = wpf_configure_frame,
	.configure_partition = wpf_configure_partition,
};

struct vsp1_rwpf *vsp1_wpf_create(struct vsp1_device *vsp1)
{
	static struct vsp1_rwpf wpf;
	int ret = 0;
	memset(&wpf, 0, sizeof(struct vsp1_rwpf));
	wpf.entity.ops = &wpf_entity_ops;
	wpf.entity.type = VSP1_ENTITY_WPF;
	vsp1_entity_init(vsp1, &wpf.entity);

	/* Initialize the display list manager. */
	wpf.dlm = vsp1_dlm_create(vsp1, 1);
	if (!wpf.dlm) {
		ret = -ENOMEM;
		goto error;
	}
	return &wpf;

error:
	return ERR_PTR(ret);
}
