// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_drm.c  --  R-Car VSP1 DRM/KMS Interface
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2015-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <common.h>
#include "vsp1_drm.h"
#include <linux/compat.h>
#include "vsp1.h"
#include "vsp1_brx.h"
#include "vsp1_dl.h"
#include "vsp1_lif.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"

extern struct vsp1_device VSP;

static void vsp1_du_pipeline_setup_brx(struct vsp1_device *vsp1,
				      struct vsp1_pipeline *pipe)
{
	struct vsp1_entity *brx;
	/*
	 * Pick a BRx:
	 * - If we need more than two inputs, use the BRU.
	 * - Otherwise, if we are not forced to release our BRx, keep it.
	 * - Else, use any free BRx (randomly starting with the BRU).
	 */
	if (!vsp1->bru->entity.pipe)
		brx = &vsp1->bru->entity;
	else
		brx = &vsp1->bru->entity;

	/* Switch BRx if needed. */
	if (brx != pipe->brx) {
		/* Add the BRx to the pipeline. */
		pipe->brx = brx;
		pipe->brx->pipe = pipe;
		pipe->brx->sink = &pipe->output->entity;

		list_add_tail(&pipe->brx->list_pipe, &pipe->entities);
	}
}

/* Setup the BRx source pad. */
/* Setup the input side of the pipeline (RPFs and BRx). */
static void vsp1_du_pipeline_setup_inputs(struct vsp1_device *vsp1,
					struct vsp1_pipeline *pipe)
{
	struct vsp1_rwpf *rpf = vsp1->rpf;

	/*
	 * Setup the BRx. This must be done before setting up the RPF input
	 * pipelines as the BRx sink compose rectangles depend on the BRx source
	 * format.
	 */
	vsp1_du_pipeline_setup_brx(vsp1, pipe);

	/* Setup the RPF input pipeline for every enabled input. */
	rpf->entity.pipe = pipe;
	list_add_tail(&rpf->entity.list_pipe, &pipe->entities);

	rpf->entity.sink = pipe->brx;
}

/* Configure all entities in the pipeline. */
static void vsp1_du_pipeline_configure(struct vsp1_pipeline *pipe)
{
	struct vsp1_entity *entity;
	struct vsp1_entity *next;
	struct vsp1_dl_list *dl;
	struct vsp1_dl_body *dlb;

	dl = vsp1_dl_list_get(pipe->output->dlm);
	dlb = vsp1_dl_list_get_body0(dl);

	list_for_each_entry_safe(entity, next, &pipe->entities, list_pipe) {
		vsp1_entity_route_setup(entity, dlb);

		vsp1_entity_configure_stream(entity, dlb);

		vsp1_entity_configure_frame(entity, dlb);

		vsp1_entity_configure_partition(entity, dlb);
	}

	vsp1_dl_list_commit(dl);
}

int vsp1_du_setup_lif(bool enable)
{
	struct vsp1_device *vsp1 = &VSP;
	struct vsp1_drm_pipeline *drm_pipe;
	struct vsp1_pipeline *pipe;
	int ret = 0;

	drm_pipe = &vsp1->drm->pipe;
	pipe = &drm_pipe->pipe;

	if (!enable) {
		/*
		 * False means the CRTC is being disabled, stop
		 * the pipeline and turn the light off.
		 */
		ret = vsp1_pipeline_stop(pipe);
		if (ret == -ETIMEDOUT)
			printf("%s: Pipeline stop timeout\n", __func__);

		struct vsp1_rwpf *rpf = pipe->inputs;
		/*
		 * Remove the RPF from the pipe and the list of BRx
		 * inputs.
		 */
		rpf->entity.pipe = NULL;
		list_del(&rpf->entity.list_pipe);
		pipe->inputs = NULL;

		list_del(&pipe->brx->list_pipe);
		pipe->brx->pipe = NULL;
		pipe->brx = NULL;

		vsp1_dlm_reset(pipe->output->dlm);
		return 0;
	}

	/* Enable the VSP1. */
	ret = vsp1_device_init();
	if (ret < 0)
		return ret;

	/* Clear the display interrupts. */
	vsp1_write(VI6_DISP_IRQ_STA(0), 0);
	/* Interrupt Enable for Display Start */
	vsp1_write(VI6_DISP_IRQ_ENB(0), VI6_DISP_IRQ_ENB_DSTE);

	return 0;
}

int vsp1_du_atomic_flush(void)
{
	struct vsp1_device *vsp1 = &VSP;
	struct vsp1_drm_pipeline *drm_pipe = &vsp1->drm->pipe;
	struct vsp1_pipeline *pipe = &drm_pipe->pipe;
	u32 disp_st = 0;
	int count = 10000;

	/* Setup formats through the pipeline. */
	vsp1_du_pipeline_setup_inputs(vsp1, pipe);

	/* Configure all entities in the pipeline. */
	vsp1_du_pipeline_configure(pipe);

	/* Start the pipeline (start WPF0 in VSP2) */
	vsp1_pipeline_run();

	/* Check display start interrupt (dst_wait) */
	while (--count) {
		disp_st = vsp1_read(VI6_DISP_IRQ_STA(0));

		if (disp_st & VI6_DISP_IRQ_STA_DST) {
			vsp1_write(VI6_DISP_IRQ_STA(0), ~disp_st & VI6_DISP_IRQ_STA_DST);
			break;
		}
		udelay(1000);
	}

	if (count == 0){
		printf("%s: Pipeline enabled timeout\n", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

/* -----------------------------------------------------------------------------
 * Initialization
 */
void vsp1_drm_init(struct vsp1_device *vsp1)
{
	static struct vsp1_drm drm;
	memset(&drm, 0, sizeof(struct vsp1_drm));
	vsp1->drm = &drm;

	/* Create one DRM pipeline per LIF. */
	struct vsp1_drm_pipeline *drm_pipe = &vsp1->drm->pipe;
	struct vsp1_pipeline *pipe = &drm_pipe->pipe;

	/* Init pipeline */
	INIT_LIST_HEAD(&pipe->entities);

	/*
	 * The output side of the DRM pipeline is static, add the
	 * corresponding entities manually.
	 */
	pipe->output = vsp1->wpf;
	pipe->lif = &vsp1->lif->entity;

	pipe->output->entity.pipe = pipe;
	pipe->output->entity.sink = pipe->lif;
	list_add_tail(&pipe->output->entity.list_pipe, &pipe->entities);

	pipe->lif->pipe = pipe;
	list_add_tail(&pipe->lif->list_pipe, &pipe->entities);

	/* Disable all RPFs initially. */
	INIT_LIST_HEAD(&vsp1->rpf->entity.list_pipe);
}

