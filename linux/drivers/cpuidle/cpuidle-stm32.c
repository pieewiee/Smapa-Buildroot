// SPDX-License-Identifier: GPL-2.0
// Copyright (C) STMicroelectronics 2019
// Author:

#include <linux/arm-smccc.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/tick.h>

#include <asm/cpuidle.h>

#include "dt_idle_states.h"

#define SMC_AUTOSTOP()				\
{								\
	struct arm_smccc_res res;				\
	arm_smccc_smc(0x8200100a, 0, 0, 0,			\
		      0, 0, 0, 0, &res);			\
}

struct stm32_pm_domain {
	struct device *dev;
	struct generic_pm_domain genpd;
	int id;
};

static atomic_t stm_idle_barrier;

static int stm32_enter_idle(struct cpuidle_device *dev,
			    struct cpuidle_driver *drv, int index)
{
	/*
	 * Call idle CPU PM enter notifier chain so that
	 * VFP and per CPU interrupt context is saved.
	 */
	cpu_pm_enter();

	/*
	 * be sure that both cpu enter at the same time
	 * normally not needed is the state is declared as coupled
	 */
	cpuidle_coupled_parallel_barrier(dev, &stm_idle_barrier);

	/* Enter broadcast mode for periodic timers */
	tick_broadcast_enable();

	/* Enter broadcast mode for one-shot timers */
	tick_broadcast_enter();

	if (dev->cpu == 0)
		cpu_cluster_pm_enter();

	SMC_AUTOSTOP();

	if (dev->cpu == 0)
		cpu_cluster_pm_exit();

	tick_broadcast_exit();

	cpuidle_coupled_parallel_barrier(dev, &stm_idle_barrier);

	/*
	 * Call idle CPU PM exit notifier chain to restore
	 * VFP and per CPU IRQ context.
	 */
	cpu_pm_exit();

	return index;
}

static const struct of_device_id stm32_idle_state_match[] = {
	{ .compatible = "arm,idle-state",
	  .data = stm32_enter_idle },
	{ },
};

static struct cpuidle_driver stm32_idle_driver = {
	.name = "stm32_idle",
	.states = {
		ARM_CPUIDLE_WFI_STATE,
		{
			.enter		  = stm32_enter_idle,
			.exit_latency	  = 620,
			.target_residency = 700,
			.flags		  = /*CPUIDLE_FLAG_TIMER_STOP | */
						CPUIDLE_FLAG_COUPLED,
			.name		  = "CStop",
			.desc		  = "Clocks off",
		},
	},
	.safe_state_index = 0,
	.state_count = 2,
};

static int stm32_pd_cpuidle_off(struct generic_pm_domain *domain)
{
	struct stm32_pm_domain *priv = container_of(domain,
						    struct stm32_pm_domain,
						    genpd);
	int cpu;

	for_each_possible_cpu(cpu) {
		struct cpuidle_device *cpuidle_dev = per_cpu(cpuidle_devices,
							     cpu);

		cpuidle_dev->states_usage[1].disable = false;
	}

	dev_dbg(priv->dev, "%s OFF\n", domain->name);

	return 0;
}

static int stm32_pd_cpuidle_on(struct generic_pm_domain *domain)
{
	struct stm32_pm_domain *priv = container_of(domain,
						    struct stm32_pm_domain,
						    genpd);
	int cpu;

	for_each_possible_cpu(cpu) {
		struct cpuidle_device *cpuidle_dev = per_cpu(cpuidle_devices,
							     cpu);

		cpuidle_dev->states_usage[1].disable = true;
	}

	dev_dbg(priv->dev, "%s ON\n", domain->name);

	return 0;
}

static void stm32_cpuidle_domain_remove(struct stm32_pm_domain *domain)
{
	int ret;

	ret = pm_genpd_remove(&domain->genpd);
	if (ret)
		dev_err(domain->dev, "failed to remove PM domain %s: %d\n",
			domain->genpd.name, ret);
}

static int stm32_cpuidle_domain_add(struct stm32_pm_domain *domain,
				    struct device *dev,
				    struct device_node *np)
{
	int ret;

	domain->dev = dev;
	domain->genpd.name = np->name;
	domain->genpd.power_off = stm32_pd_cpuidle_off;
	domain->genpd.power_on = stm32_pd_cpuidle_on;

	ret = pm_genpd_init(&domain->genpd, NULL, 0);
	if (ret < 0) {
		dev_err(domain->dev, "failed to initialise PM domain %s: %d\n",
			np->name, ret);
		return ret;
	}

	ret = of_genpd_add_provider_simple(np, &domain->genpd);
	if (ret < 0) {
		dev_err(domain->dev, "failed to register PM domain %s: %d\n",
			np->name, ret);
		stm32_cpuidle_domain_remove(domain);
		return ret;
	}

	dev_info(domain->dev, "domain %s registered\n", np->name);

	return 0;
}

static int stm32_cpuidle_probe(struct platform_device *pdev)
{
	struct cpuidle_driver *drv;
	struct stm32_pm_domain *domain;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct of_phandle_args child, parent;
	struct device_node *np_child;
	int cpu, ret;

	drv = devm_kmemdup(dev, &stm32_idle_driver, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	/* Start at index 1, index 0 standard WFI */
	ret = dt_init_idle_driver(drv, stm32_idle_state_match, 1);
	if (ret < 0)
		return ret;

	/* all the cpus of the system are coupled */
	ret = cpuidle_register(drv, cpu_possible_mask);
	if (ret)
		return ret;

	/* Declare cpuidle domain */
	domain = devm_kzalloc(dev, sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	ret = stm32_cpuidle_domain_add(domain, dev, np);
	if (ret) {
		devm_kfree(dev, domain);
		return ret;
	}

	/* disable cpu idle */
	for_each_possible_cpu(cpu) {
		struct cpuidle_device *cpuidle_dev = per_cpu(cpuidle_devices,
							     cpu);

		cpuidle_dev->states_usage[1].disable = true;
	}

	/*  link main cpuidle domain to consumer domain */
	for_each_child_of_node(np, np_child) {
		if (!of_parse_phandle_with_args(np_child, "power-domains",
						"#power-domain-cells",
						0, &child)) {
			struct device_node *np_test = child.np;

			parent.np = np;
			parent.args_count = 0;

			ret = of_genpd_add_subdomain(&parent, &child);
			if (ret < 0)
				dev_err(dev, "failed to add Sub PM domain %d\n",
					ret);

			dev_dbg(dev, "%s, add sub cpuidle of %s, with child %s\n",
				__func__, np->name, np_test->name);

			pm_runtime_put(dev);
		}
	}

	dev_info(dev, "cpuidle domain probed\n");

	return 0;
}

int stm32_cpuidle_remove(struct platform_device *pdev)
{
	cpuidle_unregister(&stm32_idle_driver);
	return 0;
}

static const struct of_device_id stm32_cpuidle_of_match[] = {
	{
		.compatible = "stm32,cpuidle",
	},
	{ /* sentinel */ },
};

static struct platform_driver stm32_cpuidle_driver = {
	.probe = stm32_cpuidle_probe,
	.remove = stm32_cpuidle_remove,
	.driver = {
		   .name = "stm32_cpuidle",
		   .owner = THIS_MODULE,
		   .of_match_table = stm32_cpuidle_of_match,
		   },
};

module_platform_driver(stm32_cpuidle_driver);

MODULE_AUTHOR("<>");
MODULE_DESCRIPTION("STM32 cpu idle driver");
MODULE_LICENSE("GPL v2");
