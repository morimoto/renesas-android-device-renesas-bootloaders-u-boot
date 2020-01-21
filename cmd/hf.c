// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 GlobalLogic
 */

#include <common.h>
#include <command.h>
#include <tee.h>
#include <malloc.h>

static int do_hf_flash(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct img_param *param = NULL;
	struct img_param *temp_param;
	int bin = 0;
	int ret;
	char *buf;
	char *bin_buf = 0;

	if (argc < 3)
		return CMD_RET_USAGE;

	if (argc > 3 && !strcmp(argv[1], "-b")) {
		bin = 1;
		argv++;
	}

	buf = (char *)simple_strtoul(argv[1], NULL, 16);
	if (!buf) {
		printf("Error: cannot parse address\n");
		return CMD_RET_FAILURE;
	}

	for (enum hf_images i = 0; i < IMG_MAX_NUM; i++) {
		temp_param = get_img_params(i);
		if (!temp_param)
			continue;

		if (!strcmp(argv[2], temp_param->img_name)) {
			param = temp_param;
			break;
		}
	}

	if (!param) {
		printf("Error: incorrect image type!\n");
		return CMD_RET_FAILURE;
	}

	if (bin) {
		bin_buf = buf;
	} else {
		bin_buf = malloc(param->total_size);
		if (!bin_buf) {
			printf("Error: can't allocate 0x%x bytes\n", param->total_size);
			return CMD_RET_FAILURE;
		}
		printf("Converting srec to binary image...\n");
		if (!srec_to_bin(buf, bin_buf, param->total_size)) {
			printf("Error: failed to convert srec to bin\n");
			free(bin_buf);
			return CMD_RET_FAILURE;
		}
	}

	printf("Flashing location is 0x%x\n", param->start_addr);
	printf("Flashing 0x%x bytes to HyperFlash... ", param->total_size);

	ret = hf_write_image(param, bin_buf, param->total_size);
	if (!ret)
		printf("Success!\n");
	else
		printf("Error %d\n", ret);

	if (!bin)
		free(bin_buf);

	return 0;
}

static int do_hf_erase(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct img_param *param = get_img_params(IMG_SSTDATA);
	char *buf = calloc(1, param->total_size);
	int ret;

	printf("Erasing SecureStorage...\n");
	if (!buf) {
		printf("Error: can't allocate 0x%x bytes\n", param->total_size);
		return CMD_RET_FAILURE;
	}

	ret = hf_write_image(param, buf, param->total_size);
	if (ret) {
		printf("Error writing to HyperFlash(%d)\n", ret);
		free(buf);
		return CMD_RET_FAILURE;
	}
	printf("Success!\n");

	free(buf);
	return 0;
}

static int do_hf_types(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct img_param *param;

	printf("Name - hf_addr\n");
	for (enum hf_images i = 0; i < IMG_MAX_NUM; i++) {
		param = get_img_params(i);
		if (param)
			printf("%s - 0x%x\n", param->img_name, param->start_addr);
	}
	return 0;
}

static cmd_tbl_t cmd_hf_sub[] = {
	U_BOOT_CMD_MKENT(flash, 5, 0, do_hf_flash, "", ""),
	U_BOOT_CMD_MKENT(erase, 0, 0, do_hf_erase, "", ""),
	U_BOOT_CMD_MKENT(types, 0, 0, do_hf_types, "", ""),
};

static int do_hf(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'hf' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_hf_sub[0], ARRAY_SIZE(cmd_hf_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

static char hf_help_text[] =
"flash [-b] m_addr img_type - flash image of img_type from m_addr, -b for binary image, otherwise srec assumed\n"
"hf types - display image types\n"
"hf erase - erase SecureStorage";

U_BOOT_CMD(hf, 6, 0, do_hf,
	   "Access HyperFlash",
	   hf_help_text
	  );

