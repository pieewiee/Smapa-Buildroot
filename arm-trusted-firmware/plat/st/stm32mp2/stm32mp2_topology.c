/*
 * Copyright (c) 2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#include <lib/psci/psci.h>
#include <plat/common/platform.h>

static const unsigned char stm32mp2_power_domain_tree_desc[PLAT_MAX_PWR_LVL + 1U] = {
	1U, /* SoC,  D2 LPLV */
	1U, /* D2 */
	1U, /* D1 LPLV */
	PLATFORM_CLUSTER_COUNT, /* D1 */
	PLATFORM_CORE_COUNT, /* Number of children for the cluster node */
};

/* This function returns the platform topology */
const unsigned char *plat_get_power_domain_tree_desc(void)
{
	return stm32mp2_power_domain_tree_desc;
}

/*******************************************************************************
 * This function implements a part of the critical interface between the psci
 * generic layer and the platform that allows the former to query the platform
 * to convert an MPIDR to a unique linear index. An error code (-1) is returned
 * in case the MPIDR is invalid.
 ******************************************************************************/
int plat_core_pos_by_mpidr(u_register_t mpidr)
{
	unsigned int cluster_id, cpu_id;
	u_register_t mpidr_copy = mpidr;

	mpidr_copy &= MPIDR_AFFINITY_MASK;

	if ((mpidr_copy & ~(MPIDR_CLUSTER_MASK | MPIDR_CPU_MASK)) != 0U) {
		return -1;
	}

	cluster_id = (mpidr_copy >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK;
	cpu_id = (mpidr_copy >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK;

	if (cluster_id >= PLATFORM_CLUSTER_COUNT) {
		return -1;
	}

	/*
	 * Validate cpu_id by checking whether it represents a CPU in one
	 * of the two clusters present on the platform.
	 */
	if (cpu_id >= PLATFORM_CORE_COUNT) {
		return -1;
	}

	return (int)cpu_id;
}
