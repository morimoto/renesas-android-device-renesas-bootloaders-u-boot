// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_entity.c  --  R-Car VSP1 Base Entity
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2014 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include "vsp1.h"
#include "vsp1_dl.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"
#include "vsp1_entity.h"

void vsp1_entity_route_setup(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb)
{
	struct vsp1_entity *source;
	u32 route = 0;

	source = entity;
	if (source->route->reg == 0)
		return;

	route = source->sink->route->input;
	vsp1_dl_body_write(dlb, source->route->reg, route);//DPR registers
}

void vsp1_entity_configure_stream(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb)
{
	if (entity->ops->configure_stream) {
		entity->ops->configure_stream(dlb);
	}
}

void vsp1_entity_configure_frame(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb)
{
	if (entity->ops->configure_frame) {
		entity->ops->configure_frame(dlb);
	}
}

void vsp1_entity_configure_partition(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb)
{
	if (entity->ops->configure_partition) {
		entity->ops->configure_partition(dlb);
	}
}

/* -----------------------------------------------------------------------------
 * Initialization
 */
static const struct vsp1_route vsp1_routes[] = {
	{ VSP1_ENTITY_BRU, VI6_DPR_BRU_ROUTE, VI6_DPR_NODE_BRU_IN(0), VI6_DPR_NODE_BRU_OUT },
	{ VSP1_ENTITY_LIF, 0, 0, 0 },
	{ VSP1_ENTITY_RPF, VI6_DPR_RPF_ROUTE(0), 0, VI6_DPR_NODE_RPF(0) },
	{ VSP1_ENTITY_WPF, 0, VI6_DPR_NODE_WPF(0), VI6_DPR_NODE_WPF(0) },
};

//BRU LIF RFP.0 WPF.0 (during booting)
int vsp1_entity_init(struct vsp1_device *vsp1, struct vsp1_entity *entity)
{
	unsigned int i = 0;

	for (i = 0; i < ARRAY_SIZE(vsp1_routes); ++i) {
		if (vsp1_routes[i].type == entity->type) {
			entity->route = &vsp1_routes[i];
			break;
		}
	}
	entity->vsp1 = vsp1;
	return 0;
}

void vsp1_entity_destroy(struct vsp1_entity *entity)
{
	if (entity->ops && entity->ops->destroy) {
		entity->ops->destroy(entity);
	}
}
