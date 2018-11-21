// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <bootm.h>
#include <command.h>
#include <image.h>
#include <lmb.h>
#include <mapmem.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <u-boot/zlib.h>
#include <android_image.h>
#include <avb_verify.h>
#include <fastboot.h>
#include <dt_table.h>
#include <malloc.h>
#include <i2c.h>
#include <dm/uclass.h>
#include <configs/rcar-gen3-common.h>

/*
 * Android Image booting support on R-Car Gen3 boards
 */

#if defined(RCAR_DRAM_AUTO) && defined(CONFIG_R8A7795)
	extern uint32_t _arg1;	/* defined at arch/arm/cpu/armv8/start.S */
#endif	/* defined(RCAR_DRAM_AUTO) && defined(CONFIG_R8A7795) */


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

/* Unpacked kernel image size must be no more than
 * andr_img_hdr.ramdisk_addr -  andr_img_hdr.kernel_addr,
 * else data will be partially erased
 */
#define KERNEL_UNPACKED_LIMIT      0x1080000UL

static struct dt_table_entry *find_fdt_from_table(const struct dt_table_header *dt_tbl,
							const ulong plat_id, ulong *dt_num);

static ulong calculate_max_dt_size(struct dt_table_entry *base_dt_entry,
					struct dt_table_header *dto_tbl,
					ulong *overlay_order, uint32_t overlay_num);

static struct fdt_header *load_dt_at_addr(struct fdt_header *addr,
					const struct dt_table_entry *dt_entry,
					const struct dt_table_header *dt_tbl);

static void *load_dt_table_from_part(struct blk_desc *dev_desc, const char *dtbo_part_name);

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
static ulong get_current_plat_id(void)
{
	ulong cpu_type = rmobile_get_cpu_type();
	ulong cpu_rev_integer = rmobile_get_cpu_rev_integer();
	ulong cpu_rev_fraction = rmobile_get_cpu_rev_fraction();
	int ret = -1;
	u8 board_id;
#ifdef CONFIG_DM_I2C
	struct udevice *bus;
	struct udevice *dev;
#endif

	if (cpu_type == CPU_ID_R8A7796) { /* R8A7796 */
		if ((cpu_rev_integer == 2) && (cpu_rev_fraction == 0)) { /* v2.0 force to v1.1 */
			cpu_rev_integer = 1; cpu_rev_fraction = 1;
		}
	} else if (cpu_type == CPU_ID_R8A77965) { /* R8A77965 */
		/* forcing zero, because cpu type does not fit in 4 bytes */
		cpu_rev_integer = cpu_rev_fraction = 0;
	}

	/* Build ID for current platform */
	ulong plat_id = ((cpu_rev_integer << 4) & 0x000000f0) |
					 (cpu_rev_fraction      & 0x0000000f);
	if (cpu_type == CPU_ID_R8A7795) {
		plat_id |= 0x779500;
	} else if (cpu_type == CPU_ID_R8A7796) {
		plat_id |= 0x779600;
	} else if (cpu_type == CPU_ID_R8A77965) {
		plat_id |= 0x779650;
	}

	/* Read board ID from PMIC EEPROM at offset 0x70 */
 #if defined(CONFIG_DM_I2C) && defined(CONFIG_I2C_SET_DEFAULT_BUS_NUM)
	ret = uclass_get_device_by_seq(UCLASS_I2C, CONFIG_I2C_DEFAULT_BUS_NUMBER, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, CONFIG_I2C_DEFAULT_BUS_NUMBER);
		return ret;
	}
	ret = i2c_get_chip(bus, I2C_POWERIC_EEPROM_ADDR, 1, &dev);
	if (!ret)
		ret = dm_i2c_read(dev, 0x70, &board_id, 1);
#endif
	if (ret ) {
		printf("Error reading platform Id (%d)\n", ret);
		return ret;
	}

	if (board_id == 0xff) { /* Not programmed? */
		board_id = 0;
	}
	plat_id |= ((board_id >> 3) << 24);

	return plat_id;
}

/*
 * This function parses @bootargs and search @androidboot.dtbo_idx
 * parameter. If it is present, parse order, in which should be applied
 * overlays. The order will be stored in @overlay_order and function
 * returns the number of overlays, which defined in the appropriative parameter.
 *
 * The parameter androidboot.dtbo_idx=x,y,z reports x, y and z as the
 * zero-based indices of the Device Tree Overlays (DTOs) from the DTBO
 * partition applied (in that order) by the bootloader to the base Device Tree.
 */
static uint32_t get_overlay_order(ulong *overlay_order, uint32_t max_size)
{
	uint32_t overlay_count = 0;
	const char *cmd_parameter = "androidboot.dtbo_idx";
	const char *bootargs = env_get("bootargs");
	char *iter = strstr(bootargs, cmd_parameter);

	if (!overlay_order || !max_size) {
		printf("Wrong parameters for determining right overlay order\n");
		return overlay_count;
	}

	if (!iter) {
		printf("bootarg parameter \"androidboot.dtbo_idx\" is not set\n");
		return overlay_count;
	}

	iter += strlen(cmd_parameter);

	if (iter[0] != '=') {
		printf("After \"androidboot.dtbo_idx\" must be \'=\' symbol\n");
		return overlay_count;
	}
	/* Skip '=' character */
	iter++;

	while (iter[0] >= '0' && iter[0] <= '9') {
		overlay_order[overlay_count] = ustrtoul(iter, &iter, 10);
		overlay_count++;
		if (!iter || iter[0] != ',' || (overlay_count >= max_size))
			break;
		iter++;
	}

	if (!overlay_count)
		printf("Invalid idx from \"androidboot.dtbo_idx\" parameter\n");

	return overlay_count;
}


