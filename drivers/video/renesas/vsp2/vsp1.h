/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1.h  --  R-Car VSP1 Driver
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_H__
#define __VSP1_H__

#include <linux/io.h>
#include <linux/list.h>
#include <linux/compat.h>
#include "vsp1_regs.h"

struct vsp1_drm;
struct vsp1_entity;
struct vsp1_brx;
struct vsp1_lif;
struct vsp1_rwpf;

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

struct vsp1_device {
	struct vsp1_brx *bru;
	struct vsp1_lif *lif;
	struct vsp1_rwpf *rpf;
	struct vsp1_rwpf *wpf;

	struct list_head entities;
	struct vsp1_drm *drm;
};

u32 vsp1_read(u32 reg);
void vsp1_write(u32 reg, u32 data);
int vsp1_reset_wpf(void);
int vsp1_device_init(void);

#endif /* __VSP1_H__ */
