/* arch/arm/mach-msm/board-lexikon-keypad.c
 *
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
 */

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <linux/keyreset.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/board_htc.h>

#include "board-lexikon.h"
#include "proc_comm.h"
#include "devices.h"

#include <linux/mfd/pmic8058.h>
#include <linux/input/matrix_keypad.h>

static char *keycaps = "--qwerty";
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "board_lexikon."
module_param_named(keycaps, keycaps, charp, 0);

#ifdef CONFIG_MSM_SSBI
static unsigned int lexikon_pmic_col_gpios[] = {
	PM8058_GPIO_PM_TO_SYS(PMGPIO(1)), PM8058_GPIO_PM_TO_SYS(PMGPIO(2)),
	PM8058_GPIO_PM_TO_SYS(PMGPIO(3)), PM8058_GPIO_PM_TO_SYS(PMGPIO(4)),
	PM8058_GPIO_PM_TO_SYS(PMGPIO(5)), PM8058_GPIO_PM_TO_SYS(PMGPIO(6)),
	PM8058_GPIO_PM_TO_SYS(PMGPIO(7)),
};
static unsigned int lexikon_pmic_row_gpios[] = {
	PM8058_GPIO_PM_TO_SYS(PMGPIO(9)), PM8058_GPIO_PM_TO_SYS(PMGPIO(10)),
	PM8058_GPIO_PM_TO_SYS(PMGPIO(11)), PM8058_GPIO_PM_TO_SYS(PMGPIO(12)),
	PM8058_GPIO_PM_TO_SYS(PMGPIO(13)), PM8058_GPIO_PM_TO_SYS(PMGPIO(14)),
	PM8058_GPIO_PM_TO_SYS(PMGPIO(15)),

};

#define KEYMAP_NUM_ROWS		ARRAY_SIZE(lexikon_pmic_row_gpios)
#define KEYMAP_NUM_COLS		ARRAY_SIZE(lexikon_pmic_col_gpios)
#define KEYMAP_INDEX(row, col)	(((row) * KEYMAP_NUM_COLS) + (col))
#define KEYMAP_SIZE		(KEYMAP_NUM_ROWS * KEYMAP_NUM_COLS)

