/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
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
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/earlysuspend.h>
#include <mach/mpp.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/mfd/pmicmax8957.h>
#include <linux/max8957_gpio.h>

static int max8957_gpio_to_irq(struct gpio_chip *chip, unsigned pin);
static int max8957_gpio_get(struct gpio_chip *chip, unsigned pin);
static void max8957_gpio_set(struct gpio_chip *chip, unsigned pin, int level);
static int max8957_gpio_dir_input(struct gpio_chip *chip, unsigned pin);
static int max8957_gpio_dir_output(struct gpio_chip *chip, unsigned pin, int val);
static void max8957_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip);

struct max8957_gpio_chip {
	struct gpio_chip	chip;
	u8 ctrl[MAX8957_GPIO_NUM];
	u8 pull[MAX8957_GPIO_NUM];
	u8 output_buffer[MAX8957_GPIO_NUM];
	int irq;
	int gpio_irq_base;
	/*
	struct mutex	irq_lock;
	*/
	struct workqueue_struct *max8957_gpio_queue;
	struct work_struct max8957_gpio_intr_work;
	struct work_struct max8957_wifi_bt_sleep_clk_on_work;
	struct work_struct max8957_wifi_bt_sleep_clk_off_work;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct max8957_gpio_ls ls_settings[MAX8957_GPIO_LS_MAX];
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void max8957_gpio_early_suspend(struct early_suspend *h);
static void max8957_gpio_late_resume(struct early_suspend *h);
#endif

static struct max8957_gpio_chip max8957_gpio_chip = {
	.chip = {
		.label			= "max8957-gpio",
		.to_irq			= max8957_gpio_to_irq,
		.get			= max8957_gpio_get,
		.set			= max8957_gpio_set,
		.direction_input	= max8957_gpio_dir_input,
		.direction_output	= max8957_gpio_dir_output,
		.dbg_show		= max8957_gpio_dbg_show,
		.ngpio			= MAX8957_GPIO_NUM,
	},
};

struct max8957_gpio_irq_data {
	u8  reg;
	u8	mask_reg;
	u8	enable;		/* enable mask data */
	u8	enable_cached;	/* cached enable mask data */
	u8  mask_bit;   /* bit in mask register*/
	u8  irq_bit;    /* bit in interrupt register*/
	u8  irq_type;   /* bit for IRQ type - 0:No irq, 1:falling edge, 2:rising edge, 3:both*/
	u8  wakeup;    /* set as wakeup interrupt */
};

static struct max8957_gpio_irq_data max8957_gpio_irqs[] = {
	[INT_GPIO0] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO0),
		.irq_bit	= MAX8957_GPIO0_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO1] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO1),
		.irq_bit	= MAX8957_GPIO1_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO2] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO2),
		.irq_bit	= MAX8957_GPIO2_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO3] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO3),
		.irq_bit	= MAX8957_GPIO3_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO4] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO4),
		.irq_bit	= MAX8957_GPIO4_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO5] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO5),
		.irq_bit	= MAX8957_GPIO5_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO6] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO6),
		.irq_bit	= MAX8957_GPIO6_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO7] = {
		.reg		= MAX8957_REG_GPIOINT0,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO7),
		.irq_bit	= MAX8957_GPIO7_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO8] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO8),
		.irq_bit	= MAX8957_GPIO8_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO9] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO9),
		.irq_bit	= MAX8957_GPIO9_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO10] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO10),
		.irq_bit	= MAX8957_GPIO10_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO11] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO11),
		.irq_bit	= MAX8957_GPIO11_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO12] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO12),
		.irq_bit	= MAX8957_GPIO12_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO13] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO13),
		.irq_bit	= MAX8957_GPIO13_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO14] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO14),
		.irq_bit	= MAX8957_GPIO14_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO15] = {
		.reg		= MAX8957_REG_GPIOINT1,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO15),
		.irq_bit	= MAX8957_GPIO15_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO16] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO16),
		.irq_bit	= MAX8957_GPIO16_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO17] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO17),
		.irq_bit	= MAX8957_GPIO17_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO18] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO18),
		.irq_bit	= MAX8957_GPIO18_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO19] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO19),
		.irq_bit	= MAX8957_GPIO19_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO20] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO20),
		.irq_bit	= MAX8957_GPIO20_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO21] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO21),
		.irq_bit	= MAX8957_GPIO21_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO22] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO22),
		.irq_bit	= MAX8957_GPIO22_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO23] = {
		.reg		= MAX8957_REG_GPIOINT2,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO23),
		.irq_bit	= MAX8957_GPIO23_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO24] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO24),
		.irq_bit	= MAX8957_GPIO24_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO25] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO25),
		.irq_bit	= MAX8957_GPIO25_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO26] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO26),
		.irq_bit	= MAX8957_GPIO26_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO27] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO27),
		.irq_bit	= MAX8957_GPIO27_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO28] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO28),
		.irq_bit	= MAX8957_GPIO28_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO29] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO29),
		.irq_bit	= MAX8957_GPIO29_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO30] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO30),
		.irq_bit	= MAX8957_GPIO30_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
	[INT_GPIO31] = {
		.reg		= MAX8957_REG_GPIOINT3,
		.mask_reg	= MAX8957_REG_GPIO_CNTL(INT_GPIO31),
		.irq_bit	= MAX8957_GPIO31_MASK,
		.mask_bit	= MAX8957_GPIO_INTM_MASK,
		.enable	= MAX8957_GPIO_INTM_MASK,
		.irq_type	= 0x00,
	},
};

