/* linux/arch/arm/mach-msm/board-kingdom-panel.c
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
#include <mach/msm_iomap.h>
#include <mach/vreg.h>
#include <mach/panel_id.h>

#include "../board-kingdom.h"
#include "../devices.h"
#include "../proc_comm.h"

#if 1
#define B(s...) printk(s)
#else
#define B(s...) do {} while (0)
#endif
#define DEFAULT_BRIGHTNESS 100

int ddt = 20;
int skuid = 0;

static struct clk *axi_clk;
static struct vreg *V_LCMIO_1V8;
static struct cabc_t {
	struct led_classdev lcd_backlight;
	struct msm_mddi_client_data *client_data;
	struct mutex lock;
	unsigned long status;
} cabc;

enum {
	GATE_ON = 1 << 0,
};

enum led_brightness brightness_value = DEFAULT_BRIGHTNESS;

/* use one flag to have better backlight on/off performance */
static int set_dimming = 1;

unsigned int pwm_freq_16k = 1;
unsigned int prev_pwm_freq_16k = 1;

static void kingdom_set_brightness(struct led_classdev *led_cdev,
				enum led_brightness val)
{
	struct msm_mddi_client_data *client = cabc.client_data;
	unsigned int shrink_br = val;
	printk(KERN_DEBUG "set brightness = %d\n", val);
	if (test_bit(GATE_ON, &cabc.status) == 0)
		return;
	if (panel_type != PANEL_ID_KIM_SAMSUNG) {
		if (val < 30)
			shrink_br = 6;
		else if ((val >= 30) && (val <= 143))
			shrink_br = 98 * (val - 30) / 113 + 6;
		else
			shrink_br = 151 * (val - 143) / 112 + 104;
	} else {
		if (val < 30)
			shrink_br = 7;
		else if ((val >= 30) && (val <= 143))
			shrink_br = 101 * (val - 30) / 106 + 7;
		else
			shrink_br = 151 * (val - 143) / 112 + 104;
	}
	mutex_lock(&cabc.lock);
	if (set_dimming == 1) {
		client->remote_write(client, 0x2C, 0x5300);
		/* we dont need set dim again */
		set_dimming = 0;
	}

	if (val <= 35) {
		pwm_freq_16k = 0;
		if (prev_pwm_freq_16k != pwm_freq_16k) {
			if (panel_type != PANEL_ID_KIM_SAMSUNG)
				client->remote_write(client, 0x12, 0xC6B1); /* 7k, orise*/
			else {
				client->remote_write(client, 0x55, 0xF000); /* 7k, nova*/
				client->remote_write(client, 0xAA, 0xF001); /* 7k, nova*/
				client->remote_write(client, 0x52, 0xF002); /* 7k, nova*/
				client->remote_write(client, 0x08, 0xF003); /* 7k, nova*/
				client->remote_write(client, 0x00, 0xF004); /* 7k, nova*/
				client->remote_write(client, 0x01, 0xE000); /* 7k, nova*/
				client->remote_write(client, 0x08, 0xE001); /* 7k, nova*/
			}
		}
	} else {
		pwm_freq_16k = 1;
		if (prev_pwm_freq_16k != pwm_freq_16k) {
			if (panel_type != PANEL_ID_KIM_SAMSUNG)
				client->remote_write(client, 0x07, 0xC6B1); /* 16k, default, orise*/
			else {
				client->remote_write(client, 0x55, 0xF000); /* 15k, default, nova*/
				client->remote_write(client, 0xAA, 0xF001); /* 15k, default, nova*/
				client->remote_write(client, 0x52, 0xF002); /* 15k, default, nova*/
				client->remote_write(client, 0x08, 0xF003); /* 15k, default, nova*/
				client->remote_write(client, 0x00, 0xF004); /* 15k, default, nova*/
				client->remote_write(client, 0x01, 0xE000); /* 15k, default, nova*/
				client->remote_write(client, 0x04, 0xE001); /* 15k, default, nova*/
			}
		}
	}
	prev_pwm_freq_16k = pwm_freq_16k;

	client->remote_write(client, 0x00, 0x5500);
	client->remote_write(client, shrink_br, 0x5100);
	brightness_value = val;
	mutex_unlock(&cabc.lock);
}

static enum led_brightness
kingdom_get_brightness(struct led_classdev *led_cdev)
{
	/*FIXME:workaround for NOVATEK driver IC*/
#if 0
	struct msm_mddi_client_data *client = cabc.client_data;
	return client->remote_read(client, 0x5100);
#else
	return brightness_value;
#endif
}

static void kingdom_backlight_switch(int on)
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
		prev_pwm_freq_16k = pwm_freq_16k = 1;
		kingdom_set_brightness(&cabc.lcd_backlight, val);
		/* set next backlight value with dim */
		set_dimming = 1;
	} else {
		clear_bit(GATE_ON, &cabc.status);
		kingdom_set_brightness(&cabc.lcd_backlight, 0);
	}
}

