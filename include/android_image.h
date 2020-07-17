/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * This is from the Android Project,
 * Repository: https://android.googlesource.com/platform/system/tools/mkbootimg
 * Branch: master
 * File: include/bootimg/bootimg.h
 * Commit: 5cc6456425d98d8fce685dcb13d3ad8baffccccf
 *
 * Copyright (C) 2007 The Android Open Source Project
 */

#ifndef _ANDROID_IMAGE_H_
#define _ANDROID_IMAGE_H_

#include <linux/compiler.h>
#include <linux/types.h>

#define ANDR_BOOT_MAGIC "ANDROID!"
#define ANDR_BOOT_MAGIC_SIZE 8
#define ANDR_BOOT_NAME_SIZE 16
#define ANDR_BOOT_ARGS_SIZE 512
#define ANDR_BOOT_EXTRA_ARGS_SIZE 1024

#define VENDOR_BOOT_MAGIC "VNDRBOOT"
#define VENDOR_BOOT_MAGIC_SIZE 8
#define VENDOR_BOOT_ARGS_SIZE 2048
#define VENDOR_BOOT_NAME_SIZE 16

#define BOOT_IMAGE_HEADER_V3_PAGESIZE 4096
#define VENDOR_BOOT_IMAGE_HEADER_V3_SIZE 2112

/* When the boot image header has a version of 3, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 4096 bytes
 * +---------------------+
 * | kernel              | m pages
 * +---------------------+
 * | ramdisk             | n pages
 * +---------------------+
 *
 * m = (kernel_size + 4096 - 1) / 4096
 * n = (ramdisk_size + 4096 - 1) / 4096
 *
 * In version 3 of the boot image header, page size is fixed at 4096 bytes.
 *
 * The structure of the vendor boot image (introduced with version 3 and
 * required to be present when a v3 boot image is used) is as follows:
 *
 * +---------------------+
 * | vendor boot header  | o pages
 * +---------------------+
 * | vendor ramdisk      | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+

 * o = (2112 + page_size - 1) / page_size
 * p = (vendor_ramdisk_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 *
 * 0. all entities in the boot image are 4096-byte aligned in flash, all
 *    entities in the vendor boot image are page_size (determined by the vendor
 *    and specified in the vendor boot image header) aligned in flash
 * 1. kernel, ramdisk, vendor ramdisk, and DTB are required (size != 0)
 * 2. load the kernel and DTB at the specified physical address (kernel_addr,
 *    dtb_addr)
 * 3. load the vendor ramdisk at ramdisk_addr
 * 4. load the generic ramdisk immediately following the vendor ramdisk in
 *    memory
 * 5. set up registers for kernel entry as required by your architecture
 * 6. if the platform has a second stage bootloader jump to it (must be
 *    contained outside boot and vendor boot partitions), otherwise
 *    jump to kernel_addr
 */
struct boot_img_hdr_v3 {
    /* Must be BOOT_MAGIC */
    char magic[ANDR_BOOT_MAGIC_SIZE];

    u32 kernel_size; /* size in bytes */
    u32 ramdisk_size; /* size in bytes */

    /* Operating system version and security patch level.
     * For version "A.B.C" and patch level "Y-M-D":
     * (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
     * os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0]
     */
    u32 os_version;

    u32 header_size;

    u32 reserved[4];

    /* Version of the boot image header */
    u32 header_version;

    char cmdline[ANDR_BOOT_ARGS_SIZE + ANDR_BOOT_EXTRA_ARGS_SIZE];
} __attribute__((packed));

struct vendor_boot_img_hdr_v3 {
    /* Must be VENDOR_BOOT_MAGIC */
    char magic[VENDOR_BOOT_MAGIC_SIZE];

    /* Version of the vendor boot image header */
    u32 header_version;

    u32 page_size; /* flash page size we assume */

    u32 kernel_addr; /* physical load addr */
    u32 ramdisk_addr; /* physical load addr */

    u32 vendor_ramdisk_size; /* size in bytes */

    char cmdline[VENDOR_BOOT_ARGS_SIZE];

    u32 tags_addr; /* physical addr for kernel tags (if required) */
    char name[VENDOR_BOOT_NAME_SIZE]; /* asciiz product name */

    u32 header_size;

    u32 dtb_size; /* size in bytes for DTB image */
    u64 dtb_addr; /* physical load address for DTB image */
} __attribute__((packed));

#endif
