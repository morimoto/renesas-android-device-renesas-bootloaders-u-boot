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

PRODUCT_OUT_ABS := $(abspath $(PRODUCT_OUT))

UBOOT_SRC           := $(abspath ./device/renesas/bootloaders/u-boot)
UBOOT_OUT           := $(PRODUCT_OUT_ABS)/obj/UBOOT_OBJ
UBOOT_KCFLAGS       := -fgnu89-inline
UBOOT_CROSS_COMPILE := $(abspath ./prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu/bin/aarch64-linux-gnu-)

ifeq ($(H3_OPTION),8GB)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP4_2
else
ifeq ($(H3_OPTION),4GB)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP4_1
else
ifeq ($(H3_OPTION),4GB2x2)
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_MAP2_2
else
    UBOOT_KCFLAGS_MEM = -DRCAR_DRAM_AUTO
endif
endif
endif

ifeq ($(ENABLE_ADSP),true)
	UBOOT_KCFLAGS_ADSP = -DENABLE_ADSP
endif

ifeq ($(TARGET_MMC_ONE_SLOT),true)
	UBOOT_KCFLAGS="-fgnu89-inline -DANDROID_MMC_ONE_SLOT"
endif

$(U-BOOT_OUT):
	$(hide) mkdir -p $(UBOOT_OUT)

u-boot: $(U-BOOT_OUT)
	@echo "Building u-boot"
	$(hide) CROSS_COMPILE=$(UBOOT_CROSS_COMPILE) ARCH=$(TARGET_ARCH) make -C $(UBOOT_SRC) O=$(UBOOT_OUT) mrproper
	$(hide) CROSS_COMPILE=$(UBOOT_CROSS_COMPILE) ARCH=$(TARGET_ARCH) make -C $(UBOOT_SRC) O=$(UBOOT_OUT) $(TARGET_BOARD_PLATFORM)_$(TARGET_BOOTLOADER_BOARD_NAME)_defconfig
	$(hide) CROSS_COMPILE=$(UBOOT_CROSS_COMPILE) ARCH=$(TARGET_ARCH) make -C $(UBOOT_SRC) O=$(UBOOT_OUT) KCFLAGS=$(UBOOT_KCFLAGS) KCFLAGS+=$(UBOOT_KCFLAGS_MEM) KCFLAGS+=$(UBOOT_KCFLAGS_ADSP)

.PHONY: u-boot

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

U_BOOT_BIN_PATH := $(UBOOT_OUT)/u-boot.bin
$(U_BOOT_BIN_PATH): u-boot

LOCAL_MODULE := u-boot.bin
LOCAL_PREBUILT_MODULE_FILE:= $(U_BOOT_BIN_PATH)
LOCAL_MODULE_PATH := $(PRODUCT_OUT_ABS)

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

U_BOOT_BIN_PATH := $(UBOOT_OUT)/u-boot.bin
$(U_BOOT_BIN_PATH): u-boot

LOCAL_MODULE := u-boot_hf.bin
LOCAL_PREBUILT_MODULE_FILE:= $(U_BOOT_BIN_PATH)
LOCAL_MODULE_PATH := $(PRODUCT_OUT_ABS)

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

U_BOOT_SREC_PATH := $(UBOOT_OUT)/u-boot-elf.srec
$(U_BOOT_SREC_PATH): u-boot

LOCAL_MODULE := u-boot-elf_hf.srec
LOCAL_PREBUILT_MODULE_FILE:= $(U_BOOT_SREC_PATH)
LOCAL_MODULE_PATH := $(PRODUCT_OUT_ABS)

include $(BUILD_EXECUTABLE)