static int max8957_gpio_write(struct max8957_chip *max8957_chip, u16 addr, u8 val,
		u8 mask, u8 *bak)
{
	u8 reg = (*bak & ~mask) | (val & mask);

	int ret = max8957_write(max8957_chip, addr, &reg, 1);

	if (!ret)
		*bak = reg;

	return ret;
}

static int max8957_gpio_get_pin(struct max8957_chip *max8957_chip, int pin)
{
	u8 addr = MAX8957_REG_GPIO_CNTL(pin);
	u8 val;

	max8957_read(max8957_chip, addr, &val, 1);

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s: addr=0x%x, reg=0x%x, val=0x%x\n", __func__, max8957_chip->dev->addr, addr, val);
#endif

	return (val & MAX8957_GPIO_INPUT_DATA_M) >> MAX8957_GPIO_INPUT_DATA_SHIFT;
}

static int max8957_gpio_to_irq(struct gpio_chip *chip, unsigned pin)
{
	struct max8957_gpio_platform_data *pdata;

	pdata = chip->dev->platform_data;

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s : gpio_irq_base=%d, pin=%d\n", __func__, pdata->irq_base, pin);
#endif

	return pdata->irq_base + pin;
}

static int max8957_irq_get_rt_status(struct max8957_chip *max8957_chip, int irq)
{
	/* Interrupt is handled by a separate driver.
	 * Need to view interface. return default for now.
	 */
	return 0;
}

static bool max8957_gpio_is_output(struct max8957_gpio_chip *io_chip, unsigned pin)
{
	if ((io_chip->ctrl[pin] & MAX8957_GPIO_DIR_M) >> MAX8957_GPIO_DIR_SHIFT ==
		MAX8957_GPIO_DIR_OUTPUT_V)
		return true;
	else
		return false;
}

int max8957_gpio_dump(void)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	uint8_t val = 0;
	int ret = 0, i = 0;

	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < MAX8957_GPIO_NUM; i++) {
		max8957_read(max8957_chip, MAX8957_REG_GPIO_CNTL(i), &val, 1);
		pr_err("%s: GPIO%d, addr=0x%x, reg_val=0x%x\n", __func__, i, MAX8957_REG_GPIO_CNTL(i), val);
	}

	return ret;
}

static int max8957_gpio_get(struct gpio_chip *chip, unsigned pin)
{
	struct max8957_gpio_chip *io_chip = dev_get_drvdata(chip->dev);
	struct max8957_chip *max8957_chip = dev_get_drvdata(chip->dev->parent);
	int level;

	if (max8957_gpio_is_output(io_chip, pin))
		level = (io_chip->ctrl[pin] & MAX8957_GPIO_DATA_M) >> MAX8957_GPIO_DATA_SHIFT;
	else
		level = max8957_gpio_get_pin(max8957_chip, pin);

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s: GPIO%d, level=%d\n", __func__, pin, level);
#endif

	return level;
}

static void max8957_gpio_set(struct gpio_chip *chip, unsigned pin, int level)
{
	struct max8957_gpio_chip *io_chip = dev_get_drvdata(chip->dev);
	struct max8957_chip *max8957_chip = dev_get_drvdata(chip->dev->parent);
	u8 reg = level ? MAX8957_GPIO_DATA_HI_V : MAX8957_GPIO_DATA_LO_V;
	int ret;

	if (max8957_gpio_is_output(io_chip, pin)) {
		reg |= (io_chip->ctrl[pin] & ~(MAX8957_GPIO_DATA_M));

		ret = max8957_gpio_write(max8957_chip,
			MAX8957_REG_GPIO_CNTL(pin),
			reg, MAX8957_GPIO_DATA_M | MAX8957_GPIO_OUT_BUF_M | MAX8957_GPIO_OUT_PULL_M,
			&io_chip->ctrl[pin]);

#if MAXIM_PMGPIO_DEBUG
		pr_err("%s: GPIO%d, level=%d, ctrl=0x%x, ret=%d\n", __func__, pin, level, io_chip->ctrl[pin], ret);
#endif
	} else
		pr_err("%s: GPIO%d is not an output pin!\n", __func__, pin);
}

