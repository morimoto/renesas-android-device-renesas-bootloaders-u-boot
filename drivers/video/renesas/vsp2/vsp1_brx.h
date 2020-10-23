/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1_brx.h  --  R-Car VSP1 Blend ROP Unit (BRU and BRS)
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_BRX_H__
#define __VSP1_BRX_H__

#include "vsp1_entity.h"

struct vsp1_brx {
	struct vsp1_entity entity;
};

struct vsp1_brx *vsp1_brx_create(struct vsp1_device *vsp1);

#endif /* __VSP1_BRX_H__ */
