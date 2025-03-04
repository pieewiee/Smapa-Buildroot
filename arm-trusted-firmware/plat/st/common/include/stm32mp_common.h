/*
 * Copyright (C) 2018-2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP_COMMON_H
#define STM32MP_COMMON_H

#include <stdbool.h>

#include <platform_def.h>
#include <drivers/st/nvmem.h>

#define JEDEC_ST_BKID U(0x0)
#define JEDEC_ST_MFID U(0x20)

#define STM32MP_CHIP_SEC_CLOSED		U(0x34D9CCC5)
#define STM32MP_CHIP_SEC_OPEN		U(0xA764D182)

/* FWU configuration (max supported value is 15) */
#define FWU_MAX_TRIAL_REBOOT		U(3)

/* Functions to save and get boot context address given by ROM code */
void stm32mp_save_boot_ctx_address(uintptr_t address);
uintptr_t stm32mp_get_boot_ctx_address(void);
uint16_t stm32mp_get_boot_itf_selected(void);
#if STM32MP13 || STM32MP15
uint32_t stm32mp_get_boot_action(void);
#endif

bool stm32mp_is_single_core(void);
bool stm32mp_is_auth_supported(void);
uint32_t stm32mp_check_closed_device(void);

/* Return the base address of the DDR controller */
uintptr_t stm32mp_ddrctrl_base(void);

/* Return the base address of the DDR PHY */
uintptr_t stm32mp_ddrphyc_base(void);

/* Return the base address of the PWR peripheral */
uintptr_t stm32mp_pwr_base(void);

/* Return the base address of the RCC peripheral */
uintptr_t stm32mp_rcc_base(void);

void stm32mp_gic_cpuif_enable(void);
void stm32mp_gic_cpuif_disable(void);
void stm32mp_gic_pcpu_init(void);
void stm32mp_gic_init(void);

void stm32mp_gic_save(void);
void stm32mp_gic_resume(void);

/* Check MMU status to allow spinlock use */
bool stm32mp_lock_available(void);

/* SMP protection on PWR registers access */
void stm32mp_pwr_regs_lock(void);
void stm32mp_pwr_regs_unlock(void);

int stm32_get_otp_index(const char *otp_name, uint32_t *otp_idx,
			uint32_t *otp_len);
int stm32_get_otp_value(const char *otp_name, uint32_t *otp_val);
int stm32_get_otp_value_from_idx(const uint32_t otp_idx, uint32_t *otp_val);
int stm32_lock_enc_key_otp(void);
/* update UID_WORD_NB array */
int stm32_get_uid_otp(uint32_t uid[]);
int stm32_get_enc_key_otp_idx_len(uint32_t *otp_idx, uint32_t *otp_len);

/* Get IWDG platform instance ID from peripheral IO memory base address */
uint32_t stm32_iwdg_get_instance(uintptr_t base);

/* Return bitflag mask for expected IWDG configuration from OTP content */
uint32_t stm32_iwdg_get_otp_config(uint32_t iwdg_inst);

#if STM32MP_UART_PROGRAMMER || !defined(IMAGE_BL2)
/* Get the UART address from its instance number */
uintptr_t get_uart_address(uint32_t instance_nb);
#endif

/* Setup the UART console */
int stm32mp_uart_console_setup(void);

#if STM32MP_EARLY_CONSOLE
#define EARLY_ERROR(...)	ERROR(__VA_ARGS__)
#define EARLY_NOTICE(...)	NOTICE(__VA_ARGS__)
#define EARLY_WARN(...)		WARN(__VA_ARGS__)
#define EARLY_INFO(...)		INFO(__VA_ARGS__)
#define EARLY_VERBOSE(...)	VERBOSE(__VA_ARGS__)

void stm32mp_setup_early_console(void);
#else /* STM32MP_EARLY_CONSOLE */
#define EARLY_ERROR(...)
#define EARLY_NOTICE(...)
#define EARLY_WARN(...)
#define EARLY_INFO(...)
#define EARLY_VERBOSE(...)

static inline void stm32mp_setup_early_console(void)
{
}
#endif /* STM32MP_EARLY_CONSOLE */

void stm32_save_header(void);
uintptr_t stm32_get_header_address(void);

bool stm32mp_skip_boot_device_after_standby(void);
bool stm32mp_is_wakeup_from_standby(void);

/*
 * Platform util functions for the GPIO driver
 * @bank: Target GPIO bank ID as per DT bindings
 *
 * Platform shall implement these functions to provide to stm32_gpio
 * driver the resource reference for a target GPIO bank. That are
 * memory mapped interface base address, interface offset (see below)
 * and clock identifier.
 *
 * stm32_get_gpio_bank_offset() returns a bank offset that is used to
 * check DT configuration matches platform implementation of the banks
 * description.
 */
uintptr_t stm32_get_gpio_bank_base(unsigned int bank);
unsigned long stm32_get_gpio_bank_clock(unsigned int bank);
uint32_t stm32_get_gpio_bank_offset(unsigned int bank);
bool stm32_gpio_is_secure_at_reset(unsigned int bank);

/* Return node offset for target GPIO bank ID @bank or a FDT error code */
int stm32_get_gpio_bank_pinctrl_node(void *fdt, unsigned int bank);

/* Get the chip revision */
uint32_t stm32mp_get_chip_version(void);
/* Get the chip device ID */
uint32_t stm32mp_get_chip_dev_id(void);

/* Get SOC name */
#define STM32_SOC_NAME_SIZE 20
void stm32mp_get_soc_name(char name[STM32_SOC_NAME_SIZE]);

/* Print CPU information */
void stm32mp_print_cpuinfo(void);

/* Print board information */
void stm32mp_print_boardinfo(void);

/* Initialise the IO layer and register platform IO devices */
void stm32mp_io_setup(void);

/* Deinitialise the IO layer */
void stm32mp_io_exit(void);

/* Functions to map DDR in MMU with non-cacheable attribute, and unmap it */
int stm32mp_map_ddr_non_cacheable(void);
int stm32mp_unmap_ddr(void);

/* Functions to map RETRAM, and unmap it */
int stm32mp_map_retram(void);
int stm32mp_unmap_retram(void);

/* Function to save boot info */
void stm32_save_boot_info(boot_api_context_t *boot_context);
/* Function to get boot peripheral info */
void stm32_get_boot_interface(uint32_t *interface, uint32_t *instance);

/* Display board information from the value found in OTP fuse */
void stm32_display_board_info(uint32_t board_id);

int stm32_tamp_nvram_init(void);
int stm32_tamp_nvram_update_rights(void);

int stm32_get_fwu_info_cell(struct nvmem_cell *fwu_info);
int stm32_get_boot_mode_cell(struct nvmem_cell *boot_mode);

#if STM32MP25
int stm32_get_stop2_entrypoint_cell(struct nvmem_cell *stop2_entrypoint);
#endif

#if STM32MP15
int stm32_get_magic_number_cell(struct nvmem_cell *magic_number);
int stm32_get_core1_branch_address_cell(struct nvmem_cell *core1_branch_address);
#endif

#if PSA_FWU_SUPPORT
void stm32_fwu_set_boot_idx(void);
uint32_t stm32_get_and_dec_fwu_trial_boot_cnt(void);
void stm32_set_max_fwu_trial_boot_cnt(void);
void stm32_clear_fwu_trial_boot_cnt(void);
#endif /* PSA_FWU_SUPPORT */

#endif /* STM32MP_COMMON_H */