static int max8957_gpio_dir_input(struct gpio_chip *chip, unsigned pin)
{
	struct max8957_gpio_chip *io_chip = dev_get_drvdata(chip->dev);
	struct max8957_chip *max8957_chip = dev_get_drvdata(chip->dev->parent);
	u8 reg;
	int ret;

	reg = ((io_chip->pull[pin] << MAX8957_GPIO_IN_PULL_SHIFT) & MAX8957_GPIO_IN_PULL_M) |
		  (MAX8957_GPIO_DIR_INPUT_V << MAX8957_GPIO_DIR_SHIFT);

	ret = max8957_gpio_write(max8957_chip,
			MAX8957_REG_GPIO_CNTL(pin),
			reg, MAX8957_GPIO_IN_PULL_M | MAX8957_GPIO_DIR_M,
			&io_chip->ctrl[pin]);

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s: GPIO%d, ctrl=0x%x, ret=%d\n", __func__, pin, io_chip->ctrl[pin], ret);
#endif

	return ret;
}

static int max8957_gpio_dir_output(struct gpio_chip *chip, unsigned pin, int val)
{
	struct max8957_gpio_chip *io_chip = dev_get_drvdata(chip->dev);
	struct max8957_chip *max8957_chip = dev_get_drvdata(chip->dev->parent);
	u8 reg = val ? MAX8957_GPIO_DATA_HI_V : MAX8957_GPIO_DATA_LO_V;
	int ret;

	reg |= ((io_chip->pull[pin] << MAX8957_GPIO_OUT_PULL_SHIFT) & MAX8957_GPIO_OUT_PULL_M) |
		  ((io_chip->output_buffer[pin] << MAX8957_GPIO_OUT_BUF_SHIFT) & MAX8957_GPIO_OUT_BUF_M) |
		  (MAX8957_GPIO_DIR_OUTPUT_V << MAX8957_GPIO_DIR_SHIFT);

	ret = max8957_gpio_write(max8957_chip,
			MAX8957_REG_GPIO_CNTL(pin), reg,
			MAX8957_GPIO_OUT_PULL_M | MAX8957_GPIO_OUT_BUF_M |
			MAX8957_GPIO_DIR_M | MAX8957_GPIO_DATA_M,
			&io_chip->ctrl[pin]);

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s: GPIO%d, ctrl=0x%x, ret=%d\n", __func__, pin, io_chip->ctrl[pin], ret);
#endif

	return ret;
}

static void max8957_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	static const char *ctype[] = { "d_in", "d_out", "bi_dir", "a_in",
			"a_out" };
	struct max8957_gpio_chip *io_chip = dev_get_drvdata(chip->dev);
	struct max8957_chip *max8957_chip = dev_get_drvdata(chip->dev->parent);
	u8 type, state;
	const char *label;
	int i;

	max8957_irq_get_rt_status(max8957_chip, 0);

	for (i = 0; i < MAX8957_GPIO_NUM; i++) {
			label = gpiochip_is_requested(chip, i);
			type = (io_chip->ctrl[i] & MAX8957_GPIO_DIR_M) >>
					MAX8957_GPIO_DIR_SHIFT;
			state = max8957_gpio_get_pin(max8957_chip, i);
			seq_printf(s, "gpio-%-3d (%-12.12s) %-10.10s"
							" %s 0x%02x\n",
							chip->base + i,
							label ? label : "--",
							ctype[type],
							state ? "hi" : "lo",
							io_chip->ctrl[i]);
	}
}

