/* arch/arm/mach-msm/board-lexikon-microp.c
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#ifdef CONFIG_MICROP_COMMON
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/atmega_microp.h>
#include <mach/drv_callback.h>
#include "board-lexikon.h"

static int lexikon_microp_function_init(struct i2c_client *client)
{
	return 0;
}

static struct microp_ops ops = {
	.init_microp_func = lexikon_microp_function_init,
};

void __init lexikon_microp_init(void)
{
	microp_register_ops(&ops);
}

#endif
