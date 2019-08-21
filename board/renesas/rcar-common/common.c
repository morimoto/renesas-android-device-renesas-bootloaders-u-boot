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
#include <dm.h>
#include <dm/uclass-internal.h>
#include <asm/arch/rmobile.h>
#include <mmc.h>

#ifdef CONFIG_RCAR_GEN3

DECLARE_GLOBAL_DATA_PTR;

/* If the firmware passed a device tree use it for U-Boot DRAM setup. */
extern u64 rcar_atf_boot_args[];

int dram_init(void)
{
	const void *atf_fdt_blob = (const void *)(rcar_atf_boot_args[1]);
	const void *blob;

	/* Check if ATF passed us DTB. If not, fall back to builtin DTB. */
	if (fdt_magic(atf_fdt_blob) == FDT_MAGIC)
		blob = atf_fdt_blob;
	else
		blob = gd->fdt_blob;

	return fdtdec_setup_mem_size_base_fdt(blob);
}

int dram_init_banksize(void)
{
	const void *atf_fdt_blob = (const void *)(rcar_atf_boot_args[1]);
	const void *blob;

	/* Check if ATF passed us DTB. If not, fall back to builtin DTB. */
	if (fdt_magic(atf_fdt_blob) == FDT_MAGIC)
		blob = atf_fdt_blob;
	else
		blob = gd->fdt_blob;

	fdtdec_setup_memory_banksize_fdt(blob);

	return 0;
}

#if CONFIG_IS_ENABLED(OF_BOARD_SETUP) && CONFIG_IS_ENABLED(PCI)
int ft_board_setup(void *blob, bd_t *bd)
{
	struct udevice *dev;
	struct uclass *uc;
	fdt_addr_t regs_addr;
	int i, off, ret;

	ret = uclass_get(UCLASS_PCI, &uc);
	if (ret)
		return ret;

	uclass_foreach_dev(dev, uc) {
		struct pci_controller hose = { 0 };

		for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
			if (hose.region_count == MAX_PCI_REGIONS) {
				printf("maximum number of regions parsed, aborting\n");
				break;
			}

			if (bd->bi_dram[i].size) {
				pci_set_region(&hose.regions[hose.region_count++],
					       bd->bi_dram[i].start,
					       bd->bi_dram[i].start,
					       bd->bi_dram[i].size,
					       PCI_REGION_MEM |
					       PCI_REGION_PREFETCH |
					       PCI_REGION_SYS_MEMORY);
			}
		}

		regs_addr = devfdt_get_addr_index(dev, 0);
		off = fdt_node_offset_by_compat_reg(blob,
				"renesas,pcie-rcar-gen3", regs_addr);
		if (off < 0) {
			printf("Failed to find PCIe node@%llx\n", regs_addr);
			return off;
		}

		fdt_pci_dma_ranges(blob, off, &hose);
	}

	return 0;
}

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
#endif
#endif
