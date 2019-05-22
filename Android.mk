#
# Copyright (C) 2011 The Android Open-Source Project
# Copyright (C) 2018 GlobalLogic
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Include only for Renesas ones.
ifneq (,$(filter $(TARGET_PRODUCT), salvator ulcb kingfisher))

PRODUCT_OUT_ABS := $(abspath $(PRODUCT_OUT))

UBOOT_SRC           := $(abspath ./device/renesas/bootloaders/u-boot)
UBOOT_OUT           := $(PRODUCT_OUT_ABS)/obj/UBOOT_OBJ
UBOOT_KCFLAGS       := -fgnu89-inline
UBOOT_CROSS_COMPILE := $(BSP_GCC_CROSS_COMPILE)
UBOOT_HOST_TOOLCHAIN := $(abspath ./prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.17-4.8/bin/x86_64-linux-)
UBOOT_ARCH_PARAMS := HOST_TOOLCHAIN=$(UBOOT_HOST_TOOLCHAIN) CROSS_COMPILE=$(UBOOT_CROSS_COMPILE) ARCH=$(TARGET_ARCH)
ifeq ($(H3_OPTION),8GB)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP4_2
else
ifeq ($(H3_OPTION),4GB)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP4_1
else
ifeq ($(H3_OPTION),4GB2x2)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP2_2
else
ifeq ($(H3_OPTION),DYNAMIC)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_AUTO
else
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP4_1
endif
endif
endif
endif

ifeq ($(ENABLE_PRODUCT_PART),true)
	UBOOT_KCFLAGS_PARTS = -DENABLE_PRODUCT_PART
endif

ifeq ($(TARGET_MMC_ONE_SLOT),true)
	UBOOT_KCFLAGS="-fgnu89-inline -DANDROID_MMC_ONE_SLOT"
endif


.PHONY: uboot_out_dir
uboot_out_dir:
	$(MKDIR) -p $(UBOOT_OUT)

.PHONY: u-boot
u-boot: uboot_out_dir
	@echo "Building u-boot"
	$(UBOOT_ARCH_PARAMS) make -C $(UBOOT_SRC) O=$(UBOOT_OUT) mrproper
	$(UBOOT_ARCH_PARAMS) make -C $(UBOOT_SRC) O=$(UBOOT_OUT) $(TARGET_BOARD_PLATFORM)_$(TARGET_BOOTLOADER_BOARD_NAME)_defconfig
	$(UBOOT_ARCH_PARAMS) make -C $(UBOOT_SRC) O=$(UBOOT_OUT) KCFLAGS=$(UBOOT_KCFLAGS) KCFLAGS+=$(UBOOT_KCFLAGS_MEM) KCFLAGS+=$(UBOOT_KCFLAGS_PARTS)

.PHONY: u-boot.bin
u-boot.bin: u-boot
	cp $(UBOOT_OUT)/u-boot.bin $(PRODUCT_OUT_ABS)

.PHONY: u-boot.srec
u-boot.srec: u-boot
	cp $(UBOOT_OUT)/u-boot.srec $(PRODUCT_OUT_ABS)

endif # TARGET_PRODUCT salvator ulcb kingfisher


