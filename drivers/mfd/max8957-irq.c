/*
 * Maxim MAX8957 IRQ driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pmicmax8957.h>
#ifdef CONFIG_HTC_BATTCHG_MAX8957
#include <mach/htc_battery_max8957.h>
#endif

enum {
	FLAGS_SID1 = 1,	/* register in MAX8957 SID1 Slave address */
	FLAGS_SID3,		/* register in MAX8957 SID3 Slave address */
	/* FLAGS_MAX8967, */	/* register in MAX8967 Slave address */
};

struct max8957_main_irq_data {
	int irq; /* IRQ from PMIC */
	int irq_base; /* IRQ base for PMIC interrupts */
#ifdef CONFIG_HTC_BATTCHG_MAX8957
	int fg_irq; /* Fuel Gauge IRQ from PMIC */
	int fg_irq_base; /* Fuel Gauge IRQ base for PMIC interrupts */
	struct work_struct max8957_fg_intr_work;
#endif
	struct max8957_chip *chip1; /* MAX8957 SID1 */
	struct max8957_chip *chip2; /* MAX8957 SID3 */
	struct workqueue_struct *max8957_intr_queue;
	struct work_struct max8957_intr_work;
};

struct max8957_irq_data {
	u8	reg;
	u8	mask_reg;
	u8	enable;		/* enable mask data */
	u8	enable_cached;	/* cached enable mask data */
	u8	mask_bit;	/* bit in mask registe*/
	u8	irq_bit;	/* bit in interrupt register*/
	u8	flags;		/* Slave ID */
};

static struct max8957_irq_data max8957_irqs[] =
{
	[MAX8957_INT_GRPA_FLASHINT] = {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_FLASHINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_FLASHINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPA_KBCINT] = {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_KBCINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_KBCINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPA_GPIOINT] = {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_GPIOINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_GPIOINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPA_WLEDINT] = {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_WLEDINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_WLEDINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPA_FGINT] =  {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_FGINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_FGINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPA_LEDINT] = {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_LEDINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_LEDINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPA_MICBINT] = {
		.reg		= MAX8957_REG_INTGRPA,
		.mask_reg	= MAX8957_REG_INTGRPA_M,
		.irq_bit	= MAX8957_INTGRPA_MICBINT_MASK,
		.mask_bit	= MAX8957_INTGRPA_MICBINT_MASK,
		.flags		= FLAGS_SID1,
		.enable		= 0xFF,
	},
#if MAXIM_GRPB_INT_ENABLE
	/* GROUP B */
	[MAX8957_INT_GRPB_CHGINT] = {
		.reg		= MAX8957_REG_INTGRPB,
		.mask_reg	= MAX8957_REG_INTGRPB_M,
		.irq_bit	= MAX8957_INTGRPB_CHGINT_MASK,
		.mask_bit	= MAX8957_INTGRPB_CHGINT_MASK,
		.flags		= FLAGS_SID3,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPB_RTCINT] = {
		.reg		= MAX8957_REG_INTGRPB,
		.mask_reg	= MAX8957_REG_INTGRPB_M,
		.irq_bit	= MAX8957_INTGRPB_RTCINT_MASK,
		.mask_bit	= MAX8957_INTGRPB_RTCINT_MASK,
		.flags		= FLAGS_SID3,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPB_ADCINT] = {
		.reg		= MAX8957_REG_INTGRPB,
		.mask_reg	= MAX8957_REG_INTGRPB_M,
		.irq_bit	= MAX8957_INTGRPB_ADCINT_MASK,
		.mask_bit	= MAX8957_INTGRPB_ADCINT_MASK,
		.flags		= FLAGS_SID3,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPB_BUCKINT] = {
		.reg		= MAX8957_REG_INTGRPB,
		.mask_reg	= MAX8957_REG_INTGRPB_M,
		.irq_bit	= MAX8957_INTGRPB_BUCKINT_MASK,
		.mask_bit	= MAX8957_INTGRPB_BUCKINT_MASK,
		.flags		= FLAGS_SID3,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPB_LDOINT] = {
		.reg		= MAX8957_REG_INTGRPB,
		.mask_reg	= MAX8957_REG_INTGRPB_M,
		.irq_bit	= MAX8957_INTGRPB_LDOINT_MASK,
		.mask_bit	= MAX8957_INTGRPB_LDOINT_MASK,
		.flags		= FLAGS_SID3,
		.enable		= 0xFF,
	},
	[MAX8957_INT_GRPB_TOPSYSINT] = {
		.reg		= MAX8957_REG_INTGRPB,
		.mask_reg	= MAX8957_REG_INTGRPB_M,
		.irq_bit	= MAX8957_INTGRPB_TOPSYSINT_MASK,
		.mask_bit	= MAX8957_INTGRPB_TOPSYSINT_MASK,
		.flags		= FLAGS_SID3,
		.enable		= 0xFF,
	},
#endif /* MAXIM_GRPB_INT_ENABLE */
};

