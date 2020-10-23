/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1_drm.h  --  R-Car VSP1 DRM/KMS Interface
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2015-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_DRM_H__
#define __VSP1_DRM_H__

#include <linux/compat.h>
#include "vsp1.h"
#include "vsp1_pipe.h"

struct vsp1_drm_pipeline {
	struct vsp1_pipeline pipe;
};

struct vsp1_drm {
	struct vsp1_drm_pipeline pipe;
};

void vsp1_drm_init(struct vsp1_device *vsp1);
int vsp1_du_setup_lif(bool enable);
int vsp1_du_atomic_flush(void);

#endif /* __VSP1_DRM_H__ */
