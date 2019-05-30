/*
 * Copyright (C) 2016 GlobalLogic
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <errno.h>
#include <version.h>
#include <malloc.h>
#include <part.h>
#include <fastboot.h>
#include <part.h>
#include <div64.h>
#include <android/bootloader.h>
#include <linux/ctype.h>
#include <environment.h>

#ifndef CONFIG_RANDOM_UUID
#error "CONFIG_RANDOM_UUID must be enabled for oem partitionng"
#endif

/* For correct work of AVB the heap size (which is defined by CONFIG_SYS_MALLOC_LEN
 * constant in include/configs/rcar-gen3-common.h) should be set higher  than  the
 * summarized value of the 'oem_partition_table' array [boot_a + boot_b  +  dtb_a +
 * dtb_b + vbmeta_a + vbmeta_b], if ANDROID_MMC_ONE_SLOT wasn't defined. The size of
 * a heap should be  set higher than the summarized value of the 'oem_partition_table'
 * array [boot + dtb + vbmeta] if ANDROID_MMC_ONE_SLOT was defined.
 */
#ifdef ANDROID_MMC_ONE_SLOT
const struct oem_part_info oem_partition_table[FASTBOOT_OEM_PARTITIONS] = {
    /* Name             Slot    F/S type    Size */
    { "misc",           NULL,   "raw",      524288          },
    { "pst",            NULL,   "raw",      524288          },
    { "vbmeta",         "_a",   "raw",      524288          },
    { "dtbo",           "_a",   "raw",      524288          },
    { "boot",           "_a",   "raw",      33554432        },
    { "metadata",       NULL,   "raw",      16777216        },
    { "system",         "_a",   "ext4",     2357198848      },
    { "vendor",         "_a",   "ext4",     268435456       },
    { "product",        "_a",   "ext4",     524288000       },
    { "odm",            "_a",   "ext4",     131072000       },
    { "userdata",       NULL,   "ext4",     0               }
};
#else /* !ANDROID_MMC_ONE_SLOT (A/B slots) */
const struct oem_part_info oem_partition_table[FASTBOOT_OEM_PARTITIONS] = {
    /* Name             Slot    F/S type    Size */
    { "misc",           NULL,   "raw",      524288          },
    { "pst",            NULL,   "raw",      524288          },
    { "vbmeta",         "_a",   "raw",      524288          },
    { "vbmeta",         "_b",   "raw",      524288          },
    { "dtbo",           "_a",   "raw",      524288          },
    { "dtbo",           "_b",   "raw",      524288          },
    { "boot",           "_a",   "raw",      33554432        },
    { "boot",           "_b",   "raw",      33554432        },
    { "metadata",       NULL,   "raw",      16777216        },
    { "system",         "_a",   "ext4",     2357198848      },
    { "system",         "_b",   "ext4",     2357198848      },
    { "vendor",         "_a",   "ext4",     268435456       },
    { "vendor",         "_b",   "ext4",     268435456       },
    { "product",        "_a",   "ext4",     524288000       },
    { "product",        "_b",   "ext4",     524288000       },
    { "odm",            "_a",   "ext4",     131072000       },
    { "odm",            "_b",   "ext4",     131072000       },
    { "userdata",       NULL,   "ext4",     0               }
};
#endif

static int oem_dump_help(char *response)
{
	fastboot_send_response("INFO  flash <IPL>  - flash specified IPL:");
	fastboot_send_response("INFO");
	fastboot_send_response("INFO  erase 	   - erase secure store.");
	fastboot_send_response("INFO  format	   - create new GPT partition table on eMMC.");
	fastboot_send_response("INFO  lock_status  - show lock status of the IPL and eMMC.");
	fastboot_send_response("INFO  setenv NAME <VALUE>  - set environment variable.\n");
	fastboot_send_response("INFO  setenv default	   - set environment variables to default.\n");
	fastboot_okay(NULL, response);
	return 0;
}

