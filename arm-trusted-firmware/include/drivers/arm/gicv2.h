/*
 * Copyright (c) 2015-2018, ARM Limited and Contributors. All rights reserved.
 * Portions copyright (c) 2021-2022, ProvenRun S.A.S. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GICV2_H
#define GICV2_H

#include <drivers/arm/gic_common.h>
#include <platform_def.h>

/*******************************************************************************
 * GICv2 miscellaneous definitions
 ******************************************************************************/

/* Interrupt group definitions */
#define GICV2_INTR_GROUP0	U(0)
#define GICV2_INTR_GROUP1	U(1)

/* Interrupt IDs reported by the HPPIR and IAR registers */
#define PENDING_G1_INTID	U(1022)

/* GICv2 can only target up to 8 PEs */
#define GICV2_MAX_TARGET_PE	U(8)

/*******************************************************************************
 * GICv2 specific Distributor interface register offsets and constants.
 ******************************************************************************/
#define GICD_ITARGETSR		U(0x800)
#define GICD_SGIR		U(0xF00)
#define GICD_CPENDSGIR		U(0xF10)
#define GICD_SPENDSGIR		U(0xF20)

/*
 * Some GICv2 implementations violate the specification and have this register
 * at a different address. Allow overriding it in platform_def.h as workaround.
 */
#ifndef GICD_PIDR2_GICV2
#define GICD_PIDR2_GICV2	U(0xFE8)
#endif

#define ITARGETSR_SHIFT		2
#define GIC_TARGET_CPU_MASK	U(0xff)

#define CPENDSGIR_SHIFT		2
#define SPENDSGIR_SHIFT		CPENDSGIR_SHIFT

#define SGIR_TGTLSTFLT_SHIFT	24
#define SGIR_TGTLSTFLT_MASK	U(0x3)
#define SGIR_TGTLST_SHIFT	16
#define SGIR_TGTLST_MASK	U(0xff)
#define SGIR_NSATT		(U(0x1) << 16)
#define SGIR_INTID_MASK		ULL(0xf)

#define SGIR_TGT_SPECIFIC	U(0)

#define GICV2_SGIR_VALUE(tgt_lst_flt, tgt, nsatt, intid) \
	((((tgt_lst_flt) & SGIR_TGTLSTFLT_MASK) << SGIR_TGTLSTFLT_SHIFT) | \
	 (((tgt) & SGIR_TGTLST_MASK) << SGIR_TGTLST_SHIFT) | \
	 ((nsatt) ? SGIR_NSATT : U(0)) | \
	 ((intid) & SGIR_INTID_MASK))

/*******************************************************************************
 * GICv2 specific CPU interface register offsets and constants.
 ******************************************************************************/
/* Physical CPU Interface registers */
#define GICC_CTLR		U(0x0)
#define GICC_PMR		U(0x4)
#define GICC_BPR		U(0x8)
#define GICC_IAR		U(0xC)
#define GICC_EOIR		U(0x10)
#define GICC_RPR		U(0x14)
#define GICC_HPPIR		U(0x18)
#define GICC_AHPPIR		U(0x28)
#define GICC_IIDR		U(0xFC)
#define GICC_DIR		U(0x1000)
#define GICC_PRIODROP		GICC_EOIR

/* GICC_CTLR bit definitions */
#define EOI_MODE_NS		BIT_32(10)
#define EOI_MODE_S		BIT_32(9)
#define IRQ_BYP_DIS_GRP1	BIT_32(8)
#define FIQ_BYP_DIS_GRP1	BIT_32(7)
#define IRQ_BYP_DIS_GRP0	BIT_32(6)
#define FIQ_BYP_DIS_GRP0	BIT_32(5)
#define CBPR			BIT_32(4)
#define FIQ_EN_SHIFT		3
#define FIQ_EN_BIT		BIT_32(FIQ_EN_SHIFT)
#define ACK_CTL			BIT_32(2)

/* GICC_IIDR bit masks and shifts */
#define GICC_IIDR_PID_SHIFT	20
#define GICC_IIDR_ARCH_SHIFT	16
#define GICC_IIDR_REV_SHIFT	12
#define GICC_IIDR_IMP_SHIFT	0

#define GICC_IIDR_PID_MASK	U(0xfff)
#define GICC_IIDR_ARCH_MASK	U(0xf)
#define GICC_IIDR_REV_MASK	U(0xf)
#define GICC_IIDR_IMP_MASK	U(0xfff)

/* HYP view virtual CPU Interface registers */
#define GICH_CTL		U(0x0)
#define GICH_VTR		U(0x4)
#define GICH_ELRSR0		U(0x30)
#define GICH_ELRSR1		U(0x34)
#define GICH_APR0		U(0xF0)
#define GICH_LR_BASE		U(0x100)

/* Virtual CPU Interface registers */
#define GICV_CTL		U(0x0)
#define GICV_PRIMASK		U(0x4)
#define GICV_BP			U(0x8)
#define GICV_INTACK		U(0xC)
#define GICV_EOI		U(0x10)
#define GICV_RUNNINGPRI		U(0x14)
#define GICV_HIGHESTPEND	U(0x18)
#define GICV_DEACTIVATE		U(0x1000)

