/*
 *  drivers/misc/sec_param.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/qcom/sec_param.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#define PARAM_RD	0
#define PARAM_WR	1

#define SEC_PARAM_FILE_NAME	"/dev/block/bootdevice/by-name/param"	/* parameter block */
#define SEC_PARAM_FILE_SIZE	0xA00000		/* 10MB */
#define SEC_PARAM_FILE_OFFSET (SEC_PARAM_FILE_SIZE - 0x100000)


static DEFINE_MUTEX(sec_param_mutex);

/* single global instance */
struct sec_param_data *param_data;
struct sec_param_data_s sched_sec_param_data;

int sec_param_sysfs_init(void);

static void param_sec_operation(struct work_struct *work)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	struct sec_param_data_s *sched_param_data =
		container_of(work, struct sec_param_data_s, sec_param_work);
	
	int flag = (sched_param_data->direction == PARAM_WR) ? (O_RDWR | O_SYNC) : O_RDONLY;

	pr_debug("%s %p %x %d %d\n", __func__, sched_param_data->value, sched_param_data->offset, sched_param_data->size, sched_param_data->direction);

	filp = filp_open(SEC_PARAM_FILE_NAME, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n",
				__func__, PTR_ERR(filp));
		complete(&sched_sec_param_data.work);
		return;
	}

	fs = get_fs();
	set_fs(get_ds());

	ret = filp->f_op->llseek(filp, sched_param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK\n", __func__);
		ret = false;
		goto param_sec_debug_out;
	}

	if (sched_param_data->direction == PARAM_RD)
		ret = filp->f_op->read(filp, (char __user *)sched_param_data->value,
				sched_param_data->size, &filp->f_pos);
	else if (sched_param_data->direction == PARAM_WR)
		ret = filp->f_op->write(filp, (char __user *)sched_param_data->value,
				sched_param_data->size, &filp->f_pos);

param_sec_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	complete(&sched_sec_param_data.work);
	return;
}

bool sec_open_param(void)
{
	int ret = true;
	
	pr_info("%s start \n",__func__);

	if (param_data != NULL)
		return true;

	mutex_lock(&sec_param_mutex);

	param_data = kmalloc(sizeof(struct sec_param_data), GFP_KERNEL);

	sched_sec_param_data.value=param_data;
	sched_sec_param_data.offset=SEC_PARAM_FILE_OFFSET;
	sched_sec_param_data.size=sizeof(struct sec_param_data);
	sched_sec_param_data.direction=PARAM_RD;
	
	schedule_work(&sched_sec_param_data.sec_param_work);
	wait_for_completion(&sched_sec_param_data.work);

	pr_info("%s end \n",__func__);

	mutex_unlock(&sec_param_mutex);

	return ret;

}

bool sec_write_param(void)
{
	int ret = true;
	
	pr_info("%s start\n",__func__);

	mutex_lock(&sec_param_mutex);

	sched_sec_param_data.value=param_data;
	sched_sec_param_data.offset=SEC_PARAM_FILE_OFFSET;
	sched_sec_param_data.size=sizeof(struct sec_param_data);
	sched_sec_param_data.direction=PARAM_WR;
	
	schedule_work(&sched_sec_param_data.sec_param_work);
	wait_for_completion(&sched_sec_param_data.work);

	pr_info("%s end\n",__func__);

	mutex_unlock(&sec_param_mutex);

	return ret;

}

