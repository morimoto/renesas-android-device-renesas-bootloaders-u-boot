// SPDX-License-Identifier: GPL-2.0
/*
 * board/renesas/rcar-common/common.c
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 * Copyright (C) 2013 Nobuhiro Iwamatsu <nobuhiro.iwamatsu.yj@renesas.com>
 * Copyright (C) 2015 Nobuhiro Iwamatsu <iwamatsu@nigauri.org>
 */

#include <common.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/rmobile.h>
#include <asm/arch/rcar-mstp.h>
#include <mmc.h>

#define TSTR0		0x04
#define TSTR0_STR0	0x01

#if !defined(CONFIG_R8A7795) && \
	!defined(CONFIG_R8A7796) && \
		!defined(CONFIG_R8A77965)
static struct mstp_ctl mstptbl[] = {
	{ SMSTPCR0, MSTP0_BITS, CONFIG_SMSTP0_ENA,
		RMSTPCR0, MSTP0_BITS, CONFIG_RMSTP0_ENA },
	{ SMSTPCR1, MSTP1_BITS, CONFIG_SMSTP1_ENA,
		RMSTPCR1, MSTP1_BITS, CONFIG_RMSTP1_ENA },
	{ SMSTPCR2, MSTP2_BITS, CONFIG_SMSTP2_ENA,
		RMSTPCR2, MSTP2_BITS, CONFIG_RMSTP2_ENA },
	{ SMSTPCR3, MSTP3_BITS, CONFIG_SMSTP3_ENA,
		RMSTPCR3, MSTP3_BITS, CONFIG_RMSTP3_ENA },
	{ SMSTPCR4, MSTP4_BITS, CONFIG_SMSTP4_ENA,
		RMSTPCR4, MSTP4_BITS, CONFIG_RMSTP4_ENA },
	{ SMSTPCR5, MSTP5_BITS, CONFIG_SMSTP5_ENA,
		RMSTPCR5, MSTP5_BITS, CONFIG_RMSTP5_ENA },
#ifdef CONFIG_RCAR_GEN3
	{ SMSTPCR6, MSTP6_BITS, CONFIG_SMSTP6_ENA,
		RMSTPCR6, MSTP6_BITS, CONFIG_RMSTP6_ENA },
#endif
	{ SMSTPCR7, MSTP7_BITS, CONFIG_SMSTP7_ENA,
		RMSTPCR7, MSTP7_BITS, CONFIG_RMSTP7_ENA },
	{ SMSTPCR8, MSTP8_BITS, CONFIG_SMSTP8_ENA,
		RMSTPCR8, MSTP8_BITS, CONFIG_RMSTP8_ENA },
	{ SMSTPCR9, MSTP9_BITS, CONFIG_SMSTP9_ENA,
		RMSTPCR9, MSTP9_BITS, CONFIG_RMSTP9_ENA },
	{ SMSTPCR10, MSTP10_BITS, CONFIG_SMSTP10_ENA,
		 RMSTPCR10, MSTP10_BITS, CONFIG_RMSTP10_ENA },
	{ SMSTPCR11, MSTP11_BITS, CONFIG_SMSTP1_ENA,
		 RMSTPCR11, MSTP11_BITS, CONFIG_RMSTP11_ENA },
};

void arch_preboot_os(void)
{
	int i;

	/* stop TMU0 */
	mstp_clrbits_le32(TMU_BASE + TSTR0, TMU_BASE + TSTR0, TSTR0_STR0);

	/* Stop module clock */
	for (i = 0; i < ARRAY_SIZE(mstptbl); i++) {
		mstp_setclrbits_le32((uintptr_t)mstptbl[i].s_addr,
				     mstptbl[i].s_dis,
				     mstptbl[i].s_ena);
		mstp_setclrbits_le32((uintptr_t)mstptbl[i].r_addr,
				     mstptbl[i].r_dis,
				     mstptbl[i].r_ena);
	}
}
#endif

void rcar_preset_env(void)
{
	u32 product = rmobile_get_cpu_type();
	switch (product) {
		case CPU_ID_R8A7795:
			env_set("platform", "r8a7795");
			break;
		case CPU_ID_R8A7796:
			env_set("platform", "r8a7796");
			break;
		case CPU_ID_R8A77965:
			env_set("platform", "r8a77965");
			break;
		case CPU_ID_R8A77990:
			env_set("platform", "r8a77990");
			break;
		case CPU_ID_R8A77995:
			env_set("platform", "r8a77995");
			break;
	}
	return;
}

struct env_pair {
	const char *var_name;
	char *value;
};

/* Variables, that can not reset by 'env default' command */
static struct env_pair env_pairs[] = {
	{ "ethaddr", NULL},
	{ "ethact",  NULL},
	{ "board_id", NULL},
	{ "serialno", NULL},
};

