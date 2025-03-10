// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2021, STMicroelectronics - All Rights Reserved
 */
#include <assert.h>
#include <config.h>
#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <drivers/rstctrl.h>
#include <io.h>
#include <kernel/boot.h>
#include <kernel/delay.h>
#include <kernel/dt.h>
#include <kernel/huk_subkey.h>
#include <kernel/mutex.h>
#include <kernel/pm.h>
#include <libfdt.h>
#include <mm/core_memprot.h>
#include <stdint.h>
#include <stm32_util.h>
#include <string_ext.h>
#include <utee_defines.h>
#include <util.h>

#include "common.h"
#include "stm32_saes.h"

#define INT8_BIT			U(8)
#define AES_BLOCK_SIZE_BIT		128U
#define AES_BLOCK_SIZE			(AES_BLOCK_SIZE_BIT / INT8_BIT)
#define AES_BLOCK_NB_U32		(AES_BLOCK_SIZE / sizeof(uint32_t))

#define AES_IVSIZE			16U

/* SAES control register */
#define _SAES_CR			0x0U
/* SAES status register */
#define _SAES_SR			0x04U
/* SAES data input register */
#define _SAES_DINR			0x08U
/* SAES data output register */
#define _SAES_DOUTR			0x0CU
/* SAES key registers [0-3] */
#define _SAES_KEYR0			0x10U
#define _SAES_KEYR1			0x14U
#define _SAES_KEYR2			0x18U
#define _SAES_KEYR3			0x1CU
/* SAES initialization vector registers [0-3] */
#define _SAES_IVR0			0x20U
#define _SAES_IVR1			0x24U
#define _SAES_IVR2			0x28U
#define _SAES_IVR3			0x2CU
/* SAES key registers [4-7] */
#define _SAES_KEYR4			0x30U
#define _SAES_KEYR5			0x34U
#define _SAES_KEYR6			0x38U
#define _SAES_KEYR7			0x3CU
/* SAES suspend registers [0-7] */
#define _SAES_SUSPR0			0x40U
#define _SAES_SUSPR1			0x44U
#define _SAES_SUSPR2			0x48U
#define _SAES_SUSPR3			0x4CU
#define _SAES_SUSPR4			0x50U
#define _SAES_SUSPR5			0x54U
#define _SAES_SUSPR6			0x58U
#define _SAES_SUSPR7			0x5CU
/* SAES Interrupt Enable Register */
#define _SAES_IER			0x300U
/* SAES Interrupt Status Register */
#define _SAES_ISR			0x304U
/* SAES Interrupt Clear Register */
#define _SAES_ICR			0x308U

/* SAES control register fields */
#define _SAES_CR_RESET_VALUE		0x0U
#define _SAES_CR_IPRST			BIT(31)
#define _SAES_CR_KEYSEL_MASK		GENMASK_32(30, 28)
#define _SAES_CR_KEYSEL_SHIFT		28U
#define _SAES_CR_KEYSEL_SOFT		0x0U
#define _SAES_CR_KEYSEL_DHUK		0x1U
#define _SAES_CR_KEYSEL_BHK		0x2U
#define _SAES_CR_KEYSEL_BHU_XOR_BH_K	0x4U
#define _SAES_CR_KEYSEL_TEST		0x7U
#define _SAES_CR_KSHAREID_MASK		GENMASK_32(27, 26)
#define _SAES_CR_KSHAREID_SHIFT		26U
#define _SAES_CR_KSHAREID_CRYP		0x0U
#define _SAES_CR_KEYMOD_MASK		GENMASK_32(25, 24)
#define _SAES_CR_KEYMOD_SHIFT		24U
#define _SAES_CR_KEYMOD_NORMAL		0x0U
#define _SAES_CR_KEYMOD_WRAPPED		0x1U
#define _SAES_CR_KEYMOD_SHARED		0x2U
#define _SAES_CR_NPBLB_MASK		GENMASK_32(23, 20)
#define _SAES_CR_NPBLB_SHIFT		20U
#define _SAES_CR_KEYPROT		BIT(19)
#define _SAES_CR_KEYSIZE		BIT(18)
#define _SAES_CR_GCMPH_MASK		GENMASK_32(14, 13)
#define _SAES_CR_GCMPH_SHIFT		13U
#define _SAES_CR_GCMPH_INIT		0U
#define _SAES_CR_GCMPH_HEADER		1U
#define _SAES_CR_GCMPH_PAYLOAD		2U
#define _SAES_CR_GCMPH_FINAL		3U
#define _SAES_CR_DMAOUTEN		BIT(12)
#define _SAES_CR_DMAINEN		BIT(11)
#define _SAES_CR_CHMOD_MASK		(BIT(16) | GENMASK_32(6, 5))
#define _SAES_CR_CHMOD_SHIFT		5U
#define _SAES_CR_CHMOD_ECB		0x0U
#define _SAES_CR_CHMOD_CBC		0x1U
#define _SAES_CR_CHMOD_CTR		0x2U
#define _SAES_CR_CHMOD_GCM		0x3U
#define _SAES_CR_CHMOD_GMAC		0x3U
#define _SAES_CR_CHMOD_CCM		0x800U
#define _SAES_CR_MODE_MASK		GENMASK_32(4, 3)
#define _SAES_CR_MODE_SHIFT		3U
#define _SAES_CR_MODE_ENC		0U
#define _SAES_CR_MODE_KEYPREP		1U
#define _SAES_CR_MODE_DEC		2U
#define _SAES_CR_DATATYPE_MASK		GENMASK_32(2, 1)
#define _SAES_CR_DATATYPE_SHIFT		1U
#define _SAES_CR_DATATYPE_NONE		0U
#define _SAES_CR_DATATYPE_HALF_WORD	1U
#define _SAES_CR_DATATYPE_BYTE		2U
#define _SAES_CR_DATATYPE_BIT		3U
#define _SAES_CR_EN			BIT(0)

/* SAES status register fields */
#define _SAES_SR_KEYVALID		BIT(7)
#define _SAES_SR_BUSY			BIT(3)
#define _SAES_SR_WRERR			BIT(2)
#define _SAES_SR_RDERR			BIT(1)
#define _SAES_SR_CCF			BIT(0)

/* SAES interrupt registers fields */
#define _SAES_I_RNG_ERR			BIT(3)
#define _SAES_I_KEY_ERR			BIT(2)
#define _SAES_I_RW_ERR			BIT(1)
#define _SAES_I_CC			BIT(0)

#define SAES_TIMEOUT_US			100000U
#define TIMEOUT_US_1MS			1000U
#define SAES_RESET_DELAY		U(2)

