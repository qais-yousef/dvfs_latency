#!/bin/sh
set -x

MODULE=dvfs_latency.ko

LOADED=$(lsmod | grep dvfs_latency)
REMOVE=0

SYSFS_CPUFREQ=/sys/devices/system/cpu/cpufreq/

SYSFS_CPU=/sys/kernel/dvfs_latency/cpu
SYSFS_RUNTIME=/sys/kernel/dvfs_latency/runtime
SYSFS_START=/sys/kernel/dvfs_latency/start


#
# Init
#
if [ "x" == "x$LOADED" ]; then
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
if [ "x" == "x$CGROUP_DIR" ]; then
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
# Measure latency for 100, 500, 1000 and 10000 us
#
for v in 100 500 1000 10000
do
	echo $v > $SYSFS_RUNTIME

	#
	# Measure latency with performance governor
	#
	echo performance > $POLICY/scaling_governor
	echo 1 > $SYSFS_START

	#
	# Measure latency with schedutil governor
	#
	echo schedutil > $POLICY/scaling_governor
	echo 1 > $SYSFS_START
done

#
# Cleanup
#
if [ $REMOVE -eq 1 ]; then
	rmmod $MODULE
fi
if [ $UNMOUNT_CPUSET -eq 1 ]; then
	umount $CPUSET_DIR
	rm -rf $CPUSET_DIR
fi