static struct max8957_main_irq_data max8957_main_irq_data = {
	/* It doesn't need to be initialized with 0. Just for cleanress.*/
	.irq = 0,
	.irq_base = MAX8957_IRQ_BASE,
	.chip1 = NULL,
	.chip2 = NULL,
};

/* Internal functions */
static void max8957_irq_mask(struct irq_data *data)
{
	struct max8957_chip *max8957_chip = NULL;
	unsigned int max8957_irq;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_chip = data->chip_data;
	max8957_irq = data->irq - max8957_main_irq_data.irq_base;

	max8957_chip->irqs_1st_allowed[max8957_irq] = 0;
	max8957_irqs[max8957_irq].enable = max8957_irqs[max8957_irq].mask_bit;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d, enable=0x%x\n", __func__, data->irq, max8957_irqs[max8957_irq].enable);
#endif

}

static void max8957_irq_unmask(struct irq_data *data)
{
	struct max8957_chip *max8957_chip = NULL;
	unsigned int max8957_irq;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_chip = data->chip_data;
	max8957_irq = data->irq - max8957_main_irq_data.irq_base;

	max8957_chip->irqs_1st_allowed[max8957_irq] = 1;
	max8957_irqs[max8957_irq].enable = ~max8957_irqs[max8957_irq].mask_bit;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d, enable=0x%x\n", __func__, data->irq, max8957_irqs[max8957_irq].enable);
#endif

}

static void max8957_irq_lock(struct irq_data *data)
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

static void max8957_irq_sync_unlock(struct irq_data *data)
{
	struct max8957_irq_data *max8957_irq_data;
	struct max8957_chip *max8957_chip = NULL;
	unsigned int max8957_irq;
	u8 config = 0;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_chip = data->chip_data;
	max8957_irq = data->irq - max8957_main_irq_data.irq_base;
	max8957_irq_data = &max8957_irqs[max8957_irq];
	config = max8957_irq_data->enable;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d, config=0x%x, mask=0x%x, enable_cached=0x%x\n",
		__func__, data->irq, config, max8957_irq_data->mask_bit, max8957_irq_data->enable_cached);
#endif

	if (max8957_irq_data->enable_cached != config) {
		max8957_set_bits(max8957_chip, max8957_irq_data->mask_reg, max8957_irq_data->mask_bit, config);
		max8957_irq_data->enable_cached = config;
#if MAXIM_INT_DEBUG
		printk(KERN_ERR "%s: update irq_num=%d(%d), config=0x%x, mask=0x%x\n",
			__func__, max8957_irq, data->irq, config, max8957_irq_data->mask_bit);
#endif
	}
	mutex_unlock(&max8957_chip->irq_lock);
}

static struct irq_chip max8957_irq_chip = {
	.name			= "max8957-irq",
	.irq_bus_lock		= max8957_irq_lock,
	.irq_bus_sync_unlock	= max8957_irq_sync_unlock,
	.irq_mask			= max8957_irq_mask,
	.irq_unmask			= max8957_irq_unmask,
};

