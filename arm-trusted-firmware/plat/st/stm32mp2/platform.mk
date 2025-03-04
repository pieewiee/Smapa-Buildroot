#
# Copyright (c) 2023-2024, STMicroelectronics - All Rights Reserved
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Extra partitions used to find FIP, contains:
# metadata (2) and fsbl-m (2) and the FIP partitions (default is 2).
STM32_EXTRA_PARTS	:=	6

include plat/st/common/common.mk

CRASH_REPORTING		:=	1
ENABLE_PIE		:=	1
PROGRAMMABLE_RESET_ADDRESS := 1
BL2_IN_XIP_MEM		:=	1

# Do not enable SPE (not supported on Arm v8.0).
ENABLE_SPE_FOR_LOWER_ELS :=	0

# Do not enable SVE (not supported on Arm v8.0).
ENABLE_SVE_FOR_NS	:=	0

# Enable PSCI v1.0 extended state ID format
PSCI_EXTENDED_STATE_ID	:= 1
PSCI_OS_INIT_MODE	:= 1

# Reduce the number of interruption in GIC context
GICV2_INTR_NUM		:=	416

# Default Device tree
DTB_FILE_NAME		?=	stm32mp257f-ev1.dtb

STM32MP25		:=	1
STM32MP_M33_TDCID	?=	0

STM32MP_USE_EXTERNAL_HEAP :=	1

ifeq (${TRUSTED_BOARD_BOOT},1)
# PKA algo to include
PKA_USE_NIST_P256	:=	1
PKA_USE_BRAINPOOL_P256T1:=	1
endif

# STM32 image header version v2.2
STM32_HEADER_VERSION_MAJOR:=	2
STM32_HEADER_VERSION_MINOR:=	2

PKA_USE_NIST_P256	?=	0
PKA_USE_BRAINPOOL_P256T1 ?=	0

STM32_HASH_VER		:=	4
STM32_RNG_VER		:=	4

# Download load address for serial boot devices
DWL_BUFFER_BASE 	?=	0x87000000

# DDR types
STM32MP_DDR3_TYPE	?=	0
STM32MP_DDR4_TYPE	?=	0
STM32MP_LPDDR4_TYPE	?=	0
ifeq ($(STM32MP_M33_TDCID),0)
ifeq (${STM32MP_DDR3_TYPE},1)
DDR_TYPE		:=	ddr3
endif
ifeq (${STM32MP_DDR4_TYPE},1)
DDR_TYPE		:=	ddr4
endif
ifeq (${STM32MP_LPDDR4_TYPE},1)
DDR_TYPE		:=	lpddr4
endif

# DDR features
STM32MP_DDR_DUAL_AXI_PORT	:=	1
STM32MP_DDR_FIP_IO_STORAGE	:=	1
else #STM32MP_M33_TDCID
STM32MP_DDR_DUAL_AXI_PORT	:=	0
STM32MP_DDR_FIP_IO_STORAGE	:=	0
endif #STM32MP_M33_TDCID

# Device tree
BL2_DTSI		:=	stm32mp25-bl2.dtsi
FDT_SOURCES		:=	$(addprefix ${BUILD_PLAT}/fdts/, $(patsubst %.dtb,%-bl2.dts,$(DTB_FILE_NAME)))
BL31_DTSI		:=	stm32mp25-bl31.dtsi
FDT_SOURCES		+=	$(addprefix ${BUILD_PLAT}/fdts/, $(patsubst %.dtb,%-bl31.dts,$(DTB_FILE_NAME)))

ifneq (,$(wildcard fdts/$(patsubst %.dtb,%.dts,$(DTB_FILE_NAME))))
DT_SOURCE_PATH		:=	fdts
else
ifneq (,$(wildcard $(TFA_EXTERNAL_DT)/$(patsubst %.dtb,%.dts,$(DTB_FILE_NAME))))
DT_SOURCE_PATH		:=	$(TFA_EXTERNAL_DT)
else
$(error Cannot find $(patsubst %.dtb,%.dts,$(DTB_FILE_NAME)) file)
endif
endif

