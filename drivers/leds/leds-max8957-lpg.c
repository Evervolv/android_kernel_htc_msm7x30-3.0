/*
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/mfd/pmicmax8957.h>
#include <linux/leds-max8957-lpg.h>
#include <linux/android_alarm.h>

static struct workqueue_struct *g_led_work_queue;

static struct max8957_lpg_data  *for_key_led_data;
static int flag_hold_virtual_key = 0;
static int virtual_key_state;
#define LED_DBG_LOG(fmt, ...) \
		printk(KERN_DEBUG "[LED]" fmt, ##__VA_ARGS__)
#define LED_INFO_LOG(fmt, ...) \
		printk(KERN_INFO "[LED]" fmt, ##__VA_ARGS__)
#define LED_ERR_LOG(fmt, ...) \
		printk(KERN_ERR "[LED][ERR]" fmt, ##__VA_ARGS__)


enum max8957_lpg_set_command {
	SET_BRIGHTNESS,
	SET_RAMPRATE,
	SET_BLINK,
	SET_PATTERN,
};

struct max8957_lpg_data {
	struct led_classdev	cdev;
	int			id;
	enum led_brightness	brightness;
	u8			flags;
	struct device	*master;
	struct mutex		lock;
	struct alarm            led_alarm;
	struct work_struct      led_off_work;
	struct delayed_work     led_delayed_blink_work;
	spinlock_t		value_lock;
	int 		command;
	u8			blinkon;
	u8			blinkper;
	u8			ledxpattern[6];
	u8			leden;
	u8			ledgrp;
	u8			ledramprate;
	u8			ledholdtime;
	u8			ledstts;
	u8			ledint;
};

static struct max8957_lpg_data lpg_data[MAX8957_MAX_LEDS];

static u8 ledbrt_reg[MAX8957_MAX_LEDS] = {MAX8957_REG_LED0BRT, MAX8957_REG_LED1BRT, MAX8957_REG_LED2BRT,
		MAX8957_REG_LED3BRT, MAX8957_REG_LED4BRT, MAX8957_REG_LED5BRT };

static u8 leden_mask[MAX8957_MAX_LEDS] = {MAX8957_LED0EN_MASK, MAX8957_LED1EN_MASK, MAX8957_LED2EN_MASK,
		MAX8957_LED3EN_MASK, MAX8957_LED4EN_MASK, MAX8957_LED5EN_MASK };

static u8 leden_shift[MAX8957_MAX_LEDS] = {MAX8957_LED0EN_SHIFT, MAX8957_LED1EN_SHIFT, MAX8957_LED2EN_SHIFT,
		MAX8957_LED3EN_SHIFT, MAX8957_LED4EN_SHIFT, MAX8957_LED5EN_SHIFT };

static u8 ledxctnl1_reg[MAX8957_MAX_LEDS] = {MAX8957_REG_LED0CNTL1, MAX8957_REG_LED1CNTL1, MAX8957_REG_LED2CNTL1,
		MAX8957_REG_LED3CNTL1, MAX8957_REG_LED4CNTL1, MAX8957_REG_LED5CNTL1 };

static u8 ledxctnl2_reg[MAX8957_MAX_LEDS] = {MAX8957_REG_LED0CNTL2, MAX8957_REG_LED1CNTL2, MAX8957_REG_LED2CNTL2,
		MAX8957_REG_LED3CNTL2, MAX8957_REG_LED4CNTL2, MAX8957_REG_LED5CNTL2 };

static u8 ledxpattern_reg[MAX8957_MAX_LEDS] = {MAX8957_REG_LED0PATTERN0, MAX8957_REG_LED1PATTERN0, MAX8957_REG_LED2PATTERN0,
		MAX8957_REG_LED3PATTERN0, MAX8957_REG_LED4PATTERN0, MAX8957_REG_LED5PATTERN0 };

static void max8957_led_set_brightness(struct max8957_lpg_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->master);
	int rc = 0;
	u8 ledbrightness;
	u8 ledxbrtreg;
	u8 ledxenmask;
	u8 ledxenshift;
	u8 ledxenval;
	unsigned long flags;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: id=%d, name=%s, brightness=%d\n", __func__, led->id, led->cdev.name, led->brightness);
#endif

	spin_lock_irqsave(&led->value_lock, flags);

	ledbrightness = led->brightness;
	ledxbrtreg = ledbrt_reg[led->id];
	ledxenmask = leden_mask[led->id];
	ledxenshift = leden_shift[led->id];
	ledxenval = (led->leden)<<ledxenshift;

	spin_unlock_irqrestore(&led->value_lock, flags);

if (led->id < 3){
#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: ledxbrtreg=0x%x, ledbrightness=0x%x, ledxenmask=0x%x, ledxenval=0x%x\n",
		__func__, ledxbrtreg, ledbrightness, ledxenmask, ledxenval);
#endif
	rc |= max8957_write(chip, ledxbrtreg, &ledbrightness, 1);
	rc |= max8957_set_bits(chip, MAX8957_REG_LEDEN, ledxenmask, ledxenval);
	if (unlikely(rc))
	LED_ERR_LOG("%s: can't set brightness level\n", __func__);
	}
/*group button-backlight led*/
if (led->id==3){
#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: ledxbrtreg=0x%x, ledbrightness=0x%x, ledxenmask=0x%x, ledxenval=0x%x\n",
		__func__, ledxbrtreg, ledbrightness, 38, 38);