static void max8957_intr_work_func(struct work_struct *work)
{
	struct max8957_chip *chip;
	struct max8957_irq_data *irq_data;
	u8 grp_irq;
	int i = 0, ret = 0;
	int handled = 0;
	u16 irqs_to_handle[MAX8957_NUM_BLOCKS];

#if MAXIM_INT_DEBUG
	printk(KERN_ERR "%s: Top IRQ ISR enter\n", __func__);
#endif

	if (max8957_main_irq_data.chip1) {
		/* read first level interrupt in group A */
		grp_irq = 0;
		chip = max8957_main_irq_data.chip1;
		ret = max8957_read(chip, MAX8957_REG_INTGRPA, &grp_irq, 1);
		if (ret == -EINVAL)
			printk(KERN_ERR "%s: GroupA Top IRQ read error\n", __func__);
#if MAXIM_INT_DEBUG
		else {
			/*
			printk(KERN_ERR "%s: GroupA Top IRQ read=0x%x\n", __func__, grp_irq);
			*/
		}
#endif

		for (i = MAX8957_INT_GRPA_START; i <= MAX8957_INT_GRPA_END; i++) {
			irq_data = &max8957_irqs[i];
			/*
			pr_err("%s: i=%d, irq_bit=0x%x, irqs_1st_allowed=%d\n", __func__, i, irq_data->irq_bit, chip->irqs_1st_allowed[i]);
			*/
			if ((grp_irq & irq_data->irq_bit) && chip->irqs_1st_allowed[i]) {
				/* top level interrupt happened */
				/* Found one */
				irqs_to_handle[handled] = i + max8957_main_irq_data.irq_base;
				handled++;
			}
		}
	}

#if MAXIM_GRPB_INT_ENABLE
	if (max8957_main_irq_data.chip2) {
		/* read first level interrupt in group B */
		grp_irq = 0;
		chip = max8957_main_irq_data.chip2;
		ret = max8957_read(chip, MAX8957_REG_INTGRPB, &grp_irq, 1);
		if (ret == -EINVAL)
			printk(KERN_ERR "%s: GroupB Top IRQ read error\n", __func__);
#if MAXIM_INT_DEBUG
		else {
			/*
			printk(KERN_ERR "%s: GroupB Top IRQ read =0x%x\n", __func__, grp_irq);
			*/
		}
#endif

		for (i = MAX8957_INT_GRPB_START; i <= MAX8957_INT_GRPB_END; i++) {
			irq_data = &max8957_irqs[i];
			if ((grp_irq & irq_data->irq_bit) && chip->irqs_1st_allowed[i]) {
				/* top level interrupt happened */
				/* Found one */
				irqs_to_handle[handled] = i + max8957_main_irq_data.irq_base;
				handled++;
			}
		}
	}
#endif /* MAXIM_GRPB_INT_ENABLE */

	for (i = 0; i < handled; i++) {
#if MAXIM_INT_DEBUG
		printk(KERN_ERR "%s: Top IRQ nested =%d\n", __func__, irqs_to_handle[i]);
#endif
		handle_nested_irq(irqs_to_handle[i]);
	}

	enable_irq(max8957_main_irq_data.irq);

#if MAXIM_INT_DEBUG
	printk(KERN_ERR "%s: Top IRQ ISR exit\n", __func__);
#endif

	return;
}

static irqreturn_t max8957_irq_isr(int irq, void *data)
{
#if MAXIM_INT_DEBUG
	pr_err("%s:+, irq=%d\n", __func__, irq);
#endif

	disable_irq_nosync(max8957_main_irq_data.irq);
	queue_work(max8957_main_irq_data.max8957_intr_queue, &max8957_main_irq_data.max8957_intr_work);

#if MAXIM_INT_DEBUG
	pr_err("%s:-\n", __func__);
#endif
	return IRQ_HANDLED;
}

/* JR: Handle Fuel Gauge interrupts here because they belong to SID1 */
#ifdef CONFIG_HTC_BATTCHG_MAX8957
struct max8957_fg_irq_data {
	u8  reg;
	u8	mask_reg;
	u8	enable;		/* enable mask data */
	u8	enable_cached;	/* cached enable mask data */
	u8  mask_bit;   /* bit in mask register*/
	u8  irq_bit;    /* bit in interrupt register*/
};

