// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2008 - 2009
 * Windriver, <www.windriver.com>
 * Tom Rix <Tom.Rix@windriver.com>
 *
 * Copyright 2011 Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 */

#include <common.h>
#include <fastboot.h>
#include <net/fastboot.h>
#include <android/bootloader.h>
#include <bootm.h>

/**
 * fastboot_buf_addr - base address of the fastboot download buffer
 */
void *fastboot_buf_addr;

/**
 * fastboot_buf_size - size of the fastboot download buffer
 */
u32 fastboot_buf_size;

/**
 * fastboot_progress_callback - callback executed during long operations
 */
void (*fastboot_progress_callback)(const char *msg);

/**
 * fastboot_response() - Writes a response of the form "$tag$reason".
 *
 * @tag: The first part of the response
 * @response: Pointer to fastboot response buffer
 * @format: printf style format string
 */
void fastboot_response(const char *tag, char *response,
		       const char *format, ...)
{
	va_list args;

	strlcpy(response, tag, FASTBOOT_RESPONSE_LEN);
	if (format) {
		va_start(args, format);
		vsnprintf(response + strlen(response),
			  FASTBOOT_RESPONSE_LEN - strlen(response) - 1,
			  format, args);
		va_end(args);
	}
}

/**
 * fastboot_fail() - Write a FAIL response of the form "FAIL$reason".
 *
 * @reason: Pointer to returned reason string
 * @response: Pointer to fastboot response buffer
 */
void fastboot_fail(const char *reason, char *response)
{
	fastboot_response("FAIL", response, "%s", reason);
}

/**
 * fastboot_okay() - Write an OKAY response of the form "OKAY$reason".
 *
 * @reason: Pointer to returned reason string, or NULL to send a bare "OKAY"
 * @response: Pointer to fastboot response buffer
 */
void fastboot_okay(const char *reason, char *response)
{
	if (reason)
		fastboot_response("OKAY", response, "%s", reason);
	else
		fastboot_response("OKAY", response, NULL);
}

/**
 * fastboot_set_reboot_flag() - Set flag to indicate reboot-bootloader
 *
 * Set flag which indicates that we should reboot into the bootloader
 * following the reboot that fastboot executes after this function.
 *
 * This function should be overridden in your board file with one
 * which sets whatever flag your board specific Android bootloader flow
 * requires in order to re-enter the bootloader.
 */
int fastboot_set_reboot_flag(void)
{
	struct bootloader_message boot;

	memset(&boot, 0, sizeof(boot));
	strlcpy(boot.command, "bootloader", sizeof(boot.command));

	return set_bootloader_message(&boot);
}

/**
 * fastboot_set_fastbootd_flag() - Set flag to indicate reboot fastbootd
 *
 * Set flag which indicates that we should reboot into the fastbootd
 */
int fastboot_set_fastbootd_flag(void)
{
	struct bootloader_message boot;

	memset(&boot, 0, sizeof(boot));

	/* Replace the command & recovery fields. */
	strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
	strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));

	strncat(boot.recovery, "--fastboot\n", sizeof(boot.recovery));

	return set_bootloader_message(&boot);
}

/**
 * fastboot_get_progress_callback() - Return progress callback
 *
 * Return: Pointer to function called during long operations
 */
void (*fastboot_get_progress_callback(void))(const char *)
{
	return fastboot_progress_callback;
}

/**
 * fastboot_boot() - Execute fastboot boot command
 *
 * If ${fastboot_bootcmd} is set, run that command to execute the boot
 * process, if that returns, then exit the fastboot server and return
 * control to the caller.
 *
 * Otherwise execute "bootm <fastboot_buf_addr>", if that fails, reset
 * the board.
 */
void fastboot_boot(void)
{
	char *s;

	s = env_get("fastboot_bootcmd");
	if (s) {
		run_command(s, CMD_FLAG_ENV);
	} else {
		static char boot_addr_start[19];
		static char *const boota_args[] = {
			"boota", "-1", "RAM", boot_addr_start, "avb", NULL
		};

        snprintf(boot_addr_start, sizeof(boot_addr_start),
			 "0x%p", fastboot_buf_addr);
        printf("Booting kernel at %s...\n\n\n", boot_addr_start);

        do_boota(NULL, 0, ARRAY_SIZE(boota_args) - 1, boota_args);

		/*
		 * This only happens if image is somehow faulty so we start
		 * over. We deliberately leave this policy to the invocation
		 * of fastbootcmd if that's what's being run
		 */
		do_reset(NULL, 0, 0, NULL);
	}
}

/**
 * fastboot_set_progress_callback() - set progress callback
 *
 * @progress: Pointer to progress callback
 *
 * Set a callback which is invoked periodically during long running operations
 * (flash and erase). This can be used (for example) by the UDP transport to
 * send INFO responses to keep the client alive whilst those commands are
 * executing.
 */
void fastboot_set_progress_callback(void (*progress)(const char *msg))
{
	fastboot_progress_callback = progress;
}

/*
 * fastboot_init() - initialise new fastboot protocol session
 *
 * @buf_addr: Pointer to download buffer, or NULL for default
 * @buf_size: Size of download buffer, or zero for default
 */
void fastboot_init(void *buf_addr, u32 buf_size)
{
	fastboot_buf_addr = buf_addr ? buf_addr :
						(void *)CONFIG_FASTBOOT_BUF_ADDR;
	fastboot_buf_size = buf_size ? buf_size : CONFIG_FASTBOOT_BUF_SIZE;
	fastboot_set_progress_callback(NULL);
}