# Macros and rules to build TF binary
STM32_TF_STM32		:=	$(addprefix ${BUILD_PLAT}/tf-a-, $(patsubst %.dtb,%.stm32,$(DTB_FILE_NAME)))
STM32_LD_FILE		:=	plat/st/stm32mp2/${ARCH}/stm32mp2.ld.S
STM32_BINARY_MAPPING	:=	plat/st/stm32mp2/${ARCH}/stm32mp2.S

STM32MP_FW_CONFIG_NAME	:=	$(patsubst %.dtb,%-fw-config.dtb,$(DTB_FILE_NAME))
STM32MP_FW_CONFIG	:=	${BUILD_PLAT}/fdts/$(STM32MP_FW_CONFIG_NAME)
STM32MP_SOC_FW_CONFIG	:=	$(addprefix ${BUILD_PLAT}/fdts/, $(patsubst %.dtb,%-bl31.dtb,$(DTB_FILE_NAME)))
ifeq (${STM32MP_DDR_FIP_IO_STORAGE},1)
STM32MP_DDR_FW_PATH	?=	drivers/st/ddr/phy/firmware/bin/stm32mp2
STM32MP_DDR_FW_NAME	:=	${DDR_TYPE}_pmu_train.bin
STM32MP_DDR_FW		:=	${STM32MP_DDR_FW_PATH}/${STM32MP_DDR_FW_NAME}
endif
FDT_SOURCES		+=	$(addprefix $(DT_SOURCE_PATH)/, $(patsubst %.dtb,%.dts,$(STM32MP_FW_CONFIG_NAME)))
# Add the FW_CONFIG to FIP and specify the same to certtool
$(eval $(call TOOL_ADD_PAYLOAD,${STM32MP_FW_CONFIG},--fw-config))
# Add the SOC_FW_CONFIG to FIP and specify the same to certtool
ifeq ($(ENCRYPT_BL31),1)
$(eval $(call TOOL_ADD_IMG,STM32MP_SOC_FW_CONFIG,--soc-fw-config,,$(ENCRYPT_BL31)))
else
$(eval $(call TOOL_ADD_PAYLOAD,${STM32MP_SOC_FW_CONFIG},--soc-fw-config))
endif
ifeq (${STM32MP_DDR_FIP_IO_STORAGE},1)
# Add the FW_DDR to FIP and specify the same to certtool
$(eval $(call TOOL_ADD_IMG,STM32MP_DDR_FW,--ddr-fw))
endif
ifeq ($(GENERATE_COT),1)
STM32MP_CFG_CERT	:=	$(BUILD_PLAT)/stm32mp_cfg_cert.crt
# Add the STM32MP_CFG_CERT to FIP and specify the same to certtool
$(eval $(call TOOL_ADD_PAYLOAD,${STM32MP_CFG_CERT},--stm32mp-cfg-cert))
endif

# Enable flags for C files
$(eval $(call assert_booleans,\
	$(sort \
		PKA_USE_BRAINPOOL_P256T1 \
		PKA_USE_NIST_P256 \
		STM32MP_CRYPTO_ROM_LIB \
		STM32MP_DDR_DUAL_AXI_PORT \
		STM32MP_DDR_FIP_IO_STORAGE \
		STM32MP_DDR3_TYPE \
		STM32MP_DDR4_TYPE \
		STM32MP_LPDDR4_TYPE \
		STM32MP_M33_TDCID \
		STM32MP_USE_EXTERNAL_HEAP \
		STM32MP25 \
)))

$(eval $(call assert_numerics,\
	$(sort \
		PLAT_PARTITION_MAX_ENTRIES \
		STM32_HASH_VER \
		STM32_RNG_VER \
		STM32_TF_A_COPIES \
)))