static int kingdom_backlight_probe(struct platform_device *pdev)
{
	int err = -EIO;

	mutex_init(&cabc.lock);
	cabc.client_data = pdev->dev.platform_data;
	cabc.lcd_backlight.name = "lcd-backlight";
	cabc.lcd_backlight.brightness_set = kingdom_set_brightness;
	cabc.lcd_backlight.brightness_get = kingdom_get_brightness;
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
struct nov_regs {
	unsigned reg;
	unsigned val;
};

static struct nov_regs sony_init_seq[] = {
	{ 0x0100 , 0x0000 },
	{REG_WAIT, 5},
	{ 0xFF00 , 0x0096 },
	{ 0xFF01 , 0x0001 },
	{ 0xFF02 , 0x0001 },
	{ 0xFF80 , 0x0096 },
	{ 0xFF81 , 0x0001 },
	{ 0xC6B1 , 0x0007 },
	{ 0x1100 , 0x0000 },
	{ 0x4400 , 0x0001 },
	{ 0x4401 , 0x00C2 },
	{REG_WAIT, 120},
	{ 0x2900 , 0x0000 },
	{ 0x3500 , 0x0000 },
};

static struct nov_regs sony_init_seq_c2[] = {
	{ 0x0100 , 0x0000 },
	{REG_WAIT, 5},
	{ 0xFF00 , 0x0096 },
	{ 0xFF01 , 0x0001 },
	{ 0xFF02 , 0x0001 },
	{ 0xFF80 , 0x0096 },
	{ 0xFF81 , 0x0001 },
	{ 0xC6B1 , 0x0007 },
	{ 0x1100 , 0x0000 },
	{ 0x4400 , 0x0001 },
	{ 0x4401 , 0x00C2 },
	{REG_WAIT, 120},
	{ 0x2900 , 0x0000 },
	{ 0x3500 , 0x0000 },
};

struct nov_regs samsung_init_seq[] = {
	{ 0xF000 , 0x55   },
	{ 0xF001 , 0xAA   },
	{ 0xF002 , 0x52   },
	{ 0xF003 , 0x08   },
	{ 0xF004 , 0x00   },
	{ 0xE000 , 0x01   },
	{ 0xE001 , 0x04   },
	{ 0x1100 , 0x0000 },
	{REG_WAIT, 120},
	{ 0x2900 , 0x0000 },
	{ 0x3500 , 0x0000 },
};

static int
kingdom_mddi_init(struct msm_mddi_bridge_platform_data *bridge_data,
		     struct msm_mddi_client_data *client_data)
{
	int i = 0, array_size;
	unsigned reg, val, tmp;
	struct nov_regs *init_seq = NULL;
	static bool bfirst = 1;
	static unsigned trim_val = 20;

	B(KERN_DEBUG "%s\n", __func__);
	client_data->auto_hibernate(client_data, 0);

	if ((panel_type == PANEL_ID_KIM_SONY_C2)) {
		init_seq = sony_init_seq_c2;
		array_size = ARRAY_SIZE(sony_init_seq_c2);
	} else if (panel_type == PANEL_ID_KIM_SAMSUNG) {
		init_seq = samsung_init_seq;
		array_size = ARRAY_SIZE(samsung_init_seq);
	} else {
		init_seq = sony_init_seq;
		array_size = ARRAY_SIZE(sony_init_seq);
	}

	for (i = 0; i < array_size; i++) {
		reg = cpu_to_le32(init_seq[i].reg);
		val = cpu_to_le32(init_seq[i].val);
		if (reg == REG_WAIT)
			msleep(val);
		else {
			client_data->remote_write(client_data, val, reg);
			if (reg == 0x0100)
				client_data->send_powerdown(client_data);

			if ((init_seq[i].reg == 0xFF81) && (init_seq[i].val == 0x0001) && (panel_type == PANEL_ID_KIM_SONY_C2)) {
				if (bfirst) {
					trim_val = ddt;
					bfirst = 0;
				}
				tmp = 0xF485;
				reg = cpu_to_le32(tmp);
				val = cpu_to_le32(trim_val);
				client_data->remote_write(client_data, val, reg);
			}
		}
	}

	client_data->auto_hibernate(client_data, 1);

	if (axi_clk)
		clk_set_rate(axi_clk, 0);

	return 0;
}

static int
kingdom_mddi_uninit(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s(%d)\n", __func__, __LINE__);
	return 0;
}

static int
kingdom_panel_blank(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s\n", __func__);
	client_data->auto_hibernate(client_data, 0);

	client_data->remote_write(client_data, 0x0, 0x5300);
	kingdom_backlight_switch(LED_OFF);

	client_data->remote_write(client_data, 0, 0x2800);
	client_data->remote_write(client_data, 0, 0x1000);
	hr_msleep(100);
	client_data->auto_hibernate(client_data, 1);
	return 0;
}

static int
kingdom_panel_unblank(struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	B(KERN_DEBUG "%s\n", __func__);
	client_data->auto_hibernate(client_data, 0);
	client_data->remote_write(client_data, 0x24, 0x5300);
	kingdom_backlight_switch(LED_FULL);
	client_data->auto_hibernate(client_data, 1);
	return 0;
}

static struct msm_mddi_bridge_platform_data novatec_client_data = {
	.init = kingdom_mddi_init,
	.uninit = kingdom_mddi_uninit,
	.blank = kingdom_panel_blank,
	.unblank = kingdom_panel_unblank,
	.fb_data = {
		.xres = 540,
		.yres = 960,
		.width = 50,
		.height = 88,
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

	if (on) {
		if (axi_clk)
			clk_set_rate(axi_clk, 192000000);

		config = PCOM_GPIO_CFG(KINGDOM_MDDI_TE, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(KINGDOM_LCD_ID1, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(KINGDOM_LCD_ID0, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);

		if (skuid == 0x2) /* incell*/
			gpio_set_value(105, 0);
		else /* not incell*/
			gpio_set_value(V_LCM_3V_EN, 0);
		hr_msleep(1);
		if (skuid == 0x2)
			gpio_set_value(105, 1);
		else
			gpio_set_value(V_LCM_3V_EN, 1);
		hr_msleep(1);
		vreg_enable(V_LCMIO_1V8);
		hr_msleep(6);
		gpio_set_value(KINGDOM_LCD_RSTz, 1);
		hr_msleep(1);
		gpio_set_value(KINGDOM_LCD_RSTz, 0);
		hr_msleep(1);
		gpio_set_value(KINGDOM_LCD_RSTz, 1);
		hr_msleep(6);
	} else {
		gpio_set_value(KINGDOM_LCD_RSTz, 0);
		hr_msleep(120);
		vreg_disable(V_LCMIO_1V8);
		hr_msleep(1);
		if (skuid == 0x2) /* incell*/
			gpio_set_value(105, 0);
		else /* not incell*/
			gpio_set_value(V_LCM_3V_EN, 0);

		config = PCOM_GPIO_CFG(KINGDOM_MDDI_TE, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(KINGDOM_LCD_ID1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
		config = PCOM_GPIO_CFG(KINGDOM_LCD_ID0, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
		rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);
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

static struct platform_driver kingdom_backlight_driver = {
	.probe = kingdom_backlight_probe,
	.driver = {
		.name = "nov_cabc",
		.owner = THIS_MODULE,
	},
};

static struct msm_mdp_platform_data mdp_pdata = {
	.color_format = MSM_MDP_OUT_IF_FMT_RGB888,
#ifdef CONFIG_MDP4_HW_VSYNC
	.xres = 540,
	.yres = 960,
	.back_porch = 4,
	.front_porch = 2,
	.pulse_width = 4,
#endif
};

int __init kingdom_init_panel(unsigned int sys_rev)
{
	int rc;

	B(KERN_INFO "%s: enter.\n", __func__);

	/* lcd panel power */
	V_LCMIO_1V8 = vreg_get(NULL, "lvsw0");

	if (IS_ERR(V_LCMIO_1V8)) {
		pr_err("%s: LCMIO_1V8 get failed (%ld)\n",
		       __func__, PTR_ERR(V_LCMIO_1V8));
		return -1;
	}

	resources_msm_fb[0].start = msm_fb_base;
	resources_msm_fb[0].end = msm_fb_base + MSM_FB_SIZE - 1;

	msm_device_mdp.dev.platform_data = &mdp_pdata;
	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;

	mddi_pdata.clk_rate = 445500000;

	mddi_pdata.type = MSM_MDP_MDDI_TYPE_II;

	axi_clk = clk_get(NULL, "ebi1_mddi_clk");
	if (IS_ERR(axi_clk)) {
		pr_err("%s: failed to get axi clock\n", __func__);
		return PTR_ERR(axi_clk);
	}

	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	rc = platform_device_register(&msm_device_mddi0);
	if (rc)
		return rc;

	rc = platform_driver_register(&kingdom_backlight_driver);
	if (rc)
		return rc;


	return 0;
}
static int __init getddt(char *str)
{
	get_option(&str, &ddt);
	printk(KERN_INFO "\n[disp] ddt = %d\n", ddt);
	return 0;
}

early_param("ddt", getddt);

static int __init getskuid(char *str)
{
	get_option(&str, &skuid);
	printk(KERN_INFO "\n[disp] skuid = %d\n", skuid);
	return 0;
}

early_param("skuid", getskuid);
