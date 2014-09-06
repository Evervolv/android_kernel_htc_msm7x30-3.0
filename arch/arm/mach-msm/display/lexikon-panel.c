/* linux/arch/arm/mach-msm/board-lexikon-panel.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Author: Jay Tu <jay_tu@htc.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/msm_fb-7x30.h>
#include <mach/msm_iomap-7x30.h>
#include <mach/vreg.h>
#include <mach/panel_id.h>

//#include "pmic.h"
#include "../board-lexikon.h"
#include "../devices.h"
#include "../proc_comm.h"

#if 1
#define B(s...) printk(s)
#else
#define B(s...) do {} while(0)
#endif
#define DEFAULT_BRIGHTNESS 100
extern int panel_type;

#define DRIVER_IC_CUT2			4
#define PANEL_WHITESTONE		0
#define PANEL_LEXIKON_SHARP		1
#define PANEL_LEXIKON_SONY		2
#define PANEL_LEXIKON_SHARP_CUT2        (PANEL_LEXIKON_SHARP | DRIVER_IC_CUT2)
#define PANEL_LEXIKON_SONY_CUT2         (PANEL_LEXIKON_SONY | DRIVER_IC_CUT2)

#define PWM_USER_DEF	 		143
#define PWM_USER_MIN			30
#define PWM_USER_MAX			255

#define PWM_SHARP_DEF			103
#define PWM_SHARP_MIN			11
#define PWM_SHARP_MAX			218

#define PWM_SONY_DEF			135
#define PWM_SONY_MIN			9
#define PWM_SONY_MAX			255

static struct clk *axi_clk;

static struct vreg *V_LCMIO_1V8, *V_LCM_2V85;
static struct cabc_t {
	struct led_classdev lcd_backlight;
	struct msm_mddi_client_data *client_data;
	struct mutex lock;
	unsigned long status;
	int last_shrink_br;
} cabc;

enum {
	GATE_ON = 1 << 0,
};

enum led_brightness brightness_value = DEFAULT_BRIGHTNESS;

/* use one flag to have better backlight on/off performance */
static int lexikon_set_dim = 1;

static int lexikon_shrink_pwm(int brightness, int user_def,
		int user_min, int user_max, int panel_def,
		int panel_min, int panel_max)
{
	if (brightness < user_min) {
		return panel_min;
	}

	if (brightness > user_def) {
		brightness = (panel_max - panel_def) *
			(brightness - user_def) /
			(user_max - user_def) +
			panel_def;
	} else {
			brightness = (panel_def - panel_min) *
			(brightness - user_min) /
			(user_def - user_min) +
			panel_min;
	}

        return brightness;
}

static void lexikon_set_brightness(struct led_classdev *led_cdev,
				enum led_brightness val)
{
	struct msm_mddi_client_data *client = cabc.client_data;
	unsigned int shrink_br = val;

	if (test_bit(GATE_ON, &cabc.status) == 0)
		return;

	if(panel_type == PANEL_LEXIKON_SONY) {
		shrink_br = lexikon_shrink_pwm(val, PWM_USER_DEF,
				PWM_USER_MIN, PWM_USER_MAX, PWM_SONY_DEF,
				PWM_SONY_MIN, PWM_SONY_MAX);
	} else {
		shrink_br = lexikon_shrink_pwm(val, PWM_USER_DEF,
				PWM_USER_MIN, PWM_USER_MAX, PWM_SHARP_DEF,
				PWM_SHARP_MIN, PWM_SHARP_MAX);
	}

	if (!client) {
		pr_info("null mddi client");
		return;
	}

	if (cabc.last_shrink_br == shrink_br) {
		pr_info("[BKL] identical shrink_br");
		return;
	}

	mutex_lock(&cabc.lock);
	if (lexikon_set_dim == 1) {
		client->remote_write(client, 0x2C, 0x5300);
		/* we dont need set dim again */
		lexikon_set_dim = 0;
	}
	client->remote_write(client, 0x00, 0x5500);
	client->remote_write(client, shrink_br, 0x5100);

	/* Update the last brightness */
	cabc.last_shrink_br = shrink_br;
	brightness_value = val;
	mutex_unlock(&cabc.lock);