static int oem_format(char *response)
{
	struct blk_desc *blkdev = blk_get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	struct disk_partition *parts;
	int i, num_parts = ARRAY_SIZE(oem_partition_table);
	char uuid[64], name[32];

	if (!blkdev || blkdev->type == DEV_TYPE_UNKNOWN) {
		fastboot_fail("FAIL eMMC device not found!", response);
		return -1;
	}

	fastboot_send_response("INFO eMMC: %s %s %s %zuMiB",
		blkdev->vendor, blkdev->product, blkdev->revision,
		(blkdev->lba * blkdev->blksz) / 1024 / 1024);

	parts = calloc(sizeof(disk_partition_t), num_parts);
	if (parts == NULL) {
		printf("%s: Unable to allocate memory for new partition table, %lu bytes",
			__func__, num_parts * sizeof(disk_partition_t));
		return -1;
	}

	/* Set offset for first partition, reserved space 512KiB */
	parts[0].start = (524288 / blkdev->blksz);

	/* Construct partition table */
	for (i = 0; i < num_parts; i++)
	{
		size_t blks, size = oem_partition_table[i].size;

		/* Calculate size of partition in blocks */
		blks = (size / blkdev->blksz);
		if ((size % blkdev->blksz) != 0) {
			blks++;
		}
		parts[i].size = blks;

		/* Set partition UUID */
		gen_rand_uuid_str(parts[i].uuid, UUID_STR_FORMAT_STD);

		/* Copy partition name */
		strcpy((char*)parts[i].name, oem_partition_table[i].name);

		/* Append slot if exist */
		if (oem_partition_table[i].slot != NULL) {
			strcat((char*)parts[i].name, oem_partition_table[i].slot);
		}
	}

	/* Gen disk UUID */
	gen_rand_uuid_str(uuid, UUID_STR_FORMAT_STD);

	/* Save partitions layout to disk */
	if (gpt_restore(blkdev, uuid, parts, num_parts) < 0) {
		fastboot_fail(" Save partitions layout to disk failed", response);
		free(parts);
		return -1;
	}

	free(parts);

	fastboot_send_response("INFO Created new GPT partition table:");

	for (i = 0; i < num_parts; i++) {
		disk_partition_t info;

		strcpy(name, oem_partition_table[i].name);

		/* Append slot if exist */
		if (oem_partition_table[i].slot != NULL) {
			strcat(name, oem_partition_table[i].slot);
		}

		if (part_get_info_by_name(blkdev, name, &info) >= 0) {
		fastboot_send_response("INFO     /%s (%zu KiB, %s)",
			info.name, (info.size * blkdev->blksz) / 1024, oem_partition_table[i].fs);
		} else {
			fastboot_send_response("INFO     /%s (ERROR unable to get info)",
				oem_partition_table[i].name);
		}
	}

	fastboot_okay(NULL, response);
	return 0;
}

static int oem_erase(char *response)
{
	struct bootloader_message bcb;

	if(get_bootloader_message(&bcb)) {
		fastboot_fail(" load bootloader control block", response);
		return -1;
	}

	memset(&bcb.command, 0, sizeof(bcb.command));
	memset(&bcb.recovery, 0, sizeof(bcb.recovery));

	strlcpy(bcb.command, "boot-recovery", sizeof(bcb.command));

	snprintf(bcb.recovery, sizeof(bcb.recovery),
		"recovery\n" \
		"--erase_sstdata\n"
	);

	if(set_bootloader_message(&bcb) < 0) {
		fastboot_fail(" write bootloader control block", response);
		return -1;
	}
	fastboot_set_reset_completion();
	fastboot_okay(NULL, response);
	return INT_MAX; /* Reboot after exit */
}

static int oem_lock_status(char *response)
{
	int ipl_locked = 0, mmc_locked = 0;

	if (!fastboot_get_lock_status(&mmc_locked, &ipl_locked)) {
		fastboot_send_response("INFO IPL %s", ipl_locked ? "locked" : "unlocked");
		fastboot_send_response("INFO MMC %s", mmc_locked ? "locked" : "unlocked");
	}
	fastboot_okay(NULL, response);
	return 0;
}

static int oem_setenv(char * varval, char *response)
{
	const char * key;

	if (!strcmp(varval, "default")) {
		set_default_env("## Resetting to default environmens \n", 0);
		if (env_save()) {
			fastboot_fail("Save environment variable error", response);
			return -1;
		}
		/* Set proper platform in environment */
		rcar_preset_env();
		fastboot_okay(NULL, response);
		return 0;
	}

	key = strsep(&varval, " ");
	if (!key) {
		fastboot_fail("You must specify the name of the environment variable", response);
		return -1;
	}
	if (varval && *varval != '\0') {
		fastboot_send_response("INFOsetting env %s=%s %d", key, varval, strlen(varval));
	} else {
		fastboot_send_response("INFOerase env %s", key);
	}
	env_set(key, varval);
	if (env_save()) {
		fastboot_response("FAIL", response, "Save environment variable '%s' error", key);
		return -1;
	}
	fastboot_okay(NULL, response);
	return 0;
}

void fastboot_cb_oem(char *cmd, char *response)
{
	int ipl_locked = 0, mmc_locked = 0;
	char *cmdarg;

	strsep(&cmd, " ");
	cmdarg = cmd;
	strsep(&cmdarg, " ");
	if (!strcmp(cmd, "help")) {
		oem_dump_help(response);
		return;
	}

	if (!strcmp(cmd, "lock_status")) {
		oem_lock_status(response);
		return;
	}

	fastboot_get_lock_status(&mmc_locked, &ipl_locked);

	if (!strcmp(cmd, "format")) {
		if (mmc_locked || ipl_locked) {
			fastboot_fail("flash device locked", response);
			return;
		}
		oem_format(response);
		return;
	}

	if (!strcmp(cmd, "erase")) {
		if (mmc_locked || ipl_locked) {
			fastboot_fail("erase device locked", response);
			return;
		}
		oem_erase(response);
		return;
	}

	if (!strcmp(cmd, "setenv")) {
		if (cmdarg == NULL) {
			fastboot_fail("oem setenv command requires one or two arguments", response);
			return;
		}
		oem_setenv(cmdarg, response);
		return;
	}
	fastboot_response("FAIL", response, " unsupported oem command: %s", cmd);
}