/*
 * On the first step, the function reads from the bootargs parameter,
 * @androidboot.dtbo_idx, where defined the indexes of overlays, which
 * need to apply to base device tree. The indexes must be comma separated
 * (without whitespaces) and start from 0. If corresponding index points
 * to overlay, with different plat_id, then it will be skipped and error
 * will be shown.
 *
 * On the second stage, the function tries to apply overlays
 * in order, which written in the @androidboot.dtbo_idx parameter. In case,
 * when it meets the overlay, that can't be applied, the cycle brakes and in
 * @load_addr will be stored base device tree with all overlays, which ware
 * applied successfully. The function returns the amount of successfully
 * applied overlays.
 *
 *   Example 1: androidboot.dtbo_idx=0,3,1,2.
 *   0, 3 and 2 overlays is correct
 *   1 - invalid overlay
 *   The function applies overlays with idx 0, 3
 *   and writes an error, that it can't apply overlay with idx=1.
 *
 *   Example 2: androidboot.dtbo_idx=2,1.
 *   1 overlay - is correct
 *   2 overlay - is incorrect
 *   The function reports, that the overlay with idx=2 is invalid
 *   and doesn't do anything.
 *
 * Parameters:
 * @load_addr - address, where will be stored overlaid device tree
 * @base_dt_entry - pointer to base device tree entry from fdt (dtb partition)
 * @dto_tbl - pointer to start of packed device tree overlay table (dtbo partition)
 * @plat_id - platform id, got from @get_current_plat_id function.
 */
static ulong apply_all_overlays(struct fdt_header *load_addr,
				struct dt_table_entry *base_dt_entry,
				struct dt_table_header *dto_tbl,
				ulong plat_id) {
	ulong dt_num = 0;
	ulong overlay_count = 0;
	struct fdt_header *result_dt_buf = NULL;
	struct fdt_header *overlay_buf = NULL;
	ulong max_dt_size = 0;
	struct dt_table_entry *overlay_dt_entry = NULL;
	const uint32_t max_overlays_num = cpu_to_fdt32(dto_tbl->dt_entry_count);
	ulong overlay_order[max_overlays_num];
	uint32_t overlay_num =
		get_overlay_order(&overlay_order[0], max_overlays_num);

	if (!overlay_num)
		return overlay_count;

	max_dt_size = calculate_max_dt_size(base_dt_entry, dto_tbl,
				&overlay_order[0], overlay_num);
	result_dt_buf = (struct fdt_header *)malloc(max_dt_size);
	if (!result_dt_buf) {
		printf("ERROR, not enough memory for applying overlays\n");
		return overlay_count;
	}

	overlay_buf = (struct fdt_header *)malloc(max_dt_size);
	if (!overlay_buf) {
		printf("ERROR, not enough memory for applying overlays\n");
		goto free_result_buf;
	}

	memmove(result_dt_buf, load_addr, fdt_totalsize(load_addr));

	while (overlay_count < overlay_num) {
		dt_num = overlay_order[overlay_count];
		overlay_dt_entry = find_fdt_from_table(dto_tbl, plat_id, &dt_num);
		if (!overlay_dt_entry) {
			printf("Overlay with idx = %lu is not for [0x%lx] platform\n",
					overlay_order[overlay_count], plat_id);
			break;
		}

		load_dt_at_addr(overlay_buf, overlay_dt_entry, dto_tbl);
		if (fdt_open_into(result_dt_buf, result_dt_buf,
			fdt_totalsize(result_dt_buf) + fdt_totalsize(overlay_buf))) {
			printf("ERROR, due to resizing device tree\n");
			break;
		}

		if (fdt_overlay_apply_verbose(result_dt_buf, overlay_buf)) {
			printf("ERROR, due applying overlay with idx %lu\n", dt_num);
			break;
		}

		/* Success - copy overlaid dt to load addr */
		memmove(load_addr, result_dt_buf, fdt_totalsize(result_dt_buf));
		overlay_count++;
	}

	free(overlay_buf);

free_result_buf:
	free(result_dt_buf);

	return overlay_count;
}

/*
 * Function returns the maximum possible size of overlaid device tree blob.
 */
static ulong calculate_max_dt_size(struct dt_table_entry *base_dt_entry,
					struct dt_table_header *dto_tbl,
					ulong *overlay_order, uint32_t overlay_num)
{
	uint32_t i = 0;
	ulong dt_num = 0;
	struct dt_table_entry *overlay_dt_entry = NULL;
	ulong size = cpu_to_fdt32(base_dt_entry->dt_size);
	ulong base_plat_id = cpu_to_fdt32(base_dt_entry->id);

	for (i = 0; i < overlay_num; ++i) {
		dt_num = overlay_order[i];
		overlay_dt_entry = find_fdt_from_table(dto_tbl, base_plat_id, &dt_num);
		if (!overlay_dt_entry)
			continue;

		size += cpu_to_fdt32(overlay_dt_entry->dt_size);
	}

	return size;
}