int max8957_gpio_config(unsigned num, struct max8957_gpio *param)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	u8	config, mask;
	int	ret;

	if (num >= MAX8957_GPIO_NUM) {
		pr_err("%s: GPIO(%d) out of range(%d)\n", __func__, num, MAX8957_GPIO_NUM);
		return -EINVAL;
	}

	if (!param) {
		pr_err("%s: GPIO(%d) with invalid param\n", __func__, num);
		return -1;
	}

	mask = MAX8957_GPIO_DIR_M | MAX8957_GPIO_DATA_M;

	if (param->direction == MAX8957_GPIO_DIR_INPUT_V) {
		/* Set direction */
		config = (param->direction << MAX8957_GPIO_DIR_SHIFT) & MAX8957_GPIO_DIR_M;

		/* Set pull-up */
		config |= (param->pull << MAX8957_GPIO_IN_PULL_SHIFT) & MAX8957_GPIO_IN_PULL_M;

		/* Set interrupt type (0:No irq, 1:falling edge, 2:rising edge, 3:both) */
		config |= (param->intr_type << MAX8957_GPIO_INTCNT_SHIFT) & MAX8957_GPIO_INTCNT_MASK;

		mask |= (MAX8957_GPIO_IN_PULL_M | MAX8957_GPIO_INTCNT_MASK);
	} else {
		/* Set direction and value */
		config = ((param->direction << MAX8957_GPIO_DIR_SHIFT) & MAX8957_GPIO_DIR_M) | \
				((param->output_value << MAX8957_GPIO_DATA_SHIFT) & MAX8957_GPIO_DATA_M);

		/* Set pull-up */
		config |= (param->pull << MAX8957_GPIO_OUT_PULL_SHIFT) & MAX8957_GPIO_OUT_PULL_M;

		/* Set output buffer */
		config |= (param->output_buffer << MAX8957_GPIO_OUT_BUF_SHIFT) & MAX8957_GPIO_OUT_BUF_M;

		mask |= (MAX8957_GPIO_OUT_PULL_M | MAX8957_GPIO_OUT_BUF_M);
	}

	ret = max8957_gpio_write(max8957_chip,
			MAX8957_REG_GPIO_CNTL(num),
			config, mask, &max8957_gpio_chip.ctrl[num]);

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s: GPIO%d, direction=%s, output_buffer=0x%x, output_value=0x%x, pull=0x%x, intr_type=0x%x, ctrl=0x%x, ret=%d\n",
		__func__, num, param->direction == MAX8957_GPIO_DIR_INPUT_V ? "INPUT" : "OUTPUT",
		param->output_buffer, param->output_value, param->pull, param->intr_type,
		max8957_gpio_chip.ctrl[num], ret);
#endif

	if (ret)
		pr_err("%s: Failed to config GPIO%d, ret=%d\n", __func__, num, ret);
	else {
		max8957_gpio_chip.output_buffer[num] = param->output_buffer;
		max8957_gpio_chip.pull[num] = param->pull;
		max8957_gpio_irqs[num].irq_type = param->intr_type;
	}

	return ret;
}
EXPORT_SYMBOL(max8957_gpio_config);

int max8957_gpio_cfg(unsigned num, unsigned direction, unsigned output_buffer, unsigned output_value, unsigned pull, unsigned intr_type)
{
	struct max8957_gpio pmic_gpio_setting = {
		.direction      = direction,
		.output_buffer  = output_buffer,
		.output_value   = output_value,
		.pull           = pull,
		.intr_type      = intr_type,
	};

	return max8957_gpio_config(num, &pmic_gpio_setting);
}
EXPORT_SYMBOL(max8957_gpio_cfg);

int max8957_wifi_bt_sleep_clk_on_off(int on)
{
	struct max8957_chip *max8957_chip = NULL;
	uint8_t val = 0;
	int ret = 0;

	max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return -ENODEV;
	}

	if (on) {
		val = (MAX8957_GPIO_RTCOUT_ENABLE << MAX8957_GPIO_RTCOUT_1_SHIFT) & MAX8957_GPIO_RTCOUT_1_M;
		pr_err("%s: EN SLEEP CLK\r\n", __func__);
	} else {
		val = (MAX8957_GPIO_RTCOUT_DISABLE << MAX8957_GPIO_RTCOUT_1_SHIFT) & MAX8957_GPIO_RTCOUT_1_M;
		pr_err("%s: DIS SLEEP CLK\r\n", __func__);
	}

    ret = max8957_set_bits(max8957_chip, MAX8957_GPIO_ALT_0, MAX8957_GPIO_RTCOUT_1_M, val);

	return ret;
}

static void max8957_wifi_bt_sleep_clk_on_work_func(struct work_struct *work)
{
	max8957_wifi_bt_sleep_clk_on_off(1);
	return;
}

static void max8957_wifi_bt_sleep_clk_off_work_func(struct work_struct *work)
{
	max8957_wifi_bt_sleep_clk_on_off(0);
	return;
}

int max8957_set_wifi_bt_sleep_clk(int on)
{
	if (!max8957_gpio_chip.chip.dev) {
		pr_err("%s: max8957_gpio is not initialized\r\n", __func__);
		return -ENODEV;
	}

	if (on)
		queue_work(max8957_gpio_chip.max8957_gpio_queue, &max8957_gpio_chip.max8957_wifi_bt_sleep_clk_on_work);
	else
		queue_work(max8957_gpio_chip.max8957_gpio_queue, &max8957_gpio_chip.max8957_wifi_bt_sleep_clk_off_work);

	return 0;
}
EXPORT_SYMBOL(max8957_set_wifi_bt_sleep_clk);

