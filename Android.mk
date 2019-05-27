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

UBOOT_SRC               := $(abspath ./device/renesas/bootloaders/u-boot)
UBOOT_OUT               := $(PRODUCT_OUT)/obj/UBOOT_OBJ
UBOOT_OUT_ABS           := $(abspath $(UBOOT_OUT))

UBOOT_BINARY            := $(UBOOT_OUT)/u-boot.bin
UBOOT_BINARY_INSTALLED  := $(PRODUCT_OUT)/u-boot.bin
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

$(UBOOT_OUT):
	$(MKDIR) -p $(UBOOT_OUT_ABS)

$(UBOOT_BINARY): $(UBOOT_OUT)
	$(UBOOT_ARCH_PARAMS) $(ANDROID_MAKE) -C $(UBOOT_SRC) O=$(UBOOT_OUT_ABS) mrproper
	$(UBOOT_ARCH_PARAMS) $(ANDROID_MAKE) -C $(UBOOT_SRC) O=$(UBOOT_OUT_ABS) $(TARGET_BOARD_PLATFORM)_$(TARGET_BOOTLOADER_BOARD_NAME)_defconfig
	$(UBOOT_ARCH_PARAMS) $(ANDROID_MAKE) -C $(UBOOT_SRC) O=$(UBOOT_OUT_ABS) KCFLAGS+="$(UBOOT_KCFLAGS)" -j `$(NPROC)`

$(UBOOT_SREC): $(UBOOT_BINARY)

# ----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE                := u-boot.bin
LOCAL_PREBUILT_MODULE_FILE  := $(UBOOT_BINARY)
LOCAL_MODULE_PATH           := $(PRODUCT_OUT)
LOCAL_MODULE_CLASS          := EXECUTABLES
include $(BUILD_PREBUILT)
$(LOCAL_BUILT_MODULE): $(LOCAL_PREBUILT_MODULE_FILE)

include $(CLEAR_VARS)
LOCAL_MODULE                := u-boot-elf.srec
LOCAL_PREBUILT_MODULE_FILE  := $(UBOOT_SREC)
LOCAL_MODULE_PATH           := $(PRODUCT_OUT)
LOCAL_MODULE_CLASS          := EXECUTABLES
include $(BUILD_PREBUILT)
$(LOCAL_BUILT_MODULE): $(LOCAL_PREBUILT_MODULE_FILE)

endif # TARGET_PRODUCT salvator ulcb kingfisher