/*
 * Macro to manage bit manipulation when we work on local variable
 * before writing only once to the real register.
 */
#define CLRBITS(v, bits)		((v) &= ~(bits))
#define SETBITS(v, bits)		((v) |= (bits))

#define IS_CHAINING_MODE(mod, cr) \
	(((cr) & _SAES_CR_CHMOD_MASK) == (_SAES_CR_CHMOD_##mod << \
					  _SAES_CR_CHMOD_SHIFT))

#define SET_CHAINING_MODE(mod, cr) \
	clrsetbits(&(cr), _SAES_CR_CHMOD_MASK, (_SAES_CR_CHMOD_##mod << \
						_SAES_CR_CHMOD_SHIFT))

#define TOBE32(x) TEE_U32_BSWAP((x))
#define FROMBE32(x) TEE_U32_BSWAP((x))

static struct stm32_saes_platdata saes_pdata;
static struct mutex saes_lock = MUTEX_INITIALIZER;

static void clrsetbits(uint32_t *v, uint32_t mask, uint32_t bits)
{
	*v = (*v & ~mask) | bits;
}

static bool does_chaining_mode_need_iv(uint32_t cr)
{
	return !IS_CHAINING_MODE(ECB, cr);
}

static bool is_encrypt(uint32_t cr)
{
	return (cr & _SAES_CR_MODE_MASK) == (_SAES_CR_MODE_ENC <<
					     _SAES_CR_MODE_SHIFT);
}

static bool is_decrypt(uint32_t cr)
{
	return (cr & _SAES_CR_MODE_MASK) == (_SAES_CR_MODE_DEC <<
					     _SAES_CR_MODE_SHIFT);
}

static bool does_need_npblb(uint32_t cr)
{
	return (IS_CHAINING_MODE(GCM, cr) && is_encrypt(cr)) ||
	       (IS_CHAINING_MODE(CCM, cr) && is_decrypt(cr));
}

static bool can_suspend(uint32_t cr)
{
	return !IS_CHAINING_MODE(GCM, cr);
}

static void write_aligned_block(vaddr_t base, uint32_t *data)
{
	unsigned int i;

	for (i = 0U; i < AES_BLOCK_NB_U32; i++) {
		/* No need to htobe() as we configure the HW to swap bytes */
		io_write32(base + _SAES_DINR, data[i]);
	}
}

static void write_block(vaddr_t base, uint8_t *data)
{
	if (IS_ALIGNED_WITH_TYPE(data, uint32_t)) {
		write_aligned_block(base, (void *)data);
	} else {
		uint32_t data_u32[AES_BLOCK_NB_U32];

		memcpy(data_u32, data, sizeof(data_u32));
		write_aligned_block(base, data_u32);
	}
}

static void read_aligned_block(vaddr_t base, uint32_t *data)
{
	unsigned int i = 0U;

	for (i = 0U; i < AES_BLOCK_NB_U32; i++) {
		/* No need to htobe() as we configure the HW to swap bytes */
		data[i] = io_read32(base + _SAES_DOUTR);
	}
}

static void read_block(vaddr_t base, uint8_t *data)
{
	if (IS_ALIGNED_WITH_TYPE(data, uint32_t)) {
		read_aligned_block(base, (void *)data);
	} else {
		uint32_t data_u32[AES_BLOCK_NB_U32];

		read_aligned_block(base, data_u32);

		memcpy(data, data_u32, sizeof(data_u32));
	}
}

static TEE_Result wait_computation_completed(vaddr_t base)
{
	uint64_t timeout_ref = timeout_init_us(SAES_TIMEOUT_US);

	while ((io_read32(base + _SAES_SR) & _SAES_SR_CCF) != _SAES_SR_CCF)
		if (timeout_elapsed(timeout_ref))
			break;

	if ((io_read32(base + _SAES_SR) & _SAES_SR_CCF) != _SAES_SR_CCF) {
		DMSG("CCF timeout");
		return TEE_ERROR_BUSY;
	}

	return TEE_SUCCESS;
}

static void clear_computation_completed(uintptr_t base)
{
	io_setbits32(base + _SAES_ICR, _SAES_I_CC);
}

static TEE_Result wait_key_valid(vaddr_t base)
{
	uint64_t timeout_ref = timeout_init_us(SAES_TIMEOUT_US);

	while ((io_read32(base + _SAES_SR) & _SAES_SR_KEYVALID) !=
	       _SAES_SR_KEYVALID)
		if (timeout_elapsed(timeout_ref))
			break;

	if ((io_read32(base + _SAES_SR) & _SAES_SR_KEYVALID) !=
	    _SAES_SR_KEYVALID) {
		DMSG("CCF timeout");
		return TEE_ERROR_BUSY;
	}

	return TEE_SUCCESS;
}

static void saes_select_instance(struct stm32_saes_context *ctx)
{
	ctx->lock = &saes_lock;
	ctx->base = io_pa_or_va(&saes_pdata.base, 1);
}

static TEE_Result saes_start(struct stm32_saes_context *ctx)
{
	uint64_t timeout_ref = 0;

	/* Reset IP */
	io_setbits32(ctx->base + _SAES_CR, _SAES_CR_IPRST);
	io_clrbits32(ctx->base + _SAES_CR, _SAES_CR_IPRST);

	timeout_ref = timeout_init_us(SAES_TIMEOUT_US);
	while (io_read32(ctx->base + _SAES_SR) & _SAES_SR_BUSY)
		if (timeout_elapsed(timeout_ref))
			break;

	if ((io_read32(ctx->base + _SAES_SR) & _SAES_SR_BUSY) ==
	    _SAES_SR_BUSY) {
		DMSG("busy timeout");
		return TEE_ERROR_BUSY;
	}

	return TEE_SUCCESS;
}

static void saes_end(struct stm32_saes_context *ctx, int prev_error)
{
	if (prev_error) {
		/* Reset IP */
		io_setbits32(ctx->base + _SAES_CR, _SAES_CR_IPRST);
		io_clrbits32(ctx->base + _SAES_CR, _SAES_CR_IPRST);
	}

	/* Disable the SAES peripheral */
	io_clrbits32(ctx->base + _SAES_CR, _SAES_CR_EN);
}

static void saes_write_iv(struct stm32_saes_context *ctx)
{
	/* If chaining mode need to restore IV */
	if (does_chaining_mode_need_iv(ctx->cr)) {
		uint8_t i = 0;

		/* Restore the _SAES_IVRx */
		for (i = 0; i < AES_IVSIZE / sizeof(uint32_t); i++) {
			io_write32(ctx->base + _SAES_IVR0 + i *
				   sizeof(uint32_t), ctx->iv[i]);
		}
	}
}