/*
 * Function loads the proper device tree from fdt at @addr.
 */
static struct fdt_header *load_dt_at_addr(struct fdt_header *addr,
					const struct dt_table_entry *dt_entry,
					const struct dt_table_header *dt_tbl)
{
	ulong dt_offset = cpu_to_fdt32(dt_entry->dt_offset);
	ulong size = cpu_to_fdt32(dt_entry->dt_size);
	return memcpy(addr, (void *)(((uchar*)dt_tbl) + dt_offset), size);
}

static void set_board_id_args(ulong plat_id) {
	char *bootargs = env_get("bootargs");
	int len = 0;
	if (bootargs)
		len += strlen(bootargs);
	len += 34;	/* for 'androidboot.board_id=0xXXXXXXXX '*/
	char *newbootargs = malloc(len);
	if (newbootargs) {
		snprintf(newbootargs, len, "androidboot.board_id=0x%lx %s",
			plat_id, bootargs);
		env_set("bootargs", newbootargs);
	} else {
		puts("Error: malloc in set_board_id_args failed!\n");
		return;
	}

	free(newbootargs);
}

static char * assemble_dtbo_idx_string(int indx[], size_t size)
{
	const char * dtbo_str = "androidboot.dtbo_idx=";
	const size_t buf_size = 3*strlen(dtbo_str)+1;
	int i = 0;

	if (size==0)
		return NULL;

	char * newstr = malloc(buf_size);
    if (!newstr) {
	pr_err("Can't allocate memory\n");
	return NULL;
    }
    strcpy(newstr, dtbo_str);

    while(i<size && (buf_size-strlen(dtbo_str))>4){
	sprintf(newstr, "%s%d%s", newstr, indx[i], i==(size-1)?"":",");
	++i;
    }

    return newstr;
}

static void add_dtbo_index(int indx[], size_t size)
{
	int len = 0;
	char * next, * param, * value;
	char * dtbo_vals = NULL, * dtbo_param = "androidboot.dtbo_idx";
	char * bootargs = env_get("bootargs");
	size_t cmdline_size =
#if defined(CONFIG_KERNEL_CMDLINE_SIZE)
			CONFIG_KERNEL_CMDLINE_SIZE;
#elif defined(COMMAND_LINE_SIZE)
			COMMAND_LINE_SIZE;
#else
			1024;
#endif

	if (!bootargs) {
		value = assemble_dtbo_idx_string(indx, size);
		if (!value) {
			return;
		}
		env_set("bootargs", value);
		free(value);
		return;
	}

	len = strlen(bootargs);
	char * copy_bootargs = malloc(len+1);
	if (!copy_bootargs) {
		pr_err("Can't allocate memory\n");
		return;
	}

	char * new_bootargs = malloc(cmdline_size);
	if (!new_bootargs) {
		pr_err("Can't allocate memory\n");
		goto add_dtbo_index_exit1;
	}

	strcpy(copy_bootargs, bootargs);
	new_bootargs[0] = '\0';
	next = skip_spaces(copy_bootargs);
	while (*next) {
		next = next_arg(next, &param, &value);
		if (param) {
			if (strcmp(dtbo_param, param)==0) {
				dtbo_vals = value;
			} else {
				if (value) {
					if (strchr(value, ' '))
						sprintf(new_bootargs, "%s%s=\"%s\" ", new_bootargs, param, value);
					else
						sprintf(new_bootargs, "%s%s=%s ", new_bootargs, param, value);
				} else {
					sprintf(new_bootargs, "%s%s ", new_bootargs, param);
				}
			}
		}
	}

	if (!dtbo_vals) {
		dtbo_vals = assemble_dtbo_idx_string(indx, size);
		if (!dtbo_vals) {
			goto add_dtbo_index_exit2;
		}
		strcat(new_bootargs, dtbo_vals);
		env_set("bootargs", new_bootargs);
		free(dtbo_vals);
	} else {
		value = assemble_dtbo_idx_string(indx, size);
		if (!value) {
			goto add_dtbo_index_exit2;
		}
		sprintf(value, "%s,%s", value, dtbo_vals);
		strcat(new_bootargs, value);
		free(value);
	}

add_dtbo_index_exit2:
	free(new_bootargs);
add_dtbo_index_exit1:
	free(copy_bootargs);
	return;
}


/*We will use this for zipped kernel loading*/
#define GZIP_LOAD_ADDR	0x4c000000
#define GZIP_MAGIC 0x8B1F
static inline bool is_zipped(ulong addr)
{
	u16 *magic = (u16 *) addr;
	if (cpu_to_le16(*magic) == GZIP_MAGIC)
		return true;

	return false;

}

#ifdef CONFIG_LZ4
#define LZ4_MAGIC 0x2204
static inline bool is_lz4(ulong addr)
{
	u16 *magic = (u16 *) addr;
	if (cpu_to_le16(*magic) == LZ4_MAGIC)
		return true;

	return false;

}
#endif

