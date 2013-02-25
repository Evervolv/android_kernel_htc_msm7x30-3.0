/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HTC_BATTERY_MAX8957_H_
#define __HTC_BATTERY_MAX8957_H_

#include <linux/notifier.h>
#include <linux/power_supply.h>

#define MAXIM_BATTERY_DEBUG			0
#define MAXIM_BATTERY_FG_LOG		1
#define MAXIM_BATTERY_FG_LOG_DEBUG	0

#define BATT_EVENT_SUSPEND		0x01

#define ENABLE_LIMITED_CHG		0x10
#define CLEAR_LIMITED_CHG		0x11
#define CHECK_CHG				0X64
#define SET_ICL500				0X65
#define SET_ICL100				0X66
#define CHECK_INT2				0X67
#define OVERTEMP_VREG_4060		0XC8
#define NORMALTEMP_VREG_4200	0XC9
#define CHECK_INT1				0XCA
#define CHECK_CONTROL			0xCB

/*===========================================================================

Definitions for Maxim MAX8957 Fuel Gauge

============================================================================*/
#define MAX8957_ALARM_TMN_MASK	0x0200
#define MAX8957_ALARM_TMX_MASK	0x2000
#define MAX8957_ALARM_VMN_MASK	0x0100
#define MAX8957_ALARM_VMX_MASK	0x1000
#define MAX8957_ALARM_SMN_MASK	0x0400
#define MAX8957_ALARM_SMX_MASK	0x4000
#define MAX8957_ALARM_BR_MASK	0x8000
#define MAX8957_ALARM_BI_MASK	0x0800

#define PRIMOC_VOL_FULL_CHARGE_HV_A 4210
#define PRIMOC_VOL_FULL_CHARGE_HV_B 4260
#define VOL_FULL_CHARGE				4110

/* information about the system we're running on */
extern unsigned int system_rev;

enum batt_ctl_t {
	DISABLE = 0,
	ENABLE_SLOW_CHG,
	ENABLE_FAST_CHG,
	ENABLE_SUPER_CHG,
	ENABLE_WIRELESS_CHG,
	CHARGER_CHK
};

/* This order is the same as htc_power_supplies[]
 * And it's also the same as htc_cable_status_update()
 */
enum charger_type_t {
	CHARGER_UNKNOWN = -1,
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC,
	CHARGER_SUPER_AC,
	CHARGER_WIRELESS
};

enum power_supplies_type {
	BATTERY_SUPPLY,
	USB_SUPPLY,
	AC_SUPPLY,
	WIRELESS_SUPPLY
};

enum charger_control_flag {
	STOP_CHARGER = 0,
	ENABLE_CHARGER,
	ENABLE_LIMIT_CHARGER,
	DISABLE_LIMIT_CHARGER,
	END_CHARGER
};

enum {
	GAUGE_NONE,
	GUAGE_MODEM,
	GAUGE_MAX8957,
};

enum {
	LINEAR_CHARGER,
	SWITCH_CHARGER,
};

struct battery_info_reply {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	s32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	s32 batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
	u32 full_level;		/* Full Level */
	u32 over_vchg;		/* 0:normal, 1:over voltage charger */
	s32 eval_current;	/* System loading current from ADC */
	s32 avg_current;    /* Battery average current from ADC */
	u32 batt_state;
	u32 INI_flag;		/* flag to record battery maxim INI parameters if need re-load */
};

struct htc_battery_max8957_platform_data {
	char *name;
	bool enable_current_sense;
	int technology;
	int (*func_show_batt_attr)(struct device_attribute *attr,
					 char *buf);
	int gpio_mbat_in;
	int gpio_usb_id;
	int gpio_mchg_en_n;
	int gpio_iset;
	int gpio_adp_9v;
	int gauge_driver;
	int m2a_cable_detect;
	int charger;
	int irq;
	int fg_irq_base;
	int (*func_is_support_super_charger)(void);
	int (*func_battery_charging_ctrl)(enum batt_ctl_t ctl);
	int (*func_battery_gpio_init)(void);
};

#if 0
struct max8957_battery_info {
	int voltage_mV;
	int current_mA;
	int temp;
	int last_temp;
	int soc;	/* for smart gauge */
	int charge_state;
	int capacity;
	int last_capacity;
	int charging_source;
	int last_charging_source;
	int charge_counter_mAh;
	int charge_full_real_mAh;
	int charging_enable;
	bool is_cable_in;
};
#endif

enum {
	INT_TMN,
	INT_TMX,
	INT_VMN,
	INT_VMX,
	INT_SMN,
	INT_SMX,
	/*
	INT_BR,
	INT_BI,
	*/
	INT_MAX_FG,
};

enum max8957_fg_alert_type {
	MAX8957_FG_ALERT_VOLTAGE,
	MAX8957_FG_ALERT_TEMPERATURE,
	MAX8957_FG_ALERT_CAPACITY,
};

enum max8957_fg_alert_threshold_type {
	MAX8957_FG_ALERT_THRES_MIN,
	MAX8957_FG_ALERT_THRES_MAX,
};