#endif
	rc |= max8957_write(chip, ledxbrtreg, &ledbrightness, 1);
	rc |= max8957_set_bits(chip, MAX8957_REG_LEDEN, 0x38, 0x38);
	if (unlikely(rc))
	LED_ERR_LOG("%s: can't set brightness level\n", __func__);

}
}

static void max8957_led_set_ramprate(struct max8957_lpg_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->master);
	int rc = 0;
	u8 led_reg;
	u8 mask_bits;
	u8 mask_data;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: name=%s, ledramprate=%d\n", __func__, led->cdev.name, led->ledramprate);
#endif

	/* set Ramprate during continuous mode */
	led_reg = MAX8957_REG_LED0CNTL3 + led->id - MAX8957_ID_LED0;
	mask_data = led->ledramprate;
	mask_bits = MAX8957_RAMPRATE_MASK;

	rc = max8957_set_bits(chip, led_reg, mask_bits, mask_data);

	if (unlikely(rc))
		LED_ERR_LOG("%s: can't set ramprate control\n", __func__);
}

static void max8957_led_set_blink(struct max8957_lpg_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->master);
	int rc = 0;
	u8 led_reg;
	u8 mask_bits;
	u8 mask_data;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: name=%s, blinkon=%d, blinkper=%d\n", __func__, led->cdev.name, led->blinkon, led->blinkper);
#endif

	led_reg = ledxctnl1_reg[led->id];
	mask_bits = (MAX8957_MODE_MASK | MAX8957_BLINKON_MASK | MAX8957_BLINKPER_MASK);
	mask_data = (BLINK_MODE << MAX8957_MODE_SHIFT) |
			(led->blinkon << MAX8957_BLINKON_SHIFT) |
			(led->blinkper << MAX8957_BLINKPER_SHIFT);

	rc = max8957_set_bits(chip, led_reg, mask_bits, mask_data);

	if (unlikely(rc))
		LED_ERR_LOG("%s: can't set blink control\n", __func__);
}

static void max8957_led_set_pattern(struct max8957_lpg_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->master);
	int rc = 0;
	int i;
	unsigned long flags;
	u8 ledpattern0reg;
	u8 ledpattern[6];
	u8 led_reg;
	u8 mask_bits;
	u8 mask_data;
	u8 ledxenmask;
	u8 ledxenshift;
	u8 ledxenval;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: name=%s\n", __func__, led->cdev.name);