/*
 * This function checks flattened device tree header @dt_tbl.
 * If @DT_TABLE_MAGIC and sizeof @dt_table_entry equal to
 * to the corresponding field in the header, then returns true.
 * In another case, returns false.
 */
static bool check_fdt_header(const struct dt_table_header *dt_tbl)
{
	if (cpu_to_fdt32(dt_tbl->magic) != DT_TABLE_MAGIC) {
		printf("ERROR: Invalid FDT table found at 0x%lx (0x%lx)\n",
			(ulong)dt_tbl, (ulong)cpu_to_fdt32(dt_tbl->magic));
		return false;
	}

	if (cpu_to_fdt32(dt_tbl->dt_entry_size) != sizeof(struct dt_table_entry)) {
		printf("ERROR: Unsupported FDT table found at 0x%lx\n", (ulong)dt_tbl);
		return false;
	}

	return true;
}


/*
 * Finds dt_table_entry from loaded DT table and returns a pointer
 * to it. If an entry isn't found or DT table is incorrect returns
 * NULL. @dt_num - start index of device tree in the table.
 * After finding the appropriative device tree with given @plat_id,
 * @dt_num will be changed to the index of found dt.
 */
static struct dt_table_entry *find_fdt_from_table(const struct dt_table_header *dt_tbl,
							const ulong plat_id, ulong *dt_num)
{
	int i = 0;

	if (!check_fdt_header(dt_tbl))
		return NULL;

	ulong dt_entry_count = cpu_to_fdt32(dt_tbl->dt_entry_count);
	ulong dt_entries_offset = cpu_to_fdt32(dt_tbl->dt_entries_offset);

	//printf("FDT: found table %lu entry(s)\n", dt_entry_count);

	struct dt_table_entry *dt_entry =
		(struct dt_table_entry *)(((uchar *)dt_tbl) + dt_entries_offset);

	if (dt_num) {
		dt_entry += (*dt_num);
		i = (*dt_num);
	} else {
		i = 0;
	}

	while (i < dt_entry_count) {
		ulong dt_id = cpu_to_fdt32(dt_entry->id);

		if (dt_id == plat_id || dt_id == RCAR_GENERIC_PLAT_ID) {
			if (dt_num)
				*dt_num = i;
			return dt_entry;
		}
		i++;
		dt_entry++;
	}

	return NULL;
}

static int load_dt_with_overlays(struct fdt_header *load_addr,
				struct dt_table_header *dt_tbl,
				struct dt_table_header *dto_tbl)
{
	int dmbo_indx[] = {0, 0, 0, 0, 0, 0, 0, 0};
	size_t dmbo_count = 0;
	ulong applied_overlays = 0;
	ulong plat_id = get_current_plat_id();
	struct dt_table_entry *base_dt_entry =
		find_fdt_from_table(dt_tbl, plat_id, NULL);

	if (!base_dt_entry) {
		printf("ERROR: No proper FDT entry found for current platform id=%lx\n", plat_id);
		return -1;
	}

#if defined(CONFIG_R8A7795)
#if defined(RCAR_DRAM_AUTO)
	if (_arg1 == 8) {
		if (plat_id == H3v3_PLAT_ID) {
			dmbo_indx[0] = V3_PLATID_DTBO_NUM;
			dmbo_indx[1] = V3_INFO_DTBO_NUM;
			dmbo_count = 2;
		} else if (plat_id == H3v2_PLAT_ID) {
			dmbo_indx[0] = V2_PLATID_DTBO_NUM;
			dmbo_indx[1] = V2_INFO_DTBO_NUM;
			dmbo_count = 2;
		}
	}
#elif defined(RCAR_DRAM_MAP4_2)
	if (plat_id == H3v3_PLAT_ID) {
		dmbo_indx[0] = V3_PLATID_DTBO_NUM;
		dmbo_indx[1] = V3_INFO_DTBO_NUM;
		dmbo_count = 2;
	} else if (plat_id == H3v2_PLAT_ID) {
		dmbo_indx[0] = V2_PLATID_DTBO_NUM;
		dmbo_indx[1] = V2_INFO_DTBO_NUM;
		dmbo_count = 2;
	}
#endif /* defined(RCAR_DRAM_AUTO) */
#endif /* defined(CONFIG_R8A7795) */

#if defined(ENABLE_ADSP)
	dmbo_indx[dmbo_count] = ADSP_DTBO_NUM;
	++dmbo_count;
#endif /* defined(ENABLE_ADSP) */

	add_dtbo_index(dmbo_indx, dmbo_count);

	set_board_id_args(plat_id);

	/* Base device tree should be loaded in defined address */
	load_dt_at_addr(load_addr, base_dt_entry, dt_tbl);

	/*
	 * Next sections is not critical - we can try to boot without overlays,
	 * so if error occurs, function should return success code.
	 */
	if (!dto_tbl) {
		printf("DTO table is NULL, load without DT overlays\n");
		return 0;
	}

	applied_overlays = apply_all_overlays(load_addr, base_dt_entry,
		dto_tbl, plat_id);