	printk(KERN_INFO "set brightness to %d\n", shrink_br);
}

static enum led_brightness
lexikon_get_brightness(struct led_classdev *led_cdev)
{
	return brightness_value;
}

static void lexikon_backlight_switch(int on)
{
	enum led_brightness val;

	if (on) {
		printk(KERN_DEBUG "turn on backlight\n");
		set_bit(GATE_ON, &cabc.status);
		val = cabc.lcd_backlight.brightness;

		/* LED core uses get_brightness for default value
		 * If the physical layer is not ready, we should
		 * not count on it */
		if (val == 0)
			val = DEFAULT_BRIGHTNESS;
		lexikon_set_brightness(&cabc.lcd_backlight, val);
		/* set next backlight value with dim */
		lexikon_set_dim = 1;
	} else {
		clear_bit(GATE_ON, &cabc.status);
		cabc.last_shrink_br = 0;
	}
}

static int lexikon_backlight_probe(struct platform_device *pdev)
{
	int err = -EIO;
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);

	mutex_init(&cabc.lock);
	cabc.last_shrink_br = 0;
	cabc.client_data = pdev->dev.platform_data;
	cabc.lcd_backlight.name = "lcd-backlight";
	cabc.lcd_backlight.brightness_set = lexikon_set_brightness;
	cabc.lcd_backlight.brightness_get = lexikon_get_brightness;
	err = led_classdev_register(&pdev->dev, &cabc.lcd_backlight);
	if (err)
		goto err_register_lcd_bl;
	return 0;

err_register_lcd_bl:
	led_classdev_unregister(&cabc.lcd_backlight);
	return err;
}

/* ------------------------------------------------------------------- */

static struct resource resources_msm_fb[] = {
	{
		.flags = IORESOURCE_MEM,
	},
};

#define REG_WAIT (0xffff)

