/*
 * rcar-fcp.c  --  R-Car Frame Compression Processor Driver
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2016 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <common.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/compat.h>
#include <rcar-logo.h>

#ifndef BIT
#define BIT(x)					(1 << (x))
#endif

#define RCAR_FCPVD1_BASE_REG	0xFEA2F000

/* No need to set these regs, it starts if VSPD accesses bus */
#define FCP_VCR					0x00
#define FCP_CFG0				0x04
#define FCP_CFG0_MODE			BIT(1) /* FCPV Mode Select (default - FCPVD) */
#define FCP_RST					0x10
#define FCP_RST_SOFTRST			BIT(0) /* Stops the FCPV module */
#define FCP_STA					0x18
#define FCP_STA_ACT				BIT(0) /* Indicates the FCPVD is active */

static inline void fcp_write(u32 value, u32 offset)
{
	iowrite32(value,(void __iomem *)
			(unsigned long)(RCAR_FCPVD1_BASE_REG + offset));
}

static inline u32 fcp_read(u32 offset)
{
	return ioread32((void __iomem *)
			(unsigned long)(RCAR_FCPVD1_BASE_REG + offset));
}

int rcar_fcp_reset(void)
{
	unsigned int timeout;
	u32 status;

	/* Stop operation of FCPV with software reset */
	fcp_write(FCP_RST_SOFTRST, FCP_RST);

	/* Wait until FCP_STA.ACT become 0 */
	for (timeout = 10; timeout > 0; --timeout) {
		status = fcp_read(FCP_STA);
		if (!(status & FCP_STA_ACT))
			break;
		udelay(2000);
	}

	if (!timeout) {
		printf("%s: Failed to reset FCP1\n", __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