	if (applied_overlays)
		printf("Successfully applied %ld overlay(s)\n", applied_overlays);

	return 0;
}


#define MMC_HEADER_SIZE 4 /*defnes header size in blocks*/
int do_boot_android_img_from_ram(ulong hdr_addr, ulong dt_addr, ulong dto_addr)
{
	ulong kernel_offset, ramdisk_offset, size;
	size_t dstn_size, kernel_space = KERNEL_UNPACKED_LIMIT;
	struct andr_img_hdr *hdr = map_sysmem(hdr_addr, 0);
	struct dt_table_header *dt_tbl = map_sysmem(dt_addr, 0);
	struct dt_table_header *dto_tbl = map_sysmem(dto_addr, 0);
	int ret;

	/*Image is in ram starting from address passed as boot parameter
	  *This happens if image was loaded using fastboot or during verified
	  *boot
	  */
	kernel_offset = hdr_addr + MMC_HEADER_SIZE * 512;
	size = ALIGN(hdr->kernel_size, hdr->page_size);
	dstn_size = size;

	if (hdr->kernel_addr < hdr->ramdisk_addr)
		kernel_space = hdr->ramdisk_addr - hdr->kernel_addr;
	else if (hdr->kernel_addr < CONFIG_SYS_TEXT_BASE)
		kernel_space = CONFIG_SYS_TEXT_BASE - hdr->kernel_addr;

	if(is_zipped((ulong) kernel_offset)) {
		dstn_size = hdr->kernel_size;
		ret = gunzip((void *)(ulong)hdr->kernel_addr, (int)kernel_space,
						(unsigned char *)kernel_offset, (ulong*)&dstn_size);
		if (ret) {
			printf("Kernel unzip error: %d\n", ret);
			return CMD_RET_FAILURE;
		} else
			printf("Unzipped kernel image size: %zu\n", dstn_size);
	} else
#ifdef CONFIG_LZ4
	if (is_lz4((ulong) kernel_offset)) {
		dstn_size = kernel_space;
		ret = ulz4fn((void *)kernel_offset, hdr->kernel_size,
						(void *)(ulong)hdr->kernel_addr, &dstn_size);
		if (ret) {
			printf("Kernel LZ4 decompression error: %d (decompressed "
						"size: %zu bytes)\n",ret, dstn_size);
			return CMD_RET_FAILURE;
		} else
			printf("LZ4 decompressed kernel image size: %zu\n", dstn_size);
	} else
#endif
		memcpy((void *)(u64)hdr->kernel_addr,(void *)kernel_offset, dstn_size);

	printf("kernel offset = %lx, size = 0x%lx, address 0x%x\n", kernel_offset,
				dstn_size, hdr->kernel_addr);

	ramdisk_offset = kernel_offset;
	ramdisk_offset += size;
	size = ALIGN(hdr->ramdisk_size, hdr->page_size);

	printf("ramdisk offset = %lx, size = 0x%lx, address = 0x%x\n",
				ramdisk_offset, size, hdr->ramdisk_addr);

	if (hdr->ramdisk_addr != ramdisk_offset)
		memcpy((void *)(u64)hdr->ramdisk_addr, (void *)ramdisk_offset, size);

	return load_dt_with_overlays((struct fdt_header *)(u64)hdr->second_addr, dt_tbl, dto_tbl);
}

static void *load_dt_table_from_part(struct blk_desc *dev_desc, const char *dtbo_part_name)
{
	void *fdt_overlay_addr;
	disk_partition_t info;
	ulong size, fdt_overlay_offset;
	int ret;

	ret = part_get_info_by_name(dev_desc, dtbo_part_name, &info);
	if (ret < 0) {
		printf("ERROR: Can't read '%s' part\n", dtbo_part_name);
		return NULL;
	}

	size = info.blksz * info.size;
	fdt_overlay_addr = malloc(size);
	fdt_overlay_offset = info.start;

	if (fdt_overlay_addr == NULL) {
		printf("ERROR: Failed to allocate %lu bytes for FDT overlay\n", size);
		return NULL;
	}

	/* Convert size to blocks */
	size /= info.blksz;

	printf("%s: fdt overlay block offset = 0x%lx, size = %lu address = 0x%p\n",
		dtbo_part_name, fdt_overlay_offset, size, fdt_overlay_addr);

	ret = blk_dread(dev_desc, fdt_overlay_offset,
						size, fdt_overlay_addr);
	if (ret != size) {
		printf("ERROR: Can't read '%s' part\n", dtbo_part_name);
		return NULL;
	}
	return fdt_overlay_addr;
}

/*Defines maximum size for certificate + signed hash*/
#define MAX_SIGN_SIZE	2048
static u32 get_signable_size(const struct andr_img_hdr *hdr)
{
	u32 signable_size;

	if ((!hdr) || (!hdr->page_size))
		return 0;

	/*include the page aligned image header*/
	signable_size = hdr->page_size
	 + ((hdr->kernel_size + hdr->page_size - 1) / hdr->page_size) * hdr->page_size
	 + ((hdr->ramdisk_size + hdr->page_size - 1) / hdr->page_size) * hdr->page_size
	 + ((hdr->second_size + hdr->page_size - 1) / hdr->page_size) * hdr->page_size;

	return signable_size;
}


