/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * vsp1_pipe.h  --  R-Car VSP1 Pipeline
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#ifndef __VSP1_PIPE_H__
#define __VSP1_PIPE_H__

#include <linux/compat.h>
#include <linux/list.h>

struct vsp1_rwpf;

/*  Four-character-code (FOURCC) */
#define v4l2_fourcc(a, b, c, d)\
	((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))

/*      Pixel format         FOURCC                          depth  Description  */
#define V4L2_PIX_FMT_XBGR32  v4l2_fourcc('X', 'R', '2', '4') /* 32  BGRX-8-8-8-8  */
#define MEDIA_BUS_FMT_ARGB8888_1X32		0x100d

/* Flags */
#define V4L2_PIX_FMT_FLAG_PREMUL_ALPHA	0x00000001

struct vsp1_pipeline {
	struct vsp1_rwpf *inputs;
	struct vsp1_rwpf *output;
	struct vsp1_entity *brx;
	struct vsp1_entity *lif;

	struct list_head entities;
};

void vsp1_pipeline_run(void);
int vsp1_pipeline_stop(struct vsp1_pipeline *pipe);

#endif /* __VSP1_PIPE_H__ */