static struct nov_regs {
	unsigned reg;
	unsigned val;
} nov_init_seq[] = {
	{0xc000, 0x86},
	{0xc001, 0x00},
	{0xc002, 0x86},
	{0xc003, 0x00},
	{0xc100, 0x40},
	{0xc200, 0x02},
	{0xc202, 0x32},
	{0xe000, 0x0e},
	{0xe001, 0x2a},
	{0xe002, 0x33},
	{0xe003, 0x38},
	{0xe004, 0x1e},
	{0xe005, 0x30},
	{0xe006, 0x64},
	{0xe007, 0x3f},
	{0xe008, 0x21},
	{0xe009, 0x27},
	{0xe00a, 0x88},
	{0xe00b, 0x14},
	{0xe00c, 0x35},
	{0xe00d, 0x56},
	{0xe00e, 0x79},
	{0xe00f, 0x88},
	{0xe010, 0x55},
	{0xe011, 0x57},
	{0xe100, 0x0e},
	{0xe101, 0x2a},
	{0xe102, 0x33},
	{0xe103, 0x3b},
	{0xe104, 0x1e},
	{0xe105, 0x30},
	{0xe106, 0x64},
	{0xe107, 0x3f},
	{0xe108, 0x21},
	{0xe109, 0x27},
	{0xe10a, 0x88},
	{0xe10b, 0x14},
	{0xe10c, 0x35},
	{0xe10d, 0x56},
	{0xe10e, 0x79},
	{0xe10f, 0x88},
	{0xe110, 0x55},
	{0xe111, 0x57},

	{0xe200, 0x0E},
	{0xe201, 0x2A},
	{0xe202, 0x33},
	{0xe203, 0x3B},
	{0xe204, 0x1e},
	{0xe205, 0x30},
	{0xe206, 0x64},
	{0xe207, 0x3F},
	{0xe208, 0x21},
	{0xe209, 0x27},
	{0xe20A, 0x88},
	{0xe20B, 0x14},
	{0xe20C, 0x35},
	{0xe20D, 0x56},
	{0xe20E, 0x79},
	{0xe20F, 0xB8},
	{0xe210, 0x55},
	{0xe211, 0x57},

	{0xe300, 0x0E},
	{0xe301, 0x2A},
	{0xe302, 0x33},
	{0xe303, 0x3B},
	{0xe304, 0x1E},
	{0xe305, 0x30},
	{0xe306, 0x64},
	{0xe307, 0x3F},
	{0xe308, 0x21},
	{0xe309, 0x27},
	{0xe30A, 0x88},
	{0xe30B, 0x14},
	{0xe30C, 0x35},
	{0xe30D, 0x56},
	{0xe30E, 0x79},
	{0xe30F, 0xB8},
	{0xe310, 0x55},
	{0xe311, 0x57},
	{0xe400, 0x0E},
	{0xe401, 0x2A},
	{0xe402, 0x33},
	{0xe403, 0x3B},
	{0xe404, 0x1E},
	{0xe405, 0x30},
	{0xe406, 0x64},
	{0xe407, 0x3F},
	{0xe408, 0x21},
	{0xe409, 0x27},
	{0xe40A, 0x88},
	{0xe40B, 0x14},
	{0xe40C, 0x35},
	{0xe40D, 0x56},
	{0xe40E, 0x79},
	{0xe40F, 0xB8},
	{0xe410, 0x55},
	{0xe411, 0x57},
	{0xe500, 0x0E},
	{0xe501, 0x2A},
	{0xe502, 0x33},
	{0xe503, 0x3B},
	{0xe504, 0x1E},
	{0xe505, 0x30},
	{0xe506, 0x64},
	{0xe507, 0x3F},
	{0xe508, 0x21},
	{0xe509, 0x27},
	{0xe50A, 0x88},
	{0xe50B, 0x14},
	{0xe50C, 0x35},
	{0xe50D, 0x56},
	{0xe50E, 0x79},
	{0xe50F, 0xB8},
	{0xe510, 0x55},
	{0xe511, 0x57},

	{0x3a00, 0x05},

	/* cabc */
	{0x4e00, 0x00},
	{0x5e00, 0x00},
	{0x6a01, 0x00},
	{0x6a02, 0x01},
	{0x5100, 0xff},
	{0x5301, 0x10},
	{0x6A18, 0xff},
	{0x6A17, 0x01},

	{0x3500, 0x00},
	{0x1100, 0x0},
	{REG_WAIT, 120},
	{0x2900, 0x0},
	{REG_WAIT, 100},
};
static struct nov_regs sharp_init_seq[] = {
	{0x1100, 0x00},
	{REG_WAIT, 120},
	{0x89C3, 0x80},
	{0x92C2, 0x08},
	{0x0180, 0x14},
	{0x0280, 0x11},
	{0x0380, 0x33},
	{0x0480, 0x63},
	{0x0580, 0x63},
	{0x0680, 0x63},
	{0x0780, 0x00},
	{0x0880, 0x44},
	{0x0980, 0x54},
	{0x0A80, 0x10},
	{0x0B80, 0x55},
	{0x0C80, 0x55},
	{0x0D80, 0x30},
	{0x0E80, 0x44},
	{0x0F80, 0x54},
	{0x1080, 0x30},
	{0x1280, 0x77},
	{0x1380, 0x21},
	{0x1480, 0x0E},
	{0x1580, 0x98},
	{0x1680, 0xCC},
	{0x1780, 0x00},
	{0x1880, 0x00},
	{0x1980, 0x00},
	{0x1C80, 0x00},
	{0x1F80, 0x05},
	{0x2480, 0x1A},
	{0x2580, 0x1F},
	{0x2680, 0x2D},
	{0x2780, 0x3E},
	{0x2880, 0x0D},
	{0x2980, 0x21},
	{0x2A80, 0x58},
	{0x2B80, 0x2A},
	{0x2D80, 0x20},
	{0x2F80, 0x27},
	{0x3080, 0x61},
	{0x3180, 0x17},
	{0x3280, 0x37},
	{0x3380, 0x53},
	{0x3480, 0x5A},
	{0x3580, 0x8E},
	{0x3680, 0xA7},
	{0x3780, 0x3E},
	{0x3880, 0x2B},
	{0x3980, 0x2E},
	{0x3A80, 0x36},
	{0x3B80, 0x41},
	{0x3D80, 0x1A},
	{0x3F80, 0x2D},
	{0x4080, 0x5D},
	{0x4180, 0x3D},
	{0x4280, 0x20},
	{0x4380, 0x27},
	{0x4480, 0x76},
	{0x4580, 0x17},
	{0x4680, 0x39},
	{0x4780, 0x55},
	{0x4880, 0x71},
	{0x4980, 0xA6},
	{0x4A80, 0xBF},
	{0x4B80, 0x55},
	{0x4C80, 0x55},
	{0x4D80, 0x58},
	{0x4E80, 0x5F},
	{0x4F80, 0x66},
	{0x5080, 0x18},
	{0x5180, 0x26},
	{0x5280, 0x57},
	{0x5380, 0x3D},
	{0x5480, 0x1E},
	{0x5580, 0x26},
	{0x5680, 0x6B},
	{0x5780, 0x17},
	{0x5880, 0x3B},
	{0x5980, 0x4F},
	{0x5A80, 0x5A},
	{0x5B80, 0x8E},
	{0x5C80, 0xA7},
	{0x5D80, 0x3E},
	{0x5E80, 0x66},
	{0x5F80, 0x68},
	{0x5500, 0x02},
	{0x5E00, 0x09},
	{0x1DC0, 0x27},
	{0x6080, 0x6C},
	{0x6180, 0x6E},
	{0x6280, 0x16},
	{0x6380, 0x2A},
	{0x6480, 0x59},
	{0x6580, 0x4C},
	{0x6680, 0x1E},
	{0x6780, 0x25},
	{0x6880, 0x7B},
	{0x6980, 0x17},
	{0x6A80, 0x3A},
	{0x6B80, 0x53},
	{0x6C80, 0x71},
	{0x6D80, 0xA6},
	{0x6E80, 0xBF},
	{0x6F80, 0x55},
	{0x7080, 0x63},
	{0x7180, 0x66},
	{0x7280, 0x70},
	{0x7380, 0x76},
	{0x7480, 0x18},
	{0x7580, 0x27},
	{0x7680, 0x58},
	{0x7780, 0x47},
	{0x7880, 0x1E},
	{0x7980, 0x25},
	{0x7A80, 0x72},
	{0x7B80, 0x18},
	{0x7C80, 0x3B},
	{0x7D80, 0x4C},
	{0x7E80, 0x5A},
	{0x7F80, 0x8E},
	{0x8080, 0xA7},
	{0x8180, 0x3E},
	{0x8280, 0x75},
	{0x8380, 0x77},
	{0x8480, 0x7C},
	{0x8580, 0x7E},
	{0x8680, 0x16},
	{0x8780, 0x2C},
	{0x8880, 0x5C},
	{0x8980, 0x55},
	{0x8A80, 0x1F},
	{0x8B80, 0x24},
	{0x8C80, 0x82},
	{0x8D80, 0x15},
	{0x8E80, 0x38},
	{0x8F80, 0x50},
	{0x9080, 0x71},
	{0x9180, 0xA6},
	{0x9280, 0xBF},
	{0x9380, 0x55},
	{0x9480, 0xB5},
	{0x9580, 0x04},
	{0x9680, 0x18},
	{0x9780, 0xB0},
	{0x9880, 0x20},
	{0x9980, 0x28},
	{0x9A80, 0x08},
	{0x9B80, 0x04},
	{0x9C80, 0x12},
	{0x9D80, 0x00},
	{0x9E80, 0x00},
	{0x9F80, 0x12},
	{0xA080, 0x00},
	{0xA280, 0x00},
	{0xA380, 0x3C},
	{0xA480, 0x01},
	{0xA580, 0xC0},
	{0xA680, 0x01},
	{0xA780, 0x00},
	{0xA980, 0x00},
	{0xAA80, 0x00},
	{0xAB80, 0x70},
	{0xE780, 0x11},
	{0xE880, 0x11},
	{0xED80, 0x0A},
	{0xEE80, 0x80},
	{0xF780, 0x0D},
	{0x2900, 0x00},
	{0x3500, 0x00},
	{0x4400, 0x02},
	{0x4401, 0x58},
};