static struct max8957_fg_irq_data max8957_fg_irqs[] = {
	[INT_TMN] = {
		.reg		= MAX8957_REG_FG_INT,
		.mask_reg	= MAX8957_REG_FG_INT_M,
		.irq_bit    = MAX8957_TMN_MASK,
		.mask_bit	= MAX8957_TMN_MASK,
		.enable     = 0xFF,
	},
	[INT_TMX] = {
		.reg		= MAX8957_REG_FG_INT,
		.mask_reg	= MAX8957_REG_FG_INT_M,
		.irq_bit    = MAX8957_TMX_MASK,
		.mask_bit	= MAX8957_TMX_MASK,
		.enable     = 0xFF,
	},
	[INT_VMN] = {
		.reg		= MAX8957_REG_FG_INT,
		.mask_reg	= MAX8957_REG_FG_INT_M,
		.irq_bit    = MAX8957_VMN_MASK,
		.mask_bit	= MAX8957_VMN_MASK,
		.enable     = 0xFF,
	},
	[INT_VMX] = {
		.reg		= MAX8957_REG_FG_INT,
		.mask_reg	= MAX8957_REG_FG_INT_M,
		.irq_bit    = MAX8957_VMX_MASK,
		.mask_bit	= MAX8957_VMX_MASK,
		.enable     = 0xFF,
	},
	[INT_SMN] = {
		.reg		= MAX8957_REG_FG_INT,
		.mask_reg	= MAX8957_REG_FG_INT_M,
		.irq_bit    = MAX8957_SMN_MASK,
		.mask_bit	= MAX8957_SMN_MASK,
		.enable     = 0xFF,
	},
	[INT_SMX] = {
		.reg		= MAX8957_REG_FG_INT,
		.mask_reg	= MAX8957_REG_FG_INT_M,
		.irq_bit    = MAX8957_SMX_MASK,
		.mask_bit	= MAX8957_SMX_MASK,
		.enable     = 0xFF,
	},
};

static void max8957_fg_intr_work_func(struct work_struct *work)
{
	struct max8957_chip *chip;
	struct max8957_fg_irq_data *irq_data;
	int i = 0, ret = 0, handled = 0;
	u8 reg_data = 0;
	u16 irqs_to_handle[INT_MAX_FG];

#if MAXIM_INT_DEBUG
	pr_err("%s:+, irq=%d\n", __func__, max8957_main_irq_data.fg_irq);
#endif

	if (max8957_main_irq_data.chip1)
		chip = max8957_main_irq_data.chip1;
	else {
		pr_err("%s: max8957_chip is not initialized\n", __func__);
		return;
	}

	/* Read FG status registers */
	ret = max8957_read(chip, MAX8957_REG_FG_INT, &reg_data, 1);
	if (unlikely(ret < 0)) {
		pr_err("%s: FG IRQ read error, ret=%d\n", __func__, ret);
		return;
	}
#if MAXIM_INT_DEBUG
	else
		pr_err("%s: FG IRQ read =0x%x\n", __func__, reg_data);
#endif

	for (i = 0; i < INT_MAX_FG; i++) {
		irq_data = &max8957_fg_irqs[i];
#if MAXIM_INT_DEBUG
		/*
		pr_err("%s: Check irqs[%d], irq_bit=0x%x, enable=0x%x, mask_bit=0x%x\n",
				__func__, i, irq_data->irq_bit, irq_data->enable, irq_data->mask_bit);
		*/
#endif

		if ((reg_data & irq_data->irq_bit) && !(irq_data->enable & irq_data->mask_bit)) {
			/* FG interrupt happened */
			/* Found one */
			irqs_to_handle[handled] = i + max8957_main_irq_data.fg_irq_base;
#if MAXIM_INT_DEBUG
			pr_err("%s: FG #%d interrupt happened IRQ=#%d\n",
					__func__, i, irqs_to_handle[handled]);
#endif
			handled++;
		}
	}

	for (i = 0; i < handled; i++) {
#if MAXIM_INT_DEBUG
		pr_err("FG IRQ nested =%d\n", irqs_to_handle[i]);
#endif
		/* Calling handler function to handle the interrupt directly */

		/* JR:Don't disable then enable the interrupt or it will be triggered again
		even the condition is not changed */

		disable_irq_nosync(irqs_to_handle[i]);

		switch (irqs_to_handle[i] - max8957_main_irq_data.fg_irq_base) {
		case INT_TMN:
			max8957_fg_alert_handler(MAX8957_FG_ALERT_TEMPERATURE, MAX8957_FG_ALERT_THRES_MIN);
			break;
		case INT_TMX:
			max8957_fg_alert_handler(MAX8957_FG_ALERT_TEMPERATURE, MAX8957_FG_ALERT_THRES_MAX);
			break;
		case INT_VMN:
			max8957_fg_alert_handler(MAX8957_FG_ALERT_VOLTAGE, MAX8957_FG_ALERT_THRES_MIN);
			break;
		case INT_VMX:
			max8957_fg_alert_handler(MAX8957_FG_ALERT_VOLTAGE, MAX8957_FG_ALERT_THRES_MAX);
			break;
		case INT_SMN:
			max8957_fg_alert_handler(MAX8957_FG_ALERT_CAPACITY, MAX8957_FG_ALERT_THRES_MIN);
			break;
		case INT_SMX:
			max8957_fg_alert_handler(MAX8957_FG_ALERT_CAPACITY, MAX8957_FG_ALERT_THRES_MAX);
			break;
		default:
			pr_err("%s: FG IRQ unknown(%d)\n", __func__, irqs_to_handle[i] - max8957_main_irq_data.fg_irq_base);
			break;
		}

		enable_irq(irqs_to_handle[i]);
	}

	enable_irq(max8957_main_irq_data.fg_irq);
#if MAXIM_INT_DEBUG
	pr_err("%s:-\n", __func__);
#endif
	return;
}

static irqreturn_t max8957_fg_isr(int irq, void *data)
{
#if MAXIM_INT_DEBUG
	pr_err("%s:+, Fuel gauge interrupt triggered\n", __func__);
#endif

	disable_irq_nosync(max8957_main_irq_data.fg_irq);
	queue_work(max8957_main_irq_data.max8957_intr_queue, &max8957_main_irq_data.max8957_fg_intr_work);

#if MAXIM_INT_DEBUG
	pr_err("%s:-\n", __func__);
#endif

	return IRQ_HANDLED;
}

/* Internal functions */
static void max8957_fg_irq_mask(struct irq_data *data)
{
	struct max8957_fg_irq_data *max8957_irq_data;
	unsigned int max8957_irq;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_irq = data->irq - max8957_main_irq_data.fg_irq_base;
	max8957_irq_data = &max8957_fg_irqs[max8957_irq];
	max8957_irq_data->enable = max8957_irq_data->mask_bit;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d(%d), enable=0x%x\n", __func__, max8957_irq, data->irq, max8957_irq_data->enable);
#endif
}

static void max8957_fg_irq_unmask(struct irq_data *data)
{
	struct max8957_fg_irq_data *max8957_irq_data;
	unsigned int max8957_irq;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	max8957_irq = data->irq - max8957_main_irq_data.fg_irq_base;
	max8957_irq_data = &max8957_fg_irqs[max8957_irq];
	max8957_irq_data->enable = ~(max8957_irq_data->mask_bit);

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d(%d), enable=0x%x\n", __func__, max8957_irq, data->irq, max8957_irq_data->enable);
#endif
}

static void max8957_fg_irq_lock(struct irq_data *data)
{
	struct max8957_chip *max8957_chip = NULL;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

#if MAXIM_INT_DEBUG
	pr_err("%s: irq=%d\n", __func__, data->irq);
#endif

	if (max8957_main_irq_data.chip1)
		max8957_chip = max8957_main_irq_data.chip1;
	else {
		pr_err("%s: max8957_chip is not initialized\n", __func__);
		return;
	}

	mutex_lock(&max8957_chip->irq_lock);
}

static void max8957_fg_irq_sync_unlock(struct irq_data *data)
{
	struct max8957_chip *max8957_chip = NULL;
	struct max8957_fg_irq_data *max8957_irq_data;
	unsigned int max8957_irq;
	u8 config = 0;

	if (!data) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return;
	}

	if (max8957_main_irq_data.chip1)
		max8957_chip = max8957_main_irq_data.chip1;
	else {
		pr_err("%s: max8957_chip is not initialized\n", __func__);
		return;
	}

	max8957_irq = data->irq - max8957_main_irq_data.fg_irq_base;
	max8957_irq_data = &max8957_fg_irqs[max8957_irq];
	config = max8957_irq_data->enable;

#if MAXIM_INT_DEBUG
	pr_err("%s: irq_num=%d(%d), config=0x%x, mask=0x%x, enable_cached=0x%x\n",
		__func__, max8957_irq, data->irq, config, max8957_irq_data->mask_bit, max8957_irq_data->enable_cached);
#endif

	if (max8957_irq_data->enable_cached != config) {
		/*
		max8957_set_bits_no_lock(max8957_chip, irq_data->mask_reg, irq_data->mask_bit, config);
		*/
		max8957_set_bits(max8957_chip, max8957_irq_data->mask_reg, max8957_irq_data->mask_bit, config);
		max8957_irq_data->enable_cached = config;
#if MAXIM_INT_DEBUG
		pr_err("%s: update irq_num=%d(%d), config=0x%x, mask=0x%x\n",
			__func__, max8957_irq, data->irq, config, max8957_irq_data->mask_bit);
#endif
	}

	mutex_unlock(&max8957_chip->irq_lock);
}