#endif

	spin_lock_irqsave(&led->value_lock, flags);

	ledpattern0reg = ledxpattern_reg[led->id];
	for (i = 0; i < 6; i++)
		ledpattern[i] = led->ledxpattern[i];

	led_reg = ledxctnl1_reg[led->id];
	mask_bits = MAX8957_MODE_MASK;
	mask_data = PATTERN_MODE_REPEAT << MAX8957_MODE_SHIFT;

	ledxenmask = leden_mask[led->id];
	ledxenshift = leden_shift[led->id];
	ledxenval = (led->leden) << ledxenshift;

	spin_unlock_irqrestore(&led->value_lock, flags);

	rc |= max8957_write(chip, ledpattern0reg, &ledpattern[0], 6);
	rc |= max8957_set_bits(chip, led_reg, mask_bits, mask_data);
	rc |= max8957_write(chip, ledxctnl2_reg[led->id], &led->ledholdtime, 1);
	rc |= max8957_write(chip, ledbrt_reg[led->id], (u8 *)&led->brightness, 1);
	rc |= max8957_set_bits(chip, MAX8957_REG_LEDEN, ledxenmask, ledxenval);

	if (unlikely(rc))
		LED_ERR_LOG("%s: can't set pattern control\n", __func__);
}
void button_backlight_flash(int brightness_key)
{
	int blinkper, blinkon;
	blinkon = 320;
	blinkper = 640;
	LED_INFO_LOG("%s Brightness_key: %d\n", __func__,brightness_key);

	if(brightness_key){
		flag_hold_virtual_key = 1;
		blinkon = BLINK_TO_ON(blinkon);
		blinkper = BLINK_TO_PERIOD(blinkper);
		for_key_led_data->leden = MAX8957_LED_ENABLE;
		for_key_led_data->blinkper = blinkper;
		for_key_led_data->blinkon = blinkon;
		for_key_led_data->brightness = 200;
		max8957_led_set_brightness(for_key_led_data);
		max8957_led_set_blink(for_key_led_data);
	}
	else{

		for_key_led_data->leden = MAX8957_LED_DISABLE;
		for_key_led_data->brightness = 0;
		for_key_led_data->blinkper = 0;
		for_key_led_data->blinkon = 0;
		max8957_led_set_brightness(for_key_led_data);
		max8957_led_set_blink(for_key_led_data);
		if (virtual_key_state != 0){
			for_key_led_data->leden = MAX8957_LED_ENABLE;
			for_key_led_data->brightness = 200;
			max8957_led_set_brightness(for_key_led_data);
			}
		flag_hold_virtual_key = 0;
	}
}

static void max8957_lpg_set_brightness(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct max8957_lpg_data *led;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: name=%s, value=%d\n", __func__, led_cdev->name, value);
#endif
	led = container_of(led_cdev, struct max8957_lpg_data, cdev);

	if(led->id == MAX8957_ID_LED3 && flag_hold_virtual_key == 1){
		if (value)
		virtual_key_state = 1;
		else
		virtual_key_state = 0;
		LED_INFO_LOG("%s, Return control by button_backlight flash \n", __func__);
		return;
	}
	if (value == LED_OFF && led->ledramprate == 0)
		led->leden = MAX8957_LED_DISABLE;
	else
		led->leden = MAX8957_LED_ENABLE;

	/*
	led->brightness = value;
	*/
	led->brightness = value > 0 ? 200 : 0; /* JR:Fix brightness to 200 when enable LED */
       if (led->id == MAX8957_ID_LED3)
               led->brightness = value > 0 ? 70 : 0; /*button-backlight set 70*/
	max8957_led_set_brightness(led);
}

