#!/bin/sh

loaded=$(lsmod dvfs_latency)
if [ "x" == "x$loaded" ]; then
	insmod dvfs_latency.ko
fi