bool sec_get_param(enum sec_param_index index, void *value)
{
	int ret = true;
	ret = sec_open_param();
	if (!ret)
		return ret;

	switch (index) {
	case param_index_debuglevel:
		memcpy(value, &(param_data->debuglevel), sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(value, &(param_data->uartsel), sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(value, &(param_data->rory_control),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_done:
		memcpy(value, &(param_data->movinand_checksum_done),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(value, &(param_data->movinand_checksum_pass),
				sizeof(unsigned int));
		break;
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(value, &(param_data->update_cp_bin),
				sizeof(unsigned int));
		printk(KERN_INFO "param_data.update_cp_bin :[%d]!!", param_data->update_cp_bin);
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_boot_alarm_set:
		memcpy(value, &(param_data->boot_alarm_set), sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_l:
		memcpy(value, &(param_data->boot_alarm_value_l), sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_h:
		memcpy(value, &(param_data->boot_alarm_value_h), sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	case param_index_normal_poweroff:
		memcpy(&(param_data->normal_poweroff), value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_RESTART_REASON_SEC_PARAM
	case param_index_restart_reason:
		memcpy(value, &(param_data->param_restart_reason), sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_BARCODE_PAINTER
	case param_index_barcode_info:
		memcpy(value, param_data->param_barcode_info, sizeof(param_data->param_barcode_info));
		break;
#endif
	default:
		return false;
	}

	return true;
}
EXPORT_SYMBOL(sec_get_param);

bool sec_set_param(enum sec_param_index index, void *value)
{
	int ret = true;

	ret = sec_open_param();
	if (!ret)
		return ret;

	switch (index) {
	case param_index_debuglevel:
		memcpy(&(param_data->debuglevel),
				value, sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(&(param_data->uartsel),
				value, sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(&(param_data->rory_control),
				value, sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_done:
		memcpy(&(param_data->movinand_checksum_done),
				value, sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(&(param_data->movinand_checksum_pass),
				value, sizeof(unsigned int));
		break;
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(&(param_data->update_cp_bin),
				value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_boot_alarm_set:
		memcpy(&(param_data->boot_alarm_set), value, sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_l:
		memcpy(&(param_data->boot_alarm_value_l), value, sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_h:
		memcpy(&(param_data->boot_alarm_value_h), value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	case param_index_normal_poweroff:
		memcpy(&(param_data->normal_poweroff), value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_RESTART_REASON_SEC_PARAM
	case param_index_restart_reason:
		memcpy(&(param_data->param_restart_reason),
				value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_BARCODE_PAINTER
	case param_index_barcode_info:
		memcpy(param_data->param_barcode_info,
				value, sizeof(param_data->param_barcode_info));
		break;
#endif
	default:
		return false;
	}

	ret = sec_write_param();

	return ret;
}
EXPORT_SYMBOL(sec_set_param);

static int __init sec_param_work_init(void)
{
	pr_info("%s: start\n", __func__);

	sched_sec_param_data.offset=0;	
	sched_sec_param_data.direction=0;	
	sched_sec_param_data.size=0;
	sched_sec_param_data.value=NULL;

	init_completion(&sched_sec_param_data.work);
	INIT_WORK(&sched_sec_param_data.sec_param_work, param_sec_operation);

	sec_param_sysfs_init();
	pr_info("%s: end\n", __func__);

	return 0;
}

static void __exit sec_param_work_exit(void)
{
	cancel_work_sync(&sched_sec_param_data.sec_param_work);
	pr_info("%s: exit\n", __func__);
}

/* ##########################################################
 * #
 * # SEC PARAM Driver sysfs file
 * # 1. movinand_checksum_done_show
 * # 2. movinand_checksum_pass_show
 * ##########################################################
 * */
static struct class *sec_param_class;
struct device *sec_param_dev;

static ssize_t movinand_checksum_done_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	sec_get_param(param_index_movinand_checksum_done, &ret);
	if (ret == 1 || ret == 0) {
		pr_err("checksum is not in valuable range.\n");
		ret = 1;
	}
	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);
}
static DEVICE_ATTR(movinand_checksum_done,
				0444, movinand_checksum_done_show, NULL);

static ssize_t movinand_checksum_pass_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	sec_get_param(param_index_movinand_checksum_pass, &ret);
	if (ret == 1 || ret == 0) {
		pr_err("checksum is not in valuable range.\n");
		ret = 1;
	}
	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);
}
static DEVICE_ATTR(movinand_checksum_pass,
				0444, movinand_checksum_pass_show, NULL);

int sec_param_sysfs_init(void)
{

	int ret = 0;

	sec_param_class = class_create(THIS_MODULE, "sec_param");

	if (IS_ERR(sec_param_class)) {
		pr_err("Failed to create class(mdnie_class)!!\n");
		ret = -1;
		goto failed_create_class;
	}

	sec_param_dev = device_create(sec_param_class,
					NULL, 0, NULL, "sec_param");

	if (IS_ERR(sec_param_dev)) {
		printk(KERN_ERR "Failed to create device(sec_param_dev)!!");
		ret = -1;
		goto failed_create_dev;
	}

	if (device_create_file(sec_param_dev,
				&dev_attr_movinand_checksum_done) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
				dev_attr_movinand_checksum_done.attr.name);
		ret = -1;
	}

	if (device_create_file(sec_param_dev,
				&dev_attr_movinand_checksum_pass) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
				dev_attr_movinand_checksum_pass.attr.name);
		ret = -1;
	}
failed_create_class:
	return ret;

failed_create_dev:
	class_destroy(sec_param_class);
	return ret;
}

module_init(sec_param_work_init);
module_exit(sec_param_work_exit);
