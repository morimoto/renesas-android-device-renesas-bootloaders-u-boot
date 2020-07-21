// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 */

#include <common.h>
#include <env.h>
#include <image.h>
#include <android_image.h>
#include <malloc.h>
#include <errno.h>
#include <asm/unaligned.h>

#define ANDROID_IMAGE_DEFAULT_KERNEL_ADDR	0x10008000

static char andr_tmp_str[ANDR_BOOT_ARGS_SIZE + 1];

static ulong android_image_get_kernel_addr(
		const struct vendor_boot_img_hdr_v3 *hdr)
{
	/*
	 * All the Android tools that generate a boot.img use this
	 * address as the default.
	 *
	 * Even though it doesn't really make a lot of sense, and it
	 * might be valid on some platforms, we treat that adress as
	 * the default value for this field, and try to execute the
	 * kernel in place in such a case.
	 *
	 * Otherwise, we will return the actual value set by the user.
	 */
	if (hdr->kernel_addr == ANDROID_IMAGE_DEFAULT_KERNEL_ADDR)
		return (ulong)hdr + hdr->page_size;

	return hdr->kernel_addr;
}

/**
 * android_image_get_kernel() - processes kernel part of Android boot images
 * @hdr:	Pointer to android image header.
 * @vhdr:	Pointer to vendor image header.
 * @verify:	Checksum verification flag. Currently unimplemented.
 * @os_data:	Pointer to a ulong variable, will hold os data start
 *			address.
 * @os_len:	Pointer to a ulong variable, will hold os data length.
 *
 * This function returns the os image's start address and length. Also,
 * it appends the kernel command line to the bootargs env variable.
 *
 * Return: Zero, os start address and length on success,
 *		otherwise on failure.
 */
int android_image_get_kernel(const struct boot_img_hdr_v3 *hdr,
		const struct vendor_boot_img_hdr_v3 *vhdr, int verify,
		ulong *os_data, ulong *os_len)
{
	/* Skip boot image header */
	const struct image_header *ihdr = (const struct image_header *)
		((uintptr_t)hdr + BOOT_IMAGE_HEADER_V3_PAGESIZE);

	/*
	 * Not all Android tools use the id field for signing the image with
	 * sha1 (or anything) so we don't check it. It is not obvious that the
	 * string is null terminated so we take care of this.
	 */
	if (vhdr) {
		strncpy(andr_tmp_str, vhdr->name, VENDOR_BOOT_NAME_SIZE);
		andr_tmp_str[VENDOR_BOOT_NAME_SIZE] = '\0';
		if (strlen(andr_tmp_str))
			printf("Product name: %s\n", andr_tmp_str);

		printf("Kernel load addr 0x%08x size %u KiB\n",
				(u32)android_image_get_kernel_addr(vhdr),
				DIV_ROUND_UP(hdr->kernel_size, 1024));
	}

	/*Append boot cmdline to vendor_boot cmdline */
	int len = 0;
	if (*hdr->cmdline) {
		printf("GKI command line: %s\n", hdr->cmdline);
		len += strlen(hdr->cmdline);
	} else {
		printf("GKI command line is empty\n");
	}
	if (vhdr && *vhdr->cmdline) {
		printf("Vendor command line: %s\n", vhdr->cmdline);
		len += strlen(vhdr->cmdline);
	} else {
		printf("Vendor command line is empty\n");
	}

	char *bootargs = env_get("bootargs");
	if (bootargs)
		len += strlen(bootargs);

	char *newbootargs = malloc(len + 2);
	if (!newbootargs) {
		puts("Error: malloc in android_image_get_kernel failed!\n");
		return -ENOMEM;
	}
	*newbootargs = '\0';

	if (bootargs) {
		strcpy(newbootargs, bootargs);
		strcat(newbootargs, " ");
	}
	if (*hdr->cmdline) {
		strcat(newbootargs, hdr->cmdline);
		strcat(newbootargs, " ");
	}
	if (vhdr && *vhdr->cmdline) {
		strcat(newbootargs, vhdr->cmdline);
	}

	env_set("bootargs", newbootargs);

	if (os_data) {
		if (image_get_magic(ihdr) == IH_MAGIC) {
			*os_data = image_get_data(ihdr);
		} else {
			*os_data = (ulong)hdr;
			/* Skip boot image header */
			*os_data += BOOT_IMAGE_HEADER_V3_PAGESIZE;
		}
	}
	if (os_len) {
		if (image_get_magic(ihdr) == IH_MAGIC)
			*os_len = image_get_data_size(ihdr);
		else
			*os_len = hdr->kernel_size;
	}
	return 0;
}

int android_image_check_header(const struct boot_img_hdr_v3 *hdr)
{
	return memcmp(ANDR_BOOT_MAGIC, hdr->magic, ANDR_BOOT_MAGIC_SIZE);
}