static struct nov_regs sony_init_seq[] = {
	{0x1100, 0x00},
	{REG_WAIT, 120},
	{0x0480, 0x63},
	{0x0580, 0x63},
	{0x0680, 0x63},
	{0x2480, 0x52},
	{0x2580, 0x56},
	{0x2680, 0x62},
	{0x2780, 0x6B},
	{0x2880, 0x17},
	{0x2980, 0x2A},
	{0x2A80, 0x5B},
	{0x2B80, 0x6B},
	{0x2D80, 0x1E},
	{0x2F80, 0x25},
	{0x3080, 0xB2},
	{0x3180, 0x1C},
	{0x3280, 0x48},
	{0x3380, 0x5E},
	{0x3480, 0xC2},
	{0x3580, 0xD2},
	{0x3680, 0xEF},
	{0x3780, 0x7F},
	{0x3880, 0x52},
	{0x3980, 0x56},
	{0x3A80, 0x62},
	{0x3B80, 0x6B},
	{0x3D80, 0x17},
	{0x3F80, 0x2A},
	{0x4080, 0x5B},
	{0x4180, 0x6B},
	{0x4280, 0x1E},
	{0x4380, 0x25},
	{0x4480, 0xB2},
	{0x4580, 0x1C},
	{0x4680, 0x48},
	{0x4780, 0x5E},
	{0x4880, 0xC2},
	{0x4980, 0xD2},
	{0x4A80, 0xEF},
	{0x4B80, 0x7F},
	{0x4C80, 0x3F},
	{0x4D80, 0x44},
	{0x4E80, 0x53},
	{0x4F80, 0x60},
	{0x5080, 0x17},
	{0x5180, 0x2A},
	{0x5280, 0x5C},
	{0x5380, 0x68},
	{0x5480, 0x1F},
	{0x5580, 0x25},
	{0x5680, 0xB1},
	{0x5780, 0x1D},
	{0x5880, 0x4C},
	{0x5980, 0x63},
	{0x5A80, 0xBA},
	{0x5B80, 0xC8},
	{0x5C80, 0xFA},
	{0x5D80, 0x7F},
	{0x5E80, 0x3F},
	{0x5F80, 0x44},
	{0x5500, 0x02},
	{0x5E00, 0x09},
	{0x1DC0, 0x27},
	{0x6080, 0x53},
	{0x6180, 0x60},
	{0x6280, 0x17},
	{0x6380, 0x2A},
	{0x6480, 0x5C},
	{0x6580, 0x68},
	{0x6680, 0x1F},
	{0x6780, 0x25},
	{0x6880, 0xB1},
	{0x6980, 0x1D},
	{0x6A80, 0x4C},
	{0x6B80, 0x63},
	{0x6C80, 0xBA},
	{0x6D80, 0xC8},
	{0x6E80, 0xFA},
	{0x6F80, 0x7F},
	{0x7080, 0x00},
	{0x7180, 0x0A},
	{0x7280, 0x25},
	{0x7380, 0x3B},
	{0x7480, 0x1D},
	{0x7580, 0x30},
	{0x7680, 0x60},
	{0x7780, 0x63},
	{0x7880, 0x1F},
	{0x7980, 0x26},
	{0x7A80, 0xB2},
	{0x7B80, 0x1B},
	{0x7C80, 0x47},
	{0x7D80, 0x5E},
	{0x7E80, 0xBC},
	{0x7F80, 0xCD},
	{0x8080, 0xF8},
	{0x8180, 0x7F},
	{0x8280, 0x00},
	{0x8380, 0x0A},
	{0x8480, 0x25},
	{0x8580, 0x3B},
	{0x8680, 0x1D},
	{0x8780, 0x30},
	{0x8880, 0x60},
	{0x8980, 0x63},
	{0x8A80, 0x1F},
	{0x8B80, 0x26},
	{0x8C80, 0xB2},
	{0x8D80, 0x1B},
	{0x8E80, 0x47},
	{0x8F80, 0x5E},
	{0x9080, 0xBC},
	{0x9180, 0xCD},
	{0x9280, 0xF8},
	{0x9380, 0x7F},
	{0x2900, 0x00},
	{0x3500, 0x00},
	{0x4400, 0x02},
	{0x4401, 0x58},
};