int max8957_config_level_shift(unsigned ls_id, unsigned mode)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	uint8_t val = 0;
	int ret = 0;

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s:+, ls_no=%d, mode=%d\n", __func__, ls_id, mode);
#endif

	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return -ENODEV;
	}

	if (ls_id == MAX8957_GPIO_LS_1) {
		val = (mode << MAX8957_GPIO_LS1_SHIFT) & MAX8957_GPIO_LS1_M;
		ret = max8957_set_bits(max8957_chip, MAX8957_GPIO_ALT_0, MAX8957_GPIO_LS1_M, val);
	} else if (ls_id == MAX8957_GPIO_LS_2) {
		val = (mode << MAX8957_GPIO_LS2_SHIFT) & MAX8957_GPIO_LS2_M;
		ret = max8957_set_bits(max8957_chip, MAX8957_GPIO_ALT_0, MAX8957_GPIO_LS2_M, val);
	} else {
		pr_err("%s: Invalid ls_id(%d)\r\n", __func__, ls_id);
		return -EINVAL;
	}

	/* Save level shift settings */
	if (!ret) {
		max8957_gpio_chip.ls_settings[ls_id].mode = mode;
		/*
		max8957_gpio_chip.ls_settings[ls_id].always_on = always_on;
		*/
	}

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s:-, ret=%d\n", __func__, ret);
#endif

	return ret;
}
EXPORT_SYMBOL(max8957_config_level_shift);

int max8957_gpio_set_intr_type(unsigned num, unsigned intr_type)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	struct max8957_gpio_irq_data *irq_data;
	uint8_t val = 0;
	int ret = 0;

#if MAXIM_INT_DEBUG
	pr_err("%s: GPIO(%d), intr_type=0x%x\n", __func__, num, intr_type);
#endif

	irq_data = &max8957_gpio_irqs[num];
	irq_data->irq_type = intr_type;

	if (!max8957_gpio_is_output(&max8957_gpio_chip, num)) {
		val = (intr_type << MAX8957_GPIO_INTCNT_SHIFT) & MAX8957_GPIO_INTCNT_MASK;
		ret = max8957_set_bits(max8957_chip, irq_data->mask_reg, MAX8957_GPIO_INTCNT_MASK, val);

#if MAXIM_INT_DEBUG
		printk(KERN_ERR "%s: update reg=0x%x, val=0x%x, mask=0x%x\n",
			__func__, irq_data->mask_reg, val, MAX8957_GPIO_INTCNT_MASK);
#endif
	}
#if MAXIM_INT_DEBUG
else
	printk(KERN_ERR "%s: skip updating intr configs for output pin\n", __func__);
#endif

	return ret;
}
EXPORT_SYMBOL(max8957_gpio_set_intr_type);

static void max8957_gpio_intr_work_func(struct work_struct *work)
{
	struct max8957_chip *max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	struct max8957_gpio_irq_data *irq_data;
	u8 gpio_irq[4] = {0, 0, 0, 0};
	int i, j = 0, ret = 0, handled = 0;
	u16 irqs_to_handle[INT_MAX_GPIO];

#if MAXIM_INT_DEBUG
	pr_err("%s:+, irq=%d\n", __func__, max8957_gpio_chip.irq);
#endif

	/* read GPIO INT registers */
	for (i = 0; i < 4; i++)
		ret |= max8957_read(max8957_chip, MAX8957_REG_GPIOINT0 + 2*i, &gpio_irq[i], 1);

	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: GPIO IRQ read error, ret=%d\n", __func__, ret);
		return;
	}
#if MAXIM_INT_DEBUG
	else
		printk(KERN_ERR "%s: GPIO IRQ read =0x%x, 0x%x, 0x%x, 0x%x\n",
		       __func__, gpio_irq[0], gpio_irq[1], gpio_irq[2], gpio_irq[3]);
#endif

	for (j = 0; j < 4; j++) {
		for (i = 0; i < 8; i++) {
		    irq_data = &max8957_gpio_irqs[i+j*8];
		    if ((gpio_irq[j] & irq_data->irq_bit) && !(irq_data->enable & MAX8957_GPIO_INTM_MASK)) {
				/* GPIO interrupt happened */
				/* Found one */
				irqs_to_handle[handled] = i + j*8 + max8957_gpio_chip.gpio_irq_base;
#if MAXIM_INT_DEBUG
				printk(KERN_ERR "%s: GPIO #%d interrupt happened IRQ=#%d\n",
					__func__, i+j*8, irqs_to_handle[handled]);
#endif
				handled++;
		    }
		}
	}

	for (i = 0; i < handled; i++) {
#if MAXIM_INT_DEBUG
		printk(KERN_ERR "GPIO IRQ nested =%d\n", irqs_to_handle[i]);
#endif
		handle_nested_irq(irqs_to_handle[i]);
	}

	enable_irq(max8957_gpio_chip.irq);

#if MAXIM_INT_DEBUG
	pr_err("%s:-\n", __func__);
#endif
	return;
}