int vendor_image_check_header(const struct vendor_boot_img_hdr_v3 *hdr)
{
	return memcmp(VENDOR_BOOT_MAGIC, hdr->magic, VENDOR_BOOT_MAGIC_SIZE);
}

ulong android_image_get_end(const struct boot_img_hdr_v3 *hdr)
{
	ulong end;
	/*
	 * The header takes a full page, the remaining components are aligned
	 * on page boundary
	 */
	end = (ulong)hdr;
	end += BOOT_IMAGE_HEADER_V3_PAGESIZE;
	end += ALIGN(hdr->kernel_size, BOOT_IMAGE_HEADER_V3_PAGESIZE);
	end += ALIGN(hdr->ramdisk_size, BOOT_IMAGE_HEADER_V3_PAGESIZE);

	return end;
}

ulong android_image_get_kload(const struct vendor_boot_img_hdr_v3 *hdr)
{
	return android_image_get_kernel_addr(hdr);
}

ulong android_image_get_kcomp(const struct boot_img_hdr_v3 *hdr)
{
	const void *p = (void *)((uintptr_t)hdr + BOOT_IMAGE_HEADER_V3_PAGESIZE);
	if (image_get_magic((image_header_t *)p) == IH_MAGIC)
		return image_get_comp((image_header_t *)p);
	else if (get_unaligned_le32(p) == LZ4F_MAGIC)
		return IH_COMP_LZ4;
	else
		return IH_COMP_NONE;
}

int concatenated_image_get_ramdisk(const struct boot_img_hdr_v3 *hdr,
		const struct vendor_boot_img_hdr_v3 *vhdr,
		ulong *rd_data, ulong *rd_len)
{
	if (!hdr->ramdisk_size) {
		*rd_data = *rd_len = 0;
		return -1;
	}

	printf("Concatenated RAM disk load addr 0x%08x size %u KiB\n",
			vhdr->ramdisk_addr,
			DIV_ROUND_UP(hdr->ramdisk_size + vhdr->vendor_ramdisk_size, 1024));
#if defined(CONFIG_RCAR_GEN3)
	*rd_data = vhdr->ramdisk_addr;
#else
	*rd_data = (unsigned long)hdr;
	*rd_data += BOOT_IMAGE_HEADER_V3_PAGESIZE;
	*rd_data += ALIGN(hdr->kernel_size, BOOT_IMAGE_HEADER_V3_PAGESIZE);
#endif
	*rd_len = hdr->ramdisk_size + vhdr->vendor_ramdisk_size;
	return 0;
}

/**
 * android_print_contents - prints out the contents of the Android format image
 * @hdr: pointer to the Android format image header
 *
 * android_print_contents() formats a multi line Android image contents
 * description.
 * The routine prints out Android image properties
 *
 * returns:
 *     no returned results
 */
void android_print_contents(const struct boot_img_hdr_v3 *hdr)
{
	const char * const p = IMAGE_INDENT_STRING;
	/* os_version = ver << 11 | lvl */
	u32 os_ver = hdr->os_version >> 11;
	u32 os_lvl = hdr->os_version & ((1U << 11) - 1);

	printf("Android image properties:\n");
	printf("%skernel size:          %x\n", p, hdr->kernel_size);
	printf("%sramdisk size:         %x\n", p, hdr->ramdisk_size);
	/* ver = A << 14 | B << 7 | C         (7 bits for each of A, B, C)
	 * lvl = ((Y - 2000) & 127) << 4 | M  (7 bits for Y, 4 bits for M) */
	printf("%sos_version:           %x (ver: %u.%u.%u, level: %u.%u)\n",
	       p, hdr->os_version,
	       (os_ver >> 7) & 0x7F, (os_ver >> 14) & 0x7F, os_ver & 0x7F,
	       (os_lvl >> 4) + 2000, os_lvl & 0x0F);
	printf("%scmdline:              %s\n", p, hdr->cmdline);
	printf("%sheader_version:       %d\n", p, hdr->header_version);
}

void vendor_print_contents(const struct vendor_boot_img_hdr_v3 *hdr)
{
	const char * const p = IMAGE_INDENT_STRING;

	printf("Vendor image properties:\n");
	printf("%skernel address:       %x\n", p, hdr->kernel_addr);
	printf("%sramdisk address:      %x\n", p, hdr->ramdisk_addr);
	printf("%svendor_ramdisk_size:  %x\n", p, hdr->vendor_ramdisk_size);
	printf("%stags address:         %x\n", p, hdr->tags_addr);
	printf("%spage size:            %x\n", p, hdr->page_size);
	printf("%sname:                 %s\n", p, hdr->name);
	printf("%scmdline:              %s\n", p, hdr->cmdline);
	printf("%sheader_version:       %d\n", p, hdr->header_version);
	printf("%sdtb size:             %x\n", p, hdr->dtb_size);
	printf("%sdtb addr:             %llx\n", p, hdr->dtb_addr);
}