static int
lexikon_mddi_init(struct msm_mddi_bridge_platform_data *bridge_data,
		     struct msm_mddi_client_data *client_data)
{
	int i = 0, array_size;
	unsigned reg, val;
	struct nov_regs *init_seq;

	if (panel_type == PANEL_WHITESTONE) {
		init_seq = nov_init_seq;
		array_size = ARRAY_SIZE(nov_init_seq);
	} else if (panel_type == PANEL_LEXIKON_SONY
		|| panel_type == PANEL_LEXIKON_SONY_CUT2) {
		init_seq = sony_init_seq;
		array_size = ARRAY_SIZE(sony_init_seq);
	} else {
		init_seq = sharp_init_seq;
		array_size = ARRAY_SIZE(sharp_init_seq);
	}

	for (i = 0; i < array_size; i++) {
		reg = cpu_to_le32(init_seq[i].reg);
		val = cpu_to_le32(init_seq[i].val);
		if (reg == REG_WAIT)
			msleep(val);
		else {
			client_data->remote_write(client_data, val, reg);
			if (reg == 0x1100)
				client_data->send_powerdown(client_data);
		}
	}
	if(axi_clk)
		clk_set_rate(axi_clk, 0);
	return 0;
}

static int
lexikon_mddi_uninit(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	return 0;
}

