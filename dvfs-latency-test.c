/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/sched.h>


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

static struct task_struct *thread;


static int setup_perf_event(void)
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

static void cleanup_perf_event(void)
{
	u64 count, enabled, running;
	count = perf_event_read_value(cycle_counter, &enabled, &running);

	pr_info("dvfs-latency-test: Stopped. CPU%d cycle counter = %llu\n", cpu, count);
	perf_event_disable(cycle_counter);
	perf_event_release_kernel(cycle_counter);
}

static int dvfs_latency_test_thread(void *data)
{
	int ret, i;
	static volatile int test;

	ret = setup_perf_event();
	if (ret)
		return ret;

	for (i = 0; i < 1000000; i++)
		test = i;

	cleanup_perf_event();

	return 0;
}

static int dvfs_latency_test_init(void)
{
	thread = kthread_create_on_node(dvfs_latency_test_thread,
					NULL, cpu_to_node(cpu),
					"dvfs-latency-test");

	if (IS_ERR(thread)) {
		pr_err("dvfs-latency-test: Failed to create test thread\n");
		return PTR_ERR(thread);
	}

	sched_set_fifo(thread);

	kthread_bind(thread, cpu);
	kthread_park(thread);
	kthread_stop(thread);

	return 0;
}

static void dvfs_latency_test_finish(void)
{
}


module_init(dvfs_latency_test_init);
module_exit(dvfs_latency_test_finish);

MODULE_LICENSE("GPL");
