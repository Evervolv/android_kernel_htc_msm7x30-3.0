/*
 * Maxim PMICMAX8957 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pmicmax8957.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#define MAX8957_I2C_RETRY_TIMES 10

/* Helper Functions */
int max8957_i2c_write(struct i2c_client *client, u8 addr, const u8 *values, size_t len)
{
	int retry, i;
	uint8_t buf[len + 1];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		}
	};

	buf[0] = addr & 0xFF;

	for (i = 0; i < len; i++)
		buf[i + 1] = values[i];

#if MAXIM_I2C_DEBUG
	printk(KERN_ERR "%s, slave_id=0x%x(0x%x), addr=0x%x, len=%d\n", __func__, client->addr, client->addr << 1, addr, len+1);
	for (i = 0; i < len+1 ; i++)
		printk(KERN_ERR " 0x%x", buf[i]);
	printk(KERN_ERR "\n");
#endif

	for (retry = 0; retry < MAX8957_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		mdelay(10);
	}

	if (retry == MAX8957_I2C_RETRY_TIMES) {
		printk(KERN_ERR "%s: i2c_write_block retry over %d\n",
			__func__, MAX8957_I2C_RETRY_TIMES);
		return -EIO;
	}

	return 0;
}

int max8957_i2c_read(struct i2c_client *client, u8 addr, u8 *values, size_t len)
{
	int retry;
	uint8_t buf[1];
#if MAXIM_I2C_DEBUG
	int i;
#endif

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = values,
		}
	};

	buf[0] = addr & 0xFF;

#if MAXIM_I2C_DEBUG
	printk(KERN_ERR "%s, slave_id=0x%x(0x%x), addr=0x%x, len=%d\n", __func__, client->addr, client->addr << 1, addr, len);
#endif

	for (retry = 0; retry < MAX8957_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		mdelay(10);
	}
	if (retry == MAX8957_I2C_RETRY_TIMES) {
		printk(KERN_ERR "i2c_read_block retry over %d\n",
		MAX8957_I2C_RETRY_TIMES);
		return -EIO;
	}

#if MAXIM_I2C_DEBUG
	printk(KERN_ERR "%s, len=%d\n", __func__, len);
	for (i = 0; i < len; i++)
		printk(KERN_ERR " 0x%x", values[i]);
	printk(KERN_ERR "\n");
#endif

	return 0;
}

/* External APIs */
int max8957_read(struct max8957_chip *chip, u8 addr, u8 *values,
		unsigned int len)
{
	int ret;

	if (chip == NULL)
		return -EINVAL;

	mutex_lock(&chip->pm_lock);

	ret = max8957_i2c_read(chip->dev, addr, values, len);

	mutex_unlock(&chip->pm_lock);

	return ret;
}
EXPORT_SYMBOL(max8957_read);

/*
int max8957_read_no_lock(struct max8957_chip *chip, u8 addr, u8 *values,
		unsigned int len)
{
	int ret;

	if (chip == NULL)
		return -EINVAL;

	ret = max8957_i2c_read(chip->dev, addr, values, len);

	return ret;
}
EXPORT_SYMBOL(max8957_read_no_lock);
*/

int max8957_write(struct max8957_chip *chip, u8 addr, u8 *values,
		unsigned int len)
{
	int ret;

	if (chip == NULL)
		return -EINVAL;

	mutex_lock(&chip->pm_lock);

	ret = max8957_i2c_write(chip->dev, addr, values, len);

	mutex_unlock(&chip->pm_lock);

	return ret;
}
EXPORT_SYMBOL(max8957_write);

/*
int max8957_write_no_lock(struct max8957_chip *chip, u8 addr, u8 *values,
		unsigned int len)
{
	int ret;

	if (chip == NULL)
		return -EINVAL;

	ret = max8957_i2c_write(chip->dev, addr, values, len);

	return ret;
}
EXPORT_SYMBOL(max8957_write_no_lock);
*/

int max8957_set_bits(struct max8957_chip *chip, u8 addr, u8 mask, u8 value)
{
	u8 tmp;
	int ret;

	if (chip == NULL)
		return -EINVAL;

	mutex_lock(&chip->pm_lock);

	ret = max8957_i2c_read(chip->dev, addr, &tmp, 1);
	if (ret == 0) {
		value = (tmp & ~mask) | (value & mask);
		ret = max8957_i2c_write(chip->dev, addr, &value, 1);
	}

	mutex_unlock(&chip->pm_lock);

	return ret;
}
EXPORT_SYMBOL(max8957_set_bits);