static int
lexikon_panel_blank(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s\n", __func__);
	client_data->auto_hibernate(client_data, 0);
	/* set dim off for performance */
	client_data->remote_write(client_data, 0x0, 0x5300);
	lexikon_backlight_switch(LED_OFF);
	client_data->remote_write(client_data, 0, 0x2800);
	client_data->remote_write(client_data, 0, 0x1000);
	client_data->auto_hibernate(client_data, 1);
	return 0;
}

static int
lexikon_panel_unblank(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s\n", __func__);
	client_data->auto_hibernate(client_data, 0);
	client_data->remote_write(client_data, 0x24, 0x5300);
	client_data->remote_write(client_data, 0x0A, 0x22C0);
	client_data->auto_hibernate(client_data, 1);
	hr_msleep(30);
	lexikon_backlight_switch(LED_FULL);
	return 0;
}

static struct msm_mddi_bridge_platform_data novatec_client_data = {
	.init = lexikon_mddi_init,
	.uninit = lexikon_mddi_uninit,
	.blank = lexikon_panel_blank,
	.unblank = lexikon_panel_unblank,
	.fb_data = {
		.xres = 480,
		.yres = 800,
		.width = 48,
		.height = 80,
		.output_format = 0,
	},
	.panel_conf = {
		.caps = MSMFB_CAP_CABC,
		.vsync_gpio = 30,
	},
};

