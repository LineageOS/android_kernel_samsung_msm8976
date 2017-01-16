/*
 * drivers/cpufreq/cpufreq_limit.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *	Minsung Kim <ms925.kim@samsung.com>
 *	Chiwoong Byun <woong.byun@samsung.com>
 *	 - 2014/10/24 Add HMP feature to support HMP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/cpu.h>
#ifdef CONFIG_SCHED_HMP
#include <linux/sched.h>
#endif

struct cpufreq_limit_handle {
	struct list_head node;
	unsigned long min;
	unsigned long max;
	char label[20];
};

static DEFINE_MUTEX(cpufreq_limit_lock);
static LIST_HEAD(cpufreq_limit_requests);

/**
 * cpufreq_limit_get - limit min_freq or max_freq, return cpufreq_limit_handle
 * @min_freq	limit minimum frequency (0: none)
 * @max_freq	limit maximum frequency (0: none)
 * @label	a literal description string of this request
 */
struct cpufreq_limit_handle *cpufreq_limit_get(unsigned long min_freq,
		unsigned long max_freq, char *label)
{
	struct cpufreq_limit_handle *handle;
	int i;

	if (max_freq && max_freq < min_freq)
		return ERR_PTR(-EINVAL);

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	handle->min = min_freq;
	handle->max = max_freq;

	if (strlen(label) < sizeof(handle->label))
		strcpy(handle->label, label);
	else
		strncpy(handle->label, label, sizeof(handle->label) - 1);

	pr_debug("%s: %s,%lu,%lu\n", __func__, handle->label, handle->min,
			handle->max);

	mutex_lock(&cpufreq_limit_lock);
	list_add_tail(&handle->node, &cpufreq_limit_requests);
	mutex_unlock(&cpufreq_limit_lock);

	/* Re-evaluate policy to trigger adjust notifier for online CPUs */
	get_online_cpus();

	for_each_online_cpu(i)
		cpufreq_update_policy(i);

	put_online_cpus();

	return handle;
}

/**
 * cpufreq_limit_put - release of a limit of min_freq or max_freq, free
 *			a cpufreq_limit_handle
 * @handle	a cpufreq_limit_handle that has been requested
 */
int cpufreq_limit_put(struct cpufreq_limit_handle *handle, int release)
{
	int i;

	if (handle == NULL || IS_ERR(handle))
		return -EINVAL;

	pr_debug("%s: %s,%lu,%lu\n", __func__, handle->label, handle->min,
			handle->max);

	mutex_lock(&cpufreq_limit_lock);
	list_del(&handle->node);
	mutex_unlock(&cpufreq_limit_lock);

	get_online_cpus();
	for_each_online_cpu(i)
		cpufreq_update_policy(i);
	put_online_cpus();

	if (release && handle->min) { /* min limit */
		for_each_online_cpu(i)
			atomic_notifier_call_chain(&load_alert_notifier_head, 0, (void *)(long)i);
	}

	kfree(handle);
	return 0;
}

#ifdef CONFIG_SCHED_HMP
struct cpufreq_limit_hmp {
	unsigned int		little_cpu_start;
	unsigned int		little_cpu_end;
	unsigned int		big_cpu_start;
	unsigned int		big_cpu_end;
	unsigned long		big_min_freq;
	unsigned long		big_max_freq;
	unsigned long		big_min_lock;
	unsigned long		little_min_freq;
	unsigned long		little_max_freq;
	unsigned long		little_min_lock;
	unsigned int		little_divider;
	unsigned int		hmp_boost_type;
	unsigned int		hmp_boost_active;
};

struct cpufreq_limit_hmp hmp_param = {
#if defined(CONFIG_ARCH_MSM8952)
	.little_cpu_start		= 4,
	.little_cpu_end			= 7,
	.big_cpu_start			= 0,
	.big_cpu_end			= 3,
	.big_min_freq			= 960000,
	.big_max_freq			= 1516800,
	.big_min_lock			= 499200,
	.little_min_freq		= 201600, // 403200 Khz
	.little_max_freq		= 604800, // 1209600 Khz
	.little_min_lock		= 403200, // 806400 Khz
#else
	.little_cpu_start		= 0,
	.little_cpu_end			= 3,
	.big_cpu_start			= 4,
	.big_cpu_end			= 7,
	.big_min_freq			= 883200,
	.big_max_freq			= 1747200,
	.big_min_lock			= 883200,
	.little_min_freq		= 345600, // 691200 Khz
	.little_max_freq		= 700800, // 1401600 Khz
	.little_min_lock		= 403200, // 806400 Khz
#endif
	.little_divider			= 2,
	.hmp_boost_type			= 1,
	.hmp_boost_active		= 0,
};

