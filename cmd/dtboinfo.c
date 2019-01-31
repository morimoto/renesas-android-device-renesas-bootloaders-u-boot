// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 GlobalLogic
 */

#include <command.h>
#include <common.h>
#include <device-tree-common.h>

int do_dtboinfo(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	/* Default slot is '_a' */
	int i = 0;
	int dt_count = 0;
	const int dev = 1;
	const char *slot_name = "dtbo_a";
	struct blk_desc *dev_desc = NULL;
	struct device_tree_info *dt_info = NULL;

	if (argc < 1 || argc > 2)
		return CMD_RET_USAGE;

	if (argc == 2) {
		if (!strncmp(argv[1], "a", 1))
			slot_name = "dtbo_a";
		else if (!strncmp(argv[1], "b", 1))
			slot_name = "dtbo_b";
		else
			return CMD_RET_USAGE;
	}

	dev_desc = blk_get_dev("mmc", dev);
	if (!dev_desc) {
		printf("Failed to get the device descriptor\n");
		return CMD_RET_FAILURE;
	}

	dt_info = get_dt_info(load_dt_table_from_part(dev_desc, slot_name), &dt_count);

	printf("Available overlays:\n\n");

	printf("\tIndex %14s %18s", "Name", "Board Id\n\n");

	for (i = 0; i < dt_count; ++i)
		printf("\t  %d  -  %16s  -  0x%x\n", dt_info[i].idx, dt_info[i].name,
			cpu_to_fdt32(dt_info[i].entry->id));

	free_dt_info(dt_info, dt_count);

	return 0;
}

static char dtboinfo_help_text[] =
	"Prints information about dtbo partition";

U_BOOT_CMD(
	dtboinfo,  CONFIG_SYS_MAXARGS, 1,  do_dtboinfo,
	"dtboinfo [a|b]", dtboinfo_help_text
);