enum max8957_fg_register {
	MAX8957_FG_STATUS   = 0x00,
	MAX8957_FG_VALRT_Th	= 0x01,
	MAX8957_FG_TALRT_Th	= 0x02,
	MAX8957_FG_SALRT_Th	= 0x03,
	MAX8957_FG_AtRate	= 0x04,
	MAX8957_FG_RepCap	= 0x05,
	MAX8957_FG_RepSOC	= 0x06,
	MAX8957_FG_Age		= 0x07,
	MAX8957_FG_TEMP		= 0x08,
	MAX8957_FG_VCELL	= 0x09,
	MAX8957_FG_Current	= 0x0A,
	MAX8957_FG_AvgCurrent	= 0x0B,
	MAX8957_FG_Qresidual	= 0x0C,
	MAX8957_FG_SOC		= 0x0D,
	MAX8957_FG_AvSOC	= 0x0E,
	MAX8957_FG_RemCap	= 0x0F,
	MAX8957_FG_FullCAP	= 0x10,
	MAX8957_FG_TTE		= 0x11,
	MAX8957_FG_QRtable00	= 0x12,
	MAX8957_FG_FullSOCthr	= 0x13,
	MAX8957_FG_RSLOW	= 0x14,
	MAX8957_FG_RFAST	= 0x15,
	MAX8957_FG_AvgTA	= 0x16,
	MAX8957_FG_Cycles	= 0x17,
	MAX8957_FG_DesignCap	= 0x18,
	MAX8957_FG_AvgVCELL	= 0x19,
	MAX8957_FG_MinMaxTemp	= 0x1A,
	MAX8957_FG_MinMaxVolt	= 0x1B,
	MAX8957_FG_MinMaxCurr	= 0x1C,
	MAX8957_FG_CONFIG	= 0x1D,
	MAX8957_FG_ICHGTerm	= 0x1E,
	MAX8957_FG_AvCap	= 0x1F,
	MAX8957_FG_ManName	= 0x20,
	MAX8957_FG_DevName	= 0x21,
	MAX8957_FG_QRtable10	= 0x22,
	MAX8957_FG_FullCAPNom	= 0x23,
	MAX8957_FG_TempNom	= 0x24,
	MAX8957_FG_TempLim	= 0x25,
	MAX8957_FG_AvgTA0	= 0x26,
	MAX8957_FG_AIN		= 0x27,
	MAX8957_FG_LearnCFG	= 0x28,
	MAX8957_FG_SHFTCFG	= 0x29,
	MAX8957_FG_RelaxCFG	= 0x2A,
	MAX8957_FG_MiscCFG	= 0x2B,
	MAX8957_FG_TGAIN	= 0x2C,
	MAX8957_FG_TOFF		= 0x2D,
	MAX8957_FG_CGAIN	= 0x2E,
	MAX8957_FG_COFF		= 0x2F,

	MAX8957_FG_dV_acc	= 0x30,
	MAX8957_FG_I_acc	= 0x31,
	MAX8957_FG_QRtable20	= 0x32,
	MAX8957_FG_MaskSOC	= 0x33,
	MAX8957_FG_CHG_CNFG_10	= 0x34,
	MAX8957_FG_FullCAP0	= 0x35,
	MAX8957_FG_Iavg_empty	= 0x36,
	MAX8957_FG_FCTC		= 0x37,
	MAX8957_FG_RCOMP0	= 0x38,
	MAX8957_FG_TempCo	= 0x39,
	MAX8957_FG_V_empty	= 0x3A,
	MAX8957_FG_AvgCurrent0	= 0x3B,
	MAX8957_FG_TaskPeriod	= 0x3C,
	MAX8957_FG_FSTAT	= 0x3D,
	MAX8957_FG_TIMER        = 0x3E,
	MAX8957_FG_SHDNTIMER	= 0x3F,

	MAX8957_FG_AvgCurrentL	= 0x40,
	MAX8957_FG_AvgTAL	= 0x41,
	MAX8957_FG_QRtable30	= 0x42,
	MAX8957_FG_RepCapL	= 0x43,
	MAX8957_FG_AvgVCELL0	= 0x44,
	MAX8957_FG_dQacc	= 0x45,
	MAX8957_FG_dp_acc	= 0x46,
	MAX8957_FG_RlxSOC	= 0x47,
	MAX8957_FG_VFSOC0	= 0x48,
	MAX8957_FG_RemCapL	= 0x49,
	MAX8957_FG_VFRemCap	= 0x4A,
	MAX8957_FG_AvgVCELLL	= 0x4B,
	MAX8957_FG_QH0		= 0x4C,
	MAX8957_FG_QH		= 0x4D,
	MAX8957_FG_QL		= 0x4E,
	MAX8957_FG_RemCapL0	= 0x4F,
	MAX8957_FG_LOCK_I	= 0x62,
	MAX8957_FG_LOCK_II	= 0x63,
	MAX8957_FG_OCV 		= 0x80,
};

extern int register_notifier_wireless_charger(struct notifier_block *nb);
extern int unregister_notifier_wireless_charger(struct notifier_block *nb);
extern int htc_is_wireless_charger(void);
extern int get_cable_status(void);
extern unsigned int batt_get_status(enum power_supply_property psp);
extern int htc_battery_charger_disable(void);
extern int max8957_fg_irq_init(struct htc_battery_max8957_platform_data *pdata);
extern void max8957_fg_alert_handler(enum max8957_fg_alert_type type, enum max8957_fg_alert_threshold_type thres_type);
#endif /* __HTC_BATTERY_MAX8957_H_ */