/**
 * cpufreq_limit_get_table - fill the cpufreq table to support HMP
 * @buf		a buf that has been requested to fill the cpufreq table
 */
ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
#if defined(CONFIG_ARCH_MSM8952)
	len += sprintf(buf + len, "1516800 1440000 1344000 1094400 960000 604800 547200 499200 403200 201600\n");
#else
	int i, count = 0;

	struct cpufreq_frequency_table *table;

	/* BIG cluster table */
	table = cpufreq_frequency_get_table(hmp_param.big_cpu_start);
	if (table == NULL) {
		pr_err("Big cpu frequencies not exist\n");
		return 0;
	}

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = count; i >= 0; i--) {
		unsigned int freq;
		freq = table[i].frequency;

		if (freq == CPUFREQ_ENTRY_INVALID || freq < hmp_param.big_min_freq)
			continue;

		len += sprintf(buf + len, "%u ", freq);
	}

	/* Little cluster table */
	table = cpufreq_frequency_get_table(hmp_param.little_cpu_start);
	if (table == NULL) {
		pr_err("Little cpu frequencies not exist\n");
		return 0;
	}

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = count; i >= 0; i--) {
		unsigned int freq;
		freq = table[i].frequency / hmp_param.little_divider;

		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		len += sprintf(buf + len, "%u ", freq);
	}
	len--;
	len += sprintf(buf + len, "\n");
#endif
	return len;
}

static inline int is_little(unsigned int cpu)
{
	return cpu >= hmp_param.little_cpu_start &&
			cpu <= hmp_param.little_cpu_end;
}

static int set_little_divider(struct cpufreq_policy *policy, unsigned long *v)
{
	if (is_little(policy->cpu))
		*v /= hmp_param.little_divider;

	return 0;
}

static int cpufreq_limit_hmp_boost(int enable)
{
	unsigned int ret = 0;

	pr_debug("%s: enable=%d, type=%d, active=%d\n", __func__,
		enable, hmp_param.hmp_boost_type, hmp_param.hmp_boost_active);

	if (enable) {
		if (hmp_param.hmp_boost_type && !hmp_param.hmp_boost_active) {
			hmp_param.hmp_boost_active = enable;
			ret = sched_set_boost(1);
			if (ret)
				pr_err("%s: HMP boost enable failed\n", __func__);
		}
	}
	else {
		if (hmp_param.hmp_boost_type && hmp_param.hmp_boost_active) {
			hmp_param.hmp_boost_active = 0;
			ret = sched_set_boost(0);
			if (ret)
				pr_err("%s: HMP boost disable failed\n", __func__);
		}
	}

	return ret;
}

static int cpufreq_limit_adjust_freq(struct cpufreq_policy *policy,
		unsigned long *min, unsigned long *max)
{

	pr_debug("%s+: cpu=%d, min=%ld, max=%ld\n", __func__, policy->cpu, *min, *max);

	if (is_little(policy->cpu)) { /* Little */
		if (*min >= hmp_param.big_min_freq) { /* Big clock */
			*min = hmp_param.little_min_lock * hmp_param.little_divider;
		}
		else { /* Little clock */
			*min *= hmp_param.little_divider;
		}

		if (*max >= hmp_param.big_min_freq) { /* Big clock */
			*max = policy->cpuinfo.max_freq;
		}
		else { /* Little clock */
			*max *= hmp_param.little_divider;
		}
	}
	else { /* BIG */
		unsigned int hmp_boost_active = 0;
		if (*min >= hmp_param.big_min_freq) { /* Big clock */
			hmp_boost_active = 1;
		}
		else { /* Little clock */
			*min = hmp_param.big_min_lock;
			hmp_boost_active = 0;
		}

		if (*max >= hmp_param.big_min_freq) { /* Big clock */
			pr_debug("%s: big_min_freq=%ld, max=%ld\n", __func__,
				hmp_param.big_min_freq, *max);
		}
		else { /* Little clock */
			*max = hmp_param.big_min_lock;
			hmp_boost_active = 0;
		}
		cpufreq_limit_hmp_boost(hmp_boost_active);
	}

	pr_debug("%s-: cpu=%d, min=%ld, max=%ld\n", __func__, policy->cpu, *min, *max);

	return 0;
}
#else
static inline int cpufreq_limit_adjust_freq(struct cpufreq_policy *policy,
		unsigned long *min, unsigned long *max) { return 0; }
