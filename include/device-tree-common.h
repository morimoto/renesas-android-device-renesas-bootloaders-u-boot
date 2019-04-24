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
#include <android_image.h>

#define MAX_OVERLAYS		100
#define MAX_DEFAULT_OVERLAYS	10

#if defined(CONFIG_R8A7795)
#if defined(CONFIG_TARGET_ULCB)
#define	V3_PLATID_DTBO_NAME		"skkf-4x2g-30"
#define	V2_PLATID_DTBO_NAME		"skkf-4x2g-20"
#define	V3_INFO_DTBO_NAME		"skkf-v3"
#define	V2_INFO_DTBO_NAME		"skkf-v2"
#define H3v3_PLAT_ID			0x0b779530
#define H3v2_PLAT_ID			0x0b779520
#elif defined(CONFIG_TARGET_SALVATOR_X)
#define	V3_PLATID_DTBO_NAME		"salv-4x2g-30"
#define	V2_PLATID_DTBO_NAME		"salv-4x2g-20"
#define	V3_INFO_DTBO_NAME		"salv-v3"
#define	V2_INFO_DTBO_NAME		"salv-v2"
#define H3v3_PLAT_ID			0x04779530
#define H3v2_PLAT_ID			0x04779520
#endif
#endif

#define AVB_DTBO_NAME			"rcar-avb"

#if defined(CONFIG_TARGET_ULCB)
#define	ADSP_SKKF_DTBO_NAME		"skkf-adsp"
#elif defined(CONFIG_TARGET_SALVATOR_X)
#define ADSP_SALV_DTBO_NAME		"salv-adsp"
#endif

int load_dt_with_overlays(struct fdt_header *load_addr,
				struct dt_table_header *dt_tbl,
				struct dt_table_header *dto_tbl);

void *load_dt_table_from_part(struct blk_desc *dev_desc,
				const char *dtb_part_name);

void *load_dt_table_from_bootimage(struct andr_img_hdr *hdr);

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

struct device_tree_info {
	struct dt_table_entry *entry;
	char *name;
	int idx;
	bool is_load;
};

struct dt_overlays {
	struct device_tree_info *info;
	struct dt_table_header *dt_header;
	int *order_idx;
	int applied_cnt;
	int size;
};

struct device_tree_info *get_dt_info(struct dt_table_header *dt_tbl, int *size);
void free_dt_info(struct device_tree_info *info, int num);

#endif /* _DEVICE_TREE_COMMON_H */
