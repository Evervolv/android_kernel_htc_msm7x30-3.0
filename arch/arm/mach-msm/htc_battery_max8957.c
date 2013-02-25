/* arch/arm/mach-msm/htc_battery_max8957.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <asm/gpio.h>
#include <mach/msm_rpcrouter.h>
#include <mach/board.h>
#include <asm/mach-types.h>
#include <mach/board_htc.h>
#include <mach/msm_fb-7x30.h> /* Jay, to register display notifier */
#include <linux/rtc.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#ifdef CONFIG_HTC_BATTCHG_SMEM
#include "smd_private.h"
#endif
#include <linux/mfd/pmicmax8957.h>
#include <mach/htc_battery_max8957.h>

#if defined(CONFIG_TROUT_BATTCHG_DOCK)
#include <mach/htc_one_wire.h>
#endif
#include <linux/android_alarm.h>

#define MAX8957_DELAY 2000

/* safety timer */
static unsigned int delta_time_sec = 0;
static ktime_t last_poll_ktime;

#define VBUS_POLL	(1 * 30)
#define FAST_POLL	(1 * 60)
#define PHONE_POLL  (5 * 60)
#define SLOW_POLL	(60 * 60)
#define LOW_LEVEL_POLL (10 * 60)
#define PREDIC_POLL	20

/*To assign fake batt parameter when boot with power supply*/
static int test_power_monitor = 0;
static struct wake_lock vbus_wake_lock;

enum {
	HTC_BATT_DEBUG_M2A_RPC = 1U << 0,
	HTC_BATT_DEBUG_A2M_RPC = 1U << 1,
	HTC_BATT_DEBUG_UEVT = 1U << 2,
	HTC_BATT_DEBUG_USER_QUERY = 1U << 3,
	HTC_BATT_DEBUG_USB_NOTIFY = 1U << 4,
	HTC_BATT_DEBUG_SMEM = 1U << 5,
};
static int htc_batt_debug_mask = HTC_BATT_DEBUG_M2A_RPC | HTC_BATT_DEBUG_A2M_RPC
	| HTC_BATT_DEBUG_UEVT | HTC_BATT_DEBUG_USB_NOTIFY | HTC_BATT_DEBUG_SMEM;
module_param_named(debug_mask, htc_batt_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define BATT_LOG(x...) do { \
struct timespec ts; \
struct rtc_time tm; \
getnstimeofday(&ts); \
rtc_time_to_tm(ts.tv_sec, &tm); \
printk(KERN_INFO "[BATT] " x); \
printk(" at %lld (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", \
ktime_to_ns(ktime_get()), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec); \
} while (0)

#define BATT_ERR(x...) do { \
struct timespec ts; \
struct rtc_time tm; \
getnstimeofday(&ts); \
rtc_time_to_tm(ts.tv_sec, &tm); \
printk(KERN_ERR "[BATT] err:" x); \
printk(" at %lld (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", \
ktime_to_ns(ktime_get()), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec); \
} while (0)

#define MSPERIOD(end, start)	ktime_to_ms(ktime_sub(end, start))

/* rpc related */
#define APP_BATT_PDEV_NAME		"rs30100001:00000000"
#define APP_BATT_PROG			0x30100001
#define APP_BATT_VER			MSM_RPC_VERS(0, 0)
#define HTC_PROCEDURE_BATTERY_NULL		0
#define HTC_PROCEDURE_GET_BATT_LEVEL	1
#define HTC_PROCEDURE_GET_BATT_INFO		2
#define HTC_PROCEDURE_GET_CABLE_STATUS	3
#define HTC_PROCEDURE_SET_BATT_DELTA	4
#define HTC_PROCEDURE_CHARGER_SWITCH    6
#define HTC_PROCEDURE_SET_FULL_LEVEL	7
#define HTC_PROCEDURE_GET_USB_ACCESSORY_ADC_LEVEL	10

#define TRUE 1
#define FALSE 0

const char *charger_tags[] = {"none", "USB", "AC", "SUPER AC", "WIRELESS CHARGER"};

struct htc_battery_max8957_info {
	int device_id;
	int present;
	unsigned long update_time;

	/* lock to protect the battery info */
	struct mutex lock;

	/* lock held while calling the arm9 to query the battery info */
	struct mutex rpc_lock;
	struct battery_info_reply rep;
	int (*func_show_batt_attr)(struct device_attribute *attr, char *buf);
	int gpio_mbat_in;
	int gpio_usb_id;
	int gpio_mchg_en_n;
	int gpio_iset;
	int gauge_driver;
	int m2a_cable_detect;
	int charger;
	int gpio_adp_9v;
	int (*func_battery_charging_ctrl)(enum batt_ctl_t ctl);
};

struct maxim8957_alarm {
	struct device *dev;
    struct workqueue_struct *monitor_wqueue;
	struct work_struct monitor_work;
	struct alarm alarm;
	struct wake_lock work_wake_lock;
	u8 slow_poll;
	ktime_t last_poll;
	unsigned long update_time;
	spinlock_t spin_lock;
	unsigned long spin_flags;
	struct delayed_work level_update_work;
};

struct max8957_fg {
	struct device *dev;
#if MAXIM_BATTERY_FG_LOG
	struct wake_lock fg_log_wake_lock;
	struct delayed_work fg_log_work;
#endif
};

static struct msm_rpc_endpoint *endpoint;

static struct htc_battery_max8957_info htc_batt_info;
struct max8957_fg *max8957_fg = NULL;
struct maxim8957_alarm *maxim8957_alarm_ptr = NULL;

/* Remove cache mechanism to prevent cable status not sync. */
static unsigned int cache_time;

static int htc_battery_initial = 0;
static int htc_full_level_flag = 0;
static unsigned int stopcharging_vol;
static int full_count = 0;
static u32 curr_percent = 55;
static int force_shutdown = 0;
static int debug_fake_temperature = FALSE;
static bool isFirstTime = TRUE;

static struct alarm batt_charger_ctrl_alarm;
static struct work_struct batt_charger_ctrl_work;
struct workqueue_struct *batt_charger_ctrl_wq;
static unsigned int charger_ctrl_stat;

/* For switch battery timer between 30s and 5min by phone status */
static int htc_batt_phone_call;

static enum power_supply_property htc_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_AVGCURRENT,
};

static enum power_supply_property htc_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

/* HTC dedicated attributes */
static ssize_t htc_battery_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);

static int htc_power_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static int htc_battery_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static struct power_supply htc_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = htc_battery_properties,
		.num_properties = ARRAY_SIZE(htc_battery_properties),
		.get_property = htc_battery_get_property,
	},
	{
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
	{
		.name = "wireless",
		.type = POWER_SUPPLY_TYPE_WIRELESS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
};

static void maxim_batt_INI_param_check(void);
static int update_batt_info(void);
static void usb_status_notifier_func(int online);
static struct t_usb_status_notifier usb_status_notifier = {
	.name = "htc_battery_max8957",
	.func = usb_status_notifier_func,
};

/* Move cable detection/notification to standard PMIC RPC. */
static BLOCKING_NOTIFIER_HEAD(cable_status_notifier_list);
int register_notifier_cable_status(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cable_status_notifier_list, nb);
}

int unregister_notifier_cable_status(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cable_status_notifier_list, nb);
}

static BLOCKING_NOTIFIER_HEAD(wireless_charger_notifier_list);
int register_notifier_wireless_charger(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&wireless_charger_notifier_list, nb);
}

int unregister_notifier_wireless_charger(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&wireless_charger_notifier_list, nb);
}

/* -------------------------------------------------------------------------- */
/* For sleep charging screen. */
static int zcharge_enabled;
static int htc_is_zcharge_enabled(void)
{
	return zcharge_enabled;
}
EXPORT_SYMBOL(htc_is_zcharge_enabled);

static int __init enable_zcharge_setup(char *str)
{
	/*
	int cal = simple_strtol(str, NULL, 0);
	*/
	long cal = 0;
	int ret = strict_strtol(str, 0, &cal);

	if (ret)
		BATT_ERR("%s: Failed to convert string, err=%d\n", __func__, ret);

	zcharge_enabled = (int)cal;
	return 1;
}
__setup("enable_zcharge=", enable_zcharge_setup);

static int htc_is_cable_in(void)
{
	if (!htc_batt_info.update_time) {
		BATT_ERR("%s: battery driver hasn't been initialized yet.", __func__);
		return -EINVAL;
	}

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: return %d\n", __func__, (htc_batt_info.rep.charging_source != CHARGER_BATTERY) ? 1 : 0);
#endif

	return (htc_batt_info.rep.charging_source != CHARGER_BATTERY) ? 1 : 0;
}
EXPORT_SYMBOL(htc_is_cable_in);

/* For touch panel, touch panel may loss wireless charger notification when system boot up */
int htc_is_wireless_charger(void)
{
	if (htc_battery_initial)
		return (htc_batt_info.rep.charging_source == CHARGER_WIRELESS) ? 1 : 0;
	else
		return -1;
}

/**
 * htc_power_policy - check if it obeys our policy
 * return 0 for no errors, to indicate it follows policy.
 * non zero otherwise.
 **/
static int __htc_power_policy(void)
{
	if (!zcharge_enabled)
		return 0;

	if (htc_is_cable_in())
		return 1;

	return 0;
}

/*
 * Jay, 7/1/09'
 */
static int htc_power_policy(struct notifier_block *nfb,
		unsigned long action, void *ignored)
{
	int rc;
	switch (action) {
	case NOTIFY_POWER:
		pr_info("%s: enter.\n", __func__);
		rc = __htc_power_policy();
		if (rc)
			return NOTIFY_STOP;
		else
			return NOTIFY_OK;
	}
	return NOTIFY_DONE; /* we did not care other action here */
}

unsigned int batt_get_status(enum power_supply_property psp)
{
	union power_supply_propval val;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s:+, psp=%d\n", __func__, psp);
#endif

	if (update_batt_info())
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
#if MAXIM_BATTERY_DEBUG
		pr_err("%s: POWER_SUPPLY_PROP_CAPACITY\n", __func__);
#endif
		mutex_lock(&htc_batt_info.lock);
		val.intval = htc_batt_info.rep.level;
		mutex_unlock(&htc_batt_info.lock);
		/* prevent shutdown before battery driver ready. */
		if (htc_batt_info.device_id == 0)
			val.intval = 55; /* 55 == ?? */
		break;
	case POWER_SUPPLY_PROP_TEMP:
#if MAXIM_BATTERY_DEBUG
		pr_err("%s: POWER_SUPPLY_PROP_TEMP\n", __func__);
#endif
		mutex_lock(&htc_batt_info.lock);
		val.intval = htc_batt_info.rep.batt_temp;
		mutex_unlock(&htc_batt_info.lock);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#if MAXIM_BATTERY_DEBUG
		pr_err("%s: POWER_SUPPLY_PROP_VOLTAGE_NOW\n", __func__);
#endif
		mutex_lock(&htc_batt_info.lock);
		val.intval = htc_batt_info.rep.batt_vol;
		mutex_unlock(&htc_batt_info.lock);
		break;
	default:
		return -EINVAL;
	}

#if MAXIM_BATTERY_DEBUG
	pr_err("%s:-,  val.intval=%d\n", __func__, val.intval);
#endif
	return val.intval;
}

#if defined(CONFIG_DEBUG_FS)
static int htc_battery_set_charging(enum batt_ctl_t ctl);
static int batt_debug_set(void *data, u64 val)
{
	return htc_battery_set_charging((enum batt_ctl_t) val);
}

static int batt_debug_get(void *data, u64 *val)
{
	return -ENOSYS;
}

DEFINE_SIMPLE_ATTRIBUTE(batt_debug_fops, batt_debug_get, batt_debug_set, "%llu\n");

#if MAXIM_BATTERY_FG_LOG

#define MAXIM_BATTERY_FG_LOG_REG_BLK1_START	0x00
#define MAXIM_BATTERY_FG_LOG_REG_BLK1_END	0x4F
#define MAXIM_BATTERY_FG_LOG_REG_BLK2_START	0xE0
#define MAXIM_BATTERY_FG_LOG_REG_BLK2_END	0xFF

#define FG_LOG_DIR "/sdcard/fg_log"
#define FG_LOG_BUFFER_SIZE 2048
#define FG_LOG_PERIOD_IN_SEC 15

int fg_log_enabled = 0;

long sys_mkdir(const char *pathname, int mode);
long sys_open(const char *filename, int flags, int mode);
long sys_close(unsigned int fd);
long sys_read(unsigned int fd, char *buf, size_t count);
long sys_write(unsigned int fd, const char *buf, size_t count);

static int htc_battery_get_fg_log(char *buf, u8 start_addr, u8 end_addr)
{
	struct max8957_chip *max8957_chip = NULL;
	u16 reg_val = 0;
	int readLen = 0;
	int rc = 0;
	int count = 0, i = 0;

	if (unlikely(!max8957_fg)) {
		pr_err("%s: max8957_fg is not initialized", __func__);
		return 0;
	}

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	count = end_addr - start_addr;
#if MAXIM_BATTERY_FG_LOG_DEBUG
	pr_err("%s: read from 0x%02X to 0x%02X for %d registers\n", __func__, start_addr, end_addr, count + 1);
#endif
	for (i = 0; i <= count; i++) {
		reg_val = 0;
		rc = max8957_read(max8957_chip, start_addr + i, (u8 *)&reg_val, 2);
		if (unlikely(rc < 0))
			pr_err("%s: Failed to read reg 0x%x, rc=%d", __func__, MAXIM_BATTERY_FG_LOG_REG_BLK1_START + i, rc);
#if MAXIM_BATTERY_FG_LOG_DEBUG
		/*
		pr_err("%s: update buffer offset %d, reg 0x%04x, val 0x%04x\n", __func__, readLen, MAXIM_BATTERY_FG_LOG_REG_BLK2_START + i, reg_val);
		*/
#endif
		readLen += sprintf(buf + readLen, "%04X ", reg_val);
	}

	return readLen;
}