static void saes_save_suspend(struct stm32_saes_context *ctx)
{
	uint8_t i = 0;

	for (i = 0; i < 8; i++) {
		ctx->susp[i] = io_read32(ctx->base + _SAES_SUSPR0 + i
					 * sizeof(uint32_t));
	}
}

static void saes_restore_suspend(struct stm32_saes_context *ctx)
{
	uint8_t i = 0;

	for (i = 0; i < 8; i++) {
		io_write32(ctx->base + _SAES_SUSPR0 + i * sizeof(uint32_t),
			   ctx->susp[i]);
	}
}

static void saes_write_key(struct stm32_saes_context *ctx)
{
	/* Restore the _SAES_KEYRx if SOFTWARE key */
	if ((ctx->cr & _SAES_CR_KEYSEL_MASK) ==
	    (_SAES_CR_KEYSEL_SOFT << _SAES_CR_KEYSEL_SHIFT)) {
		uint8_t i = 0;

		for (i = 0; i < AES_KEYSIZE_128 / sizeof(uint32_t); i++)
			io_write32(ctx->base + _SAES_KEYR0 + i *
				   sizeof(uint32_t),
				   ctx->key[i]);

		if ((ctx->cr & _SAES_CR_KEYSIZE) == _SAES_CR_KEYSIZE) {
			for (i = 0;
			     i < (AES_KEYSIZE_256 / 2U) / sizeof(uint32_t);
			     i++) {
				io_write32(ctx->base + _SAES_KEYR4 + i *
					   sizeof(uint32_t),
					   ctx->key[i + 4U]);
			}
		}
	}
}

static TEE_Result saes_prepare_key(struct stm32_saes_context *ctx)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	/* Disable the SAES peripheral */
	io_clrbits32(ctx->base + _SAES_CR, _SAES_CR_EN);

	/* Set key size */
	if ((ctx->cr & _SAES_CR_KEYSIZE) != 0U)
		io_setbits32(ctx->base + _SAES_CR, _SAES_CR_KEYSIZE);
	else
		io_clrbits32(ctx->base + _SAES_CR, _SAES_CR_KEYSIZE);

	saes_write_key(ctx);

	res = wait_key_valid(ctx->base);
	if (res)
		return res;

	/*
	 * For ECB/CBC decryption, key preparation mode must be selected
	 * to populate the key.
	 */
	if ((IS_CHAINING_MODE(ECB, ctx->cr) ||
	     IS_CHAINING_MODE(CBC, ctx->cr)) && is_decrypt(ctx->cr)) {

		/* Select Mode 2 */
		io_clrsetbits32(ctx->base + _SAES_CR, _SAES_CR_MODE_MASK,
				_SAES_CR_MODE_KEYPREP << _SAES_CR_MODE_SHIFT);

		/* Enable SAES */
		io_setbits32(ctx->base + _SAES_CR, _SAES_CR_EN);

		res = wait_computation_completed(ctx->base);
		if (res)
			return res;

		clear_computation_completed(ctx->base);

		/* Set Mode 3 */
		io_clrsetbits32(ctx->base + _SAES_CR, _SAES_CR_MODE_MASK,
				_SAES_CR_MODE_DEC << _SAES_CR_MODE_SHIFT);
	}

	return TEE_SUCCESS;
}

static TEE_Result save_context(struct stm32_saes_context *ctx)
{
	if ((io_read32(ctx->base + _SAES_SR) & _SAES_SR_CCF) != 0U) {
		/* Device should not be in a processing phase */
		return TEE_ERROR_BAD_STATE;
	}

	/* Save CR */
	ctx->cr = io_read32(ctx->base + _SAES_CR);

	if (!can_suspend(ctx->cr))
		return TEE_SUCCESS;

	saes_save_suspend(ctx);

	/* If chaining mode need to save current IV */
	if (does_chaining_mode_need_iv(ctx->cr)) {
		uint8_t i = 0;

		/* Save IV */
		for (i = 0; i < AES_IVSIZE / sizeof(uint32_t); i++) {
			ctx->iv[i] = io_read32(ctx->base + _SAES_IVR0 + i *
					       sizeof(uint32_t));
		}
	}

	/* Disable the SAES peripheral */
	io_clrbits32(ctx->base + _SAES_CR, _SAES_CR_EN);

	return TEE_SUCCESS;
}

/* To resume the processing of a message */
static TEE_Result restore_context(struct stm32_saes_context *ctx)
{
	TEE_Result res = TEE_SUCCESS;

	/* IP should be disabled */
	if ((io_read32(ctx->base + _SAES_CR) & _SAES_CR_EN) != 0U) {
		DMSG("Device is still enabled");
		return TEE_ERROR_BAD_STATE;
	}

	/* Reset internal state */
	io_setbits32(ctx->base + _SAES_CR, _SAES_CR_IPRST);

	/* Restore the _SAES_CR */
	io_write32(ctx->base + _SAES_CR, ctx->cr);

	/* Write key and, in case of CBC or ECB decrypt, prepare it */
	res = saes_prepare_key(ctx);
	if (res)
		return res;

	saes_restore_suspend(ctx);

	saes_write_iv(ctx);

	/* Enable the SAES peripheral */
	io_setbits32(ctx->base + _SAES_CR, _SAES_CR_EN);

	return TEE_SUCCESS;
}

static TEE_Result do_from_init_to_phase(struct stm32_saes_context *ctx,
					uint32_t new_phase)
{
	TEE_Result res = TEE_SUCCESS;

	/* We didn't run the init phase yet */
	res = restore_context(ctx);
	if (res)
		return res;

	res = wait_computation_completed(ctx->base);
	if (res)
		return res;

	clear_computation_completed(ctx->base);

	/* Move to 'new_phase' */
	io_clrsetbits32(ctx->base + _SAES_CR, _SAES_CR_GCMPH_MASK,
			new_phase << _SAES_CR_GCMPH_SHIFT);

	/* Enable the SAES peripheral (init disabled it) */
	io_setbits32(ctx->base + _SAES_CR, _SAES_CR_EN);

	return TEE_SUCCESS;
}