static void
mddi_novatec_power(struct msm_mddi_client_data *client_data, int on)
{
	int rc;
	unsigned config;
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);

	if (panel_type == 0) {
		if (on) {
			if(axi_clk)
				clk_set_rate(axi_clk, 192000000);
			vreg_enable(V_LCM_2V85);
			vreg_enable(V_LCMIO_1V8);
			hr_msleep(20);
			gpio_set_value(LEXIKON_LCD_RSTz, 1);
			hr_msleep(25);
			gpio_set_value(LEXIKON_LCD_RSTz, 0);
			hr_msleep(10);
			gpio_set_value(LEXIKON_LCD_RSTz, 1);
			hr_msleep(20);
		} else {
			vreg_disable(V_LCMIO_1V8);
			vreg_disable(V_LCM_2V85);
			gpio_set_value(LEXIKON_LCD_RSTz, 0);
		}
	} else {
		if (on) {
			if(axi_clk)
				clk_set_rate(axi_clk, 192000000);
			vreg_enable(V_LCM_2V85);
			hr_msleep(3);
			vreg_disable(V_LCM_2V85);
			hr_msleep(50);
			vreg_enable(V_LCM_2V85);
			vreg_enable(V_LCMIO_1V8);
			hr_msleep(2);
			gpio_set_value(LEXIKON_LCD_RSTz, 1);
			hr_msleep(1);
			gpio_set_value(LEXIKON_LCD_RSTz, 0);
			hr_msleep(1);
			gpio_set_value(LEXIKON_LCD_RSTz, 1);
			hr_msleep(60);
			config = PCOM_GPIO_CFG(LEXIKON_MDDI_TE, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			config = PCOM_GPIO_CFG(LEXIKON_LCD_ID1, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			config = PCOM_GPIO_CFG(LEXIKON_LCD_ID0, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		} else {
			config = PCOM_GPIO_CFG(LEXIKON_MDDI_TE, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			config = PCOM_GPIO_CFG(LEXIKON_LCD_ID1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			config = PCOM_GPIO_CFG(LEXIKON_LCD_ID0, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
			rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
			gpio_set_value(LEXIKON_LCD_RSTz, 0);
			hr_msleep(10);
			vreg_disable(V_LCMIO_1V8);
			vreg_disable(V_LCM_2V85);
		}
	}
}

static void panel_nov_fixup(uint16_t *mfr_name, uint16_t *product_code)
{
	printk(KERN_INFO "mddi fixup\n");
	*mfr_name = 0xb9f6;
	*product_code = 0x5560;
}

static struct msm_mddi_platform_data mddi_pdata = {
	.fixup = panel_nov_fixup,
	.power_client = mddi_novatec_power,
	.fb_resource = resources_msm_fb,
	.num_clients = 2,
	.client_platform_data = {
		{
			.product_id = (0xb9f6 << 16 | 0x5560),
			.name = "mddi_c_b9f6_5560",
			.id = 0,
			.client_data = &novatec_client_data,
			.clk_rate = 0,
		},
		{
			.product_id = (0xb9f6 << 16 | 0x5580),
			.name = "mddi_c_b9f6_5580",
			.id = 1,
			.client_data = &novatec_client_data,
			.clk_rate = 0,
		},
	},
};

static struct platform_driver lexikon_backlight_driver = {
	.probe = lexikon_backlight_probe,
	.driver = {
		.name = "nov_cabc",
		.owner = THIS_MODULE,
	},
};

static struct msm_mdp_platform_data mdp_pdata = {
#ifdef CONFIG_OVERLAY_FORCE_UPDATE
	.overrides = MSM_MDP4_MDDI_DMA_SWITCH | MSM_MDP_FORCE_UPDATE,
#else
	.overrides = MSM_MDP4_MDDI_DMA_SWITCH,
#endif
#ifdef CONFIG_MDP4_HW_VSYNC
       .xres = 480,
       .yres = 800,
       .back_porch = 4,
       .front_porch = 2,
       .pulse_width = 4,
#endif
};

int __init lexikon_init_panel(unsigned int sys_rev)
{
	int rc;

	B(KERN_INFO "%s: enter.\n", __func__);
	
	resources_msm_fb[0].start = msm_fb_base;
	resources_msm_fb[0].end = msm_fb_base + MSM_FB_SIZE - 1;	

	msm_device_mdp.dev.platform_data = &mdp_pdata;
	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;

	if (panel_type == 0) {
		mddi_pdata.clk_rate = 384000000;
		mddi_pdata.type = MSM_MDP_MDDI_TYPE_I;
	} else {
		if (panel_type & DRIVER_IC_CUT2)
			mddi_pdata.clk_rate = 384000000;
		else
			mddi_pdata.clk_rate = 256000000;
		mddi_pdata.type = MSM_MDP_MDDI_TYPE_II;
	}
	axi_clk = clk_get(NULL, "ebi1_mddi_clk");
	if (IS_ERR(axi_clk)) {
		pr_err("%s: failed to get axi clock\n", __func__);
		return PTR_ERR(axi_clk);
	}

	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	rc = platform_device_register(&msm_device_mddi0);
	if (rc)
		return rc;

	rc = platform_driver_register(&lexikon_backlight_driver);
	if (rc)
		return rc;

	/* lcd panel power */
	/* 2.85V -- LDO20 */
	V_LCM_2V85 = vreg_get(NULL, "gp13");

	if (IS_ERR(V_LCM_2V85)) {
		pr_err("%s: LCM_2V85 get failed (%ld)\n",
			__func__, PTR_ERR(V_LCM_2V85));
		return -1;
	}
	if (sys_rev > 0)
		V_LCMIO_1V8 = vreg_get(NULL, "lvsw0");
	else
		V_LCMIO_1V8 = vreg_get(NULL, "wlan2");

	if (IS_ERR(V_LCMIO_1V8)) {
		pr_err("%s: LCMIO_1V8 get failed (%ld)\n",
		       __func__, PTR_ERR(V_LCMIO_1V8));
		return -1;
	}

	return 0;
}
