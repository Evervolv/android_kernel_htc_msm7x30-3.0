/*
 * Copyright (c) 2011 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LEDS_MAX8957_FLASH_H__
#define __LEDS_MAX8957_FLASH_H__

#include <linux/leds.h>
#include <linux/htc_flashlight.h>

#define MAXIM_LEDS_FLASH_DEBUG	0

/*===========================================================================

Definitions for Maxim MAX8957/MAX8967 Flash LEDs

============================================================================*/
#define I2C_REG_ADDR_IFLASH1        0x00
#define I2C_REG_ADDR_IFLASH2        0x01
#define MAX8957_FLASH_IOUT_MASK     0x3f
#define MAX8957_FLASH_IOUT_SHIFT    0x00

#define I2C_REG_ADDR_ITORCH         0x02
#define MAX8957_TORCH_IOUT1_MASK    0x0f
#define MAX8957_TORCH_IOUT1_SHIFT   0x00
#define MAX8957_TORCH_IOUT2_MASK    0xf0
#define MAX8957_TORCH_IOUT2_SHIFT   0x04

#define I2C_REG_ADDR_TORCH_TIMER    0x03
#define MAX8957_TORCH_TMR_DUR_MASK  0x0f
#define MAX8957_TORCH_TMR_DUR_SHIFT 0x00
#define MAX8957_DIS_TORCH_TMR_MASK  0x40
#define MAX8957_DIS_TORCH_TMR_SHIFT 0x06
#define MAX8957_DIS_TORCH_TMR_EN	0x00
#define MAX8957_DIS_TORCH_TMR_DIS	0x01
#define MAX8957_TORCH_TMR_MODE_MASK 0x80
#define MAX8957_TORCH_TMR_MODE_SHIFT 0x07
#define MAX8957_TORCH_MODE_ONESHOT  0x00
#define MAX8957_TORCH_MDOE_MAXTIMER 0x01

#define I2C_REG_ADDR_FLASH_TIMER    0x04
#define MAX8957_FLASH_TMR_DUR_MASK  0x0f
#define MAX8957_FLASH_TMR_DUR_SHIFT 0x00
#define MAX8957_FLASH_TMR_MODE_MASK 0x80
#define MAX8957_FLASH_TMR_MODE_SHIFT 0x07
#define MAX8957_FLASH_MODE_ONESHOT  0x00
#define MAX8957_FLASH_MDOE_MAXTIMER 0x01

#define I2C_REG_ADDR_FLASH_EN       0x05
#define MAX8957_TORCH_FLED2_EN_MASK 0x03
#define MAX8957_TORCH_FLED2_EN_SHIFT 0x00
#define MAX8957_TORCH_FLED1_EN_MASK 0x0c
#define MAX8957_TORCH_FLED1_EN_SHIFT 0x02
#define MAX8957_FLASH_FLED2_EN_MASK 0x30
#define MAX8957_FLASH_FLED2_EN_SHIFT 0x04
#define MAX8957_FLASH_FLED1_EN_MASK 0xc0
#define MAX8957_FLASH_FLED1_EN_SHIFT 0x06
#define MAX8957_TORCH_OFF           0x00
#define MAX8957_TORCH_BY_FLASHSTROB 0x01
#define MAX8957_TORCH_BY_I2C        0X03
#define MAX8957_FLASH_OFF           0x00
#define MAX8957_FLASH_BY_FLASHSTROB 0x01
#define MAX8957_FLASH_BY_I2C        0X03

#define I2C_REG_ADDR_MAX_FLASH1     0x06
#define I2C_REG_ADDR_MAX_FLASH2     0x07
#define I2C_REG_ADDR_MAX_FLASH3     0x08
#define I2C_REG_ADDR_MAX_FLASH4     0x09

#define I2C_REG_ADDR_VOUT_CNTL      0x0a
#define I2C_REG_ADDR_VOUT_FLASH1    0x0b
#define I2C_REG_ADDR_VOUT_FLASH2    0x0c

