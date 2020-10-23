// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_pipe.c  --  R-Car VSP1 Pipeline
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <linux/compat.h>
#include "vsp1.h"
#include "vsp1_brx.h"
#include "vsp1_dl.h"
#include "vsp1_entity.h"
#include "vsp1_rwpf.h"
#include "vsp1_pipe.h"
#include <rcar-logo.h>

void vsp1_pipeline_run(void)
{
	vsp1_write(VI6_CMD(0), VI6_CMD_STRCMD);
}

int vsp1_pipeline_stop(struct vsp1_pipeline *pipe)
{
	struct vsp1_entity *entity;
	int ret = 0;
	/*
	 * When using display lists in continuous frame mode the only
	 * way to stop the pipeline is to reset the hardware.
	 */
	ret = vsp1_reset_wpf();
	ret = rcar_fcp_reset();

	/*
	 * Write to registers directly when stopping the stream as there will be
	 * no pipeline run to apply the display list.
	 */
	vsp1_write(VI6_WPF_IRQ_ENB(0), 0);
	vsp1_write(0 * VI6_WPF_OFFSET + VI6_WPF_SRCRPF, 0);

	list_for_each_entry(entity, &pipe->entities, list_pipe) {
		if (entity->route && entity->route->reg)
			vsp1_write(entity->route->reg,
				   VI6_DPR_NODE_UNUSED);
	}

	return ret;
}