static inline int cpufreq_limit_hmp_boost(int enable) { return 0; }
static inline int set_little_divider(struct cpufreq_policy *policy,
		unsigned long *v) { return 0; }
#endif /* CONFIG_SCHED_HMP */

static int cpufreq_limit_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpufreq_limit_handle *handle;
	unsigned long min = 0, max = ULONG_MAX;

	if (val != CPUFREQ_ADJUST)
		goto done;

	mutex_lock(&cpufreq_limit_lock);
	list_for_each_entry(handle, &cpufreq_limit_requests, node) {
		if (handle->min > min)
			min = handle->min;
		if (handle->max && handle->max < max)
			max = handle->max;
	}

#ifdef CONFIG_SEC_PM
	pr_debug("CPUFREQ(%d): %s: umin=%d,umax=%d\n",
		policy->cpu, __func__, policy->user_policy.min, policy->user_policy.max);

#ifndef CONFIG_SCHED_HMP /* TODO */
	if (policy->user_policy.min > min)
		min = policy->user_policy.min;
	if (policy->user_policy.max && policy->user_policy.max < max)
		max = policy->user_policy.max;
#endif
#endif

	mutex_unlock(&cpufreq_limit_lock);

#ifdef CONFIG_SCHED_HMP
	if (!min && max == ULONG_MAX) {
		cpufreq_limit_hmp_boost(0);
		goto done;
	}

	if (!min) {
		min = policy->cpuinfo.min_freq;
		set_little_divider(policy, &min);
	}
	if (max == ULONG_MAX) {
		max = policy->cpuinfo.max_freq;
		set_little_divider(policy, &max);
	}

	cpufreq_limit_adjust_freq(policy, &min, &max);

	pr_debug("%s: limiting cpu%d cpufreq to %lu-%lu\n", __func__,
			policy->cpu, min, max);
#else
	if (!min && max == ULONG_MAX)
		goto done;

	if (!min)
		min = policy->cpuinfo.min_freq;
	if (max == ULONG_MAX)
		max = policy->cpuinfo.max_freq;

	pr_debug("%s: limiting cpu%d cpufreq to %lu,%lu", __func__,
			policy->cpu, min, max);
#endif

	cpufreq_verify_within_limits(policy, min, max);
done:
	return 0;

}

static struct notifier_block notifier_policy_block = {
	.notifier_call = cpufreq_limit_notifier_policy
};

/************************** sysfs begin ************************/
static ssize_t show_cpufreq_limit_requests(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct cpufreq_limit_handle *handle;
	ssize_t len = 0;

	mutex_lock(&cpufreq_limit_lock);
	list_for_each_entry(handle, &cpufreq_limit_requests, node) {
		len += sprintf(buf + len, "%s\t%lu\t%lu\n", handle->label,
				handle->min, handle->max);
	}
	mutex_unlock(&cpufreq_limit_lock);

	return len;
}

static struct global_attr cpufreq_limit_requests_attr =
	__ATTR(cpufreq_limit_requests, 0444, show_cpufreq_limit_requests, NULL);

#ifdef CONFIG_SCHED_HMP
#define MAX_ATTRIBUTE_NUM 12

#define show_one(file_name, object)									\
static ssize_t show_##file_name										\
(struct kobject *kobj, struct attribute *attr, char *buf)			\
{																	\
	return sprintf(buf, "%u\n", hmp_param.object);					\
}

#define show_one_ulong(file_name, object)							\
static ssize_t show_##file_name										\
(struct kobject *kobj, struct attribute *attr, char *buf)			\
{																	\
	return sprintf(buf, "%lu\n", hmp_param.object);					\
}

#define store_one(file_name, object)								\
static ssize_t store_##file_name									\
(struct kobject *a, struct attribute *b, const char *buf, size_t count)		\
{																	\
	int ret;														\
																	\
	ret = sscanf(buf, "%lu", &hmp_param.object);					\
	if (ret != 1)													\
		return -EINVAL;												\
																	\
	return count;													\
}

