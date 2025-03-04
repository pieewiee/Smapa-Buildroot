/*
 * Copyright (C) 2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

uint32_t stm32_otp_probe(void)
{
	return BSEC_OK;
}

uint32_t stm32_otp_read(uint32_t *val, uint32_t otp)
{
	return BSEC_OK;
}

uint32_t stm32_otp_shadow_read(uint32_t *val, uint32_t otp)
{
	return BSEC_OK;
}

uint32_t stm32_otp_write(uint32_t val, uint32_t otp)
{
	return BSEC_OK;
}

uint32_t stm32_otp_set_sr_lock(uint32_t otp)
{
	return BSEC_OK;
}

uint32_t stm32_otp_read_sw_lock(uint32_t otp, bool *value)
{
	return BSEC_OK;
}

bool stm32_otp_is_closed_device(void)
{
	return false;
}
