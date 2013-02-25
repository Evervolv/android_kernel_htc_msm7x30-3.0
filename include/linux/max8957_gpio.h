/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __MAX8957_GPIO_H__
#define __MAX8957_GPIO_H__

#include <linux/irq.h>
#include <linux/mfd/core.h>

#define MAXIM_PMGPIO_DEBUG	0
#define MAXIM_PMGPIO_SET_WAKE_ENABLE	1

/*===========================================================================

Definitions for Maxim MAX8957/MAX8967 GPIOs

============================================================================*/
#define MAX8957_GPIO_NUM	 		32

#define MAX8957_GPIO_DATA_M			0x02
#define MAX8957_GPIO_DATA_SHIFT		1
#define MAX8957_GPIO_DATA_HI_V		0x02
#define MAX8957_GPIO_DATA_LO_V		0x00

#define MAX8957_GPIO_INPUT_DATA_M		0x20
#define MAX8957_GPIO_INPUT_DATA_SHIFT	5
#define MAX8957_GPIO_INPUT_DATA_HI_V	0x20
#define MAX8957_GPIO_INPUT_DATA_LO_V	0x00

/* direction */
#define MAX8957_GPIO_DIR_M			0x01
#define MAX8957_GPIO_DIR_SHIFT		0x00
#define MAX8957_GPIO_DIR_INPUT_V	0x01
#define MAX8957_GPIO_DIR_OUTPUT_V	0x00

/* output_buffer */
#define MAX8957_GPIO_OUT_BUF_M				0x04
#define MAX8957_GPIO_OUT_BUF_SHIFT			2
#define	MAX8957_GPIO_OUT_BUF_OPEN_DRAIN		0
#define	MAX8957_GPIO_OUT_BUF_CMOS			1

/* pull */
#define	MAX8957_GPIO_PULL_UP			0x11
#define	MAX8957_GPIO_PULL_DN			0x10
#define	MAX8957_GPIO_PULL_NO			0x00
#define MAX8957_GPIO_IN_PULL_M			0x40
#define MAX8957_GPIO_IN_PULL_SHIFT		6
#define MAX8957_GPIO_OUT_PULL_M			0x18
#define MAX8957_GPIO_OUT_PULL_SHIFT		3

#define MAX8957_GPIO_ALT_0			0x74
/* level shift */
#define MAX8957_GPIO_LS1_DISABLE	0x00
#define MAX8957_GPIO_LS1_ENABLE		0x01
#define MAX8957_GPIO_LS1_M			0x01
#define MAX8957_GPIO_LS1_SHIFT		0
#define MAX8957_GPIO_LS1_IN			16
#define MAX8957_GPIO_LS1_OUT			24
#define MAX8957_GPIO_LS2_DISABLE	0x00
#define MAX8957_GPIO_LS2_ENABLE_SINGLE_DIRECTION	0x01
#define MAX8957_GPIO_LS2_ENABLE_BI_DIRECTION		0x02
#define MAX8957_GPIO_LS2_M			0x06
#define MAX8957_GPIO_LS2_SHIFT		1
#define MAX8957_GPIO_LS2_IN			17
#define MAX8957_GPIO_LS2_OUT			25
/* RTC out */
#define MAX8957_GPIO_RTCOUT_0_M		0x08
#define MAX8957_GPIO_RTCOUT_0_SHIFT	3 /* GPIO20 */
#define MAX8957_GPIO_RTCOUT_1_M		0x10
#define MAX8957_GPIO_RTCOUT_1_SHIFT	4 /* GPIO21 */
#define MAX8957_GPIO_RTCOUT_ENABLE	0x01
#define MAX8957_GPIO_RTCOUT_DISABLE	0x00

#define MAX8957_REG_GPIO0_CNTL          0x77
#define MAX8957_REG_GPIO_CNTL(x)        (MAX8957_REG_GPIO0_CNTL + x)

#define MAX8957_GPIO_NO_INTERRUPT		0
#define MAX8957_GPIO_FALLING_EDGE_INT	1
#define MAX8957_GPIO_RISING_EDGE_INT	2
#define MAX8957_GPIO_BOTH_EDGE_INT		3
#define MAX8957_GPIO_INTCNT_MASK		0x18
#define MAX8957_GPIO_INTCNT_SHIFT		3

#define MAX8957_GPIO_INTM_MASK      0x02
#define MAX8957_GPIO_GPI_GPO_MASK   0x01

enum {
    INT_GPIO0,
    INT_GPIO1,
    INT_GPIO2,
    INT_GPIO3,
    INT_GPIO4,
    INT_GPIO5,
    INT_GPIO6,
    INT_GPIO7,
    INT_GPIO8,
    INT_GPIO9,
    INT_GPIO10,
    INT_GPIO11,
    INT_GPIO12,
    INT_GPIO13,
    INT_GPIO14,
    INT_GPIO15,
    INT_GPIO16,
    INT_GPIO17,
    INT_GPIO18,
    INT_GPIO19,
    INT_GPIO20,
    INT_GPIO21,
    INT_GPIO22,
    INT_GPIO23,
    INT_GPIO24,
    INT_GPIO25,
    INT_GPIO26,
    INT_GPIO27,
    INT_GPIO28,
    INT_GPIO29,
    INT_GPIO30,
    INT_GPIO31,
    INT_MAX_GPIO,
};

enum max8957_gpio_ls_id {
	MAX8957_GPIO_LS_1,
	MAX8957_GPIO_LS_2,
	MAX8957_GPIO_LS_MAX,
};

struct max8957_gpio_ls {
	unsigned	mode;
	/*
	boolean		always_on;
	*/
};

struct max8957_gpio {
	unsigned	direction;
	unsigned	output_buffer;
	unsigned	output_value;
	unsigned	pull;
	unsigned 	intr_type;
};

struct max8957_gpio_platform_data {
	int	gpio_base;
	int	irq_base;
	int	(*init)(void);
	void	(*max8957_gpio_device_register)(void);
};

int max8957_gpio_config(unsigned num, struct max8957_gpio *param);
int max8957_gpio_cfg(unsigned num, unsigned direction, unsigned output_buffer, unsigned output_value, unsigned pull, unsigned intr_type);
int max8957_set_wifi_bt_sleep_clk(int on);
int max8957_config_level_shift(unsigned ls_no, unsigned mode);
int max8957_gpio_set_intr_type(unsigned num, unsigned intr_type);

#endif /* __MAX8957_GPIO_H__ */

