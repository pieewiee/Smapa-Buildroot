/*
 * Copyright (c) 2015-2024, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/clk.h>
#include <drivers/delay_timer.h>
#include <drivers/st/nvmem.h>
#include <drivers/st/stm32_console.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <drivers/st/stm32mp_reset.h>
#include <lib/mmio.h>
#include <lib/smccc.h>
#include <lib/spinlock.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>
#include <services/arm_arch_svc.h>

#include <platform_def.h>

#define HEADER_VERSION_MAJOR_MASK	GENMASK(23, 16)
#define RESET_TIMEOUT_US_1MS		1000U

/* Internal layout of the 32bit OTP word board_id */
#define BOARD_ID_BOARD_NB_MASK		GENMASK_32(31, 16)
#define BOARD_ID_BOARD_NB_SHIFT		16
#define BOARD_ID_VARCPN_MASK		GENMASK_32(15, 12)
#define BOARD_ID_VARCPN_SHIFT		12
#define BOARD_ID_REVISION_MASK		GENMASK_32(11, 8)
#define BOARD_ID_REVISION_SHIFT		8
#define BOARD_ID_VARFG_MASK		GENMASK_32(7, 4)
#define BOARD_ID_VARFG_SHIFT		4
#define BOARD_ID_BOM_MASK		GENMASK_32(3, 0)

#define BOARD_ID2NB(_id)		(((_id) & BOARD_ID_BOARD_NB_MASK) >> \
					 BOARD_ID_BOARD_NB_SHIFT)
#define BOARD_ID2VARCPN(_id)		(((_id) & BOARD_ID_VARCPN_MASK) >> \
					 BOARD_ID_VARCPN_SHIFT)
#define BOARD_ID2REV(_id)		(char)(((_id) & BOARD_ID_REVISION_MASK) >> \
					       BOARD_ID_REVISION_SHIFT)
#define BOARD_ID2VARFG(_id)		(((_id) & BOARD_ID_VARFG_MASK) >> \
					 BOARD_ID_VARFG_SHIFT)
#define BOARD_ID2BOM(_id)		((_id) & BOARD_ID_BOM_MASK)

#define BOOT_AUTH_MASK			GENMASK_32(23, 20)
#define BOOT_AUTH_SHIFT			20
#define BOOT_PART_MASK			GENMASK_32(19, 16)
#define BOOT_PART_SHIFT			16
#define BOOT_ITF_MASK			GENMASK_32(15, 12)
#define BOOT_ITF_SHIFT			12
#define BOOT_INST_MASK			GENMASK_32(11, 8)
#define BOOT_INST_SHIFT			8

/* Layout for fwu update information. */
#define FWU_INFO_IDX_MSK		GENMASK(3, 0)
#define FWU_INFO_IDX_OFF		U(0)
#define FWU_INFO_CNT_MSK		GENMASK(7, 4)
#define FWU_INFO_CNT_OFF		U(4)

static console_t console;
static struct spinlock lock;

uintptr_t plat_get_ns_image_entrypoint(void)
{
	return BL33_BASE;
}

unsigned int plat_get_syscnt_freq2(void)
{
	return read_cntfrq_el0();
}

static uintptr_t boot_ctx_address;
static uint16_t boot_itf_selected;
#if STM32MP13 || STM32MP15
static uint32_t boot_action_saved;
#endif

void stm32mp_save_boot_ctx_address(uintptr_t address)
{
	boot_api_context_t *boot_context = (boot_api_context_t *)address;

	boot_ctx_address = address;
	boot_itf_selected = boot_context->boot_interface_selected;
#if STM32MP13 || STM32MP15
	boot_action_saved = boot_context->boot_action;
#endif
}

uintptr_t stm32mp_get_boot_ctx_address(void)
{
	return boot_ctx_address;
}

uint16_t stm32mp_get_boot_itf_selected(void)
{
	return boot_itf_selected;
}

#if STM32MP13 || STM32MP15
uint32_t stm32mp_get_boot_action(void)
{
	return boot_action_saved;
}
#endif

uintptr_t stm32mp_ddrctrl_base(void)
{
	return DDRCTRL_BASE;
}

uintptr_t stm32mp_ddrphyc_base(void)
{
	return DDRPHYC_BASE;
}

