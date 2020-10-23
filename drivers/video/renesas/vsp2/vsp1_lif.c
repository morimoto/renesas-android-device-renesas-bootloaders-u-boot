// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_lif.c  --  R-Car VSP1 LCD Controller Interface
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2013-2018 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include "vsp1_lif.h"
#include "vsp1.h"
#include "vsp1_dl.h"

static inline void vsp1_lif_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	vsp1_dl_body_write(dlb, reg + 0 * VI6_LIF_OFFSET, data);
}

static void lif_configure_stream(struct vsp1_dl_body *dlb)
{
	unsigned int obth = 3000;

	vsp1_lif_write(dlb, VI6_LIF_CSBTH,
			(0 << VI6_LIF_CSBTH_HBTH_SHIFT) |
			(0 << VI6_LIF_CSBTH_LBTH_SHIFT));

	/* LIF enable, DU is selected */
	vsp1_lif_write(dlb, VI6_LIF_CTRL,
			(obth << VI6_LIF_CTRL_OBTH_SHIFT) |
			VI6_LIF_CTRL_REQSEL | VI6_LIF_CTRL_LIF_EN);
}

static const struct vsp1_entity_operations lif_entity_ops = {
	.configure_stream = lif_configure_stream,
};

struct vsp1_lif *vsp1_lif_create(struct vsp1_device *vsp1)
{
	static struct vsp1_lif lif;
	memset(&lif, 0, sizeof(struct vsp1_lif));
	lif.entity.ops = &lif_entity_ops;
	lif.entity.type = VSP1_ENTITY_LIF;
	vsp1_entity_init(vsp1, &lif.entity);
	return &lif;
}
