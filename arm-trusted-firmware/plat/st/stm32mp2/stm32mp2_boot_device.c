/*
 * Copyright (c) 2021-2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <drivers/hyperflash.h>
#include <drivers/nand.h>
#include <drivers/raw_nand.h>
#include <drivers/spi_nand.h>
#include <drivers/spi_nor.h>
#include <lib/utils.h>
#include <plat/common/platform.h>

#if STM32MP_RAW_NAND || STM32MP_SPI_NAND
static int get_data_from_otp(struct nand_device *nand_dev, bool is_slc)
{
	uint32_t nand_param;
	uint32_t nand2_param;

	/* Check if NAND parameters are stored in OTP */
	if (stm32_get_otp_value(NAND_OTP, &nand_param) != 0) {
		ERROR("BSEC: NAND_OTP Error\n");
		return -EACCES;
	}

	if ((nand_param == 0U) && is_slc) {
		return 0;
	}

	if (((nand_param & NAND_PARAM_STORED_IN_OTP) == 0U) && is_slc) {
		goto ecc;
	}

	if (stm32_get_otp_value(NAND2_OTP, &nand2_param) != 0) {
		ERROR("BSEC: NAND2_OTP Error\n");
		return -EACCES;
	}

	/* Check OTP configuration for this device */
	if ((((nand2_param & NAND2_CONFIG_DISTRIB) == NAND2_PNAND_NAND1_SNAND_NAND2) && !is_slc) ||
	    (((nand2_param & NAND2_CONFIG_DISTRIB) == NAND2_PNAND_NAND2_SNAND_NAND1) && is_slc)) {
		nand_param = nand2_param << (NAND_PAGE_SIZE_SHIFT - NAND2_PAGE_SIZE_SHIFT);
	}

	/* NAND parameter shall be read from OTP */
	if ((nand_param & NAND_WIDTH_MASK) != 0U) {
		nand_dev->buswidth = NAND_BUS_WIDTH_16;
	} else {
		nand_dev->buswidth = NAND_BUS_WIDTH_8;
	}

	switch ((nand_param & NAND_PAGE_SIZE_MASK) >> NAND_PAGE_SIZE_SHIFT) {
	case NAND_PAGE_SIZE_2K:
		nand_dev->page_size = 0x800U;
		break;

	case NAND_PAGE_SIZE_4K:
		nand_dev->page_size = 0x1000U;
		break;

	case NAND_PAGE_SIZE_8K:
		nand_dev->page_size = 0x2000U;
		break;

	default:
		ERROR("Cannot read NAND page size\n");
		return -EINVAL;
	}

	switch ((nand_param & NAND_BLOCK_SIZE_MASK) >> NAND_BLOCK_SIZE_SHIFT) {
	case NAND_BLOCK_SIZE_64_PAGES:
		nand_dev->block_size = 64U * nand_dev->page_size;
		break;

	case NAND_BLOCK_SIZE_128_PAGES:
		nand_dev->block_size = 128U * nand_dev->page_size;
		break;

	case NAND_BLOCK_SIZE_256_PAGES:
		nand_dev->block_size = 256U * nand_dev->page_size;
		break;

	default:
		ERROR("Cannot read NAND block size\n");
		return -EINVAL;
	}

	nand_dev->size = ((nand_param & NAND_BLOCK_NB_MASK) >>
			  NAND_BLOCK_NB_SHIFT) *
		NAND_BLOCK_NB_UNIT * nand_dev->block_size;

ecc:
	if (is_slc) {
		switch ((nand_param & NAND_ECC_BIT_NB_MASK) >>
			NAND_ECC_BIT_NB_SHIFT) {
		case NAND_ECC_BIT_NB_1_BITS:
			nand_dev->ecc.max_bit_corr = 1U;
			break;

		case NAND_ECC_BIT_NB_4_BITS:
			nand_dev->ecc.max_bit_corr = 4U;
			break;

		case NAND_ECC_BIT_NB_8_BITS:
			nand_dev->ecc.max_bit_corr = 8U;
			break;

		case NAND_ECC_ON_DIE:
			nand_dev->ecc.mode = NAND_ECC_ONDIE;
			break;

		default:
			if (nand_dev->ecc.max_bit_corr == 0U) {
				ERROR("No valid eccbit number\n");
				return -EINVAL;
			}
		}
	} else {
		/* Selected multiple plane NAND */
		if ((nand_param & NAND_PLANE_BIT_NB_MASK) != 0U) {
			nand_dev->nb_planes = 2U;
		} else {
			nand_dev->nb_planes = 1U;
		}
	}

	VERBOSE("OTP: Block %u Page %u Size %llu\n", nand_dev->block_size,
		nand_dev->page_size, nand_dev->size);

	return 0;
}
#endif /* STM32MP_RAW_NAND || STM32MP_SPI_NAND */

#if STM32MP_RAW_NAND
int plat_get_raw_nand_data(struct rawnand_device *device)
{
	device->nand_dev->ecc.mode = NAND_ECC_HW;
	device->nand_dev->ecc.size = SZ_512;

	return get_data_from_otp(device->nand_dev, true);
}
#endif

#if STM32MP_SPI_NAND
int plat_get_spi_nand_data(struct spinand_device *device)
{
	return get_data_from_otp(device->nand_dev, false);
}
#endif

#if STM32MP_SPI_NOR
int plat_get_nor_data(struct nor_device *device)
{
	/* Quad read command used with MX25L51245G */
	device->size = SZ_64M;
	device->flags |= SPI_NOR_USE_BANK;

	zeromem(&device->read_op, sizeof(struct spi_mem_op));
	device->read_op.cmd.opcode = SPI_NOR_OP_READ_1_1_4;
	device->read_op.cmd.nbytes = 1U;
	device->read_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	device->read_op.addr.nbytes = 3U;
	device->read_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	device->read_op.dummy.nbytes = 1U;
	device->read_op.dummy.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	device->read_op.data.buswidth = SPI_MEM_BUSWIDTH_4_LINE;
	device->read_op.data.dir = SPI_MEM_DATA_IN;

	return 0;
}
#endif

#if STM32MP_HYPERFLASH
int plat_get_hyperflash_data(struct hyperflash_device *device)
{
	device->size = SZ_64M;

	return 0;
}
#endif