uintptr_t stm32mp_pwr_base(void)
{
	return PWR_BASE;
}

uintptr_t stm32mp_rcc_base(void)
{
	return RCC_BASE;
}

bool stm32mp_lock_available(void)
{
	const uint32_t c_m_bits = SCTLR_M_BIT | SCTLR_C_BIT;

	/* The spinlocks are used only when MMU and data cache are enabled */
#ifdef __aarch64__
	return (read_sctlr_el3() & c_m_bits) == c_m_bits;
#else
	return (read_sctlr() & c_m_bits) == c_m_bits;
#endif
}

void stm32mp_pwr_regs_lock(void)
{
	if (stm32mp_lock_available()) {
		spin_lock(&lock);
	}
}

void stm32mp_pwr_regs_unlock(void)
{
	if (stm32mp_lock_available()) {
		spin_unlock(&lock);
	}
}

int stm32mp_map_ddr_non_cacheable(void)
{
	return  mmap_add_dynamic_region(STM32MP_DDR_BASE, STM32MP_DDR_BASE,
					STM32MP_DDR_MAX_SIZE,
					MT_NON_CACHEABLE | MT_RW | MT_SECURE);
}

int stm32mp_unmap_ddr(void)
{
	return  mmap_remove_dynamic_region(STM32MP_DDR_BASE,
					   STM32MP_DDR_MAX_SIZE);
}

int stm32_get_otp_index(const char *otp_name, uint32_t *otp_idx,
			uint32_t *otp_len)
{
	assert(otp_name != NULL);
	assert(otp_idx != NULL);

	return dt_find_otp_name(otp_name, otp_idx, otp_len);
}

int stm32_get_otp_value(const char *otp_name, uint32_t *otp_val)
{
	uint32_t otp_idx;

	assert(otp_name != NULL);
	assert(otp_val != NULL);

	if (stm32_get_otp_index(otp_name, &otp_idx, NULL) != 0) {
		return -1;
	}

	if (stm32_get_otp_value_from_idx(otp_idx, otp_val) != 0) {
		ERROR("BSEC: %s Read Error\n", otp_name);
		return -1;
	}

	return 0;
}

int stm32_get_otp_value_from_idx(const uint32_t otp_idx, uint32_t *otp_val)
{
	uint32_t ret = BSEC_NOT_SUPPORTED;

	assert(otp_val != NULL);

#if defined(IMAGE_BL2)
	ret = stm32_otp_shadow_read(otp_val, otp_idx);
#elif defined(IMAGE_BL31) || defined(IMAGE_BL32)
	ret = stm32_otp_read(otp_val, otp_idx);
#else
#error "Not supported"
#endif
	if (ret != BSEC_OK) {
		ERROR("BSEC: idx=%u Read Error\n", otp_idx);
		return -1;
	}

	return 0;
}

int stm32_lock_enc_key_otp(void)
{
	uint32_t otp_idx;
	uint32_t otp_len;
	uint32_t i;

	if (stm32_get_otp_index(ENCKEY_OTP, &otp_idx, &otp_len) != 0) {
		return -1;
	}

	for (i = 0U; i < otp_len / CHAR_BIT / sizeof(uint32_t); i++) {
		uint32_t ret = stm32_otp_write(0U, otp_idx + i);

		if (ret != BSEC_OK) {
			return -1;
		}

		ret = stm32_otp_set_sr_lock(otp_idx + i);
		if (ret != BSEC_OK) {
			return -1;
		}
	}

	return 0;
}

int stm32_get_uid_otp(uint32_t uid[])
{
	uint8_t i;
	uint32_t otp;
	uint32_t len;

	if (stm32_get_otp_index(UID_OTP, &otp, &len) != 0) {
		ERROR("BSEC: Get UID_OTP number Error\n");
		return -1;
	}

	if ((len / __WORD_BIT) != UID_WORD_NB) {
		ERROR("BSEC: Get UID_OTP length Error\n");
		return -1;
	}

	for (i = 0U; i < UID_WORD_NB; i++) {
		if (stm32_otp_shadow_read(&uid[i], i + otp) != BSEC_OK) {
			ERROR("BSEC: UID%u Error\n", i);
			return -1;
		}
	}

	return 0;
}

