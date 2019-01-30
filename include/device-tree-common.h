// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 GlobalLogic
 */

#ifndef _DEVICE_TREE_COMMON_H
#define _DEVICE_TREE_COMMON_H

#include <common.h>
#include <mmc.h>
#include <dt_table.h>
#include <malloc.h>
#include <fastboot.h>
#include <dm/uclass.h>
#include <i2c.h>
#include <configs/rcar-gen3-common.h>

#if defined(CONFIG_R8A7795)
#if defined(CONFIG_TARGET_ULCB)
#define	V3_PLATID_DTBO_NUM		0
#define	V2_PLATID_DTBO_NUM		1
#define	V3_INFO_DTBO_NUM		2
#define	V2_INFO_DTBO_NUM		3
#define H3v3_PLAT_ID			0x0b779530
#define H3v2_PLAT_ID			0x0b779520
#elif defined(CONFIG_TARGET_SALVATOR_X)
#define	V3_PLATID_DTBO_NUM		0
#define	V2_PLATID_DTBO_NUM		1
#define	V3_INFO_DTBO_NUM		2
#define	V2_INFO_DTBO_NUM		3
#define H3v3_PLAT_ID			0x04779530
#define H3v2_PLAT_ID			0x04779520
#endif
#endif

#define	ADSP_DTBO_NUM			4
#define	PARTITIONS_DTBO_NUM		5
#define	LVDS_PANEL_DTBO_NUM		6

int load_dt_with_overlays(struct fdt_header *load_addr,
				struct dt_table_header *dt_tbl,
				struct dt_table_header *dto_tbl);

void *load_dt_table_from_part(struct blk_desc *dev_desc,
				const char *dtbo_part_name);

/*
 * Device tree ID <board id><SiP><revision>
 * Board ID:
 *   0x00 Salvator-X
 *   0x02 StarterKit Pro
 *   0x04 Salvator-XS
 *   0x05 Salvator-MS
 *   0x0A Salvator-M
 *   0x0B StarterKit Premier
 */
ulong get_current_plat_id(void);

#endif /* _DEVICE_TREE_COMMON_H */