static irqreturn_t max8957_gpio_isr(int irq, void *data)
{
#if MAXIM_INT_DEBUG
	/*
	pr_err("%s:+, irq=%d\n", __func__, irq);
	*/
#endif

	disable_irq_nosync(max8957_gpio_chip.irq);
	queue_work(max8957_gpio_chip.max8957_gpio_queue, &max8957_gpio_chip.max8957_gpio_intr_work);

#if MAXIM_INT_DEBUG
	/*
	pr_err("%s:-\n", __func__);
	*/
#endif
	return IRQ_HANDLED;
}

/* Internal functions */
static void max8957_gpio_irq_mask(struct irq_data *data)
{
	struct max8957_gpio_irq_data *max8957_irq_data;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_irq_data = &max8957_gpio_irqs[data->irq - max8957_gpio_chip.gpio_irq_base];
	max8957_irq_data->enable = max8957_irq_data->mask_bit;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d, enable=0x%x\n", __func__, data->irq, max8957_irq_data->enable);
#endif
}

static void max8957_gpio_irq_unmask(struct irq_data *data)
{
	struct max8957_gpio_irq_data *max8957_irq_data;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_irq_data = &max8957_gpio_irqs[data->irq - max8957_gpio_chip.gpio_irq_base];
	max8957_irq_data->enable = ~(max8957_irq_data->mask_bit | MAX8957_GPIO_INTCNT_MASK)
		| (max8957_irq_data->irq_type << MAX8957_GPIO_INTCNT_SHIFT);

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d, enable=0x%x\n", __func__, data->irq, max8957_irq_data->enable);
#endif
}

static void max8957_gpio_irq_lock(struct irq_data *data)
{
	struct max8957_chip *max8957_chip = NULL;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_chip = data->chip_data;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d\n", __func__, data->irq);
#endif

	mutex_lock(&max8957_chip->irq_lock);
}

static void max8957_gpio_irq_sync_unlock(struct irq_data *data)
{
	struct max8957_chip *max8957_chip = NULL;
	struct max8957_gpio_irq_data *max8957_irq_data;
	unsigned int max8957_irq;
	u8 config = 0;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d\n", __func__, data->irq);
#endif

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_chip = data->chip_data;
	max8957_irq = data->irq - max8957_gpio_chip.gpio_irq_base;

	if (!max8957_gpio_is_output(&max8957_gpio_chip, max8957_irq)) {
		max8957_irq_data = &max8957_gpio_irqs[max8957_irq];
		config = max8957_irq_data->enable;

#if MAXIM_INT_DEBUG
		printk(KERN_ERR "%s: irq_num=%d(%d), config=0x%x, mask=0x%x, enable_cached=0x%x\n",
			__func__, max8957_irq, data->irq, config, max8957_irq_data->mask_bit, max8957_irq_data->enable_cached);
#endif

		if (max8957_irq_data->enable_cached != config) {
			max8957_set_bits(max8957_chip, max8957_irq_data->mask_reg, max8957_irq_data->mask_bit, config);
			max8957_irq_data->enable_cached = config;
#if MAXIM_INT_DEBUG
			printk(KERN_ERR "%s: update irq_num=%d(%d), config=0x%x, mask=0x%x\n",
				__func__, max8957_irq, data->irq, config, max8957_irq_data->mask_bit);
#endif
		}
	}
#if MAXIM_INT_DEBUG
	else
		printk(KERN_ERR "%s: irq_num=%d(%d), don't update intr configs for output pin\n",
			__func__, max8957_irq, data->irq);
#endif

	mutex_unlock(&max8957_chip->irq_lock);
}

int	max8957_gpio_irq_set_wake(struct irq_data *data, unsigned int on)
{
#if MAXIM_PMGPIO_SET_WAKE_ENABLE
	struct max8957_gpio_irq_data *max8957_irq_data;
	uint8_t idx = 0;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return -EINVAL;
	}

	idx = data->irq - max8957_gpio_chip.gpio_irq_base;
	max8957_irq_data = &max8957_gpio_irqs[idx];
	if (!max8957_irq_data) {
		pr_err("%s: max8957_irq_data(%d) is NULL\n", __func__, idx);
		return -EINVAL;
	}

	max8957_irq_data->wakeup = on ? 1 : 0;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d, wakeup=%d\n", __func__, data->irq, max8957_irq_data->wakeup);
#endif
#endif

	return 0;
}

static struct irq_chip max8957_gpio_irq_chip = {
	.name		= "max8957-gpio-irq",
	.irq_bus_lock	= max8957_gpio_irq_lock,
	.irq_bus_sync_unlock = max8957_gpio_irq_sync_unlock,
    .irq_mask       = max8957_gpio_irq_mask,
    .irq_unmask     = max8957_gpio_irq_unmask,
	.irq_set_wake	= max8957_gpio_irq_set_wake,
};

