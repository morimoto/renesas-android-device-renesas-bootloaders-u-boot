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

NPROC := /usr/bin/nproc

ifeq ($(PRODUCT_OUT),)
$(error "PRODUCT_OUT is not set")
endif

PRODUCT_OUT_ABS         := $(abspath $(PRODUCT_OUT))

UBOOT_SRC               := $(abspath ./device/renesas/bootloaders/u-boot)
UBOOT_OUT               := $(PRODUCT_OUT)/obj/UBOOT_OBJ
UBOOT_OUT_ABS           := $(abspath $(UBOOT_OUT))

UBOOT_BINARY            := $(UBOOT_OUT)/u-boot.bin
UBOOT_SREC              := $(UBOOT_OUT)/u-boot-elf.srec

UBOOT_KCFLAGS           := -fgnu89-inline
UBOOT_ARCH_PARAMS       := HOST_TOOLCHAIN=$(BSP_GCC_HOST_TOOLCHAIN) CROSS_COMPILE=$(BSP_GCC_CROSS_COMPILE) ARCH=$(TARGET_ARCH)

ifeq ($(H3_OPTION),8GB)
    UBOOT_KCFLAGS += -DRCAR_DRAM_MAP4_2
else
ifeq ($(H3_OPTION),4GB)
    UBOOT_KCFLAGS += -DRCAR_DRAM_MAP4_1
else
ifeq ($(H3_OPTION),4GB2x2)
    UBOOT_KCFLAGS += -DRCAR_DRAM_MAP2_2
else
ifeq ($(H3_OPTION),DYNAMIC)
    UBOOT_KCFLAGS += -DRCAR_DRAM_AUTO
else
    UBOOT_KCFLAGS += -DRCAR_DRAM_MAP4_1
endif
endif
endif
endif

ifeq ($(TARGET_MMC_ONE_SLOT),true)
    UBOOT_KCFLAGS += -DANDROID_MMC_ONE_SLOT
endif

u-boot:
	$(MKDIR) -p $(UBOOT_OUT_ABS)
	$(UBOOT_ARCH_PARAMS) $(ANDROID_MAKE) -C $(UBOOT_SRC) O=$(UBOOT_OUT_ABS) mrproper
	$(UBOOT_ARCH_PARAMS) $(ANDROID_MAKE) -C $(UBOOT_SRC) O=$(UBOOT_OUT_ABS) $(TARGET_BOARD_PLATFORM)_$(TARGET_BOOTLOADER_BOARD_NAME)_defconfig
	$(UBOOT_ARCH_PARAMS) $(ANDROID_MAKE) -C $(UBOOT_SRC) O=$(UBOOT_OUT_ABS) KCFLAGS+="$(UBOOT_KCFLAGS)" -j `$(NPROC)`
	cp -vF $(UBOOT_OUT_ABS)/u-boot.bin $(UBOOT_OUT_ABS)/u-boot-elf.srec $(PRODUCT_OUT_ABS)/

# ----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE                := u-boot
LOCAL_MODULE_TAGS           := optional
include $(BUILD_PHONY_PACKAGE)

endif # TARGET_PRODUCT salvator ulcb kingfisher
