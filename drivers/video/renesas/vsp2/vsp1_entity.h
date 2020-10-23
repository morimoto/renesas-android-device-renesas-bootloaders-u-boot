/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1_entity.h  --  R-Car VSP1 Base Entity
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2014 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_ENTITY_H__
#define __VSP1_ENTITY_H__

#include <linux/list.h>
#include <linux/compat.h>

struct vsp1_device;
struct vsp1_dl_body;
struct vsp1_dl_list;
struct vsp1_pipeline;
struct vsp1_partition;
struct vsp1_partition_window;

enum vsp1_entity_type {
	VSP1_ENTITY_BRS,
	VSP1_ENTITY_BRU,
	VSP1_ENTITY_CLU,
	VSP1_ENTITY_HGO,
	VSP1_ENTITY_HGT,
	VSP1_ENTITY_HSI,
	VSP1_ENTITY_HST,
	VSP1_ENTITY_LIF,
	VSP1_ENTITY_LUT,
	VSP1_ENTITY_RPF,
	VSP1_ENTITY_SRU,
	VSP1_ENTITY_UDS,
	VSP1_ENTITY_UIF,
	VSP1_ENTITY_WPF,
};

struct vsp1_route {
	enum vsp1_entity_type type;
	unsigned int reg;
	unsigned int input;
	unsigned int output;
};

struct vsp1_entity {
	struct vsp1_device *vsp1;
	const struct vsp1_entity_operations *ops;
	enum vsp1_entity_type type;
	const struct vsp1_route *route;
	struct vsp1_pipeline *pipe;
	struct list_head list_dev;
	struct list_head list_pipe;
	struct vsp1_entity *sink;
};

struct vsp1_entity_operations {
	void (*destroy)(struct vsp1_entity *);
	void (*configure_stream)(struct vsp1_dl_body *);
	void (*configure_frame)(struct vsp1_dl_body *);
	void (*configure_partition)(struct vsp1_dl_body *);
};

int vsp1_entity_init(struct vsp1_device *vsp1, struct vsp1_entity *entity);
void vsp1_entity_destroy(struct vsp1_entity *entity);

void vsp1_entity_route_setup(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb);

void vsp1_entity_configure_stream(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb);

void vsp1_entity_configure_frame(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb);

void vsp1_entity_configure_partition(struct vsp1_entity *entity,
				 struct vsp1_dl_body *dlb);

#endif /* __VSP1_ENTITY_H__ */
