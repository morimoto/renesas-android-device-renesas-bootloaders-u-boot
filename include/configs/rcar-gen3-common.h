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

#ifdef CONFIG_SPL
#define CONFIG_SPL_TARGET	"spl/u-boot-spl.scif"
#endif

/* boot option */

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

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
#define CONFIG_SYS_MONITOR_LEN		(1 * 1024 * 1024)
#define CONFIG_SYS_MALLOC_LEN		(128 * 1024 * 1024)
#define CONFIG_SYS_BOOTM_LEN		(64 << 20)

/* The HF/QSPI layout permits up to 1 MiB large bootloader blob */
#define CONFIG_BOARD_SIZE_LIMIT		1048576

/*This values are used for RAM autodetection only*/
#define	BOARDCNF_8GBIT_1RANK	7	/* Board with H3 Ver.2.0 or later/SIP(8Gbit 1rank) */
#define	BOARDCNF_8GBIT_2RANK	8	/* Board with H3 Ver.2.0 or later/SIP(8Gbit 2rank) */


/* ENV setting */
#define CONFIG_ENV_OVERWRITE

/* SPL support */
#if defined(CONFIG_R8A7795) || defined(CONFIG_R8A7796) || defined(CONFIG_R8A77965)
#define CONFIG_SPL_BSS_START_ADDR	0xe633f000
#define CONFIG_SPL_BSS_MAX_SIZE		0x1000
#else
#define CONFIG_SPL_BSS_START_ADDR	0xe631f000
#define CONFIG_SPL_BSS_MAX_SIZE		0x1000
#endif
#define CONFIG_SPL_STACK		0xe6304000
#define CONFIG_SPL_MAX_SIZE		0x7000
#ifdef CONFIG_SPL_BUILD
#define CONFIG_CONS_SCIF2
#define CONFIG_SH_SCIF_CLK_FREQ		65000000
#endif

#define RCAR_GENERIC_PLAT_ID		0x00779000

#define CPU_ID_R8A7795				0x4F
#define CPU_ID_R8A7796				0x52
#define CPU_ID_R8A77965				0x55
#define CPU_ID_R8A77990				0x57
#define CPU_ID_R8A77995				0x58

#define I2C_POWERIC_EEPROM_OFFSET	0x70
#define I2C_POWERIC_EEPROM_ADDR		0x50

#ifndef __ASSEMBLY__
void rcar_preset_env(void);
void init_noreset_vars(void);
void restore_noreset_vars(void);
unsigned get_bootloader_size(void);
#endif

#ifdef CONFIG_OPTEE
/* Pseudo TA's UUID, which can interact with Hyper Flash */
#define HYPER_UUID \
	{ 0xc462df74, 0x657b, 0x4f2b, \
		{ 0xb7, 0x7e, 0x0e, 0x9b, 0x3e, 0x45, 0x27, 0x29 } }

/* 256KB */
#define HF_SECTOR_SIZE (0x40000U)

#ifndef __ASSEMBLY__
/* Hyper flash image IDs */
enum hf_images {
	IMG_PARAM,
	IMG_IPL2,
	IMG_CERT,
	IMG_BL31,
	IMG_OPTEE,
	IMG_UBOOT,
	IMG_SSTDATA,
	IMG_MAX_NUM
};

struct img_sector {
	unsigned size;
	unsigned flash_addr;
	unsigned char *buf;
};

struct img_param {
	struct img_sector *data;
	const char *img_name;
	unsigned sectors_num;
	unsigned img_id;
	unsigned start_addr;
	unsigned total_size;
};

struct img_param *get_img_params(enum hf_images image_id);
unsigned srec_to_bin(char *inbuf, char *outbuf, unsigned maxbin);

#endif /* !__ASSEMBLY__ */

/* Pseudo TA commands */
#define HYPER_CMD_INIT_DRV		(0)
#define HYPER_CMD_READ			(1)
#define HYPER_CMD_WRITE			(2)
#define HYPER_CMD_ERASE			(3)

#endif /* CONFIG_OPTEE */

#endif	/* __RCAR_GEN3_COMMON_H */