static unsigned short lexikon_pmic_keymap[KEYMAP_SIZE] = {
	[KEYMAP_INDEX(0, 0)] = KEY_RIGHTALT,
	[KEYMAP_INDEX(0, 1)] = KEY_RIGHTSHIFT,
	[KEYMAP_INDEX(0, 2)] =  KEY_RIGHT,
	[KEYMAP_INDEX(0, 3)] =  KEY_LEFT,
	[KEYMAP_INDEX(0, 4)] =  KEY_UP,
	[KEYMAP_INDEX(0, 5)] =  KEY_DOWN,
	[KEYMAP_INDEX(0, 6)] =  KEY_SEND,

	[KEYMAP_INDEX(1, 0)] =  KEY_MENU,
	[KEYMAP_INDEX(1, 1)] =  KEY_BACKSPACE,
	[KEYMAP_INDEX(1, 2)] =  KEY_M,
	[KEYMAP_INDEX(1, 3)] =  KEY_HOME,
	[KEYMAP_INDEX(1, 4)] =  KEY_RESERVED,
	[KEYMAP_INDEX(1, 5)] =  KEY_RESERVED,
	[KEYMAP_INDEX(1, 6)] =  KEY_END,

	[KEYMAP_INDEX(2, 0)] =  KEY_ENTER,
	[KEYMAP_INDEX(2, 1)] =  KEY_SPACE,
	[KEYMAP_INDEX(2, 2)] =  KEY_Q,
	[KEYMAP_INDEX(2, 3)] =  KEY_W,
	[KEYMAP_INDEX(2, 4)] =  KEY_E,
	[KEYMAP_INDEX(2, 5)] =  KEY_R,
	[KEYMAP_INDEX(2, 6)] =  KEY_T,

	[KEYMAP_INDEX(3, 0)] =  KEY_RESERVED,
	[KEYMAP_INDEX(3, 1)] =  KEY_QUESTION,
	[KEYMAP_INDEX(3, 2)] =  KEY_Y,
	[KEYMAP_INDEX(3, 3)] =  KEY_U,
	[KEYMAP_INDEX(3, 4)] =  KEY_I,
	[KEYMAP_INDEX(3, 5)] =  KEY_O,
	[KEYMAP_INDEX(3, 6)] =  KEY_P,

	[KEYMAP_INDEX(4, 0)] =   KEY_F13,
	[KEYMAP_INDEX(4, 1)] =   KEY_DOT,
	[KEYMAP_INDEX(4, 2)] =   KEY_A,
	[KEYMAP_INDEX(4, 3)] =   KEY_S,
	[KEYMAP_INDEX(4, 4)] =   KEY_D,
	[KEYMAP_INDEX(4, 5)] =   KEY_F,
	[KEYMAP_INDEX(4, 6)] =   KEY_G,

	[KEYMAP_INDEX(5, 0)] =  KEY_BACK,
	[KEYMAP_INDEX(5, 1)] =  KEY_COMMA,
	[KEYMAP_INDEX(5, 2)] =  KEY_H,
	[KEYMAP_INDEX(5, 3)] =  KEY_J,
	[KEYMAP_INDEX(5, 4)] =  KEY_K,
	[KEYMAP_INDEX(5, 5)] =  KEY_L,
	[KEYMAP_INDEX(5, 6)] =  KEY_Z,

	[KEYMAP_INDEX(6, 0)] =  KEY_SEARCH,
	[KEYMAP_INDEX(6, 1)] =  KEY_EMAIL,
	[KEYMAP_INDEX(6, 2)] =  KEY_X,
	[KEYMAP_INDEX(6, 3)] =  KEY_C,
	[KEYMAP_INDEX(6, 4)] =  KEY_V,
	[KEYMAP_INDEX(6, 5)] =  KEY_B,
	[KEYMAP_INDEX(6, 6)] =  KEY_N,

};

static struct gpio_event_matrix_info lexikon_keypad_matrix_info = {
	.info.func = gpio_event_matrix_func,
	.keymap = lexikon_pmic_keymap,
	.output_gpios = lexikon_pmic_row_gpios,
	.input_gpios = lexikon_pmic_col_gpios,
	.noutputs = KEYMAP_NUM_ROWS,
	.ninputs = KEYMAP_NUM_COLS,
	.settle_time.tv_nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv_nsec = 20 * NSEC_PER_MSEC,
	.flags = GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_REMOVE_PHANTOM_KEYS | GPIOKPF_PRINT_UNMAPPED_KEYS /*| GPIOKPF_PRINT_MAPPED_KEYS*/
};
#endif
static struct gpio_event_direct_entry lexikon_keypad_input_map[] = {
	{
		.gpio = LEXIKON_GPIO_KEYPAD_POWER_KEY,
		.code = KEY_POWER,
	},
	{
		.gpio = PM8058_GPIO_PM_TO_SYS(LEXIKON_VOL_UP),
		.code = KEY_VOLUMEUP,
	},
	{
		.gpio = PM8058_GPIO_PM_TO_SYS(LEXIKON_VOL_DN),
		.code = KEY_VOLUMEDOWN,
	},
};

static void lexikon_setup_input_gpio(void)
{
	uint32_t inputs_gpio_table[] = {
		PCOM_GPIO_CFG(LEXIKON_GPIO_KEYPAD_POWER_KEY, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_4MA),
	};

	config_gpio_table(inputs_gpio_table, ARRAY_SIZE(inputs_gpio_table));
}