void init_noreset_vars(void) {
	int i = 0;
	char *buf = NULL;
	int n = sizeof(env_pairs) / sizeof(struct env_pair);

	for (; i < n; ++i) {
		if (env_pairs[i].value)
			free(env_pairs[i].value);
		buf = env_get(env_pairs[i].var_name);
		if (buf) {
			env_pairs[i].value = (char *)malloc((strlen(buf) + 1) * sizeof(char));
			if (env_pairs[i].value)
				memcpy(env_pairs[i].value, buf, (strlen(buf) + 1));
			else
				goto free_mem;
		} else {
			env_pairs[i].value = NULL;
		}
	}

	return;

free_mem:
	i--;
	for (; i >= 0; i--) {
		if (env_pairs[i].value)
			free(env_pairs[i].value);
	}

	printf("## ERROR - not enough memory for saving variable(s), that can not reset\n");
}

void restore_noreset_vars(void) {
	int i = 0;
	int n = sizeof(env_pairs) / sizeof(struct env_pair);

	for (; i < n; ++i)
		env_set(env_pairs[i].var_name, env_pairs[i].value);
}

static unsigned get_boot_part_free_space(void)
{
	struct mmc *mmc = find_mmc_device(CONFIG_FASTBOOT_FLASH_MMC_DEV);
	return mmc->capacity_boot - CONFIG_ENV_SIZE;
}

/*
 * The bootloader size can be changed in runtime
 * by setting value in variable @ipl_image_size.
 * If this environment variable is not set or
 * value bigger than avaliable free memory in
 * boot partition or less than @CONFIG_DEFAULT_IPL_IMAGE_SIZE,
 * then it will use @CONFIG_DEFAULT_IPL_IMAGE_SIZE.
 */
unsigned get_bootloader_size(void)
{
	const char *bl_size_env = "ipl_image_size";
	const char *bl_size_val = NULL;
	unsigned bootloader_size = 0;
	unsigned free_space = get_boot_part_free_space();

	if (free_space < CONFIG_DEFAULT_IPL_IMAGE_SIZE) {
		printf("The CONFIG_DEFAULT_IPL_IMAGE_SIZE (%d) is bigger than avaliable "
			"free space. Using all avaliable space (%d)\n",
			CONFIG_DEFAULT_IPL_IMAGE_SIZE, free_space);
		return free_space;
	}

	bl_size_val = env_get(bl_size_env);
	if (!bl_size_val)
		return CONFIG_DEFAULT_IPL_IMAGE_SIZE;

	bootloader_size = strtoul(bl_size_val, NULL, 16);

	if (bootloader_size < CONFIG_DEFAULT_IPL_IMAGE_SIZE) {
		printf("Too small value (%d) for bootloader partition, using default value (%d)\n",
			bootloader_size, CONFIG_DEFAULT_IPL_IMAGE_SIZE);
		return CONFIG_DEFAULT_IPL_IMAGE_SIZE;
	}

	if (bootloader_size > free_space) {
		printf("The %d is bigger than available free space in boot hw part. "
			"Using all avaliable space (%d)\n", bootloader_size, free_space);
		return free_space;
	}

	return bootloader_size;
}

#ifdef CONFIG_OPTEE
/*Set legal address and maximum size for every image*/
static struct img_param img_params[IMG_MAX_NUM] = {
	[IMG_PARAM] = {
		.img_id = IMG_PARAM,
		.img_name = "param",
		.start_addr = 0,
		.total_size = 1 * HF_SECTOR_SIZE,
		.sectors_num = 1,
	},
	[IMG_IPL2] = {
		.img_id = IMG_IPL2,
		.img_name = "bl2",
		.start_addr = 0x40000,
		.total_size = 1 * HF_SECTOR_SIZE,
		.sectors_num = 1,
	},
	[IMG_CERT] = {
		.img_id = IMG_CERT,
		.img_name = "cert",
		.start_addr = 0x180000,
		.total_size = 1 * HF_SECTOR_SIZE,
		.sectors_num = 1,
	},
	[IMG_BL31] = {
		.img_id = IMG_BL31,
		.img_name = "bl31",
		.start_addr = 0x1C0000,
		.total_size = 1 * HF_SECTOR_SIZE,
		.sectors_num = 1,
	},
	[IMG_OPTEE] = {
		.img_id = IMG_OPTEE,
		.img_name = "optee",
		.start_addr = 0x200000,
		.total_size = 2 * HF_SECTOR_SIZE,
		.sectors_num = 2,
	},
	[IMG_UBOOT] = {
		.img_id = IMG_UBOOT,
		.img_name = "uboot",
		.start_addr = 0x640000,
		.total_size = 5 * HF_SECTOR_SIZE,
		.sectors_num = 5,
	},
	[IMG_SSTDATA] = {
		.img_id = IMG_SSTDATA,
		.img_name = "ssdata",
		.start_addr = 0x300000,
		.total_size = 4 * HF_SECTOR_SIZE,
		.sectors_num = 4,
	},
};

/**
 * get_img_params() - Returns pointer to img_param structire
 * @image_id:	Image id
 * Returns pointer to img_param structure on success or NULL on failure.
 */
struct img_param *get_img_params(enum hf_images image_id)
{
	if (image_id >= IMG_MAX_NUM)
		return NULL;

	return &img_params[image_id];
}

#endif