static int max8957_lpg_set_blink(struct led_classdev *led_cdev,
				unsigned long *delay_on,
				unsigned long *delay_off)
{

	struct max8957_lpg_data *led;
	int blinkper, blinkon;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: name=%s, delay_on=%lu, delay_off=%lu\n", __func__, led_cdev->name, *delay_on, *delay_off);
#endif

	led = container_of(led_cdev, struct max8957_lpg_data, cdev);

	blinkon = *delay_on;
	blinkper = *delay_on + *delay_off;

	/* units are in ms */
	if (unlikely((blinkon > 500) || (blinkper > 5000)))
		return -EINVAL;

	blinkon = BLINK_TO_ON(blinkon);
	blinkper = BLINK_TO_PERIOD(blinkper);

	if (unlikely((blinkon == -EINVAL) || (blinkper == -EINVAL)))
		return -EINVAL;

	if (*delay_off == 0)
		led->blinkper = 0;  /* continuous ON */
	else {
		led->blinkper = blinkper;
		led->blinkon = blinkon;
	}
	max8957_led_set_blink(led);
	return 0;
}

/*MAXIM_PMIC_8957_PORTING - LED Blink*/
static ssize_t max8957_lpg_blink_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct led_classdev *led_cdev = NULL;
	struct max8957_lpg_data *ldata;
	int val = -1;
	unsigned long delay_on = 0, delay_off = 0;

	sscanf(buf, "%u", &val);
	if (val < 0 || val > 255)
		return -EINVAL;

	LED_INFO_LOG("%s: blink %d\n", __func__, val);

	led_cdev = (struct led_classdev *) dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct max8957_lpg_data, cdev);

	switch (val) {
	case -1: /* stop flashing */
		delay_on = 0;
		delay_off = 0;
		max8957_lpg_set_blink(led_cdev, &delay_on, &delay_off);
		break;
	case 0:
		delay_on = 64;
		delay_off = 0;
		max8957_lpg_set_blink(led_cdev, &delay_on, &delay_off);
		break;
	case 1:
		delay_on = 64;
		delay_off = 1936;
		max8957_lpg_set_blink(led_cdev, &delay_on, &delay_off);
		break;
	case 2:
		ldata->blinkon = BLINK_TO_ON(64);
		ldata->blinkper = BLINK_TO_PERIOD(2000);
		queue_delayed_work(g_led_work_queue, &ldata->led_delayed_blink_work,
				   msecs_to_jiffies(310));
		break;
	case 3:
		ldata->blinkon = BLINK_TO_ON(64);
		ldata->blinkper = BLINK_TO_PERIOD(2000);
		queue_delayed_work(g_led_work_queue, &ldata->led_delayed_blink_work,
				   msecs_to_jiffies(1000));
		break;
	case 4:
		delay_on = 500;
		delay_off = 1500;
		max8957_lpg_set_blink(led_cdev, &delay_on, &delay_off);
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static ssize_t max8957_lpg_blink_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;

	return ret;
}
static DEVICE_ATTR(blink, 0644, max8957_lpg_blink_show,
				max8957_lpg_blink_store);
/*MAXIM_PMIC_8957_PORTING - LED Blink*/

#if MAXIM_LEDS_LPG_DEBUG
static int max8957_lpg_set_ramprate(struct led_classdev *led_cdev,
				int ramprate)
{
	struct max8957_lpg_data *led;

	LED_INFO_LOG("%s: name=%s, ramprate=%d\n", __func__, led_cdev->name, ramprate);

	led = container_of(led_cdev, struct max8957_lpg_data, cdev);

	/* units are in ms */
	if ((ramprate < 0) || (ramprate > 31))
		return -EINVAL;

	led->ledramprate = ramprate;
	max8957_led_set_ramprate(led);
	return 0;
}

static int max8957_lpg_get_ramprate(struct led_classdev *led_cdev)
{
	struct max8957_lpg_data *led;

	LED_INFO_LOG("%s: name=%s\n", __func__, led_cdev->name);

	led = container_of(led_cdev, struct max8957_lpg_data, cdev);

	return led->ledramprate;
}
static ssize_t max8957_lpg_ramprate_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct led_classdev *led_cdev = NULL;
	int val = -1;

	sscanf(buf, "%u", &val);
	if (val < 0 || val > 31)
		return -EINVAL;

	LED_INFO_LOG("%s: ramprate %d\n", __func__, val);

	led_cdev = (struct led_classdev *) dev_get_drvdata(dev);

	max8957_lpg_set_ramprate(led_cdev, val);
	return count;
}

