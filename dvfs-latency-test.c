/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/delay.h>
#include <linux/kobject.h>
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
static int start;
static s64 period = 20 * USEC_PER_MSEC;
static s64 runtime = 500;
static s64 duration = 1 * USEC_PER_SEC;

static struct task_struct *thread;
static struct kobject *dvfs_latency_kobj;


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
	unsigned long sleeptime = period - runtime;
	ktime_t begin, t1, t2;
	s64 delta;
	int ret;

	ret = setup_perf_event();
	if (ret)
		return ret;

	begin = t1 = ktime_get();
	while (true) {
		t2 = ktime_get();

		delta = ktime_us_delta(t2, begin);
		if (delta > duration)
			break;

		delta = ktime_us_delta(t2, t1);
		if (delta < runtime)
			continue;

		usleep_range(sleeptime, sleeptime);
		t1 = ktime_get();
	}

	cleanup_perf_event();

	start = 0;

	return 0;
}

static int dvfs_latency_start(void)
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

static ssize_t cpu_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", cpu);
}
static ssize_t cpu_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &cpu);
	if (ret)
		return ret;

	return count;
}
static struct kobj_attribute cpu_attribute = __ATTR_RW(cpu);

static ssize_t start_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", start);
}
static ssize_t start_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	if (start)
		return -EBUSY;

	start = 1;
	dvfs_latency_start();

	return count;
}
static struct kobj_attribute start_attribute = __ATTR_RW(start);

static int dvfs_latency_test_init(void)
{
	int ret;

	dvfs_latency_kobj = kobject_create_and_add("dvfs_latency", kernel_kobj);
	if (!dvfs_latency_kobj)
		return -ENOMEM;

	ret = sysfs_create_file(dvfs_latency_kobj, &cpu_attribute.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(dvfs_latency_kobj, &start_attribute.attr);
	if (ret)
		return ret;

	return 0;
}

static void dvfs_latency_test_finish(void)
{
	kobject_put(dvfs_latency_kobj);
}


module_init(dvfs_latency_test_init);
module_exit(dvfs_latency_test_finish);

MODULE_LICENSE("GPL");
