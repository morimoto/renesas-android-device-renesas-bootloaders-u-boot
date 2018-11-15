// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/* #define	DEBUG	*/

#include <common.h>
#include <autoboot.h>
#include <cli.h>
#include <console.h>
#include <version.h>
#include <fastboot.h>

#if defined(CONFIG_CMD_FASTBOOT)
extern int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);
#endif
/*
 * Board-specific Platform code can reimplement show_boot_progress () if needed
 */
__weak void show_boot_progress(int val) {}

static void run_preboot_environment_command(void)
{
#ifdef CONFIG_PREBOOT
	char *p;

	p = env_get("preboot");
	if (p != NULL) {
# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif

		run_command_list(p, -1, 0);

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif
	}
#endif /* CONFIG_PREBOOT */
}

/* Read command from BCB */
#if defined(CONFIG_ANDROID_BOOT_IMAGE) && defined(CONFIG_CMD_FASTBOOT)
static void load_android_bootloader_params(void)
{
	struct bootloader_message bcb;
	char * bootmode = "android";

	if(get_bootloader_message(&bcb)) {
		printf("Failed to read Android bootloader record.\n");
		return;
	}

	if (strstr(bcb.command, "bootloader") != NULL) {
		bootmode = "fastboot";

		if ((bcb.status[0] != 0) && (bcb.status[0] != 255)) {
			printf("Boot status: %.*s\n", (int)sizeof(bcb.status), bcb.status);
			if (strncmp("OKAY", bcb.status, strlen("OKAY")))
				printf("ERROR! Last operation has failed!\n");
		}

		/* Clear only recovery part of bootloader message */
		memset(&bcb.command, 0, sizeof(bcb.command));
		memset(&bcb.status, 0, sizeof(bcb.status));
		memset(&bcb.recovery, 0, sizeof(bcb.recovery));

		if(set_bootloader_message(&bcb)) {
			printf("Error writing bcb!\n");
		}
	} else if (strstr(bcb.command, "recovery") != NULL) {
		bootmode = "recovery";
	}

	printf("Setting bootmode '%s'\n", bootmode);
	env_set("bootmode", bootmode);
}
#endif

/* We come here after U-Boot is initialised and ready to process commands */
void main_loop(void)
{
	const char *s;
	char *const fastboot_usb[] = {"usb", "24"};

	bootstage_mark_name(BOOTSTAGE_ID_MAIN_LOOP, "main_loop");

#ifdef CONFIG_VERSION_VARIABLE
	env_set("ver", version_string);  /* set version variable */
#endif /* CONFIG_VERSION_VARIABLE */

	cli_init();
	/* Set proper platform in environment */
	rcar_preset_env();

#if defined(CONFIG_ANDROID_BOOT_IMAGE) && defined(CONFIG_CMD_FASTBOOT)
	load_android_bootloader_params();

	if ((s = env_get("bootmode")) != NULL && !strcmp(s, "fastboot")) {
		do_fastboot(NULL, 0, 2, fastboot_usb);
	}
#endif

	run_preboot_environment_command();

#if defined(CONFIG_UPDATE_TFTP)
	update_tftp(0UL, NULL, NULL);
#endif /* CONFIG_UPDATE_TFTP */

	s = bootdelay_process();
	if (cli_process_fdt(&s))
		cli_secure_boot_cmd(s);

	autoboot_command(s);

	cli_loop();
	panic("No CLI available");
}