static ssize_t max8957_lpg_ramprate_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct led_classdev *led_cdev = NULL;
	int ramprate = -1;

	led_cdev = (struct led_classdev *) dev_get_drvdata(dev);

	ramprate = max8957_lpg_get_ramprate(led_cdev);

	LED_INFO_LOG("%s: ramprate %d\n", __func__, ramprate);

	return sprintf(buf, "%d\n", ramprate);
}
static DEVICE_ATTR(ramprate, 0644, max8957_lpg_ramprate_show,
				max8957_lpg_ramprate_store);
#endif

int max8957_lpg_set_pattern(struct led_classdev *led_cdev,
							struct max8957_lpg_pattern *lpg_pattern,
							u8 max_hold_time, u8 min_hold_time,
							u8 max_brightness)
{
	struct max8957_lpg_data *led;
	u64 pattern;
	int i;

	led = container_of(led_cdev, struct max8957_lpg_data, cdev);

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: id=%d, name=%s, max_hold_time=0x%x, min_hold_time=0x%x, max_brightness=0x%x\n",
		__func__, led->id, led->cdev.name, max_hold_time, min_hold_time, max_brightness);
#endif

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: pattern=\n", __func__);
#endif
	pattern = *((u64 *)lpg_pattern);
	for (i = 0; i < 6; i++) {
		led->ledxpattern[i] = pattern & 0xFF;
		pattern = pattern >> 8;
#if MAXIM_LEDS_LPG_DEBUG
		LED_INFO_LOG(" 0x%x", led->ledxpattern[i]);
#endif
	}
#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("\n");
#endif

	led->leden = 1;
	led->ledholdtime = (max_hold_time << MAX8957_MAXHLDTIME_SHIFT) |
			(min_hold_time << MAX8957_MINHLDTIME_SHIFT);
	led->brightness = max_brightness;
	max8957_led_set_pattern(led);
	return 0;
}
EXPORT_SYMBOL(max8957_lpg_set_pattern);

#if MAXIM_LEDS_LPG_DEBUG
static ssize_t max8957_lpg_pattern_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct led_classdev *led_cdev = NULL;
	int val = -1;
	unsigned long delay_on = 0, delay_off = 0;
	struct max8957_lpg_pattern lpg_pattern;

	sscanf(buf, "%u", &val);
	if (val < 0 || val > 255)
		return -EINVAL;
	LED_INFO_LOG("%s: pattern %d\n", __func__, val);

	led_cdev = (struct led_classdev *) dev_get_drvdata(dev);
	if (!led_cdev)
		return -EINVAL;

	switch (val) {
	case 0:
		delay_on = 0;
		delay_off = 0;
		max8957_lpg_set_blink(led_cdev, &delay_on, &delay_off);
		break;
	case 1:
		/*
		test_data[0] = 0xDF;
		test_data[1] = 0xF7;
		test_data[2] = 0x7D;
		test_data[3] = 0xDF;
		test_data[4] = 0xF7;
		test_data[5] = 0x7D;
		max8957_lpg_set_pattern(led_cdev, &lpg_pattern, LPG_HOLD_256MS, LPG_HOLD_512MS, 0xFF);
		*/
		break;
	case 2:
		/*
		test_data[0] = 0x24;
		test_data[1] = 0x49;
		test_data[2] = 0x92;
		test_data[3] = 0x00;
		test_data[4] = 0x00;
		test_data[5] = 0x00;
		*/
		lpg_pattern.p1_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p1_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p2_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p2_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p3_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p3_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p4_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p4_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p5_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p5_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p6_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p6_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p7_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p7_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p8_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p8_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p9_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p9_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p10_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p10_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p11_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p11_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p12_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p12_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p13_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p13_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p14_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p14_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p15_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p15_ramprate = LPG_POINT_RAMP_JUMP;
		lpg_pattern.p16_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p16_ramprate = LPG_POINT_RAMP_JUMP;

		max8957_lpg_set_pattern(led_cdev, &lpg_pattern, LPG_HOLD_128MS, LPG_HOLD_128MS, 0xFF);
		break;
	case 3:
		lpg_pattern.p1_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p1_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p2_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p2_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p3_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p3_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p4_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p4_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p5_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p5_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p6_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p6_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p7_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p7_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p8_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p8_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p9_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p9_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p10_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p10_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p11_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p11_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p12_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p12_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p13_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p13_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p14_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p14_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p15_direction = LPG_POINT_DIR_UP;
		lpg_pattern.p15_ramprate = LPG_POINT_RAMP_8MS;
		lpg_pattern.p16_direction = LPG_POINT_DIR_DOWN;
		lpg_pattern.p16_ramprate = LPG_POINT_RAMP_8MS;

		max8957_lpg_set_pattern(led_cdev, &lpg_pattern, LPG_HOLD_0MS, LPG_HOLD_0MS, 0xFF);
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static ssize_t max8957_lpg_pattern_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;

	return ret;
}
static DEVICE_ATTR(pattern, 0644, max8957_lpg_pattern_show,
				max8957_lpg_pattern_store);