static struct gpio_event_input_info lexikon_keypad_input_info = {
	.info.func = gpio_event_input_func,
	.flags = GPIOEDF_PRINT_KEYS,
	.type = EV_KEY,
#if BITS_PER_LONG != 64 && !defined(CONFIG_KTIME_SCALAR)
	.debounce_time.tv.nsec = 5 * NSEC_PER_MSEC,
# else
	.debounce_time.tv64 = 5 * NSEC_PER_MSEC,
# endif
	.keymap = lexikon_keypad_input_map,
	.keymap_size = ARRAY_SIZE(lexikon_keypad_input_map),
	.setup_input_gpio = lexikon_setup_input_gpio,
};

static struct gpio_event_direct_entry lexikon_sliding_switch[] = {
	{
		.gpio = LEXIKON_SLIDING_INTZ,
		.code = SW_LID,
	},
};

static void lexikon_sliding_input_gpio(void)
{
	static uint32_t inputs_gpio_table[] = {
		GPIO_CFG(LEXIKON_SLIDING_INTZ, 0, GPIO_CFG_INPUT,
						GPIO_CFG_NO_PULL, GPIO_CFG_4MA),
	};

	config_gpio_table(inputs_gpio_table,
		ARRAY_SIZE(inputs_gpio_table));
}

static void lexikon_set_qty_irq(uint8_t disable)
{
	uint32_t i;
	static uint8_t already_disabled;
	pr_info("%s disable=%d, already_disabled=%d\n",
			__func__, disable, already_disabled);

	if (!(disable ^ already_disabled))
		return;

	already_disabled = disable;
	for (i = 0; i < KEYMAP_NUM_COLS; i++) {
		if (disable)
			disable_irq(gpio_to_irq(lexikon_pmic_col_gpios[i]));
		else
			enable_irq(gpio_to_irq(lexikon_pmic_col_gpios[i]));
	}
}

static struct gpio_event_switch_info lexikon_keypad_switch_info = {
	.info.func = gpio_event_switch_func,
	.flags = GPIOEDF_PRINT_KEYS,
	.type = EV_SW,
	.debounce_time.tv64 = 5 * NSEC_PER_MSEC,
	.keymap = lexikon_sliding_switch,
	.keymap_size = ARRAY_SIZE(lexikon_sliding_switch),
	.setup_switch_gpio = lexikon_sliding_input_gpio,
	.set_qty_irq = lexikon_set_qty_irq,
};

static struct gpio_event_info *lexikon_keypad_info[] = {
	&lexikon_keypad_input_info.info,
	&lexikon_keypad_switch_info.info,
#if CONFIG_MSM_SSBI
	&lexikon_keypad_matrix_info.info,
#endif
};

static struct gpio_event_platform_data lexikon_keypad_data = {
	.names = {
		"lexikon-keypad",
		NULL,
	},
	.info = lexikon_keypad_info,
	.info_count = ARRAY_SIZE(lexikon_keypad_info),
};

static struct platform_device lexikon_keypad_input_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev		= {
		.platform_data	= &lexikon_keypad_data,
	},
};

static struct keyreset_platform_data lexikon_reset_keys_pdata = {
	.keys_down = {
		KEY_POWER,
		KEY_VOLUMEDOWN,
		KEY_VOLUMEUP,
		0
	},
};

struct platform_device lexikon_reset_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &lexikon_reset_keys_pdata,
};

int __init lexikon_init_keypad(void)
{
	printk(KERN_DEBUG "%s\n",	__func__);

	if (platform_device_register(&lexikon_reset_keys_device))
		printk(KERN_WARNING "%s: register reset key fail\n", __func__);

	if (system_rev == 0) {
		lexikon_pmic_keymap[KEYMAP_INDEX(3, 0)] =  KEY_F13;
		lexikon_pmic_keymap[KEYMAP_INDEX(4, 0)] =  KEY_F14;
	}
	return platform_device_register(&lexikon_keypad_input_device);
}