static int do_boot_mmc(struct blk_desc *dev_desc,
		const char *boot_part, const char *dtb_part,
		const char *dtbo_part, ulong addr)
{
	int ret;
	ulong kernel_offset, signable_size;
	disk_partition_t info;
	struct andr_img_hdr *hdr;
	void *fdt_addr;
	void *fdt_overlay_addr;

	ret = part_get_info_by_name(dev_desc, boot_part, &info);
	if (ret < 0) {
		printf ("Can't find partition '%s'\n", boot_part);
		return CMD_RET_FAILURE;
	}

	printf("%s: block start = 0x%lx, block size = %ld\n", boot_part, info.start, info.blksz);

	ret = blk_dread(dev_desc, info.start,
						MMC_HEADER_SIZE, (void *) addr);

	hdr = map_sysmem(addr, 0);

	/* Read kernel from mmc */
	kernel_offset = info.start + MMC_HEADER_SIZE;

	signable_size = get_signable_size(hdr);

	signable_size += MAX_SIGN_SIZE;  /* Add certifcates and sign */
	signable_size /= info.blksz;     /* Get size in blocks */

	if (!signable_size || (signable_size > info.size)) {
		printf("Image size error (%lu)\n", signable_size);
		return CMD_RET_FAILURE;
	}

	addr += MMC_HEADER_SIZE * info.blksz;

	ret = blk_dread(dev_desc, kernel_offset,
						signable_size,
						(void *)addr);

	flush_cache(addr, signable_size * info.blksz);
	if (ret != signable_size) {
		printf("Can't read image\n");
		return CMD_RET_FAILURE;
	}

	fdt_addr = load_dt_table_from_part(dev_desc, dtb_part);

	if (!fdt_addr)
		return CMD_RET_FAILURE;

	/* This partition is not critical for booting, so we don't check the result */
	fdt_overlay_addr = load_dt_table_from_part(dev_desc, dtbo_part);

	/* By here we have image loaded into the RAM */
	printf("HDR Addr = 0x%lx, fdt_addr = 0x%lx, fdt_overlay_addr = 0x%lx\n", (ulong)hdr, (ulong)fdt_addr, (ulong)fdt_overlay_addr);
	return do_boot_android_img_from_ram((ulong) hdr, (ulong)fdt_addr,
				(ulong)fdt_overlay_addr);
}

