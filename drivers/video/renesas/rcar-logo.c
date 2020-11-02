// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 GlobalLogic
 */

#include <common.h>
#include "vsp2/vsp1.h"
#include <rcar-logo.h>
#include <cpu_func.h>

static int inProgress = 0;

static int load_argb_from_part(const char *part_name, int index)
{
	disk_partition_t info;
	void *buffer = (void*)(CONFIG_VIDEO_RENESAS_BUF_ADDR);
	struct blk_desc *dev_desc;
	unsigned long read_blk;

	if (!part_name) {
		printf("%s: Partition name is NULL\n", __func__);
		return -1;
	}

	dev_desc = blk_get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		printf("%s: Invalid mmc device\n", __func__);
		return -1;
	}

	if (part_get_info_by_name(dev_desc, part_name, &info) < 0) {
		printf("%s: Partition '%s' is not found\n", __func__, part_name);
		return -1;
	}

	read_blk = blk_dread(dev_desc, info.start +
			index * CONFIG_VIDEO_RENESAS_BUF_SIZE,
			CONFIG_VIDEO_RENESAS_BUF_SIZE, buffer);
	if (read_blk != CONFIG_VIDEO_RENESAS_BUF_SIZE) {
		printf("%s: Failed to read from '%s' partition, index %d\n", __func__,
				part_name, index);
		return -1;
	}
	/* Check image footer*/
	if (memcmp((void*)(CONFIG_VIDEO_RENESAS_BUF_ADDR +
			CONFIG_VIDEO_RENESAS_BUF_SIZE * 512 - 4), "logo", 4)) {
		printf("%s: Bad logo footer\n", __func__);
		return -1;
	}
	return 0;
}

int do_logo_start(int img_index)
{
	if (load_argb_from_part("logo", img_index)) {
		return -1;
	}

	if (inProgress == 1) {
		return 0;
	}

	dcache_disable();
	icache_disable();

	if (rcar_clks_enable()) {
		printf("%s: Clocks enable failed\n", __func__);
		goto err_clks;
	}

	if (dw_hdmi_probe()) {
		printf("%s: HDMI probe failed\n", __func__);
		goto err_hdmi;
	}

	if (dw_hdmi_connector_detect() != 1) {
		printf("%s: HDMI connector isn't detected\n", __func__);
		goto err_hdmi_conn;
	}

	dw_hdmi_bridge_enable();

	if (vsp1_probe()) {
		printf("%s: VSP probe failed\n", __func__);
		goto err_vsp1;
	}

	if (rcar_du_probe()) {
		printf("%s: DU probe failed\n", __func__);
		goto err_du;
	}

	if (rcar_du_start()) {
		printf("%s: DU start failed\n", __func__);
		goto err_du_start;
	}

	dcache_enable();
	icache_enable();

	inProgress = 1;

	return 0;

err_du_start:
	rcar_du_stop(); /* Need to disable the VSP, so call this function */

err_du:
	vsp1_remove();

err_vsp1:
	dw_hdmi_bridge_disable();

err_hdmi_conn:
	dw_hdmi_remove();

err_hdmi:
	rcar_clks_disable();

err_clks:
	dcache_enable();
	icache_enable();

	return -1;
}

void do_logo_stop(void)
{
	if (inProgress == 0)
		return;

	dcache_disable();
	icache_disable();

	dw_hdmi_bridge_disable();
	rcar_du_stop();
	dw_hdmi_remove();
	vsp1_remove();
	rcar_clks_disable();

	dcache_enable();
	icache_enable();

	inProgress = 0;
}