#endif

static int max8957_lpg_setup(struct max8957_lpg_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->master);
	int ret = 0;
	u8 reg_addr;
	u8 reg_data;
	u8 reg_mask;

#if MAXIM_LEDS_LPG_DEBUG
	LED_INFO_LOG("%s: id=%d, name=%s\n", __func__, led->id, led->cdev.name);
#endif

	/* set Ramprate during continuous mode */
	reg_addr = MAX8957_REG_LED0CNTL3 + led->id - MAX8957_ID_LED0;
	reg_data = led->ledramprate;
	reg_mask = MAX8957_RAMPRATE_MASK;
	ret |= max8957_set_bits(chip, reg_addr, reg_mask, reg_data);

	/* set LED group */
	reg_addr = MAX8957_REG_LEDGRP;
	switch (led->id) {
	case MAX8957_ID_LED0:
	case MAX8957_ID_LED1:
	case MAX8957_ID_LED2:
		reg_mask = MAX8957_LEDGRP1_MASK;
		reg_data = led->ledgrp << MAX8957_LEDGRP1_SHIFT;
		break;

	case MAX8957_ID_LED3:
	case MAX8957_ID_LED4:
	case MAX8957_ID_LED5:
		reg_mask = MAX8957_LEDGRP2_MASK;
		reg_data = led->ledgrp << MAX8957_LEDGRP2_SHIFT;
		break;

	default:
		break;
	}

	ret |= max8957_set_bits(chip, reg_addr, reg_mask, reg_data);
	return ret;
}

static void led_off_work_func(struct work_struct *work)
{
	struct max8957_lpg_data *ldata;

	ldata = container_of(work, struct max8957_lpg_data, led_off_work);
	ldata->blinkper = BLINK_TO_ON(0);
	ldata->blinkon = BLINK_TO_PERIOD(0);
	max8957_led_set_blink(ldata);

	ldata->brightness = 0;
	ldata->leden = MAX8957_LED_DISABLE;
	max8957_led_set_brightness(ldata);
}

static void led_blink_work_func(struct work_struct *work)
{
	struct max8957_lpg_data *ldata;

	ldata = container_of(work, struct max8957_lpg_data, led_delayed_blink_work.work);
	max8957_led_set_blink(ldata);
}

static void led_alarm_handler(struct alarm *alarm)
{
	struct max8957_lpg_data *ldata;

	ldata = container_of(alarm, struct max8957_lpg_data, led_alarm);
	queue_work(g_led_work_queue, &ldata->led_off_work);
}

static ssize_t max8957_led_off_timer_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	return ret;
}

