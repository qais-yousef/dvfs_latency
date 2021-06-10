/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/module.h>


static int dvfs_latency_test_init(void)
{
	return 0;
}

static void dvfs_latency_test_finish(void)
{
}


module_init(dvfs_latency_test_init);
module_exit(dvfs_latency_test_finish);

MODULE_LICENSE("GPL");