static void htc_battery_dump_fg_reg(char *buf, int fd)
{
	struct max8957_chip *max8957_chip = NULL;
	u16 reg_val = 0;
	int len = 0;
	int count = 0, i = 0;
	int ret = 0;

#if MAXIM_BATTERY_FG_LOG_DEBUG
	pr_err("%s: +, fd=%d, fg_log_enabled=%d\n", __func__, fd, fg_log_enabled);
#endif

	if (unlikely(!buf)) {
		pr_err("%s: invalid buffer\n", __func__);
		return;
	}

	if (unlikely(fd < 0)) {
		pr_err("%s: invalid file handler %d", __func__, fd);
		return;
	}

	if (unlikely(!max8957_fg)) {
		pr_err("%s: max8957_fg is not initialized", __func__);
		return;
	}

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	if (!fg_log_enabled)
		len += sprintf(buf, "\n\n");

	len += sprintf(buf + len, "Dump Fuel Gauge Registers:\n\n");

	count = (MAXIM_BATTERY_FG_LOG_REG_BLK2_END - MAXIM_BATTERY_FG_LOG_REG_BLK1_START) / 2;
	/* Dump all registers when fg_log start */
	for (i = MAXIM_BATTERY_FG_LOG_REG_BLK1_START; i <= count; i++)
		len += sprintf(buf + len, "0x%02X ", MAXIM_BATTERY_FG_LOG_REG_BLK1_START + i);

	/* Add the end of line */
	*(buf + len - 1) = '\n';

	/*
	ret = sys_write(fd, (char *)buf, len);
	if (ret < 0)
		goto err;
	*/

	for (i = MAXIM_BATTERY_FG_LOG_REG_BLK1_START; i <= count; i++)
		len += sprintf(buf + len, "---- ");

	/* Add the end of line */
	*(buf + len - 1) = '\n';

	ret = sys_write(fd, (char *)buf, len);
	if (ret < 0)
		goto err;

	/* Unlock modem access */
	reg_val = 0x0059;
	ret = max8957_write(max8957_chip, 0x62, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x62, rc=%d", __func__, ret);

	reg_val = 0x00C4;
	ret = max8957_write(max8957_chip, 0x63, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x63, rc=%d", __func__, ret);

	len = 0;
	len += htc_battery_get_fg_log(buf + len, MAXIM_BATTERY_FG_LOG_REG_BLK1_START, count);

	/* Lock modem access */
	reg_val = 0x0000;
	ret = max8957_write(max8957_chip, 0x62, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x62, rc=%d", __func__, ret);

	ret = max8957_write(max8957_chip, 0x63, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x63, rc=%d", __func__, ret);

	/* Add the end of line */
	len++;
	sprintf(buf + len - 2, "\n\n");

	ret = sys_write(fd, (char *)buf, len);
	if (ret < 0)
		goto err;

	len = 0;
	/* Dump all registers when fg_log start */
	for (i = count + 1; i <= MAXIM_BATTERY_FG_LOG_REG_BLK2_END; i++)
		len += sprintf(buf + len, "0x%02X ", i);

	/* Add the end of line */
	*(buf + len - 1) = '\n';

	/*
	ret = sys_write(fd, (char *)buf, len);
	if (ret < 0)
		goto err;
	*/

	for (i = count + 1; i <= MAXIM_BATTERY_FG_LOG_REG_BLK2_END; i++)
		len += sprintf(buf + len, "---- ");

	/* Add the end of line */
	*(buf + len - 1) = '\n';

	ret = sys_write(fd, (char *)buf, len);
	if (ret < 0)
		goto err;

	/* Unlock modem access */
	reg_val = 0x0059;
	ret = max8957_write(max8957_chip, 0x62, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x62, rc=%d", __func__, ret);

	reg_val = 0x00C4;
	ret = max8957_write(max8957_chip, 0x63, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x63, rc=%d", __func__, ret);

	len = 0;
	len += htc_battery_get_fg_log(buf + len, count + 1, MAXIM_BATTERY_FG_LOG_REG_BLK2_END);

	/* Lock modem access */
	reg_val = 0x0000;
	ret = max8957_write(max8957_chip, 0x62, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x62, rc=%d", __func__, ret);

	ret = max8957_write(max8957_chip, 0x63, (u8 *)&reg_val, 2);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to write reg 0x63, rc=%d", __func__, ret);

	if (fg_log_enabled) {
		len += 3;
		sprintf(buf + len - 3, "\n\n\n");
	}

	ret = sys_write(fd, (char *)buf, len);
	if (ret < 0)
		goto err;

#if MAXIM_BATTERY_FG_LOG_DEBUG
	pr_err("%s: -\n", __func__);
#endif

	return;

err:
	pr_err("%s: failed to write file, ret=%d\n", __func__, ret);
	return;
}

static void htc_battery_fg_log_work_func(struct work_struct *work)
{
	static char filename[128];
	static int fd = -1;
	static char *buf = NULL;
	mm_segment_t old_fs;
	int len = 0, ret = 0;
	struct timespec ts;
	struct rtc_time tm;
	int count = 0, i = 0;

	if (unlikely(!max8957_fg)) {
		pr_err("%s: max8957_fg is not initialized", __func__);
		return;
	}

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

#if MAXIM_BATTERY_FG_LOG_DEBUG
	pr_err("%s: fd=%d, fg_log_enabled=%d\n", __func__, fd, fg_log_enabled);
#endif

	if (!buf) {
		buf = kzalloc(FG_LOG_BUFFER_SIZE, GFP_KERNEL);
		if (unlikely(!buf)) {
			pr_err("%s: failed to allocate buffer %d bytes\n", __func__, FG_LOG_BUFFER_SIZE);
			return;
		}
	}

	if (fg_log_enabled) {
		/* Keep the system awake */
		wake_lock_timeout(&max8957_fg->fg_log_wake_lock, HZ * (FG_LOG_PERIOD_IN_SEC + 5));

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (fd < 0) {
			ret = sys_mkdir(FG_LOG_DIR, 0644);
			if (ret < 0 && ret != -EEXIST) {
				pr_err("%s: failed to create directory %s, ret=%d\n", __func__, FG_LOG_DIR, ret);
				goto err;
			}
#if MAXIM_BATTERY_FG_LOG_DEBUG
			else
				pr_err("%s: create directory %s success\n", __func__, FG_LOG_DIR);
#endif

			sprintf(filename, "%s/%d%02d%02d%02d%02d%02d.txt", FG_LOG_DIR,
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

			fd = sys_open(filename, O_RDWR | O_APPEND | O_CREAT, 0644);
			if (fd < 0) {
				pr_err("%s: failed to open file %s, ret=%d\n", __func__, filename, fd);
				fd = 0;
				return;
			}
#if MAXIM_BATTERY_FG_LOG_DEBUG
			else
				pr_err("%s: open file %s success, fd=%d\n", __func__, filename, fd);
#endif

			/* Dump all registers when fg_log start */
			htc_battery_dump_fg_reg(buf, fd);

			len = 0;
			len += sprintf(buf + len, "Fuel Guage Log Start: \n\n");

			/* Generate the header of fg log file */
			len += sprintf(buf + len, "%19s ", "Time");
			count = MAXIM_BATTERY_FG_LOG_REG_BLK1_END - MAXIM_BATTERY_FG_LOG_REG_BLK1_START;
			for (i = 0; i <= count; i++)
				len += sprintf(buf + len, "0x%02X ", MAXIM_BATTERY_FG_LOG_REG_BLK1_START + i);

			count = MAXIM_BATTERY_FG_LOG_REG_BLK2_END - MAXIM_BATTERY_FG_LOG_REG_BLK2_START;
			for (i = 0; i <= count; i++)
				len += sprintf(buf + len, "0x%02X ", MAXIM_BATTERY_FG_LOG_REG_BLK2_START + i);

			/* Add the end of line */
			*(buf + len - 1) = '\n';

			len += sprintf(buf + len, "------------------- ");
			count = MAXIM_BATTERY_FG_LOG_REG_BLK1_END - MAXIM_BATTERY_FG_LOG_REG_BLK1_START;
			for (i = 0; i <= count; i++)
				len += sprintf(buf + len, "---- ");

			count = MAXIM_BATTERY_FG_LOG_REG_BLK2_END - MAXIM_BATTERY_FG_LOG_REG_BLK2_START;
			for (i = 0; i <= count; i++)
				len += sprintf(buf + len, "---- ");

			/* Add the end of line */
			*(buf + len - 1) = '\n';

			ret = sys_write(fd, (char *)buf, len);
			if (ret < 0) {
				pr_err("%s: failed to write file %s, ret=%d\n", __func__, filename, ret);
				goto err;
			}
		}

		len = 0;
		len += sprintf(buf + len, "%04d/%02d/%02d %02d:%02d:%02d ",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		len += htc_battery_get_fg_log(buf + len, MAXIM_BATTERY_FG_LOG_REG_BLK1_START, MAXIM_BATTERY_FG_LOG_REG_BLK1_END);
		len += htc_battery_get_fg_log(buf + len, MAXIM_BATTERY_FG_LOG_REG_BLK2_START, MAXIM_BATTERY_FG_LOG_REG_BLK2_END);

		/* Add the end of line */
		*(buf + len - 1) = '\n';

		ret = sys_write(fd, (char *)buf, len);
		if (ret < 0) {
			pr_err("%s: failed to write file %s, ret=%d\n", __func__, filename, ret);
			goto err;
		}
#if MAXIM_BATTERY_FG_LOG_DEBUG
		else
			pr_err("%s: write file %s success, len=%d\n", __func__, filename, len);
#endif

		set_fs(old_fs);

		schedule_delayed_work(&max8957_fg->fg_log_work, msecs_to_jiffies(FG_LOG_PERIOD_IN_SEC * 1000));
	} else {
		/* Dump all registers when fg_log stop */
		htc_battery_dump_fg_reg(buf, fd);

		sys_close(fd);
		fd = -1;
	}

	return;

err:
	if (buf)
		kzfree(buf);

	set_fs(old_fs);

	if (fd > 0)
		sys_close(fd);
	fd = 0;

	return;
}

static int fg_log_debug_set(void *data, u64 val)
{
	val = (val != 0) ? 1 : 0;

	/* Return if the setting is not changed */
	if (fg_log_enabled == val) {
#if MAXIM_BATTERY_FG_LOG_DEBUG
		pr_err("%s: fg_log_enabled(%d) not changed, skip\n", __func__, fg_log_enabled);
#endif
		return 0;
	}

	fg_log_enabled = val;

	/* Cancel queued work first when disable fg_log */
	if (!fg_log_enabled)
		cancel_delayed_work_sync(&max8957_fg->fg_log_work);

	schedule_delayed_work(&max8957_fg->fg_log_work, 0);

	return 0;
}

static int fg_log_debug_get(void *data, u64 *val)
{
	*val = fg_log_enabled;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fg_log_debug_fops, fg_log_debug_get, fg_log_debug_set, "%llu\n");
#endif

static int __init batt_debug_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("htc_battery", 0);
	if (IS_ERR(dent)) {
		pr_err("%s: failed to create debugfs dir for htc_battery\n", __func__);
		return PTR_ERR(dent);
	}

	debugfs_create_file("charger_state", 0644, dent, NULL, &batt_debug_fops);

#if MAXIM_BATTERY_FG_LOG
	dent = debugfs_create_dir("fg_log", dent);
	if (IS_ERR(dent)) {
		pr_err("%s: failed to create debugfs dir for fuel_gauge_log\n", __func__);
		return PTR_ERR(dent);
	}

	debugfs_create_file("enable", 0644, dent, NULL, &fg_log_debug_fops);
#endif

	return 0;
}

device_initcall(batt_debug_init);
#endif

/*
static int init_batt_gpio(void)
{

	if (htc_batt_info.gpio_mbat_in > 0 &&
		gpio_request(htc_batt_info.gpio_mbat_in, "batt_detect") < 0)
		goto gpio_failed;
	if (htc_batt_info.gpio_mchg_en_n > 0 &&
		gpio_request(htc_batt_info.gpio_mchg_en_n, "charger_en") < 0)
		goto gpio_failed;
	if (htc_batt_info.gpio_iset > 0 &&
		gpio_request(htc_batt_info.gpio_iset, "charge_current") < 0)
		goto gpio_failed;
	if (htc_batt_info.gpio_adp_9v > 0 &&
		gpio_request(htc_batt_info.gpio_adp_9v, "super_charge_current") < 0)
		goto gpio_failed;

	return 0;

gpio_failed:
	return -EINVAL;

}
*/

/*
 *	battery_charging_ctrl - battery charing control.
 * 	@ctl:			battery control command
 *
 */
int battery_charging_ctrl(enum batt_ctl_t ctl)
{
	int result = 0;

	switch (ctl) {
	case DISABLE:
		BATT_LOG("charger OFF");
		/* 0 for enable; 1 disable */
		/*
		result = gpio_direction_output(htc_batt_info.gpio_mchg_en_n, 1);
		*/
		break;
	case ENABLE_SLOW_CHG:
		BATT_LOG("charger ON (SLOW)");
		/*
		result = gpio_direction_output(htc_batt_info.gpio_iset, 0);
		result = gpio_direction_output(htc_batt_info.gpio_mchg_en_n, 0);
		if (htc_batt_info.gpio_adp_9v > 0)
			result = gpio_direction_output(htc_batt_info.gpio_adp_9v, 0);
		*/
		break;
	case ENABLE_FAST_CHG:
		BATT_LOG("charger ON (FAST)");
		/*
		result = gpio_direction_output(htc_batt_info.gpio_iset, 1);
		result = gpio_direction_output(htc_batt_info.gpio_mchg_en_n, 0);
		if (htc_batt_info.gpio_adp_9v > 0)
			result = gpio_direction_output(htc_batt_info.gpio_adp_9v, 0);
		*/
		break;
	case ENABLE_SUPER_CHG:
		BATT_LOG("charger ON (SUPER)");
		/*
		result = gpio_direction_output(htc_batt_info.gpio_iset, 1);
		result = gpio_direction_output(htc_batt_info.gpio_mchg_en_n, 0);
		if (htc_batt_info.gpio_adp_9v > 0)
			result = gpio_direction_output(htc_batt_info.gpio_adp_9v, 1);
		*/
		break;
	default:
		BATT_ERR("%s: Not supported battery ctr called.!", __func__);
		result = -EINVAL;
		break;
	}

	return result;
}

static int htc_battery_set_charging(enum batt_ctl_t ctl)
{
	int rc;

	pr_err("%s: ctl=0x%x\n", __func__, ctl);

	rc = battery_charging_ctrl(ctl);
	if (rc < 0)
		goto result;

	if (!htc_battery_initial)
		htc_batt_info.rep.charging_enabled = ctl & 0x3;
	else {
		mutex_lock(&htc_batt_info.lock);
		htc_batt_info.rep.charging_enabled = ctl & 0x3;
		mutex_unlock(&htc_batt_info.lock);
	}

result:
	return rc;
}

#if 0 /* JR:Don't need to update battery level from modem side */
static int htc_battery_status_update(u32 curr_level)
{
	int notify;
	if (!htc_battery_initial)
		return 0;

	mutex_lock(&htc_batt_info.lock);
	notify = (htc_batt_info.rep.level != curr_level);
	htc_batt_info.rep.level = curr_level;
	mutex_unlock(&htc_batt_info.lock);
#if 0
	if (notify) {
		power_supply_changed(&htc_power_supplies[BATTERY_SUPPLY]);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_UEVT)
			BATT_LOG("power_supply_changed: battery");
	}
#else
	/* we don't check level here for charging over temp RPC call */
	power_supply_changed(&htc_power_supplies[BATTERY_SUPPLY]);
	if (htc_batt_debug_mask & HTC_BATT_DEBUG_UEVT)
		BATT_LOG("power_supply_changed: battery");
#endif
	return 0;
}
#endif

static u32 htc_get_batt_level(void);

static void htc_battery_level_update_work_func(void)
{
	static u32 prev_level = 0;
	u32 curr_level = 0;
	struct max8957_chip *max8957_chip = NULL;
	int rc = 0;
	s32 FG_AvgCurrent, FG_Voltage = 0;
	s16 val;
	static int FullSOC_flag = 1;
	u16 reg_FullCAP = 0;
	u16 reg_RepCap = 0;
	u16 level_low_byte = 0;
	u32 raw_level = 0;
	u32 batt_age = 0;
	u32 batt_cycle = 0;

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	if (!htc_battery_initial)
		return;

	curr_level = htc_get_batt_level();

	rc = max8957_read(max8957_chip, MAX8957_FG_AvgCurrent, (u8 *)&val, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_Current: %d", __func__, rc);
	FG_AvgCurrent = val * 5 / 32; /* (val * 1.25mA) >> 3 */

	rc = max8957_read(max8957_chip, MAX8957_FG_VCELL, (u8 *)&FG_Voltage, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_VCELL: %d", __func__, rc);
	FG_Voltage = FG_Voltage * 20 / 256; /* VCELL unit is 78.125uV */

	if ((FG_AvgCurrent >= 0 && FG_AvgCurrent <= 50)
		&& curr_level >= 93 && FG_Voltage > stopcharging_vol) {
		full_count = full_count + 1;
		if ( full_count > 3)
			curr_level = 100;
	}
	else
		full_count = 0;

	rc = max8957_read(max8957_chip, MAX8957_FG_RepSOC, (u8 *)&raw_level, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_RepSOC: 0x%x", __func__, rc);

	level_low_byte = (raw_level & 0xFF); // To get the decimal point value of raw level data

	if (curr_level >= 99) {
		if (full_count > 3)
			curr_level = 100;
		else if (level_low_byte > 128 && htc_batt_info.rep.charging_source == CHARGER_BATTERY)
			curr_level = 100;            // To keep 100% once gauge level > 99.5%
		else if (prev_level == 100 && curr_level == 99 && level_low_byte > 128) {
			curr_level = 100;
			full_count = 4;
		}
		else
			curr_level = 99;
	}

	if (force_shutdown)		//To trigger shutdown when battery vol reach 3.0V
		curr_level = 0;

	if (curr_level == 100 && FullSOC_flag) {
		FullSOC_flag = 0;

		max8957_read(max8957_chip, MAX8957_FG_RepCap, (u8 *)&reg_RepCap, 2); //Read RepCap
		BATT_LOG("To read MAX8957_FG_RepCap: 0x%x", reg_RepCap);
		max8957_write(max8957_chip, MAX8957_FG_FullCAP , (u8 *)&reg_RepCap, 2);	// Write FullCAP
		max8957_read(max8957_chip, MAX8957_FG_FullCAP, (u8 *)&reg_FullCAP, 2); //Read FullCAP
		BATT_LOG("To read MAX8957_FG_FullCAP: 0x%x", reg_FullCAP);
	}

	rc = max8957_read(max8957_chip, MAX8957_FG_Age, (u8 *)&batt_age, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_RepSOC: 0x%x", __func__, rc);
	batt_age = batt_age / 256;

	max8957_read(max8957_chip, MAX8957_FG_Cycles, (u8 *)&batt_cycle, 2);

	BATT_LOG("bat_vol=%d, avg_curr=%d, curr_level=%d, prev_level=%d, "
			"full_count=%d, batt_age=%d, fake_temp=%d, batt_cycle=%d, INI_flag=%d",
			FG_Voltage, FG_AvgCurrent, curr_level, prev_level,
			full_count, batt_age, debug_fake_temperature, batt_cycle, htc_batt_info.rep.INI_flag);

	if (prev_level != curr_level) {
		power_supply_changed(&htc_power_supplies[BATTERY_SUPPLY]);
		prev_level = curr_level;
	}
	htc_batt_info.rep.level = curr_level;
	curr_percent = curr_level;

	return;
}

static void update_wake_lock(int status)
{
	if (status == CHARGER_USB)
		wake_lock(&vbus_wake_lock);
	else if (__htc_power_policy())
		/* Lock suspend for DOPOD charging animation */
		wake_lock(&vbus_wake_lock);
	else
		/* give userspace some time to see the uevent and update
		 * LED state or whatnot...
		 */
		wake_lock_timeout(&vbus_wake_lock, HZ * 5);
}

#ifdef CONFIG_HTC_BATTCHG_SMEM
static int htc_set_smem_cable_type(u32 cable_type);
#else
static int htc_set_smem_cable_type(u32 cable_type) { return -1; }
#endif
#if 1 /* JH //this is for packet filter (notify port list while USB in/out) */
int update_port_list_charging_state(int enable);
#endif

static int htc_cable_status_update(int status)
{
	int rc = 0;
	unsigned last_source;

	if (!htc_battery_initial)
		return 0;

	if (status < CHARGER_BATTERY || status > CHARGER_WIRELESS) {
		BATT_ERR("%s: Not supported cable status received!", __func__);
		return -EINVAL;
	}

	mutex_lock(&htc_batt_info.lock);
#if 1
	pr_info("[BATT] %s: %d -> %d\n", __func__, htc_batt_info.rep.charging_source, status);
	if (status == htc_batt_info.rep.charging_source) {
	/* When cable overvoltage(5V => 7V) A9 will report the same source, so only sent the uevent */
		if (status == CHARGER_USB) {
			power_supply_changed(&htc_power_supplies[USB_SUPPLY]);
			if (htc_batt_debug_mask & HTC_BATT_DEBUG_UEVT)
				BATT_LOG("(htc_cable_status_update)power_supply_changed: OverVoltage");
		}
		mutex_unlock(&htc_batt_info.lock);
		return 0;
	}

	last_source = htc_batt_info.rep.charging_source;
	/* TODO: replace charging_source to vbus_present */
	htc_batt_info.rep.charging_source = status;
	/* ARM9 should know the status it notifies,
	 * keep this code for old projects. */
	/* htc_set_smem_cable_type(status); */

	update_wake_lock(status);
	/*ARM9 report CHARGER_AC while plug in htc_adaptor which is identify by usbid*/
	/*don't need to notify usb driver*/
	/* These are legacy codes for MSM7200 */
	if ((htc_batt_info.gauge_driver == GAUGE_MAX8957) && (status == CHARGER_AC)) {
		htc_set_smem_cable_type(CHARGER_AC);
		power_supply_changed(&htc_power_supplies[AC_SUPPLY]);
	} else {
		if (status == CHARGER_WIRELESS) {
			BATT_LOG("batt: Wireless charger detected. "
				"We don't need to inform USB driver.");
			blocking_notifier_call_chain(&wireless_charger_notifier_list, status, NULL);
			power_supply_changed(&htc_power_supplies[WIRELESS_SUPPLY]);
			update_wake_lock(htc_batt_info.rep.charging_source);
		} else {
			/* We need to notify other driver as wireless charger out. */
			if (last_source == CHARGER_WIRELESS)
				blocking_notifier_call_chain(&wireless_charger_notifier_list, status, NULL);
			msm_otg_set_vbus_state(!!htc_batt_info.rep.charging_source);
#ifdef CONFIG_USB_MSM_72K
			msm_hsusb_set_vbus_state(!!htc_batt_info.rep.charging_source);
#endif
		}
	}

	if (status == CHARGER_BATTERY) {
		if (full_count != 0)
			full_count = 0;
		BATT_LOG("cable out reset full_count\n");
		htc_set_smem_cable_type(CHARGER_BATTERY);
		power_supply_changed(&htc_power_supplies[BATTERY_SUPPLY]);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_UEVT)
			BATT_LOG("(htc_cable_status_update)power_supply_changed: battery");
	}

#else
	/* A9 reports USB charging when helf AC cable in and China AC charger. */
	/* notify userspace USB charging first,
	and then usb driver will notify AC while D+/D- Line short. */
	/* China AC detection:
	 * Write SMEM as USB first, and update SMEM to AC
	 * if receives AC notification */
	last_source = htc_batt_info.rep.charging_source;
	if (status == CHARGER_USB && g_usb_online == 0) {
		htc_set_smem_cable_type(CHARGER_USB);
		htc_batt_info.rep.charging_source = CHARGER_USB;
	} else {
		htc_set_smem_cable_type(status);
		htc_batt_info.rep.charging_source  = status;
		/* usb driver will not notify usb offline. */
		if (status == CHARGER_BATTERY && g_usb_online != 0)
			g_usb_online = 0;
	}

#ifdef CONFIG_USB_MSM_72K
	msm_hsusb_set_vbus_state(status == CHARGER_USB);
#endif

	if (htc_batt_info.rep.charging_source != last_source) {
#if 1 /* JH //this is for packet filter (notify port list while USB in/out) */
		update_port_list_charging_state(!(htc_batt_info.rep.charging_source == CHARGER_BATTERY));
#endif
		/* Lock suspend only when USB in for ADB or other USB functions. */
		if (htc_batt_info.rep.charging_source == CHARGER_USB) {
			wake_lock(&vbus_wake_lock);
		} else if (__htc_power_policy()) {
			/* Lock suspend for DOPOD charging animation */
			wake_lock(&vbus_wake_lock);
		} else {
			if (htc_batt_info.rep.charging_source == CHARGER_AC
				&& last_source == CHARGER_USB)
				BATT_ERR("%s: USB->AC", __func__);
			/* give userspace some time to see the uevent and update
			 * LED state or whatnot...
			 */
			wake_lock_timeout(&vbus_wake_lock, HZ * 5);
		}
		if (htc_batt_info.rep.charging_source == CHARGER_BATTERY || last_source == CHARGER_BATTERY)
			power_supply_changed(&htc_power_supplies[BATTERY_SUPPLY]);
		if (htc_batt_info.rep.charging_source == CHARGER_USB || last_source == CHARGER_USB)
			power_supply_changed(&htc_power_supplies[USB_SUPPLY]);
		if (htc_batt_info.rep.charging_source == CHARGER_AC || last_source == CHARGER_AC)
			power_supply_changed(&htc_power_supplies[AC_SUPPLY]);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_UEVT)
			BATT_LOG("power_supply_changed: %s -> %s",
				charger_tags[last_source], charger_tags[htc_batt_info.rep.charging_source]);
	}
#endif
	mutex_unlock(&htc_batt_info.lock);

	return rc;
}

#ifdef CONFIG_USB_ACCESSORY_DETECT_BY_ADC
int htc_get_usb_accessory_adc_level(uint32_t *buffer)
{
	struct rpc_request_hdr req;

	struct htc_get_usb_adc_value_rep {
		struct rpc_reply_hdr hdr;
		uint32_t adc_value;
	} rep;

	int rc;
	printk(KERN_INFO "%s\n", __func__);

	if (buffer == NULL) {
		printk(KERN_INFO "%s: buffer null\n", __func__);
		return -EINVAL;
	}

	rc = msm_rpc_call_reply(endpoint, HTC_PROCEDURE_GET_USB_ACCESSORY_ADC_LEVEL,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	if (rc < 0) {
		printk(KERN_INFO "%s: msm_rpc_call_reply fail\n", __func__);
		return rc;
	}
	*buffer 		= be32_to_cpu(rep.adc_value);

	printk(KERN_INFO "%s: adc = %d\n", __func__, *buffer);
	return 0;
}
EXPORT_SYMBOL(htc_get_usb_accessory_adc_level);
#endif

/* A9 reports USB charging when helf AC cable in and China AC charger. */
/* notify userspace USB charging first,
and then usb driver will notify AC while D+/D- Line short. */
static void usb_status_notifier_func(int online)
{
#if 1
	pr_info("batt:online=%d", online);
	/* TODO: replace charging_source to usb_status */
	htc_batt_info.rep.charging_source = online;
	htc_set_smem_cable_type(htc_batt_info.rep.charging_source);

	if (htc_battery_initial) {
		power_supply_changed(&htc_power_supplies[AC_SUPPLY]);
		power_supply_changed(&htc_power_supplies[USB_SUPPLY]);
		power_supply_changed(&htc_power_supplies[BATTERY_SUPPLY]);
	} else
		pr_err("\n\n ### htc_battery_code is not inited yet! ###\n\n");

	update_wake_lock(htc_batt_info.rep.charging_source);
#else
	mutex_lock(&htc_batt_info.lock);
	if (htc_batt_debug_mask & HTC_BATT_DEBUG_USB_NOTIFY)
		BATT_LOG("%s: online=%d, g_usb_online=%d", __func__, online, g_usb_online);
	if (g_usb_online != online) {
		g_usb_online = online;
		if (online == CHARGER_AC && htc_batt_info.rep.charging_source == CHARGER_USB) {
			mutex_unlock(&htc_batt_info.lock);
			htc_cable_status_update(CHARGER_AC);
			mutex_lock(&htc_batt_info.lock);
		}
	}
	mutex_unlock(&htc_batt_info.lock);
#endif
}

static u32 htc_get_batt_level(void)
{
	struct max8957_chip *max8957_chip = NULL;
	u32 level = 0;
	int rc = 0;

	if (unlikely(!max8957_fg)) {
		BATT_ERR("%s: max8957_fg is not initialized", __func__);
		return -EFAULT;
	}

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	rc = max8957_read(max8957_chip, MAX8957_FG_RepSOC, (u8 *)&level, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_RepSOC: 0x%x", __func__, rc);

	level = level / 256; /* High byte has unit 1 percent */

	if (level >= 100)
		level = 100;

	return level;
}

static int htc_get_batt_info(struct battery_info_reply *buffer)
{
	struct rpc_request_hdr req;

	struct htc_get_batt_info_rep {
		struct rpc_reply_hdr hdr;
		struct battery_info_reply info;
	} rep;

	struct max8957_chip *max8957_chip = NULL;
	int rc;
	s16 val;

	//BATT_LOG("%s:", __func__);

	if (buffer == NULL)
		return -EINVAL;

	rc = msm_rpc_call_reply(endpoint, HTC_PROCEDURE_GET_BATT_INFO,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	if (rc < 0) {
		BATT_ERR("%s: RPC HTC_PROCEDURE_GET_BATT_INFO failed, rc=%d", __func__, rc);
		/*
		return rc;
		*/
	}

	if (unlikely(!max8957_fg)) {
		BATT_ERR("%s: max8957_fg is not initialized", __func__);
		return -EFAULT;
	}

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	mutex_lock(&htc_batt_info.lock);
#if 1
	buffer->batt_id	= be32_to_cpu(rep.info.batt_id);
	/*
	buffer->batt_id = 1;
	*/

	buffer->batt_vol = 0;
	rc = max8957_read(max8957_chip, MAX8957_FG_VCELL, (u8 *)&buffer->batt_vol, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_VCELL: %d", __func__, rc);
	buffer->batt_vol = buffer->batt_vol * 20 / 256; /* VCELL unit is 78.125uV */
#if MAXIM_BATTERY_DEBUG
	pr_err("%s: batt_vol(MAX8957_FG_VCELL): %d(0x%x)\n", __func__, buffer->batt_vol, buffer->batt_vol);
#endif

	buffer->batt_temp = 0;
	rc = max8957_read(max8957_chip, MAX8957_FG_TEMP, (u8 *)&val, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_TEMP: %d", __func__, rc);
	buffer->batt_temp = (val / 256) * 10; /* High byte has unit 1 degree */

	if (debug_fake_temperature)
			if (buffer->batt_temp >= 680)
				buffer->batt_temp = 650;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: batt_temp(MAX8957_FG_TEMP): %d(0x%x)\n", __func__, buffer->batt_temp, buffer->batt_temp);
#endif

	buffer->batt_current = 0;
	rc = max8957_read(max8957_chip, MAX8957_FG_Current, (u8 *)&val, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_Current: %d", __func__, rc);
	buffer->batt_current = val * 5 / 32; /* (val * 1.25mA) >> 3 */
#if MAXIM_BATTERY_DEBUG
	pr_err("%s: batt_current(MAX8957_FG_Current): %d(0x%x)\n", __func__, buffer->batt_current, buffer->batt_current);
#endif

	buffer->level = htc_get_batt_level();

	/* JR:Workaround for capacity may greater than 100% */
	/*
	if (buffer->level > 100)
		buffer->level = 100;
	if (buffer->level == 100 && buffer->charging_enabled)
		buffer->level = 99;
	*/
#if MAXIM_BATTERY_DEBUG
	pr_err("%s: level(MAX8957_FG_RepSOC): %d(0x%x)\n", __func__, buffer->level, buffer->level);
#endif

	/* Move the rules of charging_source to cable_status_update. */
	/* buffer->charging_source 	= be32_to_cpu(rep.info.charging_source); */

	/* JR: Skip setting charging_enabled */
	/*
	buffer->charging_enabled 	= be32_to_cpu(rep.info.charging_enabled);
	*/

	buffer->full_bat = 0;
	rc = max8957_read(max8957_chip, MAX8957_FG_FullCAP, (u8 *)&buffer->full_bat, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_FullCAP: %d", __func__, rc);
	buffer->full_bat = buffer->full_bat / 2; /* full_bat * 0.5mAH * 1000 */

	buffer->avg_current = 0;
	rc = max8957_read(max8957_chip, MAX8957_FG_AvgCurrent, (u8 *)&val, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_Current: %d", __func__, rc);
	buffer->avg_current = val * 5 / 32; /* (val * 1.25mA) >> 3 */

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: full_bat(MAX8957_FG_FullCAP): %d(0x%x)\n", __func__, buffer->full_bat, buffer->full_bat);
#endif

	/* Over_vchg only update in SMEM from A9 */
	/* buffer->over_vchg 		= be32_to_cpu(rep.info.over_vchg); */
#else
	/* Fake battery info */
	buffer->batt_id 		= 1;
	buffer->batt_vol 		= 4200;
	buffer->batt_temp 		= 250;
	buffer->batt_current 	= 100;
	buffer->level 			= 99;
	/* Move the rules of charging_source to cable_status_update. */
	/* buffer->charging_source 	= be32_to_cpu(rep.info.charging_source); */
	buffer->charging_enabled 	= 0;
	buffer->full_bat 		= 1440000;
	/* Over_vchg only update in SMEM from A9 */
	/* buffer->over_vchg 		= be32_to_cpu(rep.info.over_vchg); */
#endif
	mutex_unlock(&htc_batt_info.lock);

	if (htc_batt_debug_mask & HTC_BATT_DEBUG_A2M_RPC)
		BATT_LOG("A2M_RPC: ID=%d, level=%d, vol=%d, temp=%d, "
			"batt_current=%d, avg_current=%d, chg_src=%d, "
			"chg_en=%d, full_bat=%d",
			buffer->batt_id, buffer->level, buffer->batt_vol, buffer->batt_temp,
			buffer->batt_current, buffer->avg_current, buffer->charging_source,
			buffer->charging_enabled, buffer->full_bat);

	return 0;
}

#ifdef CONFIG_HTC_BATTCHG_SMEM
struct htc_batt_info_full {
	u32 batt_id;
	u32 batt_vol;
	u32 batt_vol_last;
	u32 batt_temp;
	s32 batt_current;
	s32 batt_current_last;
	u32 batt_discharge_current;

	u32 VREF_2;
	u32 VREF;
	u32 ADC4096_VREF;

	u32 Rtemp;
	s32 Temp;
	s32 Temp_last;

	u32 pd_M;
	u32 MBAT_pd;
	s32 I_MBAT;

	u32 pd_temp;
	u32 percent_last;
	u32 percent_update;
	u32 dis_percent;

	u32 vbus;
	u32 usbid;
	u32 charging_source;

	u32 MBAT_IN;
	u32 full_bat;

	u32 eval_current;
	u32 eval_current_last;
	u32 charging_enabled;

	u32 timeout;
	u32 fullcharge;
	u32 level;
	u32 delta;

	u32 chg_time;
	s32 level_change;
	u32 sleep_timer_count;
	u32 OT_led_on;
	u32 overloading_charge;

	u32 a2m_cable_type;
	u32 vchg;	/* VCHG => 0: Not, 1: In */
	u32 over_vchg;	/*over voltage charger detection, 0:VCHG normal(below 6V) 1:VCHG over(upper 6V)*/
	u32 reserve4;
	u32 DETAILS_00;
	u32 DETAILS_01;
	u32 INT_OK;
	u32 CNFG_00;
	u32 vbus_vol;
	u32 INI_flag;
	u32 CHG_INT;
	u32 CNFG_09;
};

/* SMEM_BATT_INFO is allocated by A9 after first A2M RPC is sent. */
static struct htc_batt_info_full *smem_batt_info;

static int htc_get_batt_info_smem(struct battery_info_reply *buffer)
{
	if (!smem_batt_info) {
		smem_batt_info = smem_alloc(SMEM_BATT_INFO,
			sizeof(struct htc_batt_info_full));
		if (!smem_batt_info) {
			BATT_ERR("battery SMEM allocate fail, "
				"use RPC instead of");
			return htc_get_batt_info(buffer);
		}
	}

	if (!buffer)
		return -EINVAL;

	mutex_lock(&htc_batt_info.lock);
	buffer->batt_id = smem_batt_info->batt_id;
	/*
	buffer->batt_vol = smem_batt_info->batt_vol;
	buffer->batt_temp = smem_batt_info->Temp;
	buffer->batt_current = smem_batt_info->batt_current;
	buffer->eval_current = smem_batt_info->eval_current;
	*/
	/* Fix issue that recharging percent drop to 99%. */
	/* The level in SMEM is for A9 internal use,
	 * always use value reported by M2A level update RPC. */
#if 0
	buffer->level 	= smem_batt_info->percent_update;
#endif
	/* Move the rules of charging_source to cable_status_update. */
	/* buffer->charging_source 	= be32_to_cpu(smem_batt_info->charging_source); */

	/* JR: Skip setting charging_enabled */
	/*
	buffer->charging_source = CHARGER_USB;
	buffer->full_bat = smem_batt_info->full_bat;
	buffer->over_vchg = smem_batt_info->over_vchg;
	*/
	buffer->charging_enabled = smem_batt_info->charging_enabled;
	mutex_unlock(&htc_batt_info.lock);

#if 0
	if (htc_batt_debug_mask & HTC_BATT_DEBUG_SMEM)
		BATT_LOG("ID=%d, level=%d, vol=%d, temp=%d, "
			"batt_current=%d, avg_current=%d, chg_src=%d, "
			"chg_en=%d, full_bat=%d, over_vchg=%d",
			buffer->batt_id,
			buffer->level,
			buffer->batt_vol,
			buffer->batt_temp,
			buffer->batt_current,
			buffer->avg_current,
			buffer->charging_source,
			buffer->charging_enabled,
			buffer->full_bat,
			buffer->over_vchg);
#endif

	return 0;
}

static int htc_update_batt_info_smem(struct battery_info_reply *buffer)
{
	if (!smem_batt_info) {
		smem_batt_info = smem_alloc(SMEM_BATT_INFO,
			sizeof(struct htc_batt_info_full));
		if (!smem_batt_info) {
			BATT_ERR("battery SMEM allocate fail");
			return -ENOMEM;
		}
	}

	if (!buffer)
		return -EINVAL;

	mutex_lock(&htc_batt_info.lock);
	/*
	smem_batt_info->batt_id = buffer->batt_id;
	*/
	smem_batt_info->batt_vol = buffer->batt_vol;
	smem_batt_info->Temp = buffer->batt_temp;
	smem_batt_info->batt_current = buffer->batt_current;
	smem_batt_info->eval_current = buffer->eval_current;
	smem_batt_info->percent_update = buffer->level;
	/* Move the rules of charging_source to cable_status_update. */
	/* buffer->charging_source 	= be32_to_cpu(smem_batt_info->charging_source); */
	/*
	smem_batt_info->charging_enabled = buffer->charging_enabled;
	*/
	smem_batt_info->full_bat = buffer->full_bat;
	smem_batt_info->over_vchg = buffer->over_vchg;
	mutex_unlock(&htc_batt_info.lock);

	if (htc_batt_debug_mask & HTC_BATT_DEBUG_SMEM)
		BATT_LOG("smem_vbus=%d, DTLS_0=%d, DTLS_1=%d, INT_OK=%d, CNFG_0=%d, CNFG_9=%d, vbus_vol=%d, CHG_INT=%d",
			smem_batt_info->vbus,
			smem_batt_info->DETAILS_00,
			smem_batt_info->DETAILS_01,
			smem_batt_info->INT_OK,
			smem_batt_info->CNFG_00,
			smem_batt_info->CNFG_09,
			smem_batt_info->vbus_vol,
			smem_batt_info->CHG_INT);

	return 0;
}

static ssize_t htc_battery_show_smem(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int len = 0;

	if (!smem_batt_info) {
		smem_batt_info = smem_alloc(SMEM_BATT_INFO,
			sizeof(struct htc_batt_info_full));
		if (!smem_batt_info) {
			BATT_ERR("Show SMEM: allocate fail");
			return 0;
		}
	}

	if (!strcmp(attr->attr.name, "smem_raw")) {
		len = sizeof(struct htc_batt_info_full);
		memcpy(buf, smem_batt_info, len);
	} else if (!strcmp(attr->attr.name, "smem_text")) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"batt_id: %d\n"
			"batt_vol: %d\n"
			"batt_vol_last: %d\n"
			"batt_temp: %d\n"
			"batt_current: %d\n"
			"batt_current_last: %d\n"
			"batt_discharge_current: %d\n"
			"VREF_2: %d\n"
			"VREF: %d\n"
			"ADC4096_VREF: %d\n"
			"Rtemp: %d\n"
			"Temp: %d\n"
			"Temp_last: %d\n"
			"pd_M: %d\n"
			"MBAT_pd: %d\n"
			"I_MBAT: %d\n"
			"pd_temp: %d\n"
			"percent_last: %d\n"
			"percent_update: %d\n"
			"dis_percent: %d\n"
			"vbus: %d\n"
			"usbid: %d\n"
			"charging_source: %d\n"
			"MBAT_IN: %d\n"
			"full_bat: %d\n"
			"eval_current: %d\n"
			"eval_current_last: %d\n"
			"charging_enabled: %d\n"
			"timeout: %d\n"
			"fullcharge: %d\n"
			"level: %d\n"
			"delta: %d\n"
			"chg_time: %d\n"
			"level_change: %d\n"
			"sleep_timer_count: %d\n"
			"OT_led_on: %d\n"
			"overloading_charge: %d\n"
			"a2m_cable_type: %d\n"
			"vchg: %d\n"
			"over_vchg: %d\n"
			"DETAILS_00: %d\n"
			"DETAILS_01: %d\n"
			"INT_OK: %d\n"
			"CNFG_00: %d\n"
			"CNFG_09: %d\n"
			"vbus_vol: %d\n",
			smem_batt_info->batt_id,
			smem_batt_info->batt_vol,
			smem_batt_info->batt_vol_last,
			smem_batt_info->batt_temp,
			smem_batt_info->batt_current,
			smem_batt_info->batt_current_last,
			smem_batt_info->batt_discharge_current,
			smem_batt_info->VREF_2,
			smem_batt_info->VREF,
			smem_batt_info->ADC4096_VREF,
			smem_batt_info->Rtemp,
			smem_batt_info->Temp,
			smem_batt_info->Temp_last,
			smem_batt_info->pd_M,
			smem_batt_info->MBAT_pd,
			smem_batt_info->I_MBAT,
			smem_batt_info->pd_temp,
			smem_batt_info->percent_last,
			smem_batt_info->percent_update,
			smem_batt_info->dis_percent,
			smem_batt_info->vbus,
			smem_batt_info->usbid,
			smem_batt_info->charging_source,
			smem_batt_info->MBAT_IN,
			smem_batt_info->full_bat,
			smem_batt_info->eval_current,
			smem_batt_info->eval_current_last,
			smem_batt_info->charging_enabled,
			smem_batt_info->timeout,
			smem_batt_info->fullcharge,
			smem_batt_info->level,
			smem_batt_info->delta,
			smem_batt_info->chg_time,
			smem_batt_info->level_change,
			smem_batt_info->sleep_timer_count,
			smem_batt_info->OT_led_on,
			smem_batt_info->overloading_charge,
			smem_batt_info->a2m_cable_type,
			smem_batt_info->vchg,
			smem_batt_info->over_vchg,
			smem_batt_info->DETAILS_00,
			smem_batt_info->DETAILS_01,
			smem_batt_info->INT_OK,
			smem_batt_info->CNFG_00,
			smem_batt_info->CNFG_09,
			smem_batt_info->vbus_vol);
	}

	return len;
}

static int htc_set_smem_cable_type(u32 cable_type)
{
	if (!smem_batt_info) {
		smem_batt_info = smem_alloc(SMEM_BATT_INFO,
				sizeof(struct htc_batt_info_full));
		if (!smem_batt_info) {
			BATT_ERR("Update SMEM: allocate fail");
			return -EINVAL;
		}
	}

	smem_batt_info->a2m_cable_type = cable_type;
	BATT_LOG("Update SMEM: cable type %d", cable_type);

	return 0;
}
#endif

static ssize_t htc_battery_show_batt_attr(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	switch (htc_batt_info.gauge_driver) {
	case GAUGE_MAX8957:
#ifdef CONFIG_HTC_BATTCHG_SMEM
		return htc_battery_show_smem(dev, attr, buf);
#endif
		break;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int htc_power_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	enum charger_type_t charger;

	mutex_lock(&htc_batt_info.lock);

	charger = htc_batt_info.rep.charging_source;
	/* ARM9 decides charging_enabled value by battery id */
	/* if (htc_batt_info.rep.batt_id == 255)
		charger = CHARGER_BATTERY; */

	mutex_unlock(&htc_batt_info.lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
			if (charger == CHARGER_AC || charger == CHARGER_SUPER_AC)
				val->intval = 1;
			else
				val->intval = 0;
			if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
				BATT_LOG("%s: %s: online=%d", __func__, psy->name, val->intval);
		} else if (psy->type == POWER_SUPPLY_TYPE_USB) {
			val->intval = (charger ==  CHARGER_USB ? 1 : 0);
			if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
				BATT_LOG("%s: %s: online=%d", __func__, psy->name, val->intval);
		} else if (psy->type == POWER_SUPPLY_TYPE_WIRELESS) {
			val->intval = (charger ==  CHARGER_WIRELESS ? 1 : 0);
			if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
				BATT_LOG("%s: %s: online=%d", __func__, psy->name, val->intval);
		} else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Once charge full, set this flag */
static int htc_charge_full = 0;

static int htc_battery_get_charging_status(void)
{
	u32 level;
	enum charger_type_t charger;
	int ret;

	mutex_lock(&htc_batt_info.lock);

	charger = htc_batt_info.rep.charging_source;

	/* ARM9 decides charging_enabled value by battery id */
	if (htc_batt_info.rep.batt_id == 255)
		charger = CHARGER_UNKNOWN;

	switch (charger) {
	case CHARGER_BATTERY:
		htc_charge_full = 0;
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
	case CHARGER_SUPER_AC:
	case CHARGER_WIRELESS:
/* #if !defined(CONFIG_BATTERY_DS2746) */
#if 0
		if ((htc_charge_full) && (htc_batt_info.rep.full_level == 100))
			htc_batt_info.rep.level = 100;
		pr_err("%s: htc_charge_full=%d, full_level=%d\n", __func__, htc_charge_full, htc_batt_info.rep.full_level);
#endif
		level = curr_percent;

		if (level == 100)
			htc_charge_full = 1;
		else
			htc_charge_full = 0;

		if (htc_charge_full)
			ret = POWER_SUPPLY_STATUS_FULL;
		else if (htc_batt_info.rep.charging_enabled != 0)
			ret = POWER_SUPPLY_STATUS_CHARGING;
		else
			ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}
	mutex_unlock(&htc_batt_info.lock);

	return ret;
}

static int htc_battery_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = htc_battery_get_charging_status();
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
			BATT_LOG("%s: %s: status=%d", __func__, psy->name, val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		if (machine_is_paradise()) {
			if (htc_batt_info.rep.batt_temp >= 500 ||
				htc_batt_info.rep.batt_temp <= 0)
				val->intval =  POWER_SUPPLY_HEALTH_OVERHEAT;
		} else if (machine_is_spade() || machine_is_flyer()) {
			if (htc_batt_info.rep.batt_temp >= 450 ||
				htc_batt_info.rep.batt_temp <= 0)
				val->intval =  POWER_SUPPLY_HEALTH_OVERHEAT;
		} else {
			if (htc_batt_info.rep.batt_temp >= 480 ||
				htc_batt_info.rep.batt_temp <= 0)
				val->intval =  POWER_SUPPLY_HEALTH_OVERHEAT;
		}

		if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
			BATT_LOG("%s: %s: health=%d", __func__, psy->name, val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = htc_batt_info.present;
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
			BATT_LOG("%s: %s: present=%d", __func__, psy->name, val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
			BATT_LOG("%s: %s: technology=%d", __func__, psy->name, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&htc_batt_info.lock);
		val->intval = curr_percent;
		/* prevent shutdown before battery driver ready. */
		if (htc_batt_info.device_id == 0)
			val->intval = 55; /* 55 == ?? */
		if (test_power_monitor)
			val->intval = 77;
		mutex_unlock(&htc_batt_info.lock);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
			BATT_LOG("%s: %s: capacity=%d", __func__, psy->name, val->intval);
		break;
	case POWER_SUPPLY_PROP_AVGCURRENT:
		mutex_lock(&htc_batt_info.lock);
		val->intval = htc_batt_info.rep.avg_current;
		mutex_unlock(&htc_batt_info.lock);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY)
			BATT_LOG("%s: %s: avg_current=%d", __func__, psy->name, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* The *owner field is no longer used since x86 tree has been cleaned up. */
#define HTC_BATTERY_ATTR(_name)							\
{										\
	.attr = { .name = #_name, .mode = S_IRUGO},	\
	.show = htc_battery_show_property,					\
	.store = NULL,								\
}

static struct device_attribute htc_battery_attrs[] = {
	HTC_BATTERY_ATTR(batt_id),
	HTC_BATTERY_ATTR(batt_vol),
	HTC_BATTERY_ATTR(batt_temp),
	HTC_BATTERY_ATTR(batt_current),
	HTC_BATTERY_ATTR(charging_source),
	HTC_BATTERY_ATTR(charging_enabled),
	HTC_BATTERY_ATTR(full_bat),
	HTC_BATTERY_ATTR(over_vchg),
	HTC_BATTERY_ATTR(batt_state),
/*[FIXME]__ATTR(batt_attr_raw, S_IRUGO, htc_battery_show_batt_attr, NULL),*/
#ifdef CONFIG_HTC_BATTCHG_SMEM
	__ATTR(smem_raw, S_IRUGO, htc_battery_show_smem, NULL),
	__ATTR(smem_text, S_IRUGO, htc_battery_show_smem, NULL),
#endif
	__ATTR(batt_attr_text, S_IRUGO, htc_battery_show_batt_attr, NULL),
};

enum {
	BATT_ID = 0,
	BATT_VOL,
	BATT_TEMP,
	BATT_CURRENT,
	CHARGING_SOURCE,
	CHARGING_ENABLED,
	FULL_BAT,
	OVER_VCHG,
	BATT_STATE,
};

static int htc_rpc_set_delta(unsigned delta)
{
	struct set_batt_delta_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	req.data = cpu_to_be32(delta);
	return msm_rpc_call(endpoint, HTC_PROCEDURE_SET_BATT_DELTA,
			    &req, sizeof(req), 5 * HZ);
}

static int htc_rpc_charger_switch(unsigned enable)
{
	int ret = 0;
	struct charger_switch_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	BATT_LOG("%s: switch charger to mode: %u", __func__, enable);
	if (enable == ENABLE_LIMIT_CHARGER)
		/*
		return tps_set_charger_ctrl(ENABLE_LIMITED_CHG);
		*/
		return 0;
	else if (enable == DISABLE_LIMIT_CHARGER)
		/*
		return tps_set_charger_ctrl(CLEAR_LIMITED_CHG);
		*/
		return 0;
	else {
		req.data = cpu_to_be32(enable);
		ret = msm_rpc_call(endpoint, HTC_PROCEDURE_CHARGER_SWITCH,
					&req, sizeof(req), 5 * HZ);
		if (ret < 0)
			BATT_ERR("%s: msm_rpc_call failed (%d)!", __func__, ret);
		power_supply_changed(&htc_power_supplies[CHARGER_BATTERY]);
	}
	return ret;
}

static int htc_rpc_set_full_level(unsigned level)
{
	struct set_batt_full_level_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	req.data = cpu_to_be32(level);
	return msm_rpc_call(endpoint, HTC_PROCEDURE_SET_FULL_LEVEL,
			    &req, sizeof(req), 5 * HZ);
}

static ssize_t htc_battery_set_delta(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int rc;
	unsigned long delta = 0;

	/*
	delta = simple_strtoul(buf, NULL, 10);
	*/
	int ret = strict_strtoul(buf, 10, &delta);

	if (ret) {
		BATT_ERR("%s: Failed to convert string, err=%d\n", __func__, ret);
		return -EINVAL;
	}

	if (delta > 100)
		return -EINVAL;

	mutex_lock(&htc_batt_info.rpc_lock);
	rc = htc_rpc_set_delta(delta);
	mutex_unlock(&htc_batt_info.rpc_lock);
	if (rc < 0)
		return rc;
	return count;
}

/*
*	For PA and QA test
*	0x10-> fake temp to 250
*	0x11->TBD if needed
*	0x12->TBD if needed
*	....
*/
static ssize_t htc_battery_debug_flag(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long debug_flag = 0;
	/*
	debug_flag = simple_strtoul(buf, NULL, 10);
	*/
	int ret = strict_strtoul(buf, 10, &debug_flag);

	if (ret) {
		BATT_ERR("%s: Failed to convert string, err=%d\n", __func__, ret);
		return -EINVAL;
	}

	if (debug_flag > 100 || debug_flag == 0)
		return -EINVAL;

	mutex_lock(&htc_batt_info.lock);
	blocking_notifier_call_chain(&cable_status_notifier_list,
		debug_flag, 0);
	mutex_unlock(&htc_batt_info.lock);
	return 0;

}

int htc_battery_charger_disable()
{
	int rc;

	mutex_lock(&htc_batt_info.rpc_lock);
	rc = htc_rpc_charger_switch(0);
	mutex_unlock(&htc_batt_info.rpc_lock);
	return rc;
}

static ssize_t htc_battery_charger_switch(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int rc;
	unsigned long enable = 0;
	/*
	enable = simple_strtoul(buf, NULL, 10);
	*/
	int ret = strict_strtoul(buf, 10, &enable);

	if (ret) {
		BATT_ERR("%s: Failed to convert string, err=%d\n", __func__, ret);
		return -EINVAL;
	}

	if (enable >= END_CHARGER)
		return -EINVAL;

	mutex_lock(&htc_batt_info.rpc_lock);
	rc = htc_rpc_charger_switch(enable);
	mutex_unlock(&htc_batt_info.rpc_lock);
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t htc_battery_charger_timer(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	unsigned long time_out = 0;
	ktime_t interval;
	ktime_t next_alarm;

	time_out = simple_strtoul(buf, NULL, 10);

	if (time_out > 65536)
		return -EINVAL;

	if (time_out > 0) {
		rc = htc_rpc_charger_switch(STOP_CHARGER);
		if (rc < 0)
			return rc;
		interval = ktime_set(time_out, 0);
		next_alarm = ktime_add(alarm_get_elapsed_realtime(), interval);
		alarm_start_range(&batt_charger_ctrl_alarm,
					next_alarm, next_alarm);
		charger_ctrl_stat = STOP_CHARGER;
	} else if (time_out == 0) {
		rc = htc_rpc_charger_switch(ENABLE_CHARGER);
		if (rc < 0)
			return rc;
		alarm_cancel(&batt_charger_ctrl_alarm);
		charger_ctrl_stat = ENABLE_CHARGER;
	}

	return count;
}

static ssize_t htc_battery_set_full_level(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int rc = 0;
	unsigned long percent = 100;

	percent = simple_strtoul(buf, NULL, 10);

	if (percent > 100 || percent == 0)
		return -EINVAL;

	switch (htc_batt_info.gauge_driver) {
	case GUAGE_MODEM:
		mutex_lock(&htc_batt_info.rpc_lock);
		htc_batt_info.rep.full_level = percent;
		rc = htc_rpc_set_full_level(percent);
		mutex_unlock(&htc_batt_info.rpc_lock);
		break;
	case GAUGE_MAX8957:
		if (htc_full_level_flag == 0) {
			mutex_lock(&htc_batt_info.lock);
			htc_full_level_flag = 1;
			htc_batt_info.rep.full_level = percent;
			rc = htc_rpc_set_full_level(percent);
			blocking_notifier_call_chain(&cable_status_notifier_list,
				0xff, (void *) &htc_batt_info.rep.full_level);
			mutex_unlock(&htc_batt_info.lock);
		}
		rc = 0;
		break;
	}
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t htc_battery_set_phone_call(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long phone_call = 0;
	int rc = 0;
	rc = strict_strtoul(buf, 10, &phone_call);
	if (rc)
		return rc;

	if (phone_call)
		htc_batt_phone_call = 1;
	else
		htc_batt_phone_call = 0;

	BATT_LOG("%s , set context phone_call=%lu\n",__func__ ,phone_call);
	return count;
}

static struct device_attribute htc_set_delta_attrs[] = {
	__ATTR(delta, S_IWUSR | S_IWGRP, NULL, htc_battery_set_delta),
	__ATTR(full_level, S_IWUSR | S_IWGRP, NULL, htc_battery_set_full_level),
	__ATTR(batt_debug_flag, S_IWUSR | S_IWGRP, NULL, htc_battery_debug_flag),
	__ATTR(charger_control, S_IWUSR | S_IWGRP, NULL, htc_battery_charger_switch),
	__ATTR(charger_timer, S_IWUSR | S_IWGRP, NULL, htc_battery_charger_timer),
	__ATTR(phone_call, S_IWUGO, NULL, htc_battery_set_phone_call),
};

static int htc_battery_create_attrs(struct device *dev)
{
	int i = 0, j = 0, rc = 0;

	for (i = 0; i < ARRAY_SIZE(htc_battery_attrs); i++) {
		rc = device_create_file(dev, &htc_battery_attrs[i]);
		if (rc)
			goto htc_attrs_failed;
	}

	for (j = 0; j < ARRAY_SIZE(htc_set_delta_attrs); j++) {
		rc = device_create_file(dev, &htc_set_delta_attrs[j]);
		if (rc)
			goto htc_delta_attrs_failed;
	}

	goto succeed;

htc_attrs_failed:
	while (i--)
		device_remove_file(dev, &htc_battery_attrs[i]);
htc_delta_attrs_failed:
	while (j--)
		device_remove_file(dev, &htc_set_delta_attrs[j]);
succeed:
	return rc;
}

static int update_batt_info(void)
{
	int ret = 0;

	/* FIXME */
	switch (htc_batt_info.gauge_driver) {
	case GAUGE_MAX8957:
		if (htc_get_batt_info(&htc_batt_info.rep) < 0) {
			BATT_ERR("%s: Get batt_info failed!!!", __func__);
			ret = -1;
		}
#ifdef CONFIG_HTC_BATTCHG_SMEM
		if (htc_get_batt_info_smem(&htc_batt_info.rep) < 0) {
			BATT_ERR("%s: smem read failed!!!", __func__);
			ret = -1;
		}
#endif
		/* Update batt_into to SMEM for sharing with modem side */
		if (htc_update_batt_info_smem(&htc_batt_info.rep) < 0) {
			BATT_ERR("%s: smem update failed!!!", __func__);
			ret = -1;
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static ssize_t htc_battery_show_property(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - htc_battery_attrs;

	/* rpc lock is used to prevent two threads from calling
	 * into the get info rpc at the same time
	 */

	mutex_lock(&htc_batt_info.rpc_lock);
	/* check cache time to decide if we need to update */
	if (htc_batt_info.update_time &&
			time_before(jiffies, htc_batt_info.update_time +
			msecs_to_jiffies(cache_time))) {
		BATT_LOG("%s: use cached values", __func__);
			goto dont_need_update;
	}

	if (!update_batt_info())
		htc_batt_info.update_time = jiffies;

dont_need_update:
	mutex_unlock(&htc_batt_info.rpc_lock);

	mutex_lock(&htc_batt_info.lock);
	switch (off) {
	case BATT_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_id);
		break;
	case BATT_VOL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_vol);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_temp);
		break;
	case BATT_CURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_current);
		break;
	case CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.charging_source);
		break;
	case CHARGING_ENABLED:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.charging_enabled);
		break;
	case FULL_BAT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.full_bat);
		break;
	case OVER_VCHG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.over_vchg);
		break;
	case BATT_STATE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   htc_batt_info.rep.batt_state);
		break;
	default:
		i = -EINVAL;
	}
	mutex_unlock(&htc_batt_info.lock);

	if (htc_batt_debug_mask & HTC_BATT_DEBUG_USER_QUERY) {
		if (i < 0)
			BATT_LOG("%s: battery: attribute is not supported: %d", __func__, off);
		else
			BATT_LOG("%s: battery: %s=%s", __func__, attr->attr.name, buf);
	}
	return i;
}

static void batt_charger_ctrl_func(struct work_struct *work)
{
	int rc;

	rc = htc_rpc_charger_switch(ENABLE_CHARGER);

	if (rc)
		return;

	charger_ctrl_stat = (unsigned int)ENABLE_CHARGER;
}

static void batt_charger_ctrl_alarm_handler(struct alarm *alarm)
{
	BATT_LOG("charger control alarm is timeout.");

	queue_work(batt_charger_ctrl_wq, &batt_charger_ctrl_work);
}

static int max8957_fg_set_alert(enum max8957_fg_alert_type type, s16 min, s16 max)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);
	u8 reg_addr = 0;
	u16 reg_val = 0;
	int rc = 0;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: type=%d, min=%d, max=%d\n", __func__, type, min, max);
#endif

	if (min > max) {
		BATT_LOG("%s: Invalid parameter, min(%d) > max(%d)", __func__, min, max);
		return -EINVAL;
	}

	if (type == MAX8957_FG_ALERT_VOLTAGE) {
		reg_addr = MAX8957_FG_VALRT_Th;
		reg_val = (((max / 20) << 8) & 0xFF00) | ((min / 20) & 0x00FF);
	} else if (type == MAX8957_FG_ALERT_TEMPERATURE) {
		if ((min > 127 || min < -128) || (max > 127 || max < -128)) {
			BATT_LOG("%s: Invalid parameter, temperature range -128 ~ 127", __func__);
			return -EINVAL;
		}
		reg_addr = MAX8957_FG_TALRT_Th;
		reg_val = ((max << 8) & 0xFF00) | (min & 0x00FF);
	} else if (type == MAX8957_FG_ALERT_CAPACITY) {
		reg_addr = MAX8957_FG_SALRT_Th;
		reg_val = ((max << 8) & 0xFF00) | (min & 0x00FF);
	} else
		BATT_LOG("%s: Invalid alert type(%d)", __func__, type);

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: reg_addr=0x%x, reg_val=0x%x\n", __func__, reg_addr, reg_val);
#endif

	rc = max8957_write(max8957_chip, reg_addr, (u8 *)&reg_val, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to set reg(0x%x) with value 0x%x, rc=%d",
			__func__, reg_addr, reg_val, rc);

	return rc;
}

/* Temperature alarm handler */
static void max8957_fg_talert_handler(enum max8957_fg_alert_threshold_type thres_type)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);
	s16 val = 0;
	s32 batt_temp = 0;
	int rc = 0;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: type=%d\n", __func__, thres_type);
#endif

	rc = max8957_read(max8957_chip, MAX8957_FG_TEMP, (u8 *)&val, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_TEMP: %d", __func__, rc);
	batt_temp = val / 256; /* High byte has unit 1 degree */

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: batt_temp(MAX8957_FG_TEMP): %d(0x%x)\n", __func__, batt_temp, batt_temp);
#endif
}

/* Voltage alarm handler */
static void max8957_fg_valert_handler(enum max8957_fg_alert_threshold_type thres_type)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);
	u32 batt_vol = 0;
	int rc = 0;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: type=%d\n", __func__, thres_type);
#endif

	rc = max8957_read(max8957_chip, MAX8957_FG_VCELL, (u8 *)&batt_vol, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_VCELL: %d", __func__, rc);
	batt_vol = batt_vol * 20 / 256; /* VCELL unit is 78.125uV */

	force_shutdown = 1; //To force shutdown
	BATT_LOG("Voltage alarm:%d , force shutdown -> %d\n", batt_vol, force_shutdown);

	htc_battery_level_update_work_func();

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: batt_vol(MAX8957_FG_VCELL): %d(0x%x)\n", __func__, batt_vol, batt_vol);
#endif
}

/* Capacity alarm handler */
static void max8957_fg_salert_handler(enum max8957_fg_alert_threshold_type thres_type)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);
	u32 level = 0;
	int rc = 0;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: type=%d\n", __func__, thres_type);
#endif

	rc = max8957_read(max8957_chip, MAX8957_FG_RepSOC, (u8 *)&level, 2);
	if (unlikely(rc < 0))
		BATT_ERR("%s: Failed to read MAX8957_FG_RepSOC: 0x%x", __func__, rc);
	level = level / 256; /* High byte has unit 1 percent */

#if MAXIM_BATTERY_DEBUG
	pr_err("%s: level(MAX8957_FG_RepSOC): %d(0x%x)\n", __func__, level, level);
#endif
}

void max8957_fg_alert_handler(enum max8957_fg_alert_type type, enum max8957_fg_alert_threshold_type thres_type)
{
	switch (type) {
	case MAX8957_FG_ALERT_VOLTAGE:
		max8957_fg_valert_handler(thres_type);
		break;
	case MAX8957_FG_ALERT_TEMPERATURE:
		max8957_fg_talert_handler(thres_type);
		break;
	case MAX8957_FG_ALERT_CAPACITY:
		max8957_fg_salert_handler(thres_type);
		break;
	default:
		BATT_ERR("%s:Unknown alert type(%d)", __func__, type);
		break;
	}

	return;
}
EXPORT_SYMBOL(max8957_fg_alert_handler);


static void maxim8957_program_alarm(struct maxim8957_alarm *di, int seconds)
{
	ktime_t low_interval = ktime_set(seconds, 0);
	ktime_t slack = ktime_set(1, 0);
	ktime_t next;
	unsigned long now = get_seconds();

	di->last_poll = alarm_get_elapsed_realtime();
	next = ktime_add(di->last_poll, low_interval);

	BATT_LOG("%s -> seconds: %d", __func__,seconds);
	delta_time_sec = seconds;
	alarm_start_range(&di->alarm, next, ktime_add(next, slack));

	BATT_LOG("%s: last_poll=%lld + %d s = %lld; now = %ld",
		__func__, ktime_to_ns(di->last_poll), seconds, ktime_to_ns(next), now);
}

static void maxim8957_battery_alarm_handler(struct alarm *alarm)
{
	struct maxim8957_alarm *di = container_of(alarm, struct maxim8957_alarm, alarm);
	wake_lock(&di->work_wake_lock);
	queue_delayed_work(di->monitor_wqueue, &di->level_update_work, 0);
}

static int htc_battery_core_probe(struct platform_device *pdev)
{
	int i, rc;

	/* init battery gpio */
	if (htc_batt_info.charger == LINEAR_CHARGER) {
#if 0
		rc = init_batt_gpio();
		if (rc < 0) {
			BATT_ERR("%s: init battery gpio failed!", __func__);
			return rc;
		}
#endif
	}

	/* init structure data member */
	htc_batt_info.update_time 	= jiffies;
	/* A9 will shutdown the phone if battery is pluged out, so this value is always 1.
	htc_batt_info.present 		= gpio_get_value(GPIO_TROUT_MBAT_IN);
	*/
	htc_batt_info.present 		= 1;

	/* init rpc */
	endpoint = msm_rpc_connect(APP_BATT_PROG, APP_BATT_VER, 0);
	if (IS_ERR(endpoint)) {
		BATT_ERR("%s: init rpc failed! rc = %ld",
		       __func__, PTR_ERR(endpoint));
		return -EINVAL;
	}

	/* init power supplier framework */
	for (i = 0; i < ARRAY_SIZE(htc_power_supplies); i++) {
		rc = power_supply_register(&pdev->dev, &htc_power_supplies[i]);
		if (rc)
			BATT_ERR("%s: Failed to register power supply (%d)", __func__, rc);
	}

	/* create htc detail attributes */
	htc_battery_create_attrs(htc_power_supplies[BATTERY_SUPPLY].dev);

	/* After battery driver gets initialized, send rpc request to inquiry
	 * the battery status in case of we lost some info
	 */
	htc_battery_initial = 1;

	mutex_lock(&htc_batt_info.rpc_lock);
	htc_batt_info.rep.charging_source = CHARGER_BATTERY;
	if (htc_get_batt_info(&htc_batt_info.rep) < 0)
		BATT_ERR("%s: get info failed", __func__);

	if (htc_rpc_set_delta(1) < 0)
		BATT_ERR("%s: set delta failed", __func__);
	htc_batt_info.update_time = jiffies;
	mutex_unlock(&htc_batt_info.rpc_lock);

	htc_batt_info.rep.batt_state = 0;
	return 0;
}

static struct platform_driver htc_battery_core_driver = {
	.probe	= htc_battery_core_probe,
	.driver	= {
		.name	= APP_BATT_PDEV_NAME,
		.owner	= THIS_MODULE,
	},
};

/* batt_mtoa server definitions */
#define BATT_MTOA_PROG				0x30100000
#define BATT_MTOA_VERS				0
#define RPC_BATT_MTOA_NULL			0
#define RPC_BATT_MTOA_SET_CHARGING_PROC		1
#define RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC 	2
#define RPC_BATT_MTOA_LEVEL_UPDATE_PROC		3

struct rpc_batt_mtoa_set_charging_args {
	int enable;
};

struct rpc_batt_mtoa_cable_status_update_args {
	int status;
};

struct rpc_dem_battery_update_args {
	uint32_t level;
};

static int handle_battery_call(struct msm_rpc_server *server,
			       struct rpc_request_hdr *req, unsigned len)
{
	switch (req->procedure) {
	case RPC_BATT_MTOA_NULL:
		return 0;

	case RPC_BATT_MTOA_SET_CHARGING_PROC: {
		struct rpc_batt_mtoa_set_charging_args *args;
		args = (struct rpc_batt_mtoa_set_charging_args *)(req + 1);
		args->enable = be32_to_cpu(args->enable);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_M2A_RPC)
			BATT_LOG("M2A_RPC: set_charging: %d", args->enable);
#if 0  /* JR:Don't need to handle charging request from modem side */
		if (htc_batt_info.charger == SWITCH_CHARGER)
			blocking_notifier_call_chain(&cable_status_notifier_list,
				args->enable, NULL);
		else {
			htc_battery_set_charging(args->enable);
		}
#endif
		return 0;
	}
	case RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC: {
		struct rpc_batt_mtoa_cable_status_update_args *args;
		args = (struct rpc_batt_mtoa_cable_status_update_args *)(req + 1);
		args->status = be32_to_cpu(args->status);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_M2A_RPC)
			BATT_LOG("M2A_RPC: cable_update: %s", charger_tags[args->status]);
#if 0
		/* FIXME: work arround for usb function, remove it after battery driver ready */
		if (machine_is_incrediblec() && args->status == CHARGER_AC)
			args->status = CHARGER_USB;
#endif
		htc_cable_status_update(args->status);
		htc_batt_info.rep.batt_state = 1;
#if defined(CONFIG_TROUT_BATTCHG_DOCK)
		dock_detect_start(args->status);
#endif

		return 0;
	}
	case RPC_BATT_MTOA_LEVEL_UPDATE_PROC: {
#if 0  /* JR:Don't need to update battery level from modem side */
		struct rpc_dem_battery_update_args *args;
		args = (struct rpc_dem_battery_update_args *)(req + 1);
		args->level = be32_to_cpu(args->level);
		if (htc_batt_debug_mask & HTC_BATT_DEBUG_M2A_RPC)
			BATT_LOG("M2A_RPC: level_update: %d", args->level);
		htc_battery_status_update(args->level);
#endif
		return 0;
	}
	default:
		BATT_ERR("%s: program 0x%08x:%d: unknown procedure %d",
		       __func__, req->prog, req->vers, req->procedure);
		return -ENODEV;
	}
}

static struct msm_rpc_server battery_server = {
	.prog = BATT_MTOA_PROG,
	.vers = BATT_MTOA_VERS,
	.rpc_call = handle_battery_call,
};

static void maxim8957_battery_work(struct work_struct *work);

static int htc_battery_max8957_probe(struct platform_device *pdev)
{
	struct htc_battery_max8957_platform_data *pdata = pdev->dev.platform_data;
	struct max8957_chip *max8957_chip = dev_get_drvdata(pdev->dev.parent);
	u16 val;
	int rc = 0;
	struct maxim8957_alarm *di;

#if MAXIM_BATTERY_DEBUG
	pr_err("%s:+, enable_current_sense=%d\n", __func__, pdata->enable_current_sense);
#endif

	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return -ENODEV;
	}

	max8957_fg = kzalloc(sizeof(*max8957_fg), GFP_KERNEL);
	if (unlikely(!max8957_fg))
		return -ENOMEM;

	max8957_fg->dev = &pdev->dev;
	platform_set_drvdata(pdev, max8957_fg);

	maxim8957_alarm_ptr = kzalloc(sizeof(*maxim8957_alarm_ptr), GFP_KERNEL);
	if (unlikely(!maxim8957_alarm_ptr))
		return -ENOMEM;

	maxim8957_alarm_ptr->dev = &pdev->dev;
	platform_set_drvdata(pdev, maxim8957_alarm_ptr);

	if (!pdata->enable_current_sense) {
		val = 0x0000;
		rc = max8957_write(max8957_chip, MAX8957_FG_CGAIN, (u8 *)&val, 2);
		if (unlikely(rc != 0)) {
			BATT_ERR("%s: Failed to set MAX8957_FG_CGAIN: %d", __func__, rc);
			goto err;
		}
		val = 0x0003;
		rc = max8957_write(max8957_chip, MAX8957_FG_MiscCFG, (u8 *)&val, 2);
		if (unlikely(rc != 0)) {
			BATT_ERR("%s: Failed to set MAX8957_FG_MiscCFG: %d", __func__, rc);
			goto err;
		}
		val = 0x0007;
		rc = max8957_write(max8957_chip, MAX8957_FG_LearnCFG, (u8 *)&val, 2);
		if (unlikely(rc != 0)) {
			BATT_ERR("%s: Failed to set MAX8957_FG_LearnCFG: %d", __func__, rc);
			goto err;
		}
	}

	test_power_monitor =
		(get_kernel_flag() & KERNEL_FLAG_TEST_PWR_SUPPLY) ? 1 : 0;

	htc_batt_info.device_id = pdev->id;
	htc_batt_info.gpio_usb_id = pdata->gpio_usb_id;
	htc_batt_info.gauge_driver = pdata->gauge_driver;
	htc_batt_info.m2a_cable_detect = pdata->m2a_cable_detect;
	htc_batt_info.func_show_batt_attr = pdata->func_show_batt_attr;
	htc_batt_info.charger = pdata->charger;
	htc_batt_info.rep.full_level = 100;

	if (htc_batt_info.charger == LINEAR_CHARGER) {
		htc_batt_info.gpio_mbat_in = pdata->gpio_mbat_in;
		htc_batt_info.gpio_mchg_en_n = pdata->gpio_mchg_en_n;
		htc_batt_info.gpio_iset = pdata->gpio_iset;
	}

	if (pdata->func_is_support_super_charger != NULL) {
		if (pdata->func_is_support_super_charger() == 1)
			htc_batt_info.gpio_adp_9v = pdata->gpio_adp_9v;
	}

	if (pdata->func_battery_charging_ctrl != NULL && pdata->func_battery_gpio_init != NULL) {
		htc_batt_info.func_battery_charging_ctrl = pdata->func_battery_charging_ctrl;
		pdata->func_battery_gpio_init();
	} else
		htc_batt_info.func_battery_charging_ctrl = battery_charging_ctrl;

	if (pdata->gauge_driver == GAUGE_MAX8957 ||
		pdata->m2a_cable_detect)
		msm_rpc_create_server(&battery_server);

#if MAXIM_BATTERY_FG_LOG
	INIT_DELAYED_WORK(&max8957_fg->fg_log_work,
			htc_battery_fg_log_work_func);

	wake_lock_init(&max8957_fg->fg_log_wake_lock, WAKE_LOCK_SUSPEND, "fg_log_enabled");
#endif
	charger_ctrl_stat = ENABLE_CHARGER;
	INIT_WORK(&batt_charger_ctrl_work, batt_charger_ctrl_func);
	alarm_init(&batt_charger_ctrl_alarm,
		    ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
		    batt_charger_ctrl_alarm_handler);
	batt_charger_ctrl_wq =
			create_singlethread_workqueue("charger_ctrl_timer");

#if 0
	wake_lock_init(&di->work_wake_lock, WAKE_LOCK_SUSPEND, "htc_battery_max8957");
	alarm_init(&di->alarm,
		ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
		maxim8957_battery_alarm);
	wake_lock(&di->work_wake_lock);
#endif

	/* Initialize FG interrupts */
	max8957_fg_irq_init(pdata);

	max8957_fg_set_alert(MAX8957_FG_ALERT_VOLTAGE, 3000, 5000);
	max8957_fg_set_alert(MAX8957_FG_ALERT_TEMPERATURE, -30, 80);
	max8957_fg_set_alert(MAX8957_FG_ALERT_CAPACITY, 0, 100);

	#ifdef CONFIG_MACH_PRIMOC
		if (htc_batt_info.rep.batt_id == 1)
			stopcharging_vol = PRIMOC_VOL_FULL_CHARGE_HV_A;
		else if (htc_batt_info.rep.batt_id == 2)
			stopcharging_vol = PRIMOC_VOL_FULL_CHARGE_HV_B;
		else
			stopcharging_vol = VOL_FULL_CHARGE;
	#endif

#if MAXIM_BATTERY_DEBUG
	pr_err("%s:-\n", __func__);
#endif

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di){
		rc = -ENOMEM;
		goto fail_register;
	}

	di->update_time = jiffies;
	platform_set_drvdata(pdev, di);

	di->dev = &pdev->dev;

	INIT_DELAYED_WORK(&di->level_update_work,
			maxim8957_battery_work);

	di->monitor_wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));

	/* init to something sane */
	di->last_poll = alarm_get_elapsed_realtime();

	if (!di->monitor_wqueue){
		rc = -ESRCH;
		goto fail_workqueue;
	}
	wake_lock_init(&di->work_wake_lock, WAKE_LOCK_SUSPEND, "htc_battery_max8957");
	alarm_init(&di->alarm,
		ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
		maxim8957_battery_alarm_handler);

	maxim8957_alarm_ptr = di; /* save di to global */

	maxim_batt_INI_param_check();

	schedule_delayed_work(&di->level_update_work, msecs_to_jiffies(5000));

	return 0;

	fail_workqueue : fail_register : kfree(di);
	return rc;

err:
	BATT_ERR("%s:-, Failed, rc=%d", __func__, rc);
	return rc;
}

int get_cable_status(void)
{
	/*
	if(htc_batt_info.rep.charging_source == CHARGER_AC || htc_batt_info.rep.charging_source == CHARGER_USB)
		htc_cable_status_update(htc_batt_info.rep.charging_source);
	*/
	return htc_batt_info.rep.charging_source;
}

static void maxim_change_INI_func(void)
{
	struct max8957_chip *max8957_chip = NULL;
	u16 reg_val_TGAIN, reg_val_TOFF = 0;
#if MAXIM_BATTERY_DEBUG
	u32 val_TGAIN = 0;
	u32 val_TOFF = 0;
#endif

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

   if ((htc_batt_info.rep.batt_temp >= -200) && (htc_batt_info.rep.batt_temp < 0)) {
			reg_val_TGAIN		= 0xDC5B;
			reg_val_TOFF		= 0x38E3;
			max8957_write(max8957_chip, MAX8957_FG_TGAIN , (u8 *)&reg_val_TGAIN, 2);	// Write TGAIN
			max8957_write(max8957_chip, MAX8957_FG_TOFF, (u8 *)&reg_val_TOFF, 2);	// Write TOFF
#if MAXIM_BATTERY_DEBUG
			max8957_read(max8957_chip, MAX8957_FG_TGAIN, (u8 *)&val_TGAIN, 2);
			max8957_read(max8957_chip, MAX8957_FG_TOFF, (u8 *)&val_TOFF, 2);
			BATT_LOG("To read MAX8957_FG_TGAIN: 0x%x", val_TGAIN);
			BATT_LOG("To read MAX8957_FG_TOFF: 0x%x", val_TOFF);
#endif
		}
   else if ((htc_batt_info.rep.batt_temp >= 0) && (htc_batt_info.rep.batt_temp <= 400)) {
			reg_val_TGAIN		= 0xEAC0;
			reg_val_TOFF		= 0x21E2;
			max8957_write(max8957_chip, MAX8957_FG_TGAIN , (u8 *)&reg_val_TGAIN, 2);	// Write TGAIN
			max8957_write(max8957_chip, MAX8957_FG_TOFF, (u8 *)&reg_val_TOFF, 2);	// Write TOFF
#if MAXIM_BATTERY_DEBUG
			max8957_read(max8957_chip, MAX8957_FG_TGAIN, (u8 *)&val_TGAIN, 2);
			max8957_read(max8957_chip, MAX8957_FG_TOFF, (u8 *)&val_TOFF, 2);
			BATT_LOG("To read MAX8957_FG_TGAIN: 0x%x", val_TGAIN);
			BATT_LOG("To read MAX8957_FG_TOFF: 0x%x", val_TOFF);
#endif
		}
   else if ((htc_batt_info.rep.batt_temp > 400) && (htc_batt_info.rep.batt_temp <= 700)) {
			reg_val_TGAIN		= 0xDE3E;
			reg_val_TOFF		= 0x2A5A;
			max8957_write(max8957_chip, MAX8957_FG_TGAIN , (u8 *)&reg_val_TGAIN, 2);	// Write TGAIN
			max8957_write(max8957_chip, MAX8957_FG_TOFF, (u8 *)&reg_val_TOFF, 2);	// Write TOFF
#if MAXIM_BATTERY_DEBUG
			max8957_read(max8957_chip, MAX8957_FG_TGAIN, (u8 *)&val_TGAIN, 2);
			max8957_read(max8957_chip, MAX8957_FG_TOFF, (u8 *)&val_TOFF, 2);
			BATT_LOG("To read MAX8957_FG_TGAIN: 0x%x", val_TGAIN);
			BATT_LOG("To read MAX8957_FG_TOFF: 0x%x", val_TOFF);
#endif
	}
}

static int maxim_batt_INI_reload_flag_update(void)
{
	if (htc_batt_info.rep.full_bat < 1100 || htc_batt_info.rep.full_bat > 1700) {

		if (isFirstTime && htc_batt_info.rep.full_bat == 0) {
			return 1;
			}

		mutex_lock(&htc_batt_info.lock);
		smem_batt_info->INI_flag = 11223355;
		htc_batt_info.rep.INI_flag = 1;
		BATT_LOG("To force re-load batt maxim INI param -> INI_flag:%d\n.", smem_batt_info->INI_flag);
		mutex_unlock(&htc_batt_info.lock);
	}
	else {
		mutex_lock(&htc_batt_info.lock);
		smem_batt_info->INI_flag = 88888888;
		htc_batt_info.rep.INI_flag = 0;
		mutex_unlock(&htc_batt_info.lock);
	}
	return 1;
}

static void get_maxim_batt_INI_info(void)
{
	struct max8957_chip *max8957_chip = NULL;
	u16 val_FullCAP, val_TGAIN, val_TOFF, val_QRtable00, val_DesignCap, val_CONFIG = 0;
	u16 val_ICHGTerm, val_QRtable10, val_FullCAPNom, val_LearnCFG, val_SHFTCFG, val_MiscCFG = 0;
	u16 val_QRtable20, val_RCOMP0, val_TempCo, val_V_empty, val_QRtable30, val_TempNom = 0;
	u16 val_Lock_I, val_Lock_II, val_MaskSOC = 0;

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	max8957_read(max8957_chip, MAX8957_FG_FullCAP, (u8 *)&val_FullCAP, 2);
	max8957_read(max8957_chip, MAX8957_FG_QRtable00, (u8 *)&val_QRtable00, 2);
	max8957_read(max8957_chip, MAX8957_FG_DesignCap, (u8 *)&val_DesignCap, 2);
	max8957_read(max8957_chip, MAX8957_FG_CONFIG, (u8 *)&val_CONFIG, 2);
	max8957_read(max8957_chip, MAX8957_FG_ICHGTerm, (u8 *)&val_ICHGTerm, 2);
	max8957_read(max8957_chip, MAX8957_FG_QRtable10, (u8 *)&val_QRtable10, 2);
	max8957_read(max8957_chip, MAX8957_FG_FullCAPNom, (u8 *)&val_FullCAPNom, 2);
	max8957_read(max8957_chip, MAX8957_FG_LearnCFG, (u8 *)&val_LearnCFG, 2);
	max8957_read(max8957_chip, MAX8957_FG_SHFTCFG, (u8 *)&val_SHFTCFG, 2);
	max8957_read(max8957_chip, MAX8957_FG_MiscCFG, (u8 *)&val_MiscCFG, 2);
	max8957_read(max8957_chip, MAX8957_FG_TGAIN, (u8 *)&val_TGAIN, 2);
	max8957_read(max8957_chip, MAX8957_FG_TOFF, (u8 *)&val_TOFF, 2);
	max8957_read(max8957_chip, MAX8957_FG_QRtable20, (u8 *)&val_QRtable20, 2);
	max8957_read(max8957_chip, MAX8957_FG_RCOMP0, (u8 *)&val_RCOMP0, 2);
	max8957_read(max8957_chip, MAX8957_FG_TempCo, (u8 *)&val_TempCo, 2);
	max8957_read(max8957_chip, MAX8957_FG_V_empty, (u8 *)&val_V_empty, 2);
	max8957_read(max8957_chip, MAX8957_FG_QRtable30, (u8 *)&val_QRtable30, 2);
	max8957_read(max8957_chip, MAX8957_FG_TempNom, (u8 *)&val_TempNom, 2);
	max8957_read(max8957_chip, MAX8957_FG_LOCK_I, (u8 *)&val_Lock_I, 2);
	max8957_read(max8957_chip, MAX8957_FG_LOCK_II, (u8 *)&val_Lock_II, 2);
	max8957_read(max8957_chip, MAX8957_FG_MaskSOC, (u8 *)&val_MaskSOC, 2);

	if (htc_batt_debug_mask & HTC_BATT_DEBUG_A2M_RPC)
		BATT_LOG("FullCAP=0x%x, QR_00=0x%x, D_CAP=0x%x, CNFG=0x%x, "
			"I_Term=0x%x, QR_10=0x%x, F_CAP_N=0x%x, L_CFG=0x%x, S_CFG=0x%x, "
			"M_CFG=0x%x, TGAIN=0x%x, TOFF=0x%x, QR_20=0x%x, RCOM_0=0x%x, Tmp_C=0x%x, "
			"V_emp=0x%x, QR_30=0x%x, TempNom=0x%x, LockI=0x%x, LockII=0x%x, MaskSOC=0x%x",
			val_FullCAP, val_QRtable00, val_DesignCap, val_CONFIG,
			val_ICHGTerm, val_QRtable10, val_FullCAPNom, val_LearnCFG,
			val_SHFTCFG, val_MiscCFG, val_TGAIN, val_TOFF, val_QRtable20,
			val_RCOMP0, val_TempCo, val_V_empty, val_QRtable30, val_TempNom,
			val_Lock_I, val_Lock_II, val_MaskSOC);
}

static void maxim_batt_INI_param_check(void)
{
	u16 tmp[48] = {0};
	int i;
	bool lock = TRUE;
	u16 val_TempNom, val_Msk_SOC, val_LockI, val_LockII = 0;
	u16 reg_gauge_lock, reg_val_TempNom, reg_MskSOC = 0;

	struct max8957_chip *max8957_chip = NULL;

	max8957_chip = dev_get_drvdata(max8957_fg->dev->parent);

	max8957_read(max8957_chip, MAX8957_FG_TempNom, (u8 *)&val_TempNom, 2);

	if(val_TempNom != 0x1400) {
		reg_val_TempNom = 0x1400;
		max8957_write(max8957_chip, MAX8957_FG_TempNom , (u8 *)&reg_val_TempNom, 2);	// Write TempNom
		BATT_ERR("Gauge TempNom is incorrect ->0x%x", val_TempNom);
		}
	else
		BATT_LOG("Gauge TempNom is correct ->0x%x", val_TempNom);

	max8957_read(max8957_chip, MAX8957_FG_LOCK_I, (u8 *)&val_LockI, 2);
	max8957_read(max8957_chip, MAX8957_FG_LOCK_II, (u8 *)&val_LockII, 2);

	max8957_read(max8957_chip, MAX8957_FG_OCV, (u8*)&tmp, 48 * 2);	// Verify the Model Access if locked

	for(i = 0; i < 48; i++) {
		if (tmp[i] != 0)
			lock = FALSE;
		}

	if(lock == FALSE) {
		BATT_ERR("Gauge model is unlocked-> 0x%x, 0x%x, 0x%x", val_LockI, val_LockII, tmp[2]);
		reg_gauge_lock = 0x0000;
		max8957_write(max8957_chip, MAX8957_FG_LOCK_I , (u8 *)&reg_gauge_lock, 2);	// Lock Model Access
		max8957_write(max8957_chip, MAX8957_FG_LOCK_II , (u8 *)&reg_gauge_lock, 2);
		}
	else
		BATT_LOG("Gauge model is locked-> 0x%x, 0x%x, 0x%x", val_LockI, val_LockII, tmp[2]);

	max8957_read(max8957_chip, MAX8957_FG_MaskSOC, (u8 *)&val_Msk_SOC, 2);

	if(val_Msk_SOC != 0x5A00) {
		reg_MskSOC = 0x5A00;
		max8957_write(max8957_chip, MAX8957_FG_MaskSOC , (u8 *)&reg_MskSOC, 2);	//Write MaskSOC
		BATT_ERR("Gauge MaskSOC is incorrect-> 0x%x", val_Msk_SOC);
		}
	else
		BATT_LOG("Gauge MaskSOC is correct-> 0x%x", val_Msk_SOC);
}

static void maxim8957_battery_work(struct work_struct *work)
{
	struct maxim8957_alarm *di = container_of(work,
		struct maxim8957_alarm, level_update_work.work);

	htc_battery_level_update_work_func();

	maxim_batt_INI_reload_flag_update();

	get_maxim_batt_INI_info();

	maxim_change_INI_func();

	last_poll_ktime = ktime_get_real();

	wake_unlock(&di->work_wake_lock);

	if (di->slow_poll)
		maxim8957_program_alarm(di, SLOW_POLL);
	else if (htc_batt_info.rep.charging_source > 0)
		maxim8957_program_alarm(di, VBUS_POLL);
	else
		maxim8957_program_alarm(di, FAST_POLL);
}

static int htc_batt_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct maxim8957_alarm *di = platform_get_drvdata(pdev);

	di->last_poll = alarm_get_elapsed_realtime();

	/* prevent suspend before starting the alarm */
	spin_lock_irqsave(&di->spin_lock, di->spin_flags);
	wake_unlock(&di->work_wake_lock);
	alarm_cancel(&di->alarm);

	if (htc_batt_phone_call)
		maxim8957_program_alarm(di, PHONE_POLL);
	else if (htc_batt_info.rep.charging_source > 0)
		maxim8957_program_alarm(di, VBUS_POLL);
	else if (htc_batt_info.rep.level <= 5)
		maxim8957_program_alarm(di, LOW_LEVEL_POLL);
	else
		maxim8957_program_alarm(di, SLOW_POLL);

	di->slow_poll = 1;

	spin_unlock_irqrestore(&di->spin_lock, di->spin_flags);
	return 0;
}

static void htc_batt_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct maxim8957_alarm *di = platform_get_drvdata(pdev);

	di->last_poll = alarm_get_elapsed_realtime();

	if (di->slow_poll){
		spin_lock_irqsave(&di->spin_lock, di->spin_flags);
		wake_unlock(&di->work_wake_lock);
		alarm_cancel(&di->alarm);

		if (htc_batt_phone_call) {
			if (htc_batt_info.rep.charging_source > 0)
			maxim8957_program_alarm(di, VBUS_POLL);
			else
			maxim8957_program_alarm(di, FAST_POLL);
		}
		else if (MSPERIOD(ktime_get_real(), last_poll_ktime) >= FAST_POLL * 1000)
			maxim8957_program_alarm(di, 0);
		else if (htc_batt_info.rep.charging_source > 0)
			maxim8957_program_alarm(di, VBUS_POLL);
		else
			maxim8957_program_alarm(di, FAST_POLL);

		di->slow_poll = 0;
		spin_unlock_irqrestore(&di->spin_lock, di->spin_flags);
	}
}

static struct dev_pm_ops htc_batt_pm_ops = {
	.prepare = htc_batt_suspend,
	.complete  = htc_batt_resume,
};

static struct platform_driver htc_battery_max8957_driver = {
	.probe	= htc_battery_max8957_probe,
	.driver	= {
		.name	= "htc_battery_max8957",
		.owner	= THIS_MODULE,
		.pm     = &htc_batt_pm_ops,
	},
};

static struct notifier_block batt_notify = {
	.notifier_call = htc_power_policy,
};

static BLOCKING_NOTIFIER_HEAD(battery_notifier_list);
int batt_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&battery_notifier_list, nb);
}

int batt_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&battery_notifier_list, nb);
}