/*
int max8957_set_bits_no_lock(struct max8957_chip *chip, u8 addr, u8 mask,
		* u8 value)
{
	u8 tmp;
	int ret;

	if (chip == NULL)
		return -EINVAL;

	ret = max8957_i2c_read(chip->dev, addr, &tmp, 1);
	if (ret == 0) {
		value = (tmp & ~mask) | (value & mask);
		ret = max8957_i2c_write(chip->dev, addr, &value, 1);
	}

	return ret;
}
EXPORT_SYMBOL(max8957_set_bits_no_lock);
*/

/* Internal functions */
/*
static irqreturn_t max8957_isr_thread(int irq_requested, void *data)
{
	return IRQ_HANDLED;
}
*/

static int __devinit max8957_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc;
	struct  max8957_platform_data *pdata = client->dev.platform_data;
	struct  max8957_chip *max8957_chip;

	#if MAXIM_I2C_DEBUG
	pr_err("%s: +, SlaveID=0x%x(0x%x)\n", __func__, client->addr, client->addr << 1);
	#endif

	if (pdata == NULL) {
		pr_err("%s: No platform_data.\n", __func__);
		return -ENODEV;
	}

	if (pdata->num_subdevs == 0) {
		pr_err("%s: No sub devices to support.\n", __func__);
		return -ENODEV;
	}

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		pr_err("%s: i2c_check_functionality failed.\n", __func__);
		return -ENODEV;
	}

	max8957_chip = kzalloc(sizeof *max8957_chip, GFP_KERNEL);
	if (max8957_chip == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	max8957_chip->dev = client;

	(void) memcpy((void *)&max8957_chip->pdata, (const void *)pdata,
		sizeof(max8957_chip->pdata));

	i2c_set_clientdata(client, max8957_chip);

	mutex_init(&max8957_chip->pm_lock);

	/* Register for all reserved IRQs */
	max8957_device_init(max8957_chip, pdata);

	rc = mfd_add_devices(&max8957_chip->dev->dev, 0, pdata->sub_devices,
	pdata->num_subdevs, NULL, 0);
	if (rc < 0)
		pr_err("%s: mfd_add_devices failed, rc=%d\n", __func__, rc);

	rc = max8957_request_irq(max8957_chip, max8957_chip->dev->irq, pdata);

	if (rc < 0)
		pr_err("%s: could not request irq %d: %d\n", __func__,
			max8957_chip->dev->irq, rc);

	#if MAXIM_I2C_DEBUG
	pr_err("%s: -\n", __func__);
	#endif

	return 0;
}

static int __devexit max8957_remove(struct i2c_client *client)
{
	struct  max8957_chip *chip;

	chip = i2c_get_clientdata(client);

	max8957_device_exit(chip);

	if (chip) {
		mutex_destroy(&chip->pm_lock);
		chip->dev = NULL;

		kfree(chip);
	}

	return 0;
}

#ifdef CONFIG_PM
static int max8957_suspend(struct device *dev)
{
	struct i2c_client *client;
	struct  max8957_chip *chip;

	client = to_i2c_client(dev);
	chip = i2c_get_clientdata(client);

	return 0;
}

static int max8957_resume(struct device *dev)
{
	struct i2c_client *client;
	struct  max8957_chip *chip;

	client = to_i2c_client(dev);
	chip = i2c_get_clientdata(client);

	return 0;
}
#else
#define max8957_suspend	NULL
#define max8957_resume	NULL
#endif

static const struct i2c_device_id max8957_ids[] = {
	{"pmicmax8957", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, max8957_ids);

static struct dev_pm_ops max8957_pm = {
	.suspend = max8957_suspend,
	.resume = max8957_resume,
};

static struct i2c_driver max8957_driver = {
	.driver.name    = "pmicmax8957",
	.driver.pm      = &max8957_pm,
	.id_table   = max8957_ids,
	.probe      = max8957_probe,
	.remove     = __devexit_p(max8957_remove),
};

static int __init max8957_init(void)
{
	int rc = i2c_add_driver(&max8957_driver);
	pr_notice("%s: i2c_add_driver: rc = %d\n", __func__, rc);

	return rc;
}

static void __exit max8957_exit(void)
{
	i2c_del_driver(&max8957_driver);
}

module_init(max8957_init);
module_exit(max8957_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMICMAX8957 multi-function core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pmicmax8957");
