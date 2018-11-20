# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2017 Linaro Limited

obj-$(CONFIG_LIBAVB) += avb_chain_partition_descriptor.o avb_cmdline.o
obj-$(CONFIG_LIBAVB) += avb_crypto.o avb_footer.o avb_hashtree_descriptor.o
obj-$(CONFIG_LIBAVB) += avb_property_descriptor.o
obj-$(CONFIG_LIBAVB) += avb_slot_verify.o avb_util.o avb_version.o
obj-$(CONFIG_LIBAVB) += avb_descriptor.o avb_hash_descriptor.o
obj-$(CONFIG_LIBAVB) += avb_kernel_cmdline_descriptor.o avb_rsa.o avb_sha512.o
obj-$(CONFIG_LIBAVB) += avb_sysdeps_posix.o avb_vbmeta_image.o avb_crc32.o

ifdef CONFIG_LIBAVB_SHA256_V8_EXT
obj-$(CONFIG_LIBAVB_SHA256_V8_EXT) += avb_sha256_armv8.o sha256_armv8a_ce_a64.o
else
obj-$(CONFIG_LIBAVB) += avb_sha256.o
endif

ccflags-y = -DAVB_COMPILATION
