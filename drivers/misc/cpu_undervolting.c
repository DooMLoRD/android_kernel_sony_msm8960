/* drivers/misc/cpu_underclocking.c
 *
 * customvoltage interface by Ezekeel
 * uV_mV interface by netarchy
 * uV_uV interface by AndreiLux
 * and vdd_levels interface
 * all merged and a little modified for mach-msm by gokhanmoral
 * ...and added module support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/kallsyms.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/module.h>
#include "../../arch/arm/mach-msm/acpuclock-krait.h"

extern struct acpu_level *acpu_freq_tbl;

#define ROUNDDOWN_STEP 12500

#define CUSTOMVOLTAGE_VERSION 1
#define CPU_UV_MV_MAX 1400000
#define CPU_UV_MV_MIN 600000

static unsigned long max_voltages[2] = {CPU_UV_MV_MAX, 1300000};
int fcount = 0;
void customvoltage_updateintvolt(unsigned long * int_voltages)
{
}
void customvoltage_updatemaxvolt(unsigned long * max_voltages)
{
    if( max_voltages[0] > CPU_UV_MV_MAX ) max_voltages[0] = CPU_UV_MV_MAX;
    if( max_voltages[1] > CPU_UV_MV_MAX ) max_voltages[1] = CPU_UV_MV_MAX;
}

ssize_t show_UV_uV_table(struct cpufreq_policy *policy, char *buf) {
	int i, len = 0;
	if (buf)
	{
		for (i = fcount - 1; i >= 0; i--)
		{
			if(!acpu_freq_tbl[i].use_for_scaling) continue;
			len += sprintf(buf + len, "%lumhz: %d uV\n", 
				acpu_freq_tbl[i].speed.khz/1000,
				acpu_freq_tbl[i].vdd_core);
		}
	}
	return len;
}

ssize_t show_UV_mV_table(struct cpufreq_policy *policy, char *buf)
{
	int i, len = 0;
	if (buf)
	{
		for (i = fcount - 1; i >= 0; i--)
		{
			if(!acpu_freq_tbl[i].use_for_scaling) continue;
			len += sprintf(buf + len, "%lumhz: %d mV\n",
			acpu_freq_tbl[i].speed.khz/1000,
			((acpu_freq_tbl[i].vdd_core % 1000) + acpu_freq_tbl[i].vdd_core)/1000);
		}
	}
	return len;
}

ssize_t acpuclk_get_vdd_levels_str(char *buf)
{
	int i, len = 0;
	if (buf)
	{
		for (i = fcount - 1; i >= 0; i--)
		{
			if(!acpu_freq_tbl[i].use_for_scaling) continue;
			len += sprintf(buf + len, "%lu: %4d\n", 
				acpu_freq_tbl[i].speed.khz,
				((acpu_freq_tbl[i].vdd_core % 1000) + acpu_freq_tbl[i].vdd_core)/1000);
		}
}
return len;
}

void acpuclk_set_vdd(unsigned int khz, unsigned int vdd)
{
	int i;
	unsigned int new_vdd;
	for (i = fcount - 1; i >= 0; i--)
	{
		if(!acpu_freq_tbl[i].use_for_scaling) continue;
		if (khz == 0)
			new_vdd = min(
						max((unsigned int)(acpu_freq_tbl[i].vdd_core + vdd * 1000),
							(unsigned int)CPU_UV_MV_MIN),
						(unsigned int)CPU_UV_MV_MAX);
		else if (acpu_freq_tbl[i].speed.khz == khz)
			new_vdd = min(max(
							(unsigned int)vdd * 1000, 
							(unsigned int)CPU_UV_MV_MIN),
						(unsigned int)CPU_UV_MV_MAX);
		else continue;

		//always round down
		if(new_vdd % ROUNDDOWN_STEP)
			new_vdd = (new_vdd / ROUNDDOWN_STEP) * ROUNDDOWN_STEP;

		acpu_freq_tbl[i].vdd_core = new_vdd;
	}
}

ssize_t store_UV_uV_table(struct cpufreq_policy *policy, 
				 const char *buf, size_t count) {
	int i = 0;
	int j = 0;
	int u[] = { 
			0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0 };
	int tokencount = 0;
	if(count < 1) return -EINVAL;

	//parse input... time to miss strtok... -gm
	for(j = 0; i < count; i++)
	{
		char c = buf[i];
		if(c >= '0' && c <= '9')
		{
			if(tokencount < j + 1) tokencount = j + 1;
			u[j] *= 10;
			u[j] += (c - '0');
		}
		else if(c == ' ' || c == '\t')
		{
			if(u[j] != 0)
			{
				j++;
				if(j >= sizeof(u)) return -EINVAL;
			}
		}
		else
			break;
	}
	
	//do not keep backward compatibility for scripts this time.
	//I want the number of tokens to be exactly the same with stepcount -gm
	if(fcount != tokencount) return -EINVAL;
	
	//we have u[0] starting from the first available frequency to u[stepcount]
	//that is why we use an additiona j here...
	j = 0;
	for (i = fcount - 1; i >= 0; i--)
	{
		if(!acpu_freq_tbl[i].use_for_scaling) continue;
		if (u[j] > CPU_UV_MV_MAX)
		{
			u[j] = CPU_UV_MV_MAX;
		}
		else if (u[j] < CPU_UV_MV_MIN)
		{
			u[j] = CPU_UV_MV_MIN;
		}
		acpu_freq_tbl[i].vdd_core = u[j];
		j++;
	}
	return count;
}		

ssize_t store_UV_mV_table(struct cpufreq_policy *policy,
                                      const char *buf, size_t count)
{
	int i = 0;
	int j = 0;
	int u[] = { 
			0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0 };
	int tokencount = 0;

	if(count < 1) return -EINVAL;

	//parse input... time to miss strtok... -gm
	for(j = 0; i < count; i++)
	{
		char c = buf[i];
		if(c >= '0' && c <= '9')
		{
			if(tokencount < j + 1) tokencount = j + 1;
			u[j] *= 10;
			u[j] += (c - '0');
		}
		else if(c == ' ' || c == '\t')
		{
			if(u[j] != 0)
			{
				j++;
				if(j >= sizeof(u)) return -EINVAL;
			}
		}
		else
			break;
	}
	
	//do not keep backward compatibility for scripts this time.
	//I want the number of tokens to be exactly the same with stepcount -gm
	if(fcount != tokencount) return -EINVAL;
	
	//we have u[0] starting from the first available frequency to u[stepcount]
	//that is why we use an additiona j here...
	for (j = 0, i = fcount - 1; i >= 0; i--)
	{
		u[j] *= 1000;
		//always round down
		if(u[j] % ROUNDDOWN_STEP)
			u[j] = (u[j] / ROUNDDOWN_STEP) * ROUNDDOWN_STEP;

		if (u[j] > CPU_UV_MV_MAX)
		{
			u[j] = CPU_UV_MV_MAX;
		}
		else if (u[j] < CPU_UV_MV_MIN)
		{
			u[j] = CPU_UV_MV_MIN;
		}
		acpu_freq_tbl[i].vdd_core = u[j];
		j++;
	}
	return count;
}

ssize_t UV_uV_table_read(struct device * dev, struct device_attribute * attr, char * buf)
{
	return show_UV_uV_table(NULL, buf);
}
ssize_t UV_uV_table_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
	return store_UV_uV_table(NULL, buf, size);
}

ssize_t show_vdd_levels(struct cpufreq_policy *policy, char *buf)
{
	return acpuclk_get_vdd_levels_str(buf);
}

ssize_t store_vdd_levels(struct cpufreq_policy *policy, const char *buf, size_t count)
{
	int i = 0, j;
	int pair[2] = { 0, 0 };
	int sign = 0;

	if (count < 1)
		return 0;

	if (buf[0] == '-')
	{
		sign = -1;
		i++;
	}
	else if (buf[0] == '+')
	{
		sign = 1;
		i++;
	}

	for (j = 0; i < count; i++)
	{
		char c = buf[i];
		if ((c >= '0') && (c <= '9'))
		{
			pair[j] *= 10;
			pair[j] += (c - '0');
		}
		else if ((c == ' ') || (c == '\t'))
		{
			if (pair[j] != 0)
			{
				j++;
				if ((sign != 0) || (j > 1))
				break;
			}
		}
		else
			break;
	}

	if (sign != 0)
	{
		if (pair[0] > 0)
			acpuclk_set_vdd(0, sign * pair[0]);
	}
	else
	{
		if ((pair[0] > 0) && (pair[1] > 0))
			acpuclk_set_vdd((unsigned)pair[0], pair[1]);
		else
			return -EINVAL;
	}

	return count;
}

ssize_t vdd_levels_read(struct device * dev, struct device_attribute * attr, char * buf)
{
	return show_vdd_levels(NULL, buf);
}
ssize_t vdd_levels_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
	return store_vdd_levels(NULL, buf, size);
}

ssize_t customvoltage_armvolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
	return show_UV_mV_table(NULL, buf);
}

ssize_t customvoltage_armvolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
	return store_UV_mV_table(NULL, buf, size);
}

static ssize_t customvoltage_maxarmvolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "%lu mV\n", max_voltages[0] / 1000);
}

static ssize_t customvoltage_maxarmvolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    unsigned long max_volt;

    if (sscanf(buf, "%lu", &max_volt) == 1)
	{
	    max_voltages[0] = max_volt * 1000;

	    customvoltage_updatemaxvolt(max_voltages);
	}

    return size;
}

static ssize_t customvoltage_version(struct device * dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "%u\n", CUSTOMVOLTAGE_VERSION);
}

static DEVICE_ATTR(arm_volt, S_IRUGO | S_IWUGO, customvoltage_armvolt_read, customvoltage_armvolt_write);
static DEVICE_ATTR(max_arm_volt, S_IRUGO | S_IWUGO, customvoltage_maxarmvolt_read, customvoltage_maxarmvolt_write);
static DEVICE_ATTR(version, S_IRUGO , customvoltage_version, NULL);

static struct attribute *customvoltage_attributes[] = 
    {
	&dev_attr_arm_volt.attr,
	&dev_attr_max_arm_volt.attr,
	&dev_attr_version.attr,
	NULL
    };

static struct attribute_group customvoltage_group = 
    {
	.attrs  = customvoltage_attributes,
    };

static struct miscdevice customvoltage_device = 
    {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "customvoltage",
    };

cpufreq_freq_attr_rw(vdd_levels);
cpufreq_freq_attr_rw(UV_mV_table);
cpufreq_freq_attr_rw(UV_uV_table);

void create_standard_UV_interfaces(void)
{
	int ret = 0;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	int i;
	for (fcount = 0, i = 0; acpu_freq_tbl[i].speed.khz; i++)
	{
		fcount++;
	}
	ret = sysfs_create_file(&policy->kobj, &vdd_levels.attr);
	ret = sysfs_create_file(&policy->kobj, &UV_mV_table.attr);
	ret = sysfs_create_file(&policy->kobj, &UV_uV_table.attr);
}

static int __init customvoltage_init(void)
{
    int ret;

//    pr_info("%s misc_register(%s)\n", __FUNCTION__, customvoltage_device.name);

    ret = misc_register(&customvoltage_device);

    if (ret) 
	{
	    pr_err("%s misc_register(%s) fail\n", __FUNCTION__, customvoltage_device.name);

	    return 1;
	}

    if (sysfs_create_group(&customvoltage_device.this_device->kobj, &customvoltage_group) < 0) 
	{
	    pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
	    pr_err("Failed to create sysfs group for device (%s)!\n", customvoltage_device.name);
	}
#ifdef MODULE
	create_standard_UV_interfaces();
#endif
    return 0;
}

static void __exit customvoltage_exit(void)
{
	sysfs_remove_group(&customvoltage_device.this_device->kobj, &customvoltage_group);
	misc_deregister(&customvoltage_device);
}

module_init( customvoltage_init );
module_exit( customvoltage_exit );

MODULE_AUTHOR("Gokhan Moral <gm@alumni.bilkent.edu.tr>");
MODULE_DESCRIPTION("CPU Undervolting interfaces (3-in-1) module");
MODULE_LICENSE("GPL");
