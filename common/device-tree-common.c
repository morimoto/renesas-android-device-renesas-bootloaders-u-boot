// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 GlobalLogic
 */

#include <device-tree-common.h>


#if defined(RCAR_DRAM_AUTO) && defined(CONFIG_R8A7795)
	extern uint32_t _arg1;	/* defined at arch/arm/cpu/armv8/start.S */
#endif	/* defined(RCAR_DRAM_AUTO) && defined(CONFIG_R8A7795) */

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
ulong get_current_plat_id(void)
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

static char *assemble_dtbo_idx_string(int indx[], size_t size)
{
	const char *dtbo_str = "androidboot.dtbo_idx=";
	const size_t buf_size = 3 * strlen(dtbo_str) + 1;
	int i = 0;

	if (size==0)
		return NULL;

	char *newstr = malloc(buf_size);
	if (!newstr) {
		pr_err("Can't allocate memory\n");
		return NULL;
	}
	strcpy(newstr, dtbo_str);

	while(i < size && (buf_size - strlen(dtbo_str)) > 4){
		sprintf(newstr, "%s%d%s", newstr, indx[i], i == (size - 1) ? "" : ",");
		++i;
	}

	return newstr;
}

static void add_dtbo_index(int indx[], size_t size)
{
	int len = 0;
	char *next, *param, *value;
	char *dtbo_vals = NULL, *dtbo_param = "androidboot.dtbo_idx";
	char *bootargs = env_get("bootargs");
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
	char *copy_bootargs = malloc(len + 1);
	if (!copy_bootargs) {
		pr_err("Can't allocate memory\n");
		return;
	}

	char *new_bootargs = malloc(cmdline_size);
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
			if (strcmp(dtbo_param, param) == 0) {
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

int load_dt_with_overlays(struct fdt_header *load_addr,
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

#if defined(ENABLE_PRODUCT_PART)
	dmbo_indx[dmbo_count] = PARTITIONS_DTBO_NUM;
	++dmbo_count;
#endif /* defined(ENABLE_PRODUCT_PART) */

	dmbo_indx[dmbo_count] = LVDS_PANEL_DTBO_NUM;
	++dmbo_count;

	add_dtbo_index(dmbo_indx, dmbo_count);

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

	printf("Applied %ld overlay(s)\n", applied_overlays);

	return 0;
}

void *load_dt_table_from_part(struct blk_desc *dev_desc, const char *dtbo_part_name)
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