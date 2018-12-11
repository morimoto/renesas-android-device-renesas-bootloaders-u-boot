/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * include/configs/rcar-gen3-common.h
 *	This file is R-Car Gen3 common configuration file.
 *
 * Copyright (C) 2015-2017 Renesas Electronics Corporation
 */

#ifndef __RCAR_GEN3_COMMON_H
#define __RCAR_GEN3_COMMON_H

#include <asm/arch/rmobile.h>

#define CONFIG_REMAKE_ELF
#define CONFIG_BUILD_TARGET	"u-boot-elf.srec"

/* boot option */

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

#undef	CONFIG_SHOW_BOOT_PROGRESS

#define CONFIG_ARCH_CPU_INIT

/* Generic Interrupt Controller Definitions */
#define CONFIG_GICV2
#define GICD_BASE	0xF1010000
#define GICC_BASE	0xF1020000

/* console */
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BAUDRATE_TABLE	{ 115200, 38400 }

/* MEMORY */
#define CONFIG_SYS_INIT_SP_ADDR		CONFIG_SYS_TEXT_BASE

#define DRAM_RSV_SIZE			0x08000000
#define CONFIG_SYS_SDRAM_BASE		(0x40000000 + DRAM_RSV_SIZE)
#define CONFIG_SYS_SDRAM_SIZE		(0x80000000u - DRAM_RSV_SIZE)
#define CONFIG_SYS_LOAD_ADDR		0x58000000
#define CONFIG_LOADADDR			CONFIG_SYS_LOAD_ADDR
#define CONFIG_VERY_BIG_RAM
#define CONFIG_MAX_MEM_MAPPED		(0x80000000u - DRAM_RSV_SIZE)

#define CONFIG_SYS_MONITOR_BASE		0x00000000
#define CONFIG_SYS_MONITOR_LEN		(256 * 1024)
#define CONFIG_SYS_MALLOC_LEN		(40 * 1024 * 1024)
#define CONFIG_SYS_BOOTMAPSZ		(8 * 1024 * 1024)
#define CONFIG_SYS_BOOTM_LEN		(64 << 20)

/*This values are used for RAM autodetection only*/
#define	BOARDCNF_8GBIT_1RANK	7	/* Board with H3 Ver.2.0 or later/SIP(8Gbit 1rank) */
#define	BOARDCNF_8GBIT_2RANK	8	/* Board with H3 Ver.2.0 or later/SIP(8Gbit 2rank) */


/* ENV setting */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_SECT_SIZE	(128 * 1024)
#define CONFIG_ENV_SIZE		(CONFIG_ENV_SECT_SIZE)
#define CONFIG_ENV_SIZE_REDUND	(CONFIG_ENV_SIZE)

#define RCAR_GENERIC_PLAT_ID		0x00779000

#define CPU_ID_R8A7795				0x4F
#define CPU_ID_R8A7796				0x52
#define CPU_ID_R8A77965				0x55
#define CPU_ID_R8A77990				0x57
#define CPU_ID_R8A77995				0x58

#define I2C_POWERIC_EEPROM_OFFSET	0x70
#define I2C_POWERIC_EEPROM_ADDR		0x50

/*
 * This define is specify the bootloader partition size
 * in mmcblk0boot0/1.
 * 8192 blocks = 4Mb
 */
#define BOOTLOADER_BLK_SIZE			(8192)

#ifndef __ASSEMBLY__
void rcar_preset_env(void);
void init_noreset_vars(void);
void restore_noreset_vars(void);
#endif

#endif	/* __RCAR_GEN3_COMMON_H */
