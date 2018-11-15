
/*
 * (C) Copyright 2018, Linaro Limited
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_AVB_VERIFY_H
#define _AVB_VERIFY_H

#include <../lib/libavb/libavb.h>
#include <avb_ab/libavb_ab.h>

#include <mmc.h>

enum CMDLINE_FLAGS {
	LOAD_OLD_ARGS = 1,
	LOAD_AVB_ARGS = 2,
};


#define AVB_MAX_ARGS			1024
#define VERITY_TABLE_OPT_RESTART	"restart_on_corruption"
#define VERITY_TABLE_OPT_LOGGING	"ignore_corruption"
#define ALLOWED_BUF_ALIGN		8

enum avb_boot_state {
	AVB_GREEN,
	AVB_YELLOW,
	AVB_ORANGE,
	AVB_RED,
};

struct AvbOpsData {
	struct AvbOps ops;
	struct AvbABOps ab_ops;
	int mmc_dev;
	enum avb_boot_state boot_state;
};

struct mmc_part {
	int dev_num;
	struct mmc *mmc;
	struct blk_desc *mmc_blk;
	disk_partition_t info;
};

enum mmc_io_type {
	IO_READ,
	IO_WRITE
};

AvbOps *avb_ops_alloc(int boot_device);
void avb_ops_free(AvbOps *ops);

char *avb_set_state(AvbOps *ops, enum avb_boot_state boot_state);
char *avb_set_enforce_verity(const char *cmdline);
char *avb_set_ignore_corruption(const char *cmdline);

char *append_cmd_line(char *cmdline_orig, char *cmdline_new);

/**
 * ============================================================================
 * I/O helper inline functions
 * ============================================================================
 */
static inline uint64_t calc_partition_size(struct mmc_part *part)
{
	if (!part)
		return 0;

	return (uint64_t) part->info.size * part->info.blksz;
}

static inline uint64_t calc_offset(struct mmc_part *part, int64_t offset)
{
	uint64_t part_size = calc_partition_size(part);

	if (offset < 0)
		return part_size + offset;

	return offset;
}

static inline size_t get_sector_buf_size(void)
{
	return (size_t)CONFIG_LIBAVB_BUF_SIZE;
}

static inline void *get_sector_buf(void)
{
	return (void *)CONFIG_LIBAVB_BUF_ADDR;
}

static inline bool is_buf_unaligned(void *buffer)
{
	return (bool)((uintptr_t)buffer % ALLOWED_BUF_ALIGN);
}

static inline int get_boot_device(AvbOps *ops)
{
	struct AvbOpsData *data;

	if (ops) {
		data = ops->user_data;
		if (data)
			return data->mmc_dev;
	}

	return -1;
}

/*Added for RCar boards support*/

char *prepare_bootcmd_compat(AvbOps *ops,
						int boot_device,
						AvbABFlowResult ab_result,
						bool unlocked,
						AvbSlotVerifyData *slot_data,
						unsigned int flags
						);


#define set_compat_args(dev) \
		prepare_bootcmd_compat(NULL, dev, 0, false, NULL, LOAD_OLD_ARGS)


int avb_set_active_slot(unsigned int slot);

/*This is generic functions of Read/Write from/to partition that doesn't containg
*libvb related parameters
*/
int read_from_part(int mmc_dev,
				const char *partition,
				int64_t offset,
				size_t num_bytes,
				void *buffer,
				size_t *out_num_read);

int write_to_part(int mmc_dev,
				const char *partition,
				int64_t offset,
				size_t num_bytes,
				const void *buffer);

uint64_t get_part_size(int mmc_dev, const char *partition);

#endif /* _AVB_VERIFY_H */