int stm32_get_enc_key_otp_idx_len(uint32_t *otp_idx, uint32_t *otp_len)
{
	static uint32_t idx;
	static uint32_t len;
	int ret;

	if (len == 0U) {
		ret = stm32_get_otp_index(ENCKEY_OTP, &idx, &len);
		if (ret != 0) {
			len = 0U;
			ERROR("%s: get %s index error\n", __func__, ENCKEY_OTP);
			return -EINVAL;
		}
	}

	*otp_idx = idx;
	*otp_len = len;

	return 0;
}

#if  defined(IMAGE_BL2)
static void reset_uart(uint32_t reset)
{
	int ret;

	ret = stm32mp_reset_assert(reset, RESET_TIMEOUT_US_1MS);
	if (ret != 0) {
		panic();
	}

	udelay(2);

	ret = stm32mp_reset_deassert(reset, RESET_TIMEOUT_US_1MS);
	if (ret != 0) {
		panic();
	}

	mdelay(1);
}
#endif

static void set_console(uintptr_t base, uint32_t clk_rate)
{
	unsigned int console_flags;

	if (console_stm32_register(base, clk_rate,
				   (uint32_t)STM32MP_UART_BAUDRATE, &console) == 0) {
		panic();
	}

	console_flags = CONSOLE_FLAG_BOOT | CONSOLE_FLAG_CRASH |
			CONSOLE_FLAG_TRANSLATE_CRLF;
#if !defined(IMAGE_BL2) && defined(DEBUG)
	console_flags |= CONSOLE_FLAG_RUNTIME;
#endif

	console_set_scope(&console, console_flags);
}

int stm32mp_uart_console_setup(void)
{
	struct dt_node_info dt_uart_info;
	uint32_t clk_rate = 0U;
	int result;
	uint32_t boot_itf __unused;
	uint32_t boot_instance __unused;

	result = dt_get_stdout_uart_info(&dt_uart_info);

	if ((result <= 0) ||
	    (dt_uart_info.status == DT_DISABLED)) {
		return -ENODEV;
	}

#if defined(IMAGE_BL2)
	if ((dt_uart_info.clock < 0) ||
	    (dt_uart_info.reset < 0)) {
		return -ENODEV;
	}
#endif

#if (STM32MP_UART_PROGRAMMER || !defined(IMAGE_BL2)) && !STM32MP_SSP
	stm32_get_boot_interface(&boot_itf, &boot_instance);

	if ((boot_itf == BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART) &&
	    (get_uart_address(boot_instance) == dt_uart_info.base)) {
		return -EACCES;
	}
#endif

#if defined(IMAGE_BL2)
	if (dt_set_stdout_pinctrl() != 0) {
		return -ENODEV;
	}

	clk_enable((unsigned long)dt_uart_info.clock);

	reset_uart((uint32_t)dt_uart_info.reset);

	clk_rate = clk_get_rate((unsigned long)dt_uart_info.clock);
#endif

	set_console(dt_uart_info.base, clk_rate);

	return 0;
}

#if STM32MP_EARLY_CONSOLE
void stm32mp_setup_early_console(void)
{
#if defined(IMAGE_BL2) || STM32MP_RECONFIGURE_CONSOLE
	plat_crash_console_init();
#endif
	set_console(STM32MP_DEBUG_USART_BASE, STM32MP_DEBUG_USART_CLK_FRQ);
	NOTICE("Early console setup\n");
}
#endif /* STM32MP_EARLY_CONSOLE */

/*****************************************************************************
 * plat_is_smccc_feature_available() - This function checks whether SMCCC
 *                                     feature is availabile for platform.
 * @fid: SMCCC function id
 *
 * Return SMC_ARCH_CALL_SUCCESS if SMCCC feature is available and
 * SMC_ARCH_CALL_NOT_SUPPORTED otherwise.
 *****************************************************************************/
int32_t plat_is_smccc_feature_available(u_register_t fid)
{
	switch (fid) {
	case SMCCC_ARCH_SOC_ID:
		return SMC_ARCH_CALL_SUCCESS;
	default:
		return SMC_ARCH_CALL_NOT_SUPPORTED;
	}
}