static TEE_Result do_from_header_to_phase(struct stm32_saes_context *ctx,
					  uint32_t new_phase)
{
	TEE_Result res = TEE_SUCCESS;

	if (can_suspend(ctx->cr)) {
		res = restore_context(ctx);
		if (res)
			return res;
	}

	if (ctx->extra_size) {
		/* Manage unaligned header data before moving to next phase */
		memset((uint8_t *)ctx->extra + ctx->extra_size, 0,
		       AES_BLOCK_SIZE - ctx->extra_size);

		write_aligned_block(ctx->base, ctx->extra);

		res = wait_computation_completed(ctx->base);
		if (res)
			return res;

		clear_computation_completed(ctx->base);

		ctx->assoc_len += (ctx->extra_size) * INT8_BIT;
		ctx->extra_size = 0U;
	}

	/* Move to 'new_phase' */
	io_clrsetbits32(ctx->base + _SAES_CR, _SAES_CR_GCMPH_MASK,
			new_phase << _SAES_CR_GCMPH_SHIFT);

	return TEE_SUCCESS;
}

/**
 * @brief Start a AES computation.
 * @param ctx: SAES process context
 * @param is_dec: true if decryption, false if encryption
 * @param ch_mode: define the chaining mode
 * @param key_select: define where the key comes from
 * @param key: pointer to key (if key_select is KEY_SOFT, else unused)
 * @param key_size: key size
 * @param iv: pointer to initialization vector (unused if ch_mode is ECB)
 * @param iv_size: iv size
 * @note this function doesn't access to hardware but stores in ctx the values
 *
 * @retval TEE_SUCCESS if OK.
 */
