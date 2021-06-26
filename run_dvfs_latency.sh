#!/bin/sh
set -x

MODULE=dvfs_latency.ko

LOADED=$(lsmod | grep dvfs_latency)
REMOVE=0

if [ "x" == "x$LOADED" ]; then
	insmod $MODULE
	REMOVE=1
fi

if [ $REMOVE -eq 1 ]; then
	rmmod $MODULE
fi
