/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/module.h>
#include <linux/perf_event.h>


static struct perf_event_attr cycle_counter_attr = {
	.type		= PERF_TYPE_HARDWARE,
	.config		= PERF_COUNT_HW_CPU_CYCLES,
	.size		= sizeof(struct perf_event_attr),
	.pinned		= 1,
	.disabled	= 1,
	.sample_period	= sizeof(unsigned int),
};

static struct perf_event *cycle_counter;

static int cpu;
module_param(cpu, int, 0644);


static int dvfs_latency_test_init(void)
{
	cycle_counter = perf_event_create_kernel_counter(&cycle_counter_attr,
							 cpu, NULL, NULL, NULL);
	if (IS_ERR(cycle_counter)) {
		pr_err("Failed to allocate perf counter\n");
		return PTR_ERR(cycle_counter);
	}

	pr_info("dvfs-latency-test: Starting @CPU%d\n", cpu);
	perf_event_enable(cycle_counter);

	return 0;
}

static void dvfs_latency_test_finish(void)
{
	u64 count, enabled, running;
	count = perf_event_read_value(cycle_counter, &enabled, &running);
	pr_info("dvfs-latency-test: Stopped. CPU%d cycle counter = %llu\n", cpu, count);
	perf_event_disable(cycle_counter);
	perf_event_release_kernel(cycle_counter);
}


module_init(dvfs_latency_test_init);
module_exit(dvfs_latency_test_finish);

MODULE_LICENSE("GPL");
