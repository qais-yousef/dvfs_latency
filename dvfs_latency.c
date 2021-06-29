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
static u64 cycles;
static u64 counter;

static struct task_struct *thread;
static struct kobject *dvfs_latency_kobj;

#define dvfs_info(args...)	pr_info("dvfs_lateny: " args)
#define dvfs_err(args...)	pr_err("dvfs_lateny: " args)


static void cycles_overflow(struct perf_event *event,
			    struct perf_sample_data *data,
			    struct pt_regs *regs)
{
	cycles += sizeof(unsigned int);
}

static int setup_perf_event(void)
{
	cycle_counter = perf_event_create_kernel_counter(&cycle_counter_attr,
							 cpu, NULL,
							 cycles_overflow,
							 NULL);
	if (IS_ERR(cycle_counter)) {
		dvfs_err("Failed to allocate perf counter\n");
		return PTR_ERR(cycle_counter);
	}

	dvfs_info("Starting @CPU%d\n", cpu);
	dvfs_info("runtime: %lldus\n", runtime);
	dvfs_info("period: %lldus\n", period);
	dvfs_info("duration: %lldus\n", duration);

	perf_event_enable(cycle_counter);

	cycles = 0;

	return 0;
}

static void cleanup_perf_event(void)
{
	dvfs_info("Stopped. CPU%d cycle counter = %llu\n", cpu, cycles);
	perf_event_disable(cycle_counter);
	perf_event_release_kernel(cycle_counter);
}

static void read_perf_event(void)
{
	u64 enabled, running;
	cycles += perf_event_read_value(cycle_counter, &enabled, &running);
}

static int dvfs_latency_thread(void *data)
{
	unsigned long sleeptime = period - runtime;
	ktime_t begin, t1, t2;
	s64 delta;
	int ret;

	ret = setup_perf_event();
	if (ret)
		return ret;

	counter = 0;
	begin = t1 = ktime_get();
	while (true) {
		counter++;
		read_perf_event();

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
	thread = kthread_create_on_node(dvfs_latency_thread,
					NULL, cpu_to_node(cpu),
					"dvfs_latency");

	if (IS_ERR(thread)) {
		dvfs_err("Failed to create test thread\n");
		return PTR_ERR(thread);
	}

#if 1
	sched_set_fifo(thread);
#else
	{
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wincompatible-pointer-types"
		#include <linux/sched/types.h>
		const struct sched_param sp = { .sched_priority = MAX_RT_PRIO / 2 };
		sched_setscheduler_nocheck(thread, SCHED_FIFO, &sp);
		#pragma clang diagnostic pop
	}
#endif

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

static ssize_t runtime_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%lld\n", runtime);
}
static ssize_t runtime_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	s64 temp;
	int ret;

	ret = kstrtos64(buf, 10, &temp);
	if (ret)
		return ret;

	if (temp > period)
		return -EINVAL;

	runtime = temp;

	return count;
}
static struct kobj_attribute runtime_attribute = __ATTR_RW(runtime);

static ssize_t cycles_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	while (start)
		msleep(500);

	return sprintf(buf, "%llu\n", cycles);
}
static struct kobj_attribute cycles_attribute = __ATTR_RO(cycles);

static ssize_t counter_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	while (start)
		msleep(500);

	return sprintf(buf, "%llu\n", counter);
}
static struct kobj_attribute counter_attribute = __ATTR_RO(counter);

static int dvfs_latency_init(void)
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

	ret = sysfs_create_file(dvfs_latency_kobj, &runtime_attribute.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(dvfs_latency_kobj, &cycles_attribute.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(dvfs_latency_kobj, &counter_attribute.attr);
	if (ret)
		return ret;

	return 0;
}

static void dvfs_latency_finish(void)
{
	kobject_put(dvfs_latency_kobj);
}


module_init(dvfs_latency_init);
module_exit(dvfs_latency_finish);

MODULE_LICENSE("GPL");
