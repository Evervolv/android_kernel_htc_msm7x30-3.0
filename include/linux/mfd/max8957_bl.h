/* include/linux/mfd/max8957_bl.h
 *
 * Functions to access max8957 WLED.
 *
 * Copyright (c) 2011 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BACKLIGHT_MAX8957_H__
#define __BACKLIGHT_MAX8957_H__

#define MAXIM_BACKLIGHT_DEBUG	0

/*===========================================================================

Definitions for Maxim MAX8957 WLED

============================================================================*/
#define MAX8957_REG_WLEDBSTCNTL1	0x98
#define MAX8957_WLEDFOSC_M			0x08
#define MAX8957_WLEDFOSC_SHIFT		3
#define MAX8957_WLEDPWM2EN_M		0x10
#define MAX8957_WLEDPWM2EN_SHIFT	4
#define MAX8957_WLEDPWM1EN_M		0x20
#define MAX8957_WLEDPWM1EN_SHIFT	5
#define MAX8957_WLED2EN_M			0x40
#define MAX8957_WLED2EN_SHIFT		6
#define MAX8957_WLED1EN_M			0x80
#define MAX8957_WLED1EN_SHIFT		7

#define MAX8957_REG_IWLED			0x99
#define MAX8957_IWLED_MAX			0xCC	/*20mA*/

enum max8957_wled_ch {
	MAX8957_WLED_1,
	MAX8957_WLED_2,
};

enum max8957_wled_en {
	MAX8957_WLED_DIS,
	MAX8957_WLED_EN,
};

enum max8957_wledpwm_en {
	MAX8957_WLEDPWM_DIS,
	MAX8957_WLEDPWM_EN,
};

enum max8957_wledfosc {
	MAX8957_WLEDFOSC_1P47MHZ,
	MAX8957_WLEDFOSC_2P2MHZ,
};

struct max8957_backlight_platform_data {
	char *name;
	enum max8957_wled_en 	wled1_en;
	enum max8957_wledpwm_en wledpwm1_en;
	enum max8957_wled_en 	wled2_en;
	enum max8957_wledpwm_en wledpwm2_en;
	enum max8957_wledfosc 	wledfosc;
	u8	iwled;
};

int max8957_set_bl_on_off(enum max8957_wled_ch wled_ch, enum max8957_wled_en wled_en);

#endif /* __BACKLIGHT_MAX8957_H__ */
