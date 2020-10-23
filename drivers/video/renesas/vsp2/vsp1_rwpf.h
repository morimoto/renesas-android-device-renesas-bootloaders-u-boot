/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1_rwpf.h  --  R-Car VSP1 Read and Write Pixel Formatters
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_RWPF_H__
#define __VSP1_RWPF_H__

#include <linux/compat.h>
#include "vsp1.h"
#include "vsp1_entity.h"

struct vsp1_dl_manager;
struct vsp1_rwpf;

struct vsp1_rwpf {
	struct vsp1_entity entity;
	struct vsp1_dl_manager *dlm;
};

static inline struct vsp1_rwpf *entity_to_rwpf(struct vsp1_entity *entity)
{
	return container_of(entity, struct vsp1_rwpf, entity);
}

struct vsp1_rwpf *vsp1_rpf_create(struct vsp1_device *vsp1);
struct vsp1_rwpf *vsp1_wpf_create(struct vsp1_device *vsp1);

#endif /* __VSP1_RWPF_H__ */
