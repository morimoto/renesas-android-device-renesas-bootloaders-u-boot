/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1_lif.h  --  R-Car VSP1 LCD Controller Interface
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2014 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_LIF_H__
#define __VSP1_LIF_H__

#include "vsp1_entity.h"

struct vsp1_lif {
	struct vsp1_entity entity;
};

struct vsp1_lif *vsp1_lif_create(struct vsp1_device *vsp1);

#endif /* __VSP1_LIF_H__ */