static char hexc[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static char *hex_to_str(u8 *data, size_t size)
{
	static char load_addr[32];
	char *paddr = &load_addr[31];
	int i;

	if (size > sizeof(load_addr) - 1)
		return NULL;

	*paddr-- =  '\0';
	for (i = 0; i < size; i++) {
		*paddr-- = (hexc[data[i] & 0x0f]);
		*paddr-- = (hexc[(data[i] >> 4) & 0x0f]);
	}
	return paddr + 1;
}

#define MAX_BOOTI_ARGC 4
static void build_new_args(ulong addr, char *argv[MAX_BOOTI_ARGC]) {
	struct andr_img_hdr *hdr = map_sysmem(addr, 0);

	argv[1] = avb_strdup(hex_to_str((u8 *)&hdr->kernel_addr, sizeof(hdr->kernel_addr)));
	argv[2] = avb_strdup(hex_to_str((u8 *)&hdr->ramdisk_addr, sizeof(hdr->ramdisk_addr)));
	argv[3] = avb_strdup(hex_to_str((u8 *)&hdr->second_addr, sizeof(hdr->second_addr)));
}

/*
 * Sets corret boot address if image was loaded
* using fastboot
*/
#define DEFAULT_RD_ADDR 0x49100000
#define DEFAULT_SECOND_ADDR 0x48000800

/*We need virtual device and partition to support fastboot boot command*/
#define VIRT_BOOT_DEVICE "-1"
#define VIRT_BOOT_PARTITION "RAM"

void do_correct_boot_address(ulong hdr_addr)
{
	struct andr_img_hdr *hdr = map_sysmem(hdr_addr, 0);
	hdr->kernel_addr = CONFIG_SYS_LOAD_ADDR;
	hdr->ramdisk_addr = DEFAULT_RD_ADDR;
	hdr->second_addr = DEFAULT_SECOND_ADDR;
}

static inline void avb_set_boot_device(AvbOps *ops, int boot_device)
{
	struct AvbOpsData *data = (struct AvbOpsData *) ops->user_data;
	data->mmc_dev = boot_device;
}


#define DEFAULT_AVB_DELAY 5
int avb_main(int boot_device, char **argv)
{
	AvbOps *ops;
	AvbABFlowResult ab_result;
	AvbSlotVerifyData *slot_data;
	const char *requested_partitions[] = {"boot", "dtb", "dtbo", NULL};
	bool unlocked = false;
	char *cmdline = NULL;
	bool abort = false;
	int boot_delay;
	unsigned long ts;
	const char *avb_delay  = env_get("avb_delay");
	AvbPartitionData *avb_boot_part = NULL, *avb_dtb_part = NULL,
			*avb_dtbo_part = NULL, avb_ram_data;
	AvbSlotVerifyFlags flags = AVB_SLOT_VERIFY_FLAGS_NONE;

	boot_delay = avb_delay ? (int)simple_strtol(avb_delay, NULL, 10)
						: DEFAULT_AVB_DELAY;

	avb_printv("AVB-based bootloader using libavb version ",
		avb_version_string(),
		"\n",
		NULL);

	ops = avb_ops_alloc(boot_device);
	if (!ops) {
		avb_fatal("Error allocating AvbOps.\n");
	}

	if (ops->read_is_device_unlocked(ops, &unlocked) != AVB_IO_RESULT_OK) {
		avb_fatal("Error determining whether device is unlocked.\n");
	}
	avb_printv("read_is_device_unlocked() ops returned that device is ",
		unlocked ? "UNLOCKED" : "LOCKED",
		"\n",
		NULL);

	printf("boot_device = %d\n", boot_device);

	if (boot_device == (int) simple_strtoul(VIRT_BOOT_DEVICE, NULL, 10)) {
		/*
		* We are booting by fastboot boot command
		* This is only supported in unlocked state
		*/
		printf("setting ram partition..\n");
		if (unlocked) {
			requested_partitions[0] = "dtb";
			requested_partitions[1] = "dtbo";
			requested_partitions[2] = NULL;
			avb_ram_data.partition_name = VIRT_BOOT_PARTITION;
			avb_ram_data.data = (uint8_t*)simple_strtol(argv[0], NULL, 10);
			avb_boot_part = &avb_ram_data;
			boot_device = CONFIG_FASTBOOT_FLASH_MMC_DEV;
			do_correct_boot_address((ulong) avb_ram_data.data);
		} else {
			avb_fatal("Fastboot boot not supported in locked state!\n");
			return -1;
		}
	}
	avb_set_boot_device(ops, boot_device);
	if (unlocked)
		flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	ab_result = avb_ab_flow(ops->ab_ops,
			requested_partitions,
			flags,
			AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
			&slot_data);
	avb_printv("avb_ab_flow() returned ",
		avb_ab_flow_result_to_string(ab_result),
		"\n",
		NULL);
	switch (ab_result) {
	case AVB_AB_FLOW_RESULT_OK_WITH_VERIFICATION_ERROR:
		if (!unlocked) {
			avb_fatal("Verification Error in Locked State!\n");
			break;
		}
		/*We are in unlocked state
		* Set warning and wait for user interaction;
		*/
		avb_printv("OS was not verified! Press any key to halt booting!..\n", NULL);
		while ((boot_delay > 0) && (!abort)) {
			--boot_delay;
			/* delay 1000 ms */
			ts = get_timer(0);
			do {
				if (tstc()) {	/* we got a key press	*/
					abort  = 1; /* don't auto boot	*/
					boot_delay = 0;	/* no more delay	*/
					(void) getc();	/* consume input	*/
					break;
				}
				udelay(10000);
			} while (!abort && get_timer(ts) < 1000);
			printf("\b\b\b%2d ", boot_delay);
		}
		if (abort) {
			avb_fatal("Booting halted by user request\n");
			break;
		}
		/*Fall Through*/
	case AVB_AB_FLOW_RESULT_OK:
		avb_printv("slot_suffix:    ", slot_data->ab_suffix, "\n", NULL);
		avb_printv("cmdline:        ", slot_data->cmdline, "\n", NULL);
		avb_printv(
		"release string: ",
		(const char *)((((AvbVBMetaImageHeader *)
		(slot_data->vbmeta_images[0]
		.vbmeta_data)))->release_string),
		"\n",
		NULL);
		cmdline = prepare_bootcmd_compat(ops, boot_device,
								ab_result, unlocked,
								slot_data,
								LOAD_AVB_ARGS);

		if (!cmdline) {
			avb_fatal("Error while setting cmd line\n");
			break;
		}

		if (!avb_boot_part)
			*argv = hex_to_str((u8 *)&slot_data->loaded_partitions->data, sizeof(ulong));

		AvbPartitionData *avb_part = slot_data->loaded_partitions;

		for(int i = 0; i < slot_data->num_loaded_partitions; i++, avb_part++)
		{
			if (!avb_boot_part &&
			    !strncmp(avb_part->partition_name, "boot", 4)) {
					avb_boot_part = avb_part;
			} else if (strncmp(avb_part->partition_name, "dtb", 3) == 0 &&
				strlen(avb_part->partition_name) == 3) {
				avb_dtb_part = avb_part;
			} else if (strncmp(avb_part->partition_name, "dtbo", 4) == 0) {
				avb_dtbo_part = avb_part;
			}
		}

		if (avb_boot_part == NULL || avb_dtb_part == NULL || avb_dtbo_part == NULL) {
			if (avb_boot_part == NULL) {
				avb_fatal("Boot partition is not found\n");
			}
			if (avb_dtb_part == NULL) {
				avb_fatal("dtb partition is not found\n");
			}
			if (avb_dtbo_part == NULL) {
				avb_fatal("dtbo partition is not found\n");
			}
		} else {
			return do_boot_android_img_from_ram((ulong)avb_boot_part->data,
							(ulong)avb_dtb_part->data,
							(ulong)avb_dtbo_part->data);
		}

	case AVB_AB_FLOW_RESULT_ERROR_OOM:
		avb_fatal("OOM error while doing A/B select flow.\n");
		break;
	case AVB_AB_FLOW_RESULT_ERROR_IO:
		avb_fatal("I/O error while doing A/B select flow.\n");
		break;
	case AVB_AB_FLOW_RESULT_ERROR_NO_BOOTABLE_SLOTS:
		avb_fatal("No bootable slots - enter repair mode\n");
		break;
	case AVB_AB_FLOW_RESULT_ERROR_INVALID_ARGUMENT:
		avb_fatal("Invalid Argument error while doing A/B select flow.\n");
		break;
	}
	avb_ops_free(ops);
	return 0;
}

int do_boot_avb(int device, char **argv)
{
	return avb_main(device, argv);
}

int do_boota(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct mmc *mmc;
	int dev = 0, part = 0, ret = CMD_RET_FAILURE;
	ulong addr;
	char *boot_part = NULL, *fdt_part = "dtb", *dtbo_part = "dtbo";
	bool load = true, avb = false;
	struct blk_desc *dev_desc;
	char *new_argv[MAX_BOOTI_ARGC];

	new_argv[0] = argv[0];
	argc--; argv++;

	if (argc < 1) {
		return CMD_RET_USAGE;
	}

	if (argc >= 2) {
		dev = (int) simple_strtoul(argv[0], NULL, 10);
		argc--; argv++;
	}

	if (argc >= 2) {
		boot_part = argv[0];
		if (!strncmp(argv[2], "avb", strlen("avb"))) {
			avb = true;
			printf("AVB verification is ON ..\n");
			if (boot_part && !strcmp(boot_part, "RAM")) {
				 load = false;
			}
			argc--;
		}
		argc--; argv++;
	}

	addr = simple_strtoul(argv[0], NULL, 16);
	if (load) {
		if (part > PART_ACCESS_MASK) {
			printf("#part_num shouldn't be larger than %d\n",
					PART_ACCESS_MASK);
			return CMD_RET_FAILURE;
		}
		printf("Looking for mmc device ..\n");
		mmc = find_mmc_device(dev);
		if (!mmc)
			return CMD_RET_FAILURE;
		printf("Found (0x%p)\n", mmc); 

		if (mmc_init(mmc))
			return CMD_RET_FAILURE;

		printf("Switching to partition\n"); 

		ret = mmc_switch_part(mmc, part);
		printf("switch to HW partition #%d, %s\n",
				part, (!ret) ? "OK" : "ERROR");
		if (ret)
			return CMD_RET_FAILURE;
 
		if (!avb) {
			/* We are booting in a legacy mode without avb */
			const char *slot_suffix = cb_get_slot_char();
			if (slot_suffix) {
				boot_part = avb_strdupv(boot_part, "_", slot_suffix, NULL);
				fdt_part = avb_strdupv(fdt_part, "_", slot_suffix, NULL);
				dtbo_part = avb_strdupv(dtbo_part, "_", slot_suffix, NULL);
			}
			printf ("boot from MMC device=%d part=%d (%s, %s, %s) addr=0x%lx\n",
				   dev, part, boot_part, fdt_part, dtbo_part, addr);
 
			set_compat_args(dev);
			dev_desc = blk_get_dev("mmc", dev);
			if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
				pr_err("invalid mmc device\n");
				return -EIO;
			}
			ret = do_boot_mmc(dev_desc, boot_part, fdt_part, dtbo_part, addr);
		}
	}

	if (avb) {
		ret = do_boot_avb(dev, &new_argv[1]);
		if (ret == CMD_RET_SUCCESS) {
			addr = (int) simple_strtoul(new_argv[1], NULL, 16);
		}
	} else {
		/*We have to remove vbmeta node if we boot without avb*/
		struct andr_img_hdr *hdr = map_sysmem(addr, 0);
		struct fdt_header *fdt = map_sysmem(hdr->second_addr, 0);
		int nodeoffset = fdt_path_offset(fdt, "/firmware/android/vbmeta");
		if (nodeoffset > 0) {
				if (fdt_del_node(fdt, nodeoffset) == 0) {
						printf("DTB node '/firmware/android/vbmeta' was deleted\n");
				}
		}
	}

	if(ret != CMD_RET_SUCCESS) {
		printf("ERROR: Boot Failed!\n");
		return ret;
	}
	build_new_args(addr, new_argv);
	argc = MAX_BOOTI_ARGC;
	images.os.start = addr;
	return do_booti(cmdtp, flag, argc, new_argv);
}

static char boota_help_text[] ="mmc_dev [mmc_part] boot_addr [verify]\n"
	"	  - boot Android Image from MMC\n"
	"\tThe argument 'mmc_dev' defines mmc device\n"
	"\tThe argument 'mmc_part' is optional and defines mmc partition\n"
	"\tdefault partiotion is 'boot' \n"
	"\tThe argument boot_addr defines memory address for booting\n"
	"\tThe argument verify enables verified boot\n"
	"";

U_BOOT_CMD(
	boota,  CONFIG_SYS_MAXARGS, 1,  do_boota,
	"boot Android Image from mmc", boota_help_text
);