int batt_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&battery_notifier_list, val, v);
}

/* Set Fake temperature by writeconfig 6 4 */
static int __init max8957_fake_temp_setup(char *str)
{
	if(!strcmp(str,"true"))
		debug_fake_temperature = TRUE;
	else
		debug_fake_temperature = FALSE;
	return 1;
}
__setup("battery_fake_temp=", max8957_fake_temp_setup);

static int __init htc_battery_max8957_init(void)
{
	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	mutex_init(&htc_batt_info.lock);
	mutex_init(&htc_batt_info.rpc_lock);
	usb_register_notifier(&usb_status_notifier);
	platform_driver_register(&htc_battery_max8957_driver);
	platform_driver_register(&htc_battery_core_driver);
	batt_register_client(&batt_notify);
#ifdef CONFIG_FB
#ifndef CONFIG_ARCH_MSM7X27A
	/* Jay, The msm_fb need to consult htc_battery for power policy */
	display_notifier(htc_power_policy, NOTIFY_POWER);
#endif
#endif
	return 0;
}
module_init(htc_battery_max8957_init);

static void __exit max8957_fg_exit(void)
{
	if (max8957_fg) {
#if MAXIM_BATTERY_FG_LOG
		wake_lock_destroy(&max8957_fg->fg_log_wake_lock);
#endif
		kzfree(max8957_fg);
	}

	batt_unregister_client(&batt_notify);
	platform_driver_unregister(&htc_battery_max8957_driver);
	platform_driver_unregister(&htc_battery_core_driver);
}
module_exit(max8957_fg_exit);

MODULE_DESCRIPTION("MAX8957 Fuel Gauge");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
