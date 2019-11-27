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
#ifdef CONFIG_OPTEE
#include <s_record.h>
#include <tee.h>
#include <u-boot/crc.h>
#endif /* CONFIG_OPTEE */

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

#define BIN_NAME_EXT ".bin"
#define SREC_NAME_EXT ".srec"

/**
 * get_image_type() - Returns image type by name
 * @name:	Pointer to image name
 *
 * Returns nonzero value on success or @IMG_TYPE_UNKNOWN (0) on failure.
 * The first 2 least significant bytes contain info about
 * image number (from 0 to @IMG_MAX_NUM ).
 * According to image type detected by name, @IMG_BIN_MASK or
 * @IMG_SREC_MASK will be applied.
 */
unsigned get_image_type(const char *name)
{
	unsigned i;
	unsigned img_type;
	const char *name_ext;
	const int buf_len = 16;
	char buf[buf_len];

	if (!name)
		return IMG_TYPE_UNKNOWN;

	if (strstr(name, BIN_NAME_EXT)) {
		img_type = IMG_BIN_MASK;
		name_ext = BIN_NAME_EXT;
	} else if (strstr(name, SREC_NAME_EXT)) {
		img_type = IMG_SREC_MASK;
		name_ext = SREC_NAME_EXT;
	} else
		return IMG_TYPE_UNKNOWN;

	for (i = 0; i < IMG_MAX_NUM; ++i) {
		snprintf(&buf[0], buf_len, "%s%s", img_params[i].img_name, name_ext);
		if (!strncmp(name, &buf[0], strlen(&buf[0])))
			return (img_type | i);
	}

	return IMG_TYPE_UNKNOWN;
}

#define OPTEE_PAGE_ALIGNMENT (4 * 1024)

/**
 * hf_write_image() - Writes image to HyperFlash
 * @img_param:	Pointer to an image parameter info structure
 * @buffer:		Pointer to a buffer which will be written
 * @size:		Input buffer size
 *
 * Returns zero value on success or nonzero on failure.
 * The function invokes pseudo TA in OP-TEE, which can
 * write to HyperFlash.
 */
int hf_write_image(struct img_param *img_param, void *buffer, unsigned size)
{
	int i;
	int ret;
	unsigned char *buf_ptr = buffer;
	struct udevice *tee = NULL;
	struct tee_shm *shmem;
	struct tee_open_session_arg arg_ses;
	const struct tee_optee_ta_uuid uuid = HYPER_UUID;
	struct tee_invoke_arg arg;
	struct tee_param param[4];

	img_param->data = calloc(img_param->sectors_num, sizeof(*img_param->data));

	if (!img_param->data)
		return -1;

	for (i = 0; i < img_param->sectors_num; ++i) {
		img_param->data[i].size = HF_SECTOR_SIZE;
		img_param->data[i].flash_addr = img_param->start_addr + i * HF_SECTOR_SIZE;
		img_param->data[i].buf = (void *)(buf_ptr + i * HF_SECTOR_SIZE);
	}

	tee = tee_find_device(tee, NULL, NULL, NULL);
	if (!tee)
		goto error;

	memset(&arg_ses, 0, sizeof(arg_ses));
	tee_optee_ta_uuid_to_octets(arg_ses.uuid, &uuid);
	if (tee_open_session(tee, &arg_ses, 0, NULL))
		goto error;

	if (__tee_shm_add(tee, OPTEE_PAGE_ALIGNMENT, NULL, HF_SECTOR_SIZE,
	    TEE_SHM_ALLOC, &shmem))
		goto error_close_session;

	for (i = 0; i < img_param->sectors_num; ++i) {

		param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
		param[1].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
		param[2].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
		param[3].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INOUT;

		param[0].u.value.a = img_param->data[i].flash_addr;
		param[0].u.value.b = img_param->data[i].size;
		param[1].u.value.a = crc32(0, img_param->data[i].buf, img_param->data[i].size);
		param[1].u.value.b = img_param->img_id;
		param[2].u.value.a = i;

		/* Clear buffer before use */
		memset(shmem->addr, 0, HF_SECTOR_SIZE);
		/* Copy write buffer to shared memory */
		memcpy(shmem->addr, img_param->data[i].buf, img_param->data[i].size);

		param[3].u.memref.size = img_param->data[i].size;
		param[3].u.memref.shm_offs = (uint64_t)shmem->addr;
		param[3].u.memref.shm = shmem;

		arg.func = HYPER_CMD_WRITE;
		arg.session = arg_ses.session;

		ret = tee_invoke_func(tee, &arg, ARRAY_SIZE(param), param);

		if (ret)
			goto error_free_shmem;
	}
	tee_shm_free(shmem);

	tee_close_session(tee, arg_ses.session);
	free(img_param->data);
	return 0;

error_free_shmem:
	tee_shm_free(shmem);
error_close_session:
	tee_close_session(tee, arg_ses.session);
error:
	free(img_param->data);
	return -1;
}