TEE_Result stm32_saes_init(struct stm32_saes_context *ctx, bool is_dec,
			   enum stm32_saes_chaining_mode ch_mode,
			   enum stm32_saes_key_selection key_select,
			   const void *key, size_t key_size, const void *iv,
			   size_t iv_size)
{
	unsigned int i = 0;
	const uint32_t *iv_u32 = NULL;
	uint32_t local_iv[4] = { 0 };
	const uint32_t *key_u32 = NULL;
	uint32_t local_key[8] = { 0 };

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	saes_select_instance(ctx);

	ctx->assoc_len = 0U;
	ctx->load_len = 0U;
	ctx->extra_size = 0U;

	ctx->cr = _SAES_CR_RESET_VALUE;

	/* We want buffer to be u32 aligned */
	if (IS_ALIGNED_WITH_TYPE(key, uint32_t)) {
		key_u32 = key;
	} else {
		memcpy(local_key, key, key_size);
		key_u32 = local_key;
	}

	if (IS_ALIGNED_WITH_TYPE(iv, uint32_t)) {
		iv_u32 = iv;
	} else {
		memcpy(local_iv, iv, iv_size);
		iv_u32 = local_iv;
	}

	if (is_dec)
		clrsetbits(&ctx->cr, _SAES_CR_MODE_MASK,
			   _SAES_CR_MODE_DEC << _SAES_CR_MODE_SHIFT);
	else
		clrsetbits(&ctx->cr, _SAES_CR_MODE_MASK,
			   _SAES_CR_MODE_ENC << _SAES_CR_MODE_SHIFT);

	/* Save chaining mode */
	switch (ch_mode) {
	case STM32_SAES_MODE_ECB:
		SET_CHAINING_MODE(ECB, ctx->cr);
		break;
	case STM32_SAES_MODE_CBC:
		SET_CHAINING_MODE(CBC, ctx->cr);
		break;
	case STM32_SAES_MODE_CTR:
		SET_CHAINING_MODE(CTR, ctx->cr);
		break;
	case STM32_SAES_MODE_GCM:
		SET_CHAINING_MODE(GCM, ctx->cr);
		break;
	case STM32_SAES_MODE_CCM:
		SET_CHAINING_MODE(CCM, ctx->cr);
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/*
	 * We will use HW Byte swap (_SAES_CR_DATATYPE_BYTE) for data.
	 * So we won't need to
	 * TOBE32(data) before write to DINR
	 * nor
	 * FROMBE32 after reading from DOUTR.
	 *
	 * But note that wrap key only accept _SAES_CR_DATATYPE_NONE.
	 */
	clrsetbits(&ctx->cr, _SAES_CR_DATATYPE_MASK,
		   _SAES_CR_DATATYPE_BYTE << _SAES_CR_DATATYPE_SHIFT);

	/* Configure keysize */
	switch (key_size) {
	case AES_KEYSIZE_128:
		CLRBITS(ctx->cr, _SAES_CR_KEYSIZE);
		break;
	case AES_KEYSIZE_256:
		SETBITS(ctx->cr, _SAES_CR_KEYSIZE);
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Configure key */
	switch (key_select) {
	case STM32_SAES_KEY_SOFT:
		clrsetbits(&ctx->cr, _SAES_CR_KEYSEL_MASK,
			   _SAES_CR_KEYSEL_SOFT << _SAES_CR_KEYSEL_SHIFT);
		/* Save key */
		switch (key_size) {
		case AES_KEYSIZE_128:
			/* First 16 bytes == 4 u32 */
			for (i = 0U; i < AES_KEYSIZE_128 / sizeof(uint32_t);
			     i++) {
				ctx->key[i] = TOBE32(key_u32[3 - i]);
				/*
				 * /!\ we save the key in HW byte order
				 * and word order: key[i] is for _SAES_KEYRi.
				 */
			}
			break;
		case AES_KEYSIZE_256:
			for (i = 0U; i < AES_KEYSIZE_256 / sizeof(uint32_t);
			     i++) {
				ctx->key[i] = TOBE32(key_u32[7 - i]);
				/*
				 * /!\ we save the key in HW byte order
				 * and word order: key[i] is for _SAES_KEYRi.
				 */
			}
			break;
		default:
			return TEE_ERROR_BAD_PARAMETERS;
		}
		break;
	case STM32_SAES_KEY_DHU:
		clrsetbits(&ctx->cr, _SAES_CR_KEYSEL_MASK,
			   _SAES_CR_KEYSEL_DHUK << _SAES_CR_KEYSEL_SHIFT);
		break;
	case STM32_SAES_KEY_BH:
		clrsetbits(&ctx->cr, _SAES_CR_KEYSEL_MASK,
			   _SAES_CR_KEYSEL_BHK << _SAES_CR_KEYSEL_SHIFT);
		break;
	case STM32_SAES_KEY_BHU_XOR_BH:
		clrsetbits(&ctx->cr, _SAES_CR_KEYSEL_MASK,
			   _SAES_CR_KEYSEL_BHU_XOR_BH_K <<
			   _SAES_CR_KEYSEL_SHIFT);
		break;
	case STM32_SAES_KEY_WRAPPED:
		clrsetbits(&ctx->cr, _SAES_CR_KEYSEL_MASK,
			   _SAES_CR_KEYSEL_SOFT << _SAES_CR_KEYSEL_SHIFT);
		break;

	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Save IV */
	if (ch_mode != STM32_SAES_MODE_ECB) {
		if (!iv || iv_size != AES_IVSIZE)
			return TEE_ERROR_BAD_PARAMETERS;

		for (i = 0U; i < AES_IVSIZE / sizeof(uint32_t); i++) {
			ctx->iv[i] = TOBE32(iv_u32[3 - i]);
			/* /!\ We save the iv in HW byte order */
		}
	}

	/* Reset suspend registers */
	memset(ctx->susp, 0, sizeof(ctx->susp));

	return saes_start(ctx);
}

/**
 * @brief Update (or start) a AES authentificate process of
 *        associated data (CCM or GCM).
 * @param ctx: SAES process context
 * @param data: pointer to associated data
 * @param data_size: data size
 *
 * @retval 0 if OK.
 */
TEE_Result stm32_saes_update_assodata(struct stm32_saes_context *ctx,
				      uint8_t *data, size_t data_size)
{
	TEE_Result res = 0;
	unsigned int i = 0U;
	uint32_t previous_phase = 0U;

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	/* If no associated data, nothing to do */
	if (!data || !data_size)
		return TEE_SUCCESS;

	mutex_lock(ctx->lock);

	previous_phase = (ctx->cr & _SAES_CR_GCMPH_MASK) >>
			 _SAES_CR_GCMPH_SHIFT;

	switch (previous_phase) {
	case _SAES_CR_GCMPH_INIT:
		res = do_from_init_to_phase(ctx, _SAES_CR_GCMPH_HEADER);
		break;
	case _SAES_CR_GCMPH_HEADER:
		/*
		 * Function update_assodata was already called.
		 * We only need to restore the context.
		 */
		if (can_suspend(ctx->cr))
			res = restore_context(ctx);

		break;
	default:
		DMSG("out of order call");
		res = TEE_ERROR_BAD_STATE;
	}

	if (res)
		goto out;

	/* Manage if remaining data from a previous update_assodata call */
	if (ctx->extra_size &&
	    ((ctx->extra_size + data_size) >= AES_BLOCK_SIZE)) {
		uint32_t block[AES_BLOCK_NB_U32] = { 0 };

		memcpy(block, ctx->extra, ctx->extra_size);
		memcpy((uint8_t *)block + ctx->extra_size, data,
		       AES_BLOCK_SIZE - ctx->extra_size);

		write_aligned_block(ctx->base, block);

		res = wait_computation_completed(ctx->base);
		if (res)
			goto out;

		clear_computation_completed(ctx->base);

		i += AES_BLOCK_SIZE - ctx->extra_size;
		ctx->extra_size = 0;
		ctx->assoc_len += AES_BLOCK_SIZE_BIT;
	}

	while (data_size - i >= AES_BLOCK_SIZE) {
		write_block(ctx->base, data + i);

		res = wait_computation_completed(ctx->base);
		if (res)
			goto out;

		clear_computation_completed(ctx->base);

		/* Process next block */
		i += AES_BLOCK_SIZE;
		ctx->assoc_len += AES_BLOCK_SIZE_BIT;
	}

	/*
	 * Manage last block if not a block size multiple:
	 * Save remaining data to manage them later (potentially with new
	 * associated data).
	 */
	if (i < data_size) {
		memcpy((uint8_t *)ctx->extra + ctx->extra_size, data + i,
		       data_size - i);
		ctx->extra_size += data_size - i;
	}

	res = save_context(ctx);
out:
	if (res)
		saes_end(ctx, res);

	mutex_unlock(ctx->lock);

	return res;
}

/**
 * @brief Update (or start) a AES authenticate and de/encrypt with
 *        payload data (CCM or GCM).
 * @param ctx: SAES process context
 * @param last_block: true if last payload data block
 * @param data_in: pointer to payload
 * @param data_out: pointer where to save de/encrypted payload
 * @param data_size: payload size
 *
 * @retval TEE_SUCCESS if OK.
 */
TEE_Result stm32_saes_update_load(struct stm32_saes_context *ctx,
				  bool last_block, uint8_t *data_in,
				  uint8_t *data_out, size_t data_size)
{
	TEE_Result res = TEE_SUCCESS;
	unsigned int i = 0U;
	uint32_t previous_phase = 0U;

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	/* If there is no data, nothing to do */
	if (!data_in || !data_size)
		return TEE_SUCCESS;

	mutex_lock(ctx->lock);

	previous_phase = ((ctx->cr & _SAES_CR_GCMPH_MASK) >>
			  _SAES_CR_GCMPH_SHIFT);

	switch (previous_phase) {
	case _SAES_CR_GCMPH_INIT:
		res = do_from_init_to_phase(ctx, _SAES_CR_GCMPH_PAYLOAD);
		break;
	case _SAES_CR_GCMPH_HEADER:
		res = do_from_header_to_phase(ctx, _SAES_CR_GCMPH_PAYLOAD);
		break;
	case _SAES_CR_GCMPH_PAYLOAD:
		/* new update_load call, we only need to restore context */
		if (can_suspend(ctx->cr))
			res = restore_context(ctx);

		break;
	default:
		DMSG("out of order call");
		res = TEE_ERROR_BAD_STATE;
	}

	if (res)
		goto out;

	while (i < ROUNDDOWN(data_size, AES_BLOCK_SIZE)) {
		write_block(ctx->base, data_in + i);

		res = wait_computation_completed(ctx->base);
		if (res)
			goto out;

		read_block(ctx->base, data_out + i);

		clear_computation_completed(ctx->base);

		/* Process next block */
		i += AES_BLOCK_SIZE;
		ctx->load_len += AES_BLOCK_SIZE_BIT;
	}

	/* Manage last block if not a block size multiple */
	if (last_block && i < data_size) {
		uint32_t block_in[AES_BLOCK_NB_U32] = { 0 };
		uint32_t block_out[AES_BLOCK_NB_U32] = { 0 };

		memcpy(block_in, data_in + i, data_size - i);

		if (does_need_npblb(ctx->cr)) {
			uint32_t npblb = AES_BLOCK_SIZE - (data_size - i);

			io_clrsetbits32(ctx->base + _SAES_CR,
					_SAES_CR_NPBLB_MASK,
					 npblb << _SAES_CR_NPBLB_SHIFT);
		}

		write_aligned_block(ctx->base, block_in);

		res = wait_computation_completed(ctx->base);
		if (res)
			goto out;

		read_aligned_block(ctx->base, block_out);

		clear_computation_completed(ctx->base);

		memcpy(data_out + i, block_out, data_size - i);

		ctx->load_len += (data_size - i) * INT8_BIT;
	}

	res = save_context(ctx);
out:
	if (res)
		saes_end(ctx, res);

	mutex_unlock(ctx->lock);

	return res;
}

/**
 * @brief Get authentication tag for AES authenticated algorithms (CCM or GCM).
 * @param ctx: SAES process context
 * @param tag: pointer where to save the tag
 * @param data_size: tag size
 *
 * @retval TEE_SUCCESS if OK.
 */
TEE_Result stm32_saes_final(struct stm32_saes_context *ctx, uint8_t *tag,
			    size_t tag_size)
{
	TEE_Result res = TEE_SUCCESS;
	uint32_t tag_u32[4] = { 0 };
	uint32_t previous_phase = 0U;

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	mutex_lock(ctx->lock);

	previous_phase = ((ctx->cr & _SAES_CR_GCMPH_MASK) >>
			  _SAES_CR_GCMPH_SHIFT);

	switch (previous_phase) {
	case _SAES_CR_GCMPH_INIT:
		res = do_from_init_to_phase(ctx, _SAES_CR_GCMPH_FINAL);
		break;
	case _SAES_CR_GCMPH_HEADER:
		res = do_from_header_to_phase(ctx, _SAES_CR_GCMPH_FINAL);
		break;
	case _SAES_CR_GCMPH_PAYLOAD:
		if (can_suspend(ctx->cr))
			res = restore_context(ctx);

		/* Move to final phase */
		io_clrsetbits32(ctx->base + _SAES_CR, _SAES_CR_GCMPH_MASK,
				_SAES_CR_GCMPH_FINAL << _SAES_CR_GCMPH_SHIFT);
		break;
	default:
		DMSG("out of order call");
		res = TEE_ERROR_BAD_STATE;
	}
	if (res)
		goto out;

	if (IS_CHAINING_MODE(GCM, ctx->cr)) {
		/* No need to htobe() as we configure the HW to swap bytes */
		io_write32(ctx->base + _SAES_DINR, 0U);
		io_write32(ctx->base + _SAES_DINR, ctx->assoc_len);
		io_write32(ctx->base + _SAES_DINR, 0U);
		io_write32(ctx->base + _SAES_DINR, ctx->load_len);
	}

	res = wait_computation_completed(ctx->base);
	if (res)
		goto out;

	read_aligned_block(ctx->base, tag_u32);

	clear_computation_completed(ctx->base);

	memcpy(tag, tag_u32, MIN(sizeof(tag_u32), tag_size));

out:
	saes_end(ctx, res);
	mutex_unlock(ctx->lock);

	return res;
}

/**
 * @brief Update (or start) a AES de/encrypt process (ECB, CBC or CTR).
 * @param ctx: SAES process context
 * @param last_block: true if last payload data block
 * @param data_in: pointer to payload
 * @param data_out: pointer where to save de/encrypted payload
 * @param data_size: payload size
 *
 * @retval TEE_SUCCESS if OK.
 */
TEE_Result stm32_saes_update(struct stm32_saes_context *ctx, bool last_block,
			     uint8_t *data_in, uint8_t *data_out,
			     size_t data_size)
{
	TEE_Result res = TEE_SUCCESS;
	unsigned int i = 0U;

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	mutex_lock(ctx->lock);

	/*
	 * In CBC encryption we need to manage specifically last 2 128bits
	 * blocks if total size in not a block size aligned
	 * work TODO. Currently return TEE_ERROR_NOT_IMPLEMENTED.
	 * Morevoer as we need to know last 2 blocks, if unaligned and
	 * call with less than two blocks, return TEE_ERROR_BAD_STATE.
	 */
	if (last_block && IS_CHAINING_MODE(CBC, ctx->cr) &&
	    is_encrypt(ctx->cr) &&
	    (ROUNDDOWN(data_size, AES_BLOCK_SIZE) != data_size)) {
		if (data_size < AES_BLOCK_SIZE * 2) {
			/*
			 * If CBC, size of the last part should be at
			 * least 2*AES_BLOCK_SIZE
			 */
			EMSG("Unexpected last block size");
			res = TEE_ERROR_BAD_STATE;
			goto out;
		}
		/*
		 * Moreover the CBC specific padding for encrypt is not
		 * yet implemented, and not used in OPTEE
		 */
		res = TEE_ERROR_NOT_IMPLEMENTED;
		goto out;
	}

	/* Manage remaining CTR mask from previous update call */
	if (IS_CHAINING_MODE(CTR, ctx->cr) &&
	    ctx->extra_size) {
		unsigned int j = 0;
		uint8_t *mask = (uint8_t *)ctx->extra;

		for (j = 0; j < ctx->extra_size && i < data_size; j++, i++)
			data_out[i] = data_in[i] ^ mask[j];

		if (j != ctx->extra_size) {
			/*
			 * We didn't consume all saved mask,
			 * but no more data.
			 */

			/* We save remaining mask and its new size */
			memmove(ctx->extra, ctx->extra + j,
				ctx->extra_size - j);
			ctx->extra_size -= j;

			/*
			 * We don't need to save HW context we didn't
			 * modify HW state.
			 */
			res = TEE_SUCCESS;
			goto out;
		}
		/* All extra mask consumed */
		ctx->extra_size = 0U;
	}

	res = restore_context(ctx);
	if (res)
		goto out;

	while (data_size - i >= AES_BLOCK_SIZE) {
		write_block(ctx->base, data_in + i);

		res = wait_computation_completed(ctx->base);
		if (res)
			goto out;

		read_block(ctx->base, data_out + i);

		clear_computation_completed(ctx->base);

		/* Process next block */
		i += AES_BLOCK_SIZE;
	}

	/* Manage last block if not a block size multiple */
	if (i < data_size) {
		if (IS_CHAINING_MODE(CTR, ctx->cr)) {
			/*
			 * For CTR we save the generated mask to use it at next
			 * update call.
			 */
			uint32_t block_in[AES_BLOCK_NB_U32] = { 0 };
			uint32_t block_out[AES_BLOCK_NB_U32] = { 0 };

			memcpy(block_in, data_in + i, data_size - i);

			write_aligned_block(ctx->base, block_in);

			res = wait_computation_completed(ctx->base);
			if (res)
				goto out;

			read_aligned_block(ctx->base, block_out);

			clear_computation_completed(ctx->base);

			memcpy(data_out + i, block_out, data_size - i);

			/* Save mask for possibly next call */
			ctx->extra_size = AES_BLOCK_SIZE - (data_size - i);
			memcpy(ctx->extra, (uint8_t *)block_out + data_size - i,
			       ctx->extra_size);
		} else {
			/* CBC and ECB can manage only multiple of block_size */
			res = TEE_ERROR_BAD_PARAMETERS;
			goto out;
		}
	}

	if (!last_block)
		res = save_context(ctx);

out:
	/* If last block or error, end of SAES process */
	if (last_block || res)
		saes_end(ctx, res);

	mutex_unlock(ctx->lock);

	return res;
}

static void xor_block(uint8_t *b1, uint8_t *b2, size_t size)
{
	size_t i = 0;

	for (i = 0; i < size; i++)
		b1[i] ^= b2[i];
}

static TEE_Result stm32_saes_cmac_prf_128(struct stm32_saes_context *ctx,
				enum stm32_saes_key_selection key_select,
				const void *key, size_t key_size, uint8_t *data,
				size_t data_size, uint8_t *out)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint8_t l[AES_BLOCK_SIZE] = { 0 };
	uint8_t k1[AES_BLOCK_SIZE] = { 0 };
	uint8_t k2[AES_BLOCK_SIZE] = { 0 };
	uint8_t block[AES_BLOCK_SIZE] = { 0 };
	int i = 0;
	uint8_t bit = 0;
	size_t processed = 0;

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Get K1 and K2 */
	res = stm32_saes_init(ctx, false, STM32_SAES_MODE_ECB, key_select,
			      key, key_size, NULL, 0);
	if (res)
		goto out;

	res = stm32_saes_update(ctx, true, l, l, sizeof(l));
	if (res)
		goto out;

	/* MSB(L) == 0 => K1 = L << 1 */
	bit = 0;
	for (i = sizeof(l) - 1; i >= 0; i--) {
		k1[i] = (l[i] << 1) | bit;
		bit = (l[i] & 0x80) >> 7;
	}
	/* MSB(L) == 1 => K1 = (L << 1) XOR const_Rb */
	if ((l[0] & 0x80))
		k1[sizeof(k1) - 1] = k1[sizeof(k1) - 1] ^ 0x87;

	/* MSB(K1) == 0 => K2 = K1 << 1 */
	bit = 0;
	for (i = sizeof(k1) - 1; i >= 0; i--) {
		k2[i] = (k1[i] << 1) | bit;
		bit = (k1[i] & 0x80) >> 7;
	}

	/* MSB(K1) == 1 => K2 = (K1 << 1) XOR const_Rb */
	if ((k1[0] & 0x80))
		k2[sizeof(k2) - 1] = k2[sizeof(k2) - 1] ^ 0x87;

	memset(block, 0, sizeof(block));

	if (data_size > AES_BLOCK_SIZE) {
		uint8_t *data_out;

		/* all block but last in CBC mode */
		res = stm32_saes_init(ctx, false, STM32_SAES_MODE_CBC,
				      key_select, key, key_size, block,
				      sizeof(block));
		if (res)
			goto out;

		processed = ROUNDDOWN(data_size - 1, AES_BLOCK_SIZE);
		data_out = malloc(processed);
		if (!data_out)
			return TEE_ERROR_OUT_OF_MEMORY;

		res = stm32_saes_update(ctx, true, data, data_out, processed);
		if (res) {
			free(data_out);
			goto out;
		}

		/* Copy last out block or keep block as { 0 } */
		memcpy(block, data_out + processed - AES_BLOCK_SIZE,
		       AES_BLOCK_SIZE);

		free(data_out);
	}

	/* Manage last block */
	xor_block(block, data + processed, data_size - processed);
	if (data_size - processed == AES_BLOCK_SIZE) {
		xor_block(block, k1, AES_BLOCK_SIZE);
	} else {
		/* xor with padding = 0b100... */
		block[data_size - processed] ^= 0x80;
		xor_block(block, k2, AES_BLOCK_SIZE);
	}

	/*
	 * AES last block.
	 * We need to use same chaining mode to keep same key if DHUK is
	 * selected.
	 * So we reuse l as a { 0 } iv
	 */
	memset(l, 0, sizeof(l));
	res = stm32_saes_init(ctx, false, STM32_SAES_MODE_CBC, key_select, key,
			      key_size, l, sizeof(l));
	if (res)
		goto out;

	res = stm32_saes_update(ctx, true, block, out, AES_BLOCK_SIZE);

out:
	return res;
}

TEE_Result stm32_saes_kdf_aes_cmac_prf(struct stm32_saes_context *ctx,
				       enum stm32_saes_key_selection key_select,
				       const void *key, size_t key_size,
				       const void *input, size_t input_size,
				       uint8_t *subkey, size_t subkey_size)

{
	TEE_Result res = TEE_SUCCESS;
	uint32_t index = 0;
	uint32_t index_be = 0;
	uint8_t *data = NULL;
	size_t data_index = 0;
	size_t subkey_index = 0;
	size_t data_size = input_size + sizeof(index_be);
	uint8_t cmac[AES_BLOCK_SIZE] = { 0 };

	if (!ctx || !input || !input_size)
		return TEE_ERROR_BAD_PARAMETERS;

	/* For each K(i) we will add an index */
	data = malloc(data_size);
	if (!data)
		return TEE_ERROR_OUT_OF_MEMORY;

	data_index = 0;
	index_be = TOBE32(index);
	memcpy(data + data_index, &index_be, sizeof(index_be));
	data_index += sizeof(index_be);
	memcpy(data + data_index, input, input_size);
	data_index += input_size;

	/*
	 * K(i) computation.
	 */
	index = 0;
	while (subkey_index < subkey_size) {
		/* update index */
		index++;
		index_be = TOBE32(index);
		memcpy(data, &index_be, sizeof(index_be));

		res = stm32_saes_cmac_prf_128(ctx, key_select, key, key_size,
					      data, data_size, cmac);
		if (res)
			goto out;

		memcpy(subkey + subkey_index, cmac, MIN(subkey_size -
							subkey_index,
							sizeof(cmac)));
		subkey_index += sizeof(cmac);
	}

out:
	free(data);
	if (res)
		memzero_explicit(subkey, subkey_size);

	return res;
}

TEE_Result huk_subkey_derive(enum huk_subkey_usage usage,
			     const void *const_data, size_t const_data_len,
			     uint8_t *subkey, size_t subkey_len)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint8_t *input = NULL;
	size_t input_index = 0;
	size_t subkey_bitlen = 0;
	struct stm32_saes_context ctx = { };
	uint8_t separator = 0;

	// Check if driver is probed
	if (saes_pdata.base.pa == 0) {
		DMSG("Use __huk_subkey_derive instead of SAES IP features");
		return __huk_subkey_derive(usage, const_data, const_data_len,
					   subkey, subkey_len);
	}

	input = malloc(const_data_len + sizeof(separator) + sizeof(usage) +
		       sizeof(subkey_bitlen) + AES_BLOCK_SIZE);
	if (!input)
		return TEE_ERROR_OUT_OF_MEMORY;

	input_index = 0;
	if (const_data) {
		memcpy(input + input_index, const_data, const_data_len);
		input_index += const_data_len;

		memcpy(input + input_index, &separator, sizeof(separator));
		input_index += sizeof(separator);
	}

	memcpy(input + input_index, &usage, sizeof(usage));
	input_index += sizeof(usage);

	/*
	 * We should add the subkey_len in bits at end of input.
	 * And we choose to put in a MSB first uint32_t.
	 */
	subkey_bitlen = TOBE32(subkey_len * INT8_BIT);
	memcpy(input + input_index, &subkey_bitlen, sizeof(subkey_bitlen));
	input_index += sizeof(subkey_bitlen);

	/*
	 * We get K(0) to avoid some key control attack
	 * and store it at end of input.
	 */
	res = stm32_saes_cmac_prf_128(&ctx, STM32_SAES_KEY_DHU, NULL,
				      AES_KEYSIZE_128,
				      input, input_index,
				      input + input_index);
	if (res)
		goto out;

	/* We just added K(0) to input */
	input_index += AES_BLOCK_SIZE;

	res = stm32_saes_kdf_aes_cmac_prf(&ctx, STM32_SAES_KEY_DHU, NULL,
					  AES_KEYSIZE_128, input, input_index,
					  subkey, subkey_len);

out:
	free(input);
	return res;
}

#ifdef CFG_EMBED_DTB
static TEE_Result stm32_saes_parse_fdt(struct stm32_saes_platdata *pdata,
				       const void *fdt, int node)
{
	struct dt_node_info dt_saes = { };
	TEE_Result res;

	_fdt_fill_device_info(fdt, &dt_saes, node);

	if (dt_saes.reg == DT_INFO_INVALID_REG ||
	    dt_saes.reg_size == DT_INFO_INVALID_REG_SIZE)
		return TEE_ERROR_BAD_PARAMETERS;

	pdata->base.pa = dt_saes.reg;
	io_pa_or_va_secure(&pdata->base, dt_saes.reg_size);
	if (!pdata->base.va)
		panic();

	res = rstctrl_dt_get_by_index(fdt, node, 0, &pdata->reset);
	if (res != TEE_SUCCESS && res != TEE_ERROR_ITEM_NOT_FOUND)
		return res;

	res = clk_dt_get_by_name(fdt, node, "bus", &pdata->clk);
	if (res)
		return res;

	return clk_dt_get_by_name(fdt, node, "rng", &pdata->clk_rng);
}

__weak
TEE_Result stm32_saes_get_platdata(struct stm32_saes_platdata *pdata __unused)
{
	/* In DT config, the platform data are filled by DT file */
	return TEE_SUCCESS;
}
#else /* CFG_EMBED_DTB */
static int stm32_saes_parse_fdt(struct stm32_tamp_platdata *pdata,
				const void *fdt, int node)
{
	/* Do nothing, there is no fdt to parse in this case */
	return 0;
}

/* This function can be overridden by platform to define pdata of SAES driver */
__weak
TEE_Result stm32_saes_get_platdata(struct stm32_saes_platdata *pdata __unused)
{
	return TEE_ITEM_NO_FOUND;
}
#endif

static TEE_Result stm32_saes_reset(void)
{
	TEE_Result ret = TEE_SUCCESS;
	vaddr_t base = io_pa_or_va(&saes_pdata.base, 1);

	if (!saes_pdata.reset) {
		/* Internal reset of SAES */
		io_setbits32(base + _SAES_CR, _SAES_CR_IPRST);
		udelay(SAES_RESET_DELAY);
		io_clrbits32(base + _SAES_CR, _SAES_CR_IPRST);
		return ret;
	}

	ret = rstctrl_assert_to(saes_pdata.reset, TIMEOUT_US_1MS);
	if (ret)
		return ret;

	udelay(SAES_RESET_DELAY);

	return rstctrl_deassert_to(saes_pdata.reset, TIMEOUT_US_1MS);
}

static TEE_Result stm32_saes_pm(enum pm_op op, uint32_t pm_hint,
				const struct pm_callback_handle *hdl __unused)
{
	TEE_Result ret = TEE_ERROR_NOT_IMPLEMENTED;

	switch (op) {
	case PM_OP_SUSPEND:
		clk_disable(saes_pdata.clk);
		clk_disable(saes_pdata.clk_rng);
		ret = TEE_SUCCESS;
		break;
	case PM_OP_RESUME:
		if (clk_enable(saes_pdata.clk) ||  clk_enable(saes_pdata.clk_rng))
			panic();

		if (PM_HINT_IS_STATE(pm_hint, CONTEXT)) {
			ret = stm32_saes_reset();
			if (ret)
				panic();
		}
		ret = TEE_SUCCESS;
		break;
	default:
		break;
	}
	return ret;
}

/**
 * @brief Initialize SAES driver.
 * @param None.
 * @retval TEE_SUCCESS if OK.
 */
static TEE_Result stm32_saes_probe(const void *fdt, int node,
				   const void *compat_data __unused)
{
	TEE_Result res = TEE_SUCCESS;

	if (stm32_saes_get_platdata(&saes_pdata))
		return TEE_ERROR_NOT_SUPPORTED;

	res = stm32_saes_parse_fdt(&saes_pdata, fdt, node);
	if (res)
		return res;

	if (clk_enable(saes_pdata.clk) || clk_enable(saes_pdata.clk_rng))
		panic();

	if (stm32_saes_reset())
		panic();

	if (IS_ENABLED(CFG_CRYPTO_DRV_CIPHER)) {
		res = stm32_register_cipher(SAES_IP);
		if (res) {
			EMSG("Failed to register to cipher: %#"PRIx32, res);
			panic();
		}
	}

	if (IS_ENABLED(CFG_PM))
		register_pm_core_service_cb(stm32_saes_pm, NULL, "stm32-saes");

	return res;
}

#if CFG_EMBED_DTB
static const struct dt_device_match saes_match_table[] = {
	{ .compatible = "st,stm32mp13-saes" },
	{ }
};

DEFINE_DT_DRIVER(stm32_saes_dt_driver) = {
	.name = "stm32-saes",
	.match_table = saes_match_table,
	.probe = stm32_saes_probe,
};
#endif
