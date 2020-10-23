// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_drv.c  --  R-Car VSP1 Driver
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <common.h>
#include <linux/io.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <rcar-logo.h>

#include "vsp1.h"
#include "vsp1_brx.h"
#include "vsp1_dl.h"
#include "vsp1_drm.h"
#include "vsp1_lif.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"

/* Global struct VSP device */
struct vsp1_device VSP;

u32 vsp1_read(u32 reg)
{
	return ioread32((void __iomem *)(unsigned long)(RCAR_VSPD1_BASE_REG + reg));
}

void vsp1_write(u32 reg, u32 data)
{
	iowrite32(data, (void __iomem *)(unsigned long)(RCAR_VSPD1_BASE_REG + reg));
}

static void vsp1_destroy_entities(struct vsp1_device *vsp1)
{
	struct vsp1_entity *entity, *_entity;
	list_for_each_entry_safe(entity, _entity, &vsp1->entities, list_dev) {
		list_del(&entity->list_dev);
		vsp1_entity_destroy(entity);
	}
}

static void vsp1_create_entities(struct vsp1_device *vsp1)
{
	vsp1->bru = vsp1_brx_create(vsp1);
	list_add_tail(&vsp1->bru->entity.list_dev, &vsp1->entities);

	vsp1->lif = vsp1_lif_create(vsp1);
	list_add_tail(&vsp1->lif->entity.list_dev, &vsp1->entities);

	vsp1->rpf = vsp1_rpf_create(vsp1);
	list_add_tail(&vsp1->rpf->entity.list_dev, &vsp1->entities);

	vsp1->wpf = vsp1_wpf_create(vsp1);
	list_add_tail(&vsp1->wpf->entity.list_dev, &vsp1->entities);

	vsp1_drm_init(vsp1);
}

int vsp1_reset_wpf(void)
{
	u32 timeout = 0;
	u32 status = 0;

	status = vsp1_read(VI6_STATUS);
	if (!(status & VI6_STATUS_SYS_ACT(0))) {
		return 0;
	}

	/* WPF0 S/W reset  */
	vsp1_write(VI6_SRESET, VI6_SRESET_SRTS(0));

	for (timeout = 10; timeout > 0; --timeout) {
		status = vsp1_read(VI6_STATUS);
		if (!(status & VI6_STATUS_SYS_ACT(0)))
			break;

		udelay(2000);
	}

	if (!timeout) {
		printf("%s: Failed to reset WPF0\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

int vsp1_device_init(void)
{
	int ret = 0;

	/* Reset any channel that might be running. */
	ret = vsp1_reset_wpf();
	if (ret < 0) {
		return ret;
	}

	/* Always specify 8 */
	vsp1_write(VI6_CLK_DCSWT, (8 << VI6_CLK_DCSWT_CSTPW_SHIFT) |
		   (8 << VI6_CLK_DCSWT_CSTRW_SHIFT));

	vsp1_write(VI6_DPR_RPF_ROUTE(0), VI6_DPR_NODE_UNUSED);
	vsp1_write(VI6_DPR_SRU_ROUTE, VI6_DPR_NODE_UNUSED);
	vsp1_write(VI6_DPR_LUT_ROUTE, VI6_DPR_NODE_UNUSED);
	vsp1_write(VI6_DPR_CLU_ROUTE, VI6_DPR_NODE_UNUSED);
	vsp1_write(VI6_DPR_HST_ROUTE, VI6_DPR_NODE_UNUSED);
	vsp1_write(VI6_DPR_HSI_ROUTE, VI6_DPR_NODE_UNUSED);
	vsp1_write(VI6_DPR_BRU_ROUTE, VI6_DPR_NODE_UNUSED);

	vsp1_write(VI6_DPR_HGO_SMPPT, (7 << VI6_DPR_SMPPT_TGW_SHIFT) |
		   (VI6_DPR_NODE_UNUSED << VI6_DPR_SMPPT_PT_SHIFT));
	vsp1_write(VI6_DPR_HGT_SMPPT, (7 << VI6_DPR_SMPPT_TGW_SHIFT) |
		   (VI6_DPR_NODE_UNUSED << VI6_DPR_SMPPT_PT_SHIFT));


	vsp1_dlm_setup();

	return 0;
}

int vsp1_probe(void)
{
	struct vsp1_device *vsp1;
	memset(&VSP, 0, sizeof(struct vsp1_device));
	vsp1 = &VSP;

	INIT_LIST_HEAD(&vsp1->entities);

	/* Disable interrupts before request irq.
	 * If the interrupt is not cleared and starts up,
	 * the driver may be malfunctioned.
	 */
	vsp1_write(VI6_DISP_IRQ_ENB(0), 0);
	vsp1_write(VI6_WPF_IRQ_ENB(0), 0);

	/* Instanciate entities */
	vsp1_create_entities(vsp1);

	/* vsp1_du_atomic_update */
	struct vsp1_drm_pipeline *drm_pipe = &vsp1->drm->pipe;
	drm_pipe->pipe.inputs = vsp1->rpf;

	return 0;
}

void vsp1_remove(void)
{
	vsp1_destroy_entities(&VSP);
}