#define I2C_REG_ADDR_DUMMY          0x0d

#define I2C_REG_ADDR_FLASH_INT      0x0e
#define I2C_REG_ADDR_FLASH_INT_MASK 0x0f
#define I2C_REG_ADDR_FLASH_INT_STATUS 0x10

#define MAX_FLASH_CURRENT 1000	/* 1000mA(0x1f) */
#define MAX_TORCH_CURRENT 250	/* 250mA(0x0f) */
#define MAX_FLASH_DRV_LEVEL	63	/* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL 15	/* 15.625 + 15.625*15 mA */

#define MAX8957_MAX_FLEDS		4

#define FLASHLIGHT_NAME "flashlight"

#define FLASHLIGHT_OFF   0
#define FLASHLIGHT_TORCH 1
#define FLASHLIGHT_FLASH 2
#define FLASHLIGHT_NUM   3

enum max8957_fleds {
	MAX8957_ID_FLASH_LED_1 = 0,
	MAX8957_ID_FLASH_LED_2,
	MAX8957_ID_TORCH_LED_1,
	MAX8957_ID_TORCH_LED_2,
	MAX8957_ID_LED_MAX,
};

enum max8957_flash_time {
	FLASH_TIME_62P5MS,
	FLASH_TIME_125MS,
	FLASH_TIME_187P5MS,
	FLASH_TIME_250MS,
	FLASH_TIME_312P5MS,
	FLASH_TIME_375MS,
	FLASH_TIME_437P5MS,
	FLASH_TIME_500MS,
	FLASH_TIME_562P5MS,
	FLASH_TIME_625MS,
	FLASH_TIME_687P5MS,
	FLASH_TIME_750MS,
	FLASH_TIME_812P5MS,
	FLASH_TIME_875MS,
	FLASH_TIME_937P5MS,
	FLASH_TIME_1000MS,
	FLASH_TIME_MAX,
};

enum max8957_torch_time {
	TORCH_TIME_262MS,
	TORCH_TIME_524MS,
	TORCH_TIME_786MS,
	TORCH_TIME_1048MS,
	TORCH_TIME_1572MS,
	TORCH_TIME_2096MS,
	TORCH_TIME_2620MS,
	TORCH_TIME_3114MS,
	TORCH_TIME_4193MS,
	TORCH_TIME_5242MS,
	TORCH_TIME_6291MS,
	TORCH_TIME_7340MS,
	TORCH_TIME_9437MS,
	TORCH_TIME_11534MS,
	TORCH_TIME_13631MS,
	TORCH_TIME_15728MS,
	TORCH_TIME_MAX,
};

enum max8957_timer_mode {
	TIMER_MODE_ONE_SHOT,
	TIMER_MODE_MAX_TIMER,
};

enum max8957_led_cntrl_mode {
	LED_CTRL_BY_FLASHSTB,
	LED_CTRL_BY_I2C,
};

/*
enum flashlight_mode_flags {
	FL_MODE_OFF = 0,
	FL_MODE_TORCH,
	FL_MODE_FLASH,
	FL_MODE_PRE_FLASH,
	FL_MODE_TORCH_LED_A,
	FL_MODE_TORCH_LED_B,
	FL_MODE_TORCH_LEVEL_1,
	FL_MODE_TORCH_LEVEL_2,
};
*/

struct max8957_fled {
	const char	*name;
	const char	*default_trigger;
	unsigned	max_brightness;
	int		id;
	int		timer;
	int		timer_mode;
	int		torch_timer_disable;
	int		cntrl_mode;
};

struct max8957_fleds_platform_data {
	const char	*name;
	int num_leds;
	struct max8957_fled *leds;
	void (*gpio_init) (void);
	int flash;
	unsigned pre_flash_duration_ms;
	unsigned flash_duration_ms;
};

int max8957_flashlight_control(int mode);

#endif /* __LEDS_MAX8957_FLASH_H__ */