$(eval $(call add_defines,\
	$(sort \
		DWL_BUFFER_BASE \
		PKA_USE_BRAINPOOL_P256T1 \
		PKA_USE_NIST_P256 \
		PLAT_DEF_FIP_UUID \
		PLAT_PARTITION_MAX_ENTRIES \
		PLAT_TBBR_IMG_DEF \
		STM32_HASH_VER \
		STM32_RNG_VER \
		STM32_TF_A_COPIES \
		STM32MP_CRYPTO_ROM_LIB \
		STM32MP_DDR_DUAL_AXI_PORT \
		STM32MP_DDR_FIP_IO_STORAGE \
		STM32MP_DDR3_TYPE \
		STM32MP_DDR4_TYPE \
		STM32MP_LPDDR4_TYPE \
		STM32MP_M33_TDCID \
		STM32MP_USE_EXTERNAL_HEAP \
		STM32MP25 \
)))

# STM32MP2x is based on Cortex-A35, which is Armv8.0, and does not support BTI
# Disable mbranch-protection to avoid adding useless code
TF_CFLAGS		+=	-mbranch-protection=none

# Include paths and source files
PLAT_INCLUDES		+=	-Iplat/st/stm32mp2/include/
PLAT_INCLUDES		+=	-Idrivers/st/ddr/phy/phyinit/include/
PLAT_INCLUDES		+=	-Idrivers/st/ddr/phy/firmware/include/

PLAT_BL_COMMON_SOURCES	+=	drivers/st/pmic/stm32mp_pmic2.c				\
				drivers/st/pmic/stpmic2.c				\

PLAT_BL_COMMON_SOURCES	+=	drivers/st/i2c/stm32_i2c.c

PLAT_BL_COMMON_SOURCES	+=	plat/st/stm32mp2/stm32mp2_private.c

PLAT_BL_COMMON_SOURCES	+=	drivers/st/uart/${ARCH}/stm32_console.S

PLAT_BL_COMMON_SOURCES	+=	lib/cpus/${ARCH}/cortex_a35.S

ifeq ($(STM32MP_M33_TDCID),0)
PLAT_BL_COMMON_SOURCES	+=	drivers/st/bsec/bsec3.c
else
PLAT_BL_COMMON_SOURCES	+=	plat/st/stm32mp2/stm32mp2_otp.c
endif

PLAT_BL_COMMON_SOURCES	+=	drivers/st/iwdg/stm32_iwdg.c				\
				drivers/st/reset/stm32mp2_reset.c			\
				plat/st/stm32mp2/${ARCH}/stm32mp2_helper.S		\
				plat/st/stm32mp2/stm32mp2_syscfg.c			\
				plat/st/stm32mp2/stm32mp2_pwr.c

PLAT_BL_COMMON_SOURCES	+=	drivers/st/clk/clk-stm32-core.c				\
				drivers/st/clk/clk-stm32mp2.c				\
				drivers/st/crypto/stm32_rng.c				\
				drivers/st/crypto/stm32_saes.c

PLAT_BL_COMMON_SOURCES  +=	drivers/st/nvmem/stm32mp_tamp_nvram_mp2.c

ifeq ($(filter 1,${STM32MP_UART_PROGRAMMER} ${STM32MP_USB_PROGRAMMER}),)
PLAT_BL_COMMON_SOURCES  +=	plat/st/stm32mp2/stm32mp2_context.c
endif

BL2_SOURCES		+=	plat/st/stm32mp2/plat_bl2_mem_params_desc.c

BL2_SOURCES		+=	drivers/st/crypto/stm32_hash.c				\
				plat/st/stm32mp2/bl2_plat_setup.c			\
				plat/st/stm32mp2/plat_ddr.c

BL2_SOURCES		+=	drivers/st/rif/stm32_rifsc.c

ifeq ($(STM32MP_M33_TDCID),0)
BL2_SOURCES		+=	drivers/st/rif/stm32mp2_risaf.c
endif