static int read_record(char *buf, unsigned len, char *input)
{
	char *p;
	char c;
	unsigned count = 0;

	for (p = buf; p < buf + len; p++) {
		c = input[count];		/* read character */
		switch (c) {
		case '\r':
		case '\n':
			return count + 2; /* 2 - to skip terminating symbols */
		case '\0':
			return (-1);
		default:
			*p = c;
			count++;
		}
	}

	*p = '\0'; /* line too long - truncate */
	return count;
}

unsigned srec_to_bin(char *inbuf, char *outbuf, unsigned maxbin)
{
	char	record[SREC_MAXRECLEN + 1];	/* buffer for one S-Record	*/
	char	binbuf[SREC_MAXBINLEN];		/* buffer for binary data	*/
	int	first_data_line = 1;		/* Needed to deal with gaps in file */
	int	binlen;				/* no. of data bytes in S-Rec.	*/
	int	type;				/* return code for record type	*/
	int	b_read = 0;				/* bytes read */
	int	file_over = 0;
	ulong	addr;
	unsigned	out_size = 0;		/* Return value(size of bin file) */
	unsigned	prev_addr = 0;
	unsigned	prev_binlen = 0;

	while (!file_over) {
		b_read = read_record(record, SREC_MAXRECLEN, inbuf);
		if (b_read < 0) {
			printf("Corrupted s-record file\n");
			out_size = 0;
			break;
		}
		inbuf += b_read;
		type = srec_decode(record, &binlen, &addr, binbuf);
		if (type < 0) {
			printf("Invalid S-Record\n");
			out_size = 0;
			file_over = 1;		/* Invalid S-Record */
			break;
		}
		switch (type) {
		case SREC_DATA2:
		case SREC_DATA3:
		case SREC_DATA4:
			/* Need to check if there is empty space between our
			 * blocks of data
			 */
			if ((addr - prev_addr) > prev_binlen &&
						!first_data_line) {
				unsigned diff;
				diff = addr - prev_addr - prev_binlen;
				memset(outbuf, 0, diff);
				outbuf += diff;
				out_size += diff;
			}
			memcpy(outbuf, binbuf, binlen);
			outbuf += binlen;
			out_size += binlen;
			if (out_size > maxbin) {
				printf("Binary file exceeded its limit\n");
				file_over = 1;
				out_size = 0;
				break;
			}
			first_data_line = 0;
			prev_binlen = binlen;
			prev_addr = addr;
			break;
		case SREC_END2:
		case SREC_END3:
		case SREC_END4:
			file_over = 1;
			break;
		case SREC_START:
		    break;
		case SREC_EMPTY:
		    out_size = 0;
		    file_over = 1;
		    break;
		default:
		    break;
		}
	}

	return out_size;
}
#endif

#endif
#endif