/* Get SOC version */
int32_t plat_get_soc_version(void)
{
	uint32_t chip_id = stm32mp_get_chip_dev_id();
	uint32_t manfid = SOC_ID_SET_JEP_106(JEDEC_ST_BKID, JEDEC_ST_MFID);

	return (int32_t)(manfid | (chip_id & SOC_ID_IMPL_DEF_MASK));
}

/* Get SOC revision */
int32_t plat_get_soc_revision(void)
{
	return (int32_t)(stm32mp_get_chip_version() & SOC_ID_REV_MASK);
}

void stm32_display_board_info(uint32_t board_id)
{
	NOTICE("Board: MB%04x Var%u.%u Rev.%c-%02u\n",
	       BOARD_ID2NB(board_id),
	       BOARD_ID2VARCPN(board_id),
	       BOARD_ID2VARFG(board_id),
	       BOARD_ID2REV(board_id) - 1 + 'A',
	       BOARD_ID2BOM(board_id));
}

#if !STM32MP_SSP
void stm32_save_boot_info(boot_api_context_t *boot_context)
{
	struct nvmem_cell boot_mode = {};
	uint32_t reg_val = 0;

	uint32_t clear = BOOT_ITF_MASK | BOOT_INST_MASK | BOOT_PART_MASK |
			 BOOT_AUTH_MASK;
	uint32_t set =
		((boot_context->boot_interface_selected << BOOT_ITF_SHIFT) &
		 BOOT_ITF_MASK) |
		((boot_context->boot_interface_instance << BOOT_INST_SHIFT) &
		 BOOT_INST_MASK) |
		((boot_context->boot_partition_used_toboot << BOOT_PART_SHIFT) &
		 BOOT_PART_MASK) |
		((boot_context->auth_status << BOOT_AUTH_SHIFT) &
		 BOOT_AUTH_MASK);

	stm32_get_boot_mode_cell(&boot_mode);
	nvmem_cell_read(&boot_mode, (uint8_t *)&reg_val, sizeof(reg_val), NULL);
	reg_val &= ~clear;
	reg_val |= set;
	nvmem_cell_write(&boot_mode, (uint8_t *)&reg_val, sizeof(reg_val));
}

void stm32_get_boot_interface(uint32_t *interface, uint32_t *instance)
{
	static uint32_t itf;
	struct nvmem_cell boot_mode = {};
	uint32_t reg_val = 0;

	if (itf == 0U) {
		stm32_get_boot_mode_cell(&boot_mode);
		nvmem_cell_read(&boot_mode, (uint8_t *)&reg_val,
				sizeof(reg_val), NULL);
		itf = reg_val & (BOOT_ITF_MASK | BOOT_INST_MASK);
	}

	*interface = (itf & BOOT_ITF_MASK) >> BOOT_ITF_SHIFT;
	*instance = (itf & BOOT_INST_MASK) >> BOOT_INST_SHIFT;
}

static int stm32_get_bootinfo_cell(const char *name, struct nvmem_cell *cell)
{
	void *fdt = NULL;
	int node = 0;
	int ret;

	if (fdt_get_address(&fdt) == 0) {
		ret = -ENODEV;
	} else {
		node = fdt_node_offset_by_compatible(fdt, -1,
						     "st,stm32mp-bootinfo");
		if (node >= 0) {
			ret = nvmem_get_cell_by_name(fdt, node, name, cell);
		} else {
			ret = -ENODEV;
		}
	}
	return ret;
}

#if STM32MP15
int stm32_get_magic_number_cell(struct nvmem_cell *magic_number)
{
	static bool initialized = false;
	static struct nvmem_cell s_magic_number = { 0 };

	if (!initialized) {
		stm32_get_bootinfo_cell("magic-number", &s_magic_number);
		initialized = true;
	}

	memcpy(magic_number, &s_magic_number, sizeof(*magic_number));

	return 0;
}

int stm32_get_core1_branch_address_cell(struct nvmem_cell *core1_branch_address)
{
	static bool initialized = false;
	static struct nvmem_cell s_core1_branch_address = { 0 };

	if (!initialized) {
		stm32_get_bootinfo_cell("core1-branch-address", &s_core1_branch_address);
		initialized = true;
	}

	memcpy(core1_branch_address, &s_core1_branch_address, sizeof(*core1_branch_address));

	return 0;
}
#endif