ifeq (${TRUSTED_BOARD_BOOT},1)
BL2_SOURCES		+=	drivers/st/crypto/stm32_pka.c
endif

ifneq ($(filter 1,${STM32MP_EMMC} ${STM32MP_SDMMC}),)
BL2_SOURCES		+=	drivers/st/mmc/stm32_sdmmc2.c
endif

ifeq (${STM32MP_RAW_NAND},1)
BL2_SOURCES		+=	drivers/st/fmc/stm32_fmc2_nand.c
endif

ifneq ($(filter 1,${STM32MP_SPI_NAND} ${STM32MP_SPI_NOR} ${STM32MP_HYPERFLASH}),)
BL2_SOURCES		+=	drivers/st/spi/stm32_ospi.c
endif

ifneq ($(filter 1,${STM32MP_RAW_NAND} ${STM32MP_SPI_NAND} ${STM32MP_SPI_NOR} ${STM32MP_HYPERFLASH}),)
BL2_SOURCES		+=	plat/st/stm32mp2/stm32mp2_boot_device.c
endif

ifeq (${STM32MP_UART_PROGRAMMER},1)
BL2_SOURCES		+=	drivers/st/uart/stm32_uart.c
endif

ifeq (${STM32MP_USB_PROGRAMMER},1)
#The DFU stack uses only one end point, reduce the USB stack footprint
$(eval $(call add_define_val,CONFIG_USBD_EP_NB,1U))
$(eval $(call add_define,USB_CORE_AVOID_PACKET_SPLIT_MPS))
BL2_SOURCES		+=	drivers/st/usb_dwc3/usb_dwc3.c				\
				plat/st/stm32mp2/stm32mp2_usb_dfu.c
endif

ifeq ($(STM32MP_M33_TDCID),0)
BL2_SOURCES		+=	drivers/st/ddr/stm32mp2_ddr.c				\
				drivers/st/ddr/stm32mp2_ddr_helpers.c			\
				drivers/st/ddr/stm32mp2_ram.c

BL2_SOURCES		+=	drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_c_initphyconfig.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_calcmb.c					\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_i_loadpieimage.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_initstruct.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_isdbytedisabled.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_loadpieprodcode.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_mapdrvstren.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_progcsrskiptrain.c			\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_reginterface.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_restore_sequence.c			\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_sequence.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_softsetmb.c				\
				drivers/st/ddr/phy/phyinit/usercustom/ddrphy_phyinit_usercustom_custompretrain.c	\
				drivers/st/ddr/phy/phyinit/usercustom/ddrphy_phyinit_usercustom_saveretregs.c

BL2_SOURCES		+=	drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_d_loadimem.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_f_loaddmem.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_g_execfw.c				\
				drivers/st/ddr/phy/phyinit/src/ddrphy_phyinit_writeoutmem.c				\
				drivers/st/ddr/phy/phyinit/usercustom/ddrphy_phyinit_usercustom_g_waitfwdone.c
endif #STM32MP_M33_TDCID

BL2_SOURCES		+=	plat/st/stm32mp2/plat_image_load.c

# BL31 sources
BL31_SOURCES		+=	${FDT_WRAPPERS_SOURCES}

BL31_SOURCES		+=	plat/st/stm32mp2/bl31_plat_setup.c			\
				plat/st/stm32mp2/stm32mp2_topology.c

ifeq ($(filter 1,${STM32MP_UART_PROGRAMMER} ${STM32MP_USB_PROGRAMMER}),)
BL31_SOURCES		+=	plat/st/stm32mp2/stm32mp2_pm.c
endif

BL31_SOURCES		+=	drivers/st/ddr/stm32mp2_ddr_helpers.c

# Generic GIC v2
include drivers/arm/gic/v2/gicv2.mk

BL31_SOURCES		+=	${GICV2_SOURCES}					\
				plat/common/plat_gicv2.c				\
				plat/st/common/stm32mp_gic.c