static struct irq_chip max8957_fg_irq_chip = {
	.name		= "max8957-fg-irq",
	.irq_bus_lock	= max8957_fg_irq_lock,
	.irq_bus_sync_unlock = max8957_fg_irq_sync_unlock,
    .irq_mask       = max8957_fg_irq_mask,
    .irq_unmask     = max8957_fg_irq_unmask,
};

int max8957_fg_irq_init(struct htc_battery_max8957_platform_data *pdata)
{
	int i, rc = 0;

	if (pdata->irq) {
#if MAXIM_INT_DEBUG
		pr_err("%s: init interrupts for fuel gauge, irq=%d\n", __func__, pdata->irq);
#endif

		max8957_main_irq_data.fg_irq = pdata->irq,
		max8957_main_irq_data.fg_irq_base = pdata->fg_irq_base;

		INIT_WORK(&max8957_main_irq_data.max8957_fg_intr_work, max8957_fg_intr_work_func);
#if MAXIM_INT_DEBUG
		pr_err("%s: Initialize max8957_fg_intr_work_func done\n", __func__);
#endif

		rc = request_any_context_irq(pdata->irq,
				max8957_fg_isr,
				IRQF_ONESHOT,
				"max8957-fg",
				NULL);

		if (rc < 0) {
			pr_err("%s: request irq %d failed\n", __func__, pdata->irq);
			return rc;
		}

		/* Set IRQ data for FG interrupts */
		for (i = pdata->fg_irq_base; i < (pdata->fg_irq_base + INT_MAX_FG); i++) {
#if MAXIM_INT_DEBUG
			pr_err("%s: register MAX8957_FG_INT(%d) with irq(%d)\n", __func__, i-pdata->fg_irq_base, i);
#endif
#if 0
			set_irq_chip(i, &max8957_fg_irq_chip);
			/*
			set_irq_data(i, (void *)max8957_chip);
			*/
			set_irq_handler(i, handle_edge_irq);
			set_irq_flags(i, IRQF_VALID);
			set_irq_nested_thread(i, 1);
#else
			irq_set_chip_and_handler(i, &max8957_fg_irq_chip, handle_edge_irq);
			set_irq_flags(i, IRQF_VALID);
			irq_set_nested_thread(i, 1);
#endif

			/* enable IRQ directly */
			enable_irq(i);
		}
	}

	return 0;
}
#endif

static int max8957_irq_init(struct max8957_chip *max8957_chip, int irq,
			    struct max8957_platform_data *pdata)
{
	int i = 0;

	mutex_init(&max8957_chip->irq_lock);

#if MAXIM_INT_DEBUG
	pr_err("%s: pdata_number=%d, irq_base=%d, irq=%d\n", __func__,
		pdata->pdata_number, max8957_main_irq_data.irq_base, irq);
#endif

	if (pdata->pdata_number == MAX8957_PDATA_SID1) {
		/* register with genirq */
		for (i = max8957_main_irq_data.irq_base; i <= (max8957_main_irq_data.irq_base + MAX8957_INT_GRPA_END); i++) {
#if MAXIM_INT_DEBUG
			pr_err("%s: MAX8957_PDATA_SID1 processing irq=%d\n", __func__,  i);
#endif
#if 0
			set_irq_chip(i, &max8957_irq_chip);
			set_irq_data(i, (void *)max8957_chip);
			set_irq_handler(i, handle_edge_irq);
			set_irq_flags(i, IRQF_VALID);
			set_irq_nested_thread(i, 1);
#else
			irq_set_chip_data(i, (void *)max8957_chip);
			irq_set_chip_and_handler(i, &max8957_irq_chip, handle_level_irq);
			irq_set_nested_thread(i, 1);
#ifdef CONFIG_ARM
			set_irq_flags(i, IRQF_VALID);
#else
			irq_set_noprobe(__irq);
#endif
#endif
		}
	}
#if MAXIM_GRPB_INT_ENABLE
	else if (pdata->pdata_number == MAX8957_PDATA_SID3) {
		/* register with genirq */
		for (i = max8957_main_irq_data.irq_base + MAX8957_INT_GRPB_START; i <= (max8957_main_irq_data.irq_base + MAX8957_INT_GRPB_END); i++) {
#if MAXIM_INT_DEBUG
			pr_err("%s: MAX8957_PDATA_SID3 processing irq=%d\n", __func__,  i);
#endif
#if 0
			set_irq_chip(i, &max8957_irq_max8957_chip);
			set_irq_data(i, (void *)max8957_chip);
			set_irq_handler(i, handle_edge_irq);
			set_irq_flags(i, IRQF_VALID);
			set_irq_nested_thread(i, 1);
#else
			irq_set_chip_data(i, (void *)max8957_chip);
			irq_set_chip_and_handler(i, &max8957_irq_chip, handle_edge_irq);
			set_irq_flags(i, IRQF_VALID);
			irq_set_nested_thread(i, 1);
#endif
		}
	}
#endif /* MAXIM_GRPB_INT_ENABLE */