static int __devinit max8957_gpio_probe(struct platform_device *pdev)
{
	struct max8957_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct max8957_chip *max8957_chip = dev_get_drvdata(pdev->dev.parent);
	int ret;
	int i;

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s: +\n", __func__);
#endif

	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return -ENODEV;
	}

	/* Read back MAX8957_REG_GPIO_CNTL registers for cache */
	for (i = 0; i < MAX8957_GPIO_NUM; i++) {
		ret = max8957_read(max8957_chip,
			MAX8957_REG_GPIO_CNTL(i),
			&max8957_gpio_chip.ctrl[i], 1);

		if (ret)
			goto err;

#if MAXIM_PMGPIO_DEBUG
		pr_err("%s: ctrl[%d]=0x%x\n", __func__, i, max8957_gpio_chip.ctrl[i]);
#endif

		if (max8957_gpio_is_output(&max8957_gpio_chip, i)) {
			/* Save output puffer type */
			max8957_gpio_chip.output_buffer[i] =
				(max8957_gpio_chip.ctrl[i] & MAX8957_GPIO_OUT_BUF_M) >> MAX8957_GPIO_OUT_BUF_SHIFT;

			/* Save pull-up status */
			max8957_gpio_chip.pull[i] =
				(max8957_gpio_chip.ctrl[i] & MAX8957_GPIO_OUT_PULL_M) >> MAX8957_GPIO_OUT_PULL_SHIFT;
		}
		/* Save status for input pin */
		else {
			/* Save pull-up status */
			max8957_gpio_chip.pull[i] =
				(max8957_gpio_chip.ctrl[i] & MAX8957_GPIO_IN_PULL_M) >> MAX8957_GPIO_IN_PULL_SHIFT;
		}
	}

	max8957_gpio_chip.chip.dev = &pdev->dev;
	max8957_gpio_chip.chip.base = pdata->gpio_base;

	platform_set_drvdata(pdev, &max8957_gpio_chip);

	/* Init GPIOs before configuring interrupts */
	if (pdata->init) {
		ret = pdata->init();
		if (unlikely(ret < 0))
			goto err;
	}

	max8957_gpio_chip.irq = platform_get_irq(pdev, 0);
	max8957_gpio_chip.gpio_irq_base = pdata->irq_base;

	/* register with genirq */
	/* Set IRQ data for GPIO interrupts */
	for (i = max8957_gpio_chip.gpio_irq_base; i < (max8957_gpio_chip.gpio_irq_base + MAX8957_GPIO_NUM); i++) {
#if MAXIM_INT_DEBUG
		pr_err("%s: register MAX8957_GPIO_NUM(%d) with irq(%d)\n", __func__, i-max8957_gpio_chip.gpio_irq_base, i);
#endif
#if 0
		set_irq_chip(i, &max8957_gpio_irq_chip);
		set_irq_data(i, (void *)max8957_chip);
		set_irq_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
		set_irq_nested_thread(i, 1);
#else
		irq_set_chip_data(i, (void *)max8957_chip);
		irq_set_chip_and_handler(i, &max8957_gpio_irq_chip, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
		irq_set_nested_thread(i, 1);
#endif
	}

	/* Enable KBC/GPIO - Already initialized in hboot */
	/*
	val = 0xFF;
	ret = max8957_write(max8957_chip, 0x65, &val, 1);
	if (unlikely(ret < 0))
		goto err;

	val = 0xFF;
	ret = max8957_write(max8957_chip, 0x66, &val, 1);
	if (unlikely(ret < 0))
		goto err;

	val = 0xFF;
	ret = max8957_write(max8957_chip, 0x67, &val, 1);
	if (unlikely(ret < 0))
		goto err;
	*/

	max8957_gpio_chip.max8957_gpio_queue = create_singlethread_workqueue("max8957_gpio_work_q");
	if (max8957_gpio_chip.max8957_gpio_queue == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	INIT_WORK(&max8957_gpio_chip.max8957_gpio_intr_work, max8957_gpio_intr_work_func);
	INIT_WORK(&max8957_gpio_chip.max8957_wifi_bt_sleep_clk_on_work, max8957_wifi_bt_sleep_clk_on_work_func);
	INIT_WORK(&max8957_gpio_chip.max8957_wifi_bt_sleep_clk_off_work, max8957_wifi_bt_sleep_clk_off_work_func);

	ret = request_any_context_irq(max8957_gpio_chip.irq, max8957_gpio_isr,
			IRQF_ONESHOT, "max8957-gpio-irq", max8957_chip);
	if (ret < 0)
		dev_err(&pdev->dev, "%s: Failed to request GPIO IRQ#%d, ret=%d\n", __func__, max8957_gpio_chip.irq, ret);

	ret = gpiochip_add(&max8957_gpio_chip.chip);
	if (unlikely(ret < 0))
		goto err;

#ifdef CONFIG_HAS_EARLYSUSPEND
	max8957_gpio_chip.early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	max8957_gpio_chip.early_suspend.suspend = max8957_gpio_early_suspend;
	max8957_gpio_chip.early_suspend.resume = max8957_gpio_late_resume;
	register_early_suspend(&max8957_gpio_chip.early_suspend);
#endif

#if MAXIM_PMGPIO_DEBUG
	pr_info("%s: -, ret=%d\n", __func__, ret);
#endif

	if (pdata->max8957_gpio_device_register)
		pdata->max8957_gpio_device_register();

	return ret;

err:

	if (max8957_gpio_chip.max8957_gpio_queue)
		destroy_workqueue(max8957_gpio_chip.max8957_gpio_queue);

	pr_info("%s: -, Failed, ret=%d\n", __func__, ret);

	return ret;
}

static int __devexit max8957_gpio_remove(struct platform_device *pdev)
{
	if (max8957_gpio_chip.max8957_gpio_queue)
		destroy_workqueue(max8957_gpio_chip.max8957_gpio_queue);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&max8957_gpio_chip.early_suspend);
#endif

	free_irq(max8957_gpio_chip.irq, pdev);

	return gpiochip_remove(&max8957_gpio_chip.chip);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void max8957_gpio_early_suspend(struct early_suspend *h)
{
#if MAXIM_PMGPIO_SET_WAKE_ENABLE
	struct max8957_chip *max8957_chip = NULL;
	struct max8957_gpio_irq_data *max8957_irq_data = NULL;
	uint8_t idx = 0;
#endif

#if MAXIM_PMGPIO_DEBUG
	int ret = 0;
	pr_err("%s:+\n", __func__);
#endif

#if MAXIM_PMGPIO_SET_WAKE_ENABLE
	max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return;
	}

	for (idx = 0; idx < INT_MAX_GPIO; idx++) {
		max8957_irq_data = &max8957_gpio_irqs[idx];
		if (!max8957_irq_data) {
			pr_err("%s: max8957_irq_data(%d) is NULL, skip\n", __func__, idx);
			continue;
		}

		/* Disable non-wakeup interrupt when suspend */
		if (!max8957_irq_data->wakeup &&
			!(max8957_irq_data->enable & max8957_irq_data->mask_bit)) {
			max8957_set_bits(max8957_chip, max8957_irq_data->mask_reg, max8957_irq_data->mask_bit, max8957_irq_data->mask_bit);
#if MAXIM_INT_DEBUG
			pr_err("%s: Disable int of GPIO(%d)\n", __func__, idx);
#endif
		}
	}
#endif

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s:-, ret=%d\n", __func__, ret);
#endif
}