/* GICD_CTLR bit definitions */
#define CTLR_ENABLE_G1_SHIFT		1
#define CTLR_ENABLE_G1_MASK		U(0x1)
#define CTLR_ENABLE_G1_BIT		BIT_32(CTLR_ENABLE_G1_SHIFT)

/* Interrupt ID mask for HPPIR, AHPPIR, IAR and AIAR CPU Interface registers */
#define INT_ID_MASK		U(0x3ff)

#ifndef __ASSEMBLER__

#include <cdefs.h>
#include <stdbool.h>
#include <stdint.h>

#include <common/interrupt_props.h>

/*******************************************************************************
 * This structure describes some of the implementation defined attributes of
 * the GICv2 IP. It is used by the platform port to specify these attributes
 * in order to initialize the GICv2 driver. The attributes are described
 * below.
 *
 * The 'gicd_base' field contains the base address of the Distributor interface
 * programmer's view.
 *
 * The 'gicc_base' field contains the base address of the CPU Interface
 * programmer's view.
 *
 * The 'target_masks' is a pointer to an array containing 'target_masks_num'
 * elements. The GIC driver will populate the array with per-PE target mask to
 * use to when targeting interrupts.
 *
 * The 'interrupt_props' field is a pointer to an array that enumerates secure
 * interrupts and their properties. If this field is not NULL, both
 * 'g0_interrupt_array' and 'g1s_interrupt_array' fields are ignored.
 *
 * The 'interrupt_props_num' field contains the number of entries in the
 * 'interrupt_props' array. If this field is non-zero, 'g0_interrupt_num' is
 * ignored.
 ******************************************************************************/
typedef struct gicv2_driver_data {
	uintptr_t gicd_base;
	uintptr_t gicc_base;
	unsigned int *target_masks;
	unsigned int target_masks_num;
	const interrupt_prop_t *interrupt_props;
	unsigned int interrupt_props_num;
} gicv2_driver_data_t;

/*******************************************************************************
 * Function prototypes
 ******************************************************************************/
void gicv2_driver_init(const gicv2_driver_data_t *plat_driver_data);
void gicv2_distif_init(void);
void gicv2_pcpu_distif_init(void);
void gicv2_cpuif_enable(void);
void gicv2_cpuif_disable(void);
unsigned int gicv2_is_fiq_enabled(void);
unsigned int gicv2_get_pending_interrupt_type(void);
unsigned int gicv2_get_pending_interrupt_id(void);
unsigned int gicv2_acknowledge_interrupt(void);
void gicv2_end_of_interrupt(unsigned int id);
unsigned int gicv2_get_interrupt_group(unsigned int id);
unsigned int gicv2_get_running_priority(void);
void gicv2_set_pe_target_mask(unsigned int proc_num);
unsigned int gicv2_get_interrupt_active(unsigned int id);
void gicv2_enable_interrupt(unsigned int id);
void gicv2_disable_interrupt(unsigned int id);
void gicv2_set_interrupt_priority(unsigned int id, unsigned int priority);
void gicv2_set_interrupt_type(unsigned int id, unsigned int type);
void gicv2_raise_sgi(int sgi_num, bool ns, int proc_num);
void gicv2_set_spi_routing(unsigned int id, int proc_num);
void gicv2_set_interrupt_pending(unsigned int id);
void gicv2_clear_interrupt_pending(unsigned int id);
unsigned int gicv2_set_pmr(unsigned int mask);
void gicv2_interrupt_set_cfg(unsigned int id, unsigned int cfg);

/* Use GICV2_INTR_NUM to reduce the size of the GICV2 context, for value 0 use default */
#if GICV2_INTR_NUM == 0
#define TOTAL_SHARED_INTR_NUM TOTAL_SPI_INTR_NUM
#else
#define TOTAL_SHARED_INTR_NUM (GICV2_INTR_NUM - MIN_SPI_ID)
#endif

/*
 * This macro returns the total number of GICD registers corresponding to
 * the register name
 */
#define GICD_NUM_REGS(reg_name)	\
	DIV_ROUND_UP_2EVAL(TOTAL_SHARED_INTR_NUM, (1U << reg_name##_SHIFT))

typedef struct gicv2_dist_ctx {
	/* 32 bits registers */
	uint32_t gicd_ctlr;
	uint32_t gicd_igroupr[GICD_NUM_REGS(IGROUPR)];
	uint32_t gicd_isenabler[GICD_NUM_REGS(ISENABLER)];
	uint32_t gicd_ispendr[GICD_NUM_REGS(ISPENDR)];
	uint32_t gicd_isactiver[GICD_NUM_REGS(ISACTIVER)];
	uint32_t gicd_ipriorityr[GICD_NUM_REGS(IPRIORITYR)];
	uint32_t gicd_itargetsr[GICD_NUM_REGS(ITARGETSR)];
	uint32_t gicd_icfgr[GICD_NUM_REGS(ICFGR)];
} gicv2_dist_ctx_t;

void gicv2_distif_restore(const gicv2_dist_ctx_t * const dist_ctx);
void gicv2_distif_save(gicv2_dist_ctx_t * const dist_ctx);

#endif /* __ASSEMBLER__ */
#endif /* GICV2_H */
