#!/bin/sh

MODULE=dvfs_latency.ko

LOADED=$(lsmod | grep dvfs_latency)
REMOVE=0

SYSFS_CPUFREQ=/sys/devices/system/cpu/cpufreq/

SYSFS_CPU=/sys/kernel/dvfs_latency/cpu
SYSFS_RUNTIME=/sys/kernel/dvfs_latency/runtime
SYSFS_START=/sys/kernel/dvfs_latency/start
SYSFS_CYCLES=/sys/kernel/dvfs_latency/cycles
SYSFS_COUNTER=/sys/kernel/dvfs_latency/counter


#
# Init
#
if [ -z "$LOADED" ]; then
	insmod $MODULE
	REMOVE=1
fi

#
# Detect which policy to act on
#
for p in $(ls $SYSFS_CPUFREQ)
do
	POLICY=$SYSFS_CPUFREQ/$p
done
echo "Tetsing policy: $POLICY"

#
# Detect and set which cpus to operate on
#
for cpu in $(cat $POLICY/affected_cpus)
do
	if [ -e $cpus ]; then
		cpus=$cpu
		echo $cpu > $SYSFS_CPU
	else
		cpus="$cpus,$cpu"
	fi
done
echo "Affected cpus: $cpus"
echo "Testing cpu: $(cat $SYSFS_CPU)"

#
# Setup exclusive cpuset
#
CGROUP_DIR=$(mount | grep cpuset)
CPUSET_DIR=cpuset
UNMOUNT_CPUSET=0
if [ -z "$CGROUP_DIR" ]; then
	mkdir $CPUSET_DIR
	mount -t cgroup -o cpuset none $CPUSET_DIR
	UNMOUNT_CPUSET=1
else
	CPUSET_DIR=$(mount | grep cpuset | awk '{print $3}')
	echo "cpuset already mounted on: $CPUSET_DIR"
fi
cd $CPUSET_DIR
mkdir tg
echo "$cpus" > tg/cpuset.cpus
echo 1 > tg/cpuset.cpu_exclusive

echo "cpuset cpus: $(cat tg/cpuset.cpus)"
echo "cpuset exclusive: $(cat tg/cpuset.cpu_exclusive)"
cd -

#
# Remember schedutil rate limit then reset it
#
SCHEDUTIL_RLIMIT=$(cat $POLICY/schedutil/rate_limit_us)
echo "Original schedutil rate limit: $(cat $POLICY/schedutil/rate_limit_us) us"
echo 0 > $POLICY/schedutil/rate_limit_us
echo "New schedutil rate limit: $(cat $POLICY/schedutil/rate_limit_us) us"

#
# Measure latency for 100, 500, 1000 and 10000 us
#
for v in 100 200 300 400 500 1000 2000 3000 4000 5000 10000 20000
do
	echo "Testing $v us..."
	echo $v > $SYSFS_RUNTIME

	#
	# Measure latency with performance governor
	#
	echo performance > $POLICY/scaling_governor
	sleep 1
	echo 1 > $SYSFS_START

	perf_cycles=$(cat $SYSFS_CYCLES)
	perf_counter=$(cat $SYSFS_COUNTER)
	echo -e "\tperformance gov: $perf_cycles\t$perf_counter"

	#
	# Measure latency with schedutil governor
	#
	echo schedutil > $POLICY/scaling_governor
	sleep 1
	echo 1 > $SYSFS_START

	sched_cycles=$(cat $SYSFS_CYCLES)
	sched_counter=$(cat $SYSFS_COUNTER)
	echo -e "\tschedutil gov:   $sched_cycles\t$sched_counter"

	cycles_ratio=$(echo "scale=4; $perf_cycles/$sched_cycles" | bc)
	counter_ratio=$(echo "scale=4; $perf_counter/$sched_counter" | bc)
	echo -e "\tratio:           $cycles_ratio    \t$counter_ratio"
done

echo "Done!"

#
# Cleanup
#
echo $SCHEDUTIL_RLIMIT > $POLICY/schedutil/rate_limit_us
echo "Restored schedutil rate limit: $(cat $POLICY/schedutil/rate_limit_us) us"
if [ $REMOVE -eq 1 ]; then
	rmmod $MODULE
fi
if [ $UNMOUNT_CPUSET -eq 1 ]; then
	umount $CPUSET_DIR
	rm -rf $CPUSET_DIR/tg
	rm -rf $CPUSET_DIR
fi