	return 0;
}

int max8957_request_irq(struct max8957_chip *max8957_chip, int irq,
			struct max8957_platform_data *pdata)
{
	int ret = 0;

#if 0
	unsigned long flags = IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_DISABLED;
#else
	unsigned long flags = IRQF_TRIGGER_LOW;
#endif

#if MAXIM_INT_DEBUG
	pr_err("%s:+, pdata_number=%d, irq=%d\n", __func__,  pdata->pdata_number, irq);
#endif

	/* Mutex is not needed. This function is called only on initializing time. */
	if (max8957_main_irq_data.irq == 0 || max8957_main_irq_data.irq == irq) 	{
		if (pdata->pdata_number == MAX8957_PDATA_SID1)
			max8957_main_irq_data.chip1 = max8957_chip;
#if MAXIM_GRPB_INT_ENABLE
		else if (pdata->pdata_number == MAX8957_PDATA_SID3)
			max8957_main_irq_data.chip2 = max8957_chip;
#endif

		if (max8957_main_irq_data.irq == 0) {
#if MAXIM_INT_DEBUG
			pr_err("%s: request_threaded_irq, irq=%d\n", __func__, irq);
#endif

			max8957_main_irq_data.irq = irq;
			max8957_main_irq_data.max8957_intr_queue = create_singlethread_workqueue("max8957_intr_work_q");
			if (max8957_main_irq_data.max8957_intr_queue == NULL) {
				pr_err("%s: Create max8957_intr_queue failed\n", __func__);
				return -ENOMEM;
			}
#if MAXIM_INT_DEBUG
			else
				pr_err("%s: Create max8957_intr_queue success\n", __func__);
#endif

			INIT_WORK(&max8957_main_irq_data.max8957_intr_work, max8957_intr_work_func);
#if MAXIM_INT_DEBUG
			pr_err("%s: Initialize max8957_intr_work done\n", __func__);
#endif

			ret = request_any_context_irq(irq, max8957_irq_isr,
					flags, "max8957-irq", &max8957_main_irq_data);

			/* Failed to request ITQ, destroy woruqueue */
			if ((ret < 0) && max8957_main_irq_data.max8957_intr_queue) {
				dev_err(&max8957_chip->dev->dev, "%s: Destroy workqueue max8957_intr_queue\n", __func__);
				destroy_workqueue(max8957_main_irq_data.max8957_intr_queue);
			} else
				irq_set_irq_wake(irq, 1);
		}
	} else {
		dev_err(&max8957_chip->dev->dev, "%s: IRQ No is not valid : old=%d new=%d\n",
				__func__, max8957_main_irq_data.irq, irq);
		ret = -EINVAL;
	}

	if (unlikely(ret)) {
		dev_err(&max8957_chip->dev->dev, "%s: Failed to request IRQ: %d, SID=%d\n",
				__func__, ret, pdata->pdata_number);
	}

#if MAXIM_INT_DEBUG
	pr_err("%s:-, max8957_main_irq_data->irq=%d\n", __func__, max8957_main_irq_data.irq);
#endif

	return ret;
}

int __devinit max8957_device_init(struct max8957_chip *max8957_chip,
				  struct max8957_platform_data *pdata)
{
	int ret;

	ret = max8957_irq_init(max8957_chip, max8957_chip->dev->irq, pdata);

	/* initialize some device or register setting here */

	return ret;
}

void __devexit max8957_device_exit(struct max8957_chip *max8957_chip)
{
	if (max8957_main_irq_data.max8957_intr_queue)
		destroy_workqueue(max8957_main_irq_data.max8957_intr_queue);

	if (max8957_chip->dev->irq)
		free_irq(max8957_chip->dev->irq, max8957_chip);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MAX8957 IRQ driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:max8957-irq");