static ssize_t max8957_led_off_timer_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct max8957_lpg_data *ldata;
	int min, sec;
	uint16_t off_timer;
	ktime_t interval;
	ktime_t next_alarm;

	min = -1;
	sec = -1;
	sscanf(buf, "%d %d", &min, &sec);

	if (min < 0 || min > 255)
		return -EINVAL;
	if (sec < 0 || sec > 255)
		return -EINVAL;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct max8957_lpg_data, cdev);

	LED_INFO_LOG("Setting %s off_timer to %d min %d sec\n",
					   led_cdev->name, min, sec);

	off_timer = min * 60 + sec;

	alarm_cancel(&ldata->led_alarm);
	cancel_work_sync(&ldata->led_off_work);
	if (off_timer) {
		interval = ktime_set(off_timer, 0);
		next_alarm = ktime_add(alarm_get_elapsed_realtime(), interval);
		alarm_start_range(&ldata->led_alarm, next_alarm, next_alarm);
	}
	return count;
}

static DEVICE_ATTR(off_timer, 0644, max8957_led_off_timer_show, max8957_led_off_timer_store);

static int max8957_lpg_probe(struct platform_device *pdev)
{
	struct max8957_lpg_platform_data *pdata = pdev->dev.platform_data;
	struct max8957_lpg_data *led_dat;
	struct max8957_lpg *curr_led;

	int rc, i = 0;

	if (pdata == NULL) {
		LED_ERR_LOG("platform data not supplied\n");
		return -EINVAL;
	}

	g_led_work_queue = create_workqueue("max8957_led");
	if (g_led_work_queue == NULL) {
		LED_ERR_LOG("amx8957 led work queue create fail\n");
		return 0;
	}

	for (i = 0; i < pdata->num_leds; i++) {
		curr_led	= &pdata->leds[i];
		led_dat		= &lpg_data[curr_led->id];

		led_dat->cdev.name		= curr_led->name;
		led_dat->cdev.brightness_set    = max8957_lpg_set_brightness;
		led_dat->cdev.blink_set         = max8957_lpg_set_blink;
		/*
		led_dat->cdev.flags		= LED_CORE_SUSPENDRESUME;
		*/
		led_dat->id			= curr_led->id;
		led_dat->ledgrp			= curr_led->ledgrp;
		led_dat->ledramprate		= curr_led->ramprate;
		led_dat->ledholdtime		= curr_led->holdtime;

		if (unlikely(!((led_dat->id >= MAX8957_ID_LED0) && (led_dat->id <= MAX8957_ID_LED5)))) {
			LED_ERR_LOG("invalid LED ID (%d) specified\n",
						 led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;
		}

		mutex_init(&led_dat->lock);
		spin_lock_init(&led_dat->value_lock);

#if MAXIM_LEDS_LPG_DEBUG
		LED_INFO_LOG("%s: register LED id=%d, name=%s\n", __func__, curr_led->id, curr_led->name);
#endif
		if (led_dat->id < 4){
			rc = led_classdev_register(&pdev->dev, &led_dat->cdev);
			if (unlikely(rc)) {
				LED_ERR_LOG("unable to register led %d\n",
							 led_dat->id);
				goto fail_id_check;
			}
		}
		/* initial setup */
		led_dat->master = pdev->dev.parent;
		rc = max8957_lpg_setup(led_dat);
		if (unlikely(rc)) {
		    LED_ERR_LOG("failed to write\n");
		    goto err_reg_write;
		}

		/*MAXIM_PMIC_8957_PORTING - LED Blink*/
		if (led_dat->id < 2) {
			rc = device_create_file(led_dat->cdev.dev, &dev_attr_blink);
			if (rc < 0) {
				LED_ERR_LOG("%s: Failed to create attr blink: %d\n", __func__, i);
				goto err_register_attr_blink;
			}

			rc = device_create_file(led_dat->cdev.dev, &dev_attr_off_timer);
			if (rc < 0) {
				LED_ERR_LOG("%s: Failed to create attr off timer: %d\n", __func__, i);
				goto err_register_attr_off_timer;
			}

			INIT_WORK(&led_dat->led_off_work, led_off_work_func);
			INIT_DELAYED_WORK(&led_dat->led_delayed_blink_work, led_blink_work_func);
			alarm_init(&led_dat->led_alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP, led_alarm_handler);
		}
		/*MAXIM_PMIC_8957_PORTING - LED Blink*/

#if MAXIM_LEDS_LPG_DEBUG
		rc = device_create_file(led_dat->cdev.dev,	&dev_attr_ramprate);
		if (rc < 0) {
			LED_ERR_LOG("%s: Failed to create attr ramprate"
					" [%d]\n", __func__, i);
			goto err_register_attr_off_ramprate;
		}
		rc = device_create_file(led_dat->cdev.dev,	&dev_attr_pattern);
		if (rc < 0) {
			LED_ERR_LOG("%s: Failed to create attr pattern"
					" [%d]\n", __func__, i);
			goto err_register_attr_off_pattern;
		}
#endif
	if (led_dat->id == MAX8957_ID_LED3)
	for_key_led_data=led_dat;

	}

	platform_set_drvdata(pdev, lpg_data);
	return 0;

#if MAXIM_LEDS_LPG_DEBUG
err_register_attr_off_pattern:
	for (i--; i >= 0; i--)
		device_remove_file(lpg_data[i].cdev.dev, &dev_attr_pattern);

	i = pdata->num_leds;

err_register_attr_off_ramprate:
	for (i--; i >= 0; i--)
		device_remove_file(lpg_data[i].cdev.dev, &dev_attr_ramprate);

	i = pdata->num_leds;
#endif
err_register_attr_off_timer:
	for (i--; i >= 0; i--)
		device_remove_file(lpg_data[i].cdev.dev, &dev_attr_off_timer);

	i = pdata->num_leds;

err_register_attr_blink:
	for (i--; i >= 0; i--)
		device_remove_file(lpg_data[i].cdev.dev, &dev_attr_blink);

	i = pdata->num_leds;

fail_id_check:
err_reg_write:
	LED_ERR_LOG("%s: Initialization failed!\n", __func__);
	if (i > 0) {
		for (i = i - 1; i >= 0; i--)
			led_classdev_unregister(&lpg_data[i].cdev);
	}

	destroy_workqueue(g_led_work_queue);

	return rc;
}

static int __devexit max8957_lpg_remove(struct platform_device *pdev)
{
	int i;
	struct max8957_lpg_platform_data *pdata = pdev->dev.platform_data;
	struct max8957_lpg_data *led = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		led_classdev_unregister(&led[led->id].cdev);

		/*MAXIM_PMIC_8957_PORTING - LED Blink*/
		if (led->id  < 2)
			device_remove_file(led[led->id].cdev.dev, &dev_attr_blink);

		/*MAXIM_PMIC_8957_PORTING - LED Blink*/
#if MAXIM_LEDS_LPG_DEBUG
		device_remove_file(led[led->id].cdev.dev, &dev_attr_ramprate);
		device_remove_file(led[led->id].cdev.dev, &dev_attr_pattern);
#endif
	}

	return 0;
}

static struct platform_driver max8957_lpg_driver = {
	.probe		= max8957_lpg_probe,
	.remove		= __devexit_p(max8957_lpg_remove),
	.driver		=
	{
		.name	= "max8957-lpg",
		.owner	= THIS_MODULE,
	},
};

static int __init max8957_lpg_init(void)
{
	return platform_driver_register(&max8957_lpg_driver);
}
module_init(max8957_lpg_init);

static void __exit max8957_lpg_exit(void)
{
	platform_driver_unregister(&max8957_lpg_driver);
}
module_exit(max8957_lpg_exit);

MODULE_AUTHOR("Clark Kim <clark.kim@maxim-ic.com>");
MODULE_DESCRIPTION("MAX8957 LPG driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:max8957-lpg");