static void max8957_gpio_late_resume(struct early_suspend *h)
{
#if MAXIM_PMGPIO_SET_WAKE_ENABLE
	struct max8957_chip *max8957_chip = NULL;
	struct max8957_gpio_irq_data *max8957_irq_data = NULL;
	uint8_t idx = 0;
#endif

#if MAXIM_PMGPIO_DEBUG
	int ret = 0;
	pr_err("%s:+\n", __func__);
#endif

#if MAXIM_PMGPIO_SET_WAKE_ENABLE
	max8957_chip = dev_get_drvdata(max8957_gpio_chip.chip.dev->parent);
	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return;
	}

	for (idx = 0; idx < INT_MAX_GPIO; idx++) {
		max8957_irq_data = &max8957_gpio_irqs[idx];
		if (!max8957_irq_data) {
			pr_err("%s: max8957_irq_data(%d) is NULL, skip\n", __func__, idx);
			continue;
		}

		/* Enable non-wakeup interrupt when resume */
		if (!max8957_irq_data->wakeup &&
			!(max8957_irq_data->enable & max8957_irq_data->mask_bit)) {
			max8957_set_bits(max8957_chip, max8957_irq_data->mask_reg, max8957_irq_data->mask_bit, max8957_irq_data->enable);
#if MAXIM_INT_DEBUG
			pr_err("%s: Enable int of GPIO(%d)\n", __func__, idx);
#endif
		}
	}
#endif

#if MAXIM_PMGPIO_DEBUG
	pr_err("%s:-, ret=%d\n", __func__, ret);
#endif
}
#endif

static struct platform_driver max8957_gpio_driver = {
	.probe		= max8957_gpio_probe,
	.remove		= __devexit_p(max8957_gpio_remove),
	.driver		= {
		.name = "max8957-gpio",
		.owner = THIS_MODULE,
	},
};

static int __init max8957_gpio_init(void)
{
	return platform_driver_register(&max8957_gpio_driver);
}

static void __exit max8957_gpio_exit(void)
{
	platform_driver_unregister(&max8957_gpio_driver);
}

subsys_initcall(max8957_gpio_init);
module_exit(max8957_gpio_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MAX8957 GPIO driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:max8957-gpio");