int stm32_get_fwu_info_cell(struct nvmem_cell *fwu_info)
{
	static bool initialized = false;
	static struct nvmem_cell s_fwu_info = { 0 };

	if (!initialized) {
		stm32_get_bootinfo_cell("fwu-info", &s_fwu_info);
		initialized = true;
	}

	memcpy(fwu_info, &s_fwu_info, sizeof(*fwu_info));

	return 0;
}

int stm32_get_boot_mode_cell(struct nvmem_cell *boot_mode)
{
	static bool initialized = false;
	static struct nvmem_cell s_boot_mode = { 0 };

	if (!initialized) {
		stm32_get_bootinfo_cell("boot-mode", &s_boot_mode);
		initialized = true;
	}

	memcpy(boot_mode, &s_boot_mode, sizeof(*boot_mode));

	return 0;
}

#if STM32MP25
int stm32_get_stop2_entrypoint_cell(struct nvmem_cell *stop2_entrypoint)
{
	static bool initialized = false;
	static struct nvmem_cell s_stop2_entrypoint = { 0 };

	if (!initialized) {
		stm32_get_bootinfo_cell("stop2-entrypoint", &s_stop2_entrypoint);
		initialized = true;
	}

	memcpy(stop2_entrypoint, &s_stop2_entrypoint, sizeof(*stop2_entrypoint));

	return 0;
}
#endif
#if PSA_FWU_SUPPORT
static int stm32_nvmem_cell_clrset(struct nvmem_cell *cell, uint32_t clear,
				   uint32_t set)
{
	int ret = 0;
	uint32_t reg_val = 0;

	ret = nvmem_cell_read(cell, (uint8_t *)&reg_val, sizeof(reg_val), NULL);
	if (ret != 0) {
		return ret;
	}

	reg_val &= ~clear;
	reg_val |= set;

	ret = nvmem_cell_write(cell, (uint8_t *)&reg_val, sizeof(reg_val));
	if (ret != 0) {
		return ret;
	}

	return 0;
}

void stm32_fwu_set_boot_idx(void)
{
	struct nvmem_cell fwu_info = {};

	uint32_t clear = FWU_INFO_IDX_MSK;
	uint32_t set = (plat_fwu_get_boot_idx() << FWU_INFO_IDX_OFF) &
		       FWU_INFO_IDX_MSK;

	stm32_get_fwu_info_cell(&fwu_info);

	stm32_nvmem_cell_clrset(&fwu_info, clear, set);
}

uint32_t stm32_get_and_dec_fwu_trial_boot_cnt(void)
{
	struct nvmem_cell fwu_info_cell = {};
	uint32_t try_cnt;
	uint32_t fwu_info = 0;

	stm32_get_fwu_info_cell(&fwu_info_cell);

	nvmem_cell_read(&fwu_info_cell, (uint8_t *)&fwu_info, sizeof(fwu_info),
			NULL);

	try_cnt = (fwu_info & FWU_INFO_CNT_MSK) >> FWU_INFO_CNT_OFF;

	assert(try_cnt <= FWU_MAX_TRIAL_REBOOT);

	if (try_cnt != 0U) {
		stm32_nvmem_cell_clrset(&fwu_info_cell, FWU_INFO_CNT_MSK,
					(try_cnt - 1U) << FWU_INFO_CNT_OFF);
	}

	return try_cnt;
}

void stm32_set_max_fwu_trial_boot_cnt(void)
{
	struct nvmem_cell fwu_info_cell = {};

	stm32_get_fwu_info_cell(&fwu_info_cell);
	stm32_nvmem_cell_clrset(&fwu_info_cell, FWU_INFO_CNT_MSK,
				(FWU_MAX_TRIAL_REBOOT << FWU_INFO_CNT_OFF) &
					FWU_INFO_CNT_MSK);
}

void stm32_clear_fwu_trial_boot_cnt(void)
{
	struct nvmem_cell fwu_info_cell = {};

	stm32_get_fwu_info_cell(&fwu_info_cell);
	stm32_nvmem_cell_clrset(&fwu_info_cell, FWU_INFO_CNT_MSK, 0U);
}
#endif /* PSA_FWU_SUPPORT */
#endif /* !STM32MP_SSP */