BL31_SOURCES		+=	plat/st/common/stm32mp_svc_setup.c			\
				plat/st/stm32mp2/services/stgen_svc.c			\
				plat/st/stm32mp2/services/stm32mp2_svc_setup.c

# Arm Archtecture services
BL31_SOURCES		+=	services/arm_arch_svc/arm_arch_svc_setup.c

# Compilation rules
.SUFFIXES:
ifeq ($(STM32MP_M33_TDCID),0)
.PHONY: check_ddr_type

bl2: check_ddr_type

check_ddr_type:
	$(eval DDR_TYPE = $(shell echo $$(($(STM32MP_DDR3_TYPE) + \
					   $(STM32MP_DDR4_TYPE) + \
					   $(STM32MP_LPDDR4_TYPE)))))
	@if [ ${DDR_TYPE} != 1 ]; then \
		echo "One and only one DDR type must be defined"; \
		false; \
	fi
endif #STM32MP_M33_TDCID

# generate separate DDR FIP image
ifeq (${STM32MP_DDR_FIP_IO_STORAGE},1)
ifneq ($(filter 1,${STM32MP_UART_PROGRAMMER} ${STM32MP_USB_PROGRAMMER}),)

STM32MP_DDR_FW_COPY	:=	${STM32MP_DDR_FW}
DDR_FIP_NAME		?=	fip-ddr.bin

$(eval $(call TOOL_ADD_IMG,STM32MP_DDR_FW_COPY,--ddr-fw,DDR_))

ifeq (${TRUSTED_BOARD_BOOT},1)
ifneq (${GENERATE_COT},0)
$(eval $(call TOOL_ADD_PAYLOAD,${STM32MP_CFG_CERT},--stm32mp-cfg-cert,,DDR_))
$(if ${ROT_KEY},$(eval $(call CERT_ADD_CMD_OPT,${ROT_KEY},--rot-key,DDR_)))
$(if ${TFW_NVCTR_VAL},$(eval $(call CERT_ADD_CMD_OPT,${TFW_NVCTR_VAL},--tfw-nvctr,DDR_)))
$(if ${TRUSTED_WORLD_KEY},$(eval $(call CERT_ADD_CMD_OPT,${TRUSTED_WORLD_KEY},--trusted-world-key,DDR_)))

ifneq (${CREATE_KEYS},0)
$(eval DDR_CRT_ARGS += -n)
ifneq (${SAVE_KEYS},0)
$(eval DDR_CRT_ARGS += -k)
endif
endif

ddr_certificates: ${DDR_CRT_DEPS} certtool
	@echo "Build $@"
	${Q}${CRTTOOL} ${DDR_CRT_ARGS}
	@${ECHO_BLANK_LINE}
	@echo "Built $@ successfully"
	@echo "DDR certificates can be found in ${BUILD_PLAT}"
	@${ECHO_BLANK_LINE}

DDR_FIP_DEPS		+=	ddr_certificates
endif
endif

${BUILD_PLAT}/${DDR_FIP_NAME}: ${DDR_FIP_DEPS} fiptool
	${Q}${FIPTOOL} create ${DDR_FIP_ARGS} $@
	${Q}${FIPTOOL} info $@
	@${ECHO_BLANK_LINE}
	@echo "Built $@ successfully"
	@${ECHO_BLANK_LINE}

fip-ddr: ${BUILD_PLAT}/${DDR_FIP_NAME}

fip: fip-ddr

endif
endif

# Create DTB file for BL31
${BUILD_PLAT}/fdts/%-bl31.dts: $(DT_SOURCE_PATH)/%.dts fdts/${BL31_DTSI} | ${BUILD_PLAT} fdt_dirs
	@echo '#include "$(patsubst fdts/%,%,$<)"' > $@
	@echo '#include "${BL31_DTSI}"' >> $@

${BUILD_PLAT}/fdts/%-bl31.dtb: ${BUILD_PLAT}/fdts/%-bl31.dts

include plat/st/common/common_rules.mk