static ssize_t show_little_cpu_num(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u-%u\n", hmp_param.little_cpu_start, hmp_param.little_cpu_end);
}

static ssize_t show_big_cpu_num(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u-%u\n", hmp_param.big_cpu_start, hmp_param.big_cpu_end);
}

show_one_ulong(big_min_freq, big_min_freq);
show_one_ulong(big_max_freq, big_max_freq);
show_one_ulong(big_min_lock, big_min_lock);
show_one_ulong(little_min_freq, little_min_freq);
show_one_ulong(little_max_freq, little_max_freq);
show_one_ulong(little_min_lock, little_min_lock);
show_one(little_divider, little_divider);
show_one(hmp_boost_type, hmp_boost_type);

static ssize_t store_little_cpu_num(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input, input2;
	int ret;

	ret = sscanf(buf, "%u-%u", &input, &input2);
	if (ret != 2)
		return -EINVAL;

	if (input >= MAX_ATTRIBUTE_NUM || input2 >= MAX_ATTRIBUTE_NUM)
		return -EINVAL;

	pr_info("%s: %u-%u, ret=%d\n", __func__, input, input2, ret);

	hmp_param.little_cpu_start = input;
	hmp_param.little_cpu_end = input2;

	return count;
}

static ssize_t store_big_cpu_num(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input, input2;
	int ret;

	ret = sscanf(buf, "%u-%u", &input, &input2);
	if (ret != 2)
		return -EINVAL;

	if (input >= MAX_ATTRIBUTE_NUM || input2 >= MAX_ATTRIBUTE_NUM)
		return -EINVAL;

	pr_info("%s: %u-%u, ret=%d\n", __func__, input, input2, ret);

	hmp_param.big_cpu_start = input;
	hmp_param.big_cpu_end = input2;

	return count;
}

store_one(big_min_freq, big_min_freq);
store_one(big_max_freq, big_max_freq);
store_one(big_min_lock, big_min_lock);
store_one(little_min_freq, little_min_freq);
store_one(little_max_freq, little_max_freq);
store_one(little_min_lock, little_min_lock);

static ssize_t store_little_divider(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input >= MAX_ATTRIBUTE_NUM)
		return -EINVAL;

	hmp_param.little_divider = input;

	return count;
}

static ssize_t store_hmp_boost_type(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 2)
		return -EINVAL;

	hmp_param.hmp_boost_type = input;

	return count;
}

define_one_global_rw(little_cpu_num);
define_one_global_rw(big_cpu_num);
define_one_global_rw(big_min_freq);
define_one_global_rw(big_max_freq);
define_one_global_rw(big_min_lock);
define_one_global_rw(little_min_freq);
define_one_global_rw(little_max_freq);
define_one_global_rw(little_min_lock);
define_one_global_rw(little_divider);
define_one_global_rw(hmp_boost_type);
#endif /* CONFIG_SCHED_HMP */

static struct attribute *limit_attributes[] = {
	&cpufreq_limit_requests_attr.attr,
#ifdef CONFIG_SCHED_HMP
	&little_cpu_num.attr,
	&big_cpu_num.attr,
	&big_min_freq.attr,
	&big_max_freq.attr,
	&big_min_lock.attr,
	&little_min_freq.attr,
	&little_max_freq.attr,
	&little_min_lock.attr,
	&little_divider.attr,
	&hmp_boost_type.attr,
#endif
	NULL,
};

static struct attribute_group limit_attr_group = {
	.attrs = limit_attributes,
	.name = "cpufreq_limit",
};
/************************** sysfs end ************************/

static int __init cpufreq_limit_init(void)
{
	int ret;

	ret = cpufreq_register_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		return ret;

	ret = cpufreq_get_global_kobject();

	if (!ret) {
		ret = sysfs_create_group(cpufreq_global_kobject,
				&limit_attr_group);
		if (ret)
			cpufreq_put_global_kobject();
	}

	return ret;
}

static void __exit cpufreq_limit_exit(void)
{
	cpufreq_unregister_notifier(&notifier_policy_block,
			CPUFREQ_POLICY_NOTIFIER);

	sysfs_remove_group(cpufreq_global_kobject, &limit_attr_group);
	cpufreq_put_global_kobject();
}

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("'cpufreq_limit' - A driver to limit cpu frequency");
MODULE_LICENSE("GPL");

module_init(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);
