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

#include <asm/gpio.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/mfd/pmicmax8957.h>
#include <linux/leds-max8957-flash.h>

#define current_step (15.625)
#define torch_mode_current (125)
#define torch_level1_current (31.25)
#define torch_level2_current (78.125)
#define flash_mode_current (750)
#define flash_mode_current_level1 (150)
#define flash_mode_current_level2 (200)
#define flash_mode_current_level3 (300)
#define flash_mode_current_level4 (400)
#define flash_mode_current_level5 (500)
#define flash_mode_current_level6 (600)
#define flash_mode_current_level7 (700)
#define pre_flash_current (93.75)

struct max8957_fled_data {
	int id;
	const char	*name;
	enum led_brightness brightness;
	u8 flags;
	struct device *dev;
	struct work_struct work;
	struct mutex lock;
	spinlock_t value_lock;
	u8 cntrl_mode;
	u8 timer;
	u8 timer_mode;
	u8 torch_timer_disable;
};

struct max8957_flashlight_struct {
	struct led_classdev cdev;
	struct max8957_fled_data fled_data[MAX8957_MAX_FLEDS];
	int gpio_flash;
	uint32_t pre_flash_sw_timeout_ms;
	uint32_t flash_sw_timeout_ms;
	enum flashlight_mode_flags mode_status;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_flashlight;
#endif
	struct wake_lock wake_lock;
	spinlock_t spin_lock;
	unsigned long spinlock_flags;
};

static struct max8957_flashlight_struct *this_fl_str = NULL;
static struct max8957_fled_data *cur_fled_data = NULL;

static u8 led_en_mask[MAX8957_ID_LED_MAX] = {MAX8957_FLASH_FLED1_EN_MASK, MAX8957_FLASH_FLED2_EN_MASK,
		MAX8957_TORCH_FLED1_EN_MASK, MAX8957_TORCH_FLED2_EN_MASK};
static u8 led_en_shift[MAX8957_ID_LED_MAX] = {MAX8957_FLASH_FLED1_EN_SHIFT, MAX8957_FLASH_FLED2_EN_SHIFT,
		MAX8957_TORCH_FLED1_EN_SHIFT, MAX8957_TORCH_FLED2_EN_SHIFT};
static u8 reg_led_timer[MAX8957_ID_LED_MAX] = {I2C_REG_ADDR_FLASH_TIMER, I2C_REG_ADDR_FLASH_TIMER,
		I2C_REG_ADDR_TORCH_TIMER, I2C_REG_ADDR_TORCH_TIMER};

static u8 reg_led_current[MAX8957_MAX_FLEDS] = {I2C_REG_ADDR_IFLASH1, I2C_REG_ADDR_IFLASH2,
		I2C_REG_ADDR_ITORCH, I2C_REG_ADDR_ITORCH};
static u8 led_current_mask[MAX8957_MAX_FLEDS] = {MAX8957_FLASH_IOUT_MASK, MAX8957_FLASH_IOUT_MASK,
		MAX8957_TORCH_IOUT1_MASK, MAX8957_TORCH_IOUT2_MASK};
static u8 led_current_shift[MAX8957_MAX_FLEDS] = {MAX8957_FLASH_IOUT_SHIFT, MAX8957_FLASH_IOUT_SHIFT,
		MAX8957_TORCH_IOUT1_SHIFT, MAX8957_TORCH_IOUT2_SHIFT};

#ifdef MAX8957_LEDS_FLASH_TEST
static struct timer_list fl_timer;
#endif

static void led_flash_set(struct max8957_fled_data *led);
static void max8957_fled_set(struct max8957_fled_data *led, enum led_brightness brightness);
static int max8957_fled_setup(struct max8957_fled_data *led);

static void flashlight_turn_off(void)
{
#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: current mode_status=%d\n", __func__, this_fl_str->mode_status);
#endif

	if (this_fl_str->mode_status == FL_MODE_OFF) {
#if MAXIM_LEDS_FLASH_DEBUG
		printk(KERN_INFO "%s: already off, skip\n", __func__);
#endif
		return;
	}

	/* Turn off current LED */
	this_fl_str->cdev.brightness = LED_OFF;
	max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
	this_fl_str->mode_status = FL_MODE_OFF;
}

int max8957_flashlight_control(int mode)
{
	int ret = 0;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: mode=%d\n", __func__, mode);
#endif

	if (this_fl_str->mode_status == mode) {
#if MAXIM_LEDS_FLASH_DEBUG
		printk(KERN_INFO "%s: mode is same: %d\n",
							__func__, mode);
#endif
		return ret;
	}

	spin_lock_irqsave(&this_fl_str->spin_lock,
						this_fl_str->spinlock_flags);
	if (this_fl_str->mode_status == FL_MODE_FLASH) {
		wake_unlock(&this_fl_str->wake_lock);
		flashlight_turn_off();
	}
	switch (mode) {
	case FL_MODE_OFF:
		flashlight_turn_off();
	break;
	case FL_MODE_TORCH:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1];
		if (cur_fled_data) {
			this_fl_str->mode_status = FL_MODE_TORCH;
			this_fl_str->cdev.brightness = (torch_mode_current/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
	break;
	case FL_MODE_TORCH_LED_A:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1];
		if (cur_fled_data) {
			this_fl_str->mode_status = FL_MODE_TORCH_LED_A;
			this_fl_str->cdev.brightness = 1;
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
	break;
	case FL_MODE_TORCH_LED_B:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1];
		if (cur_fled_data) {
			this_fl_str->mode_status = FL_MODE_TORCH_LED_B;
			this_fl_str->cdev.brightness = 2;
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
	break;
	case FL_MODE_FLASH:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH;
			this_fl_str->cdev.brightness = (flash_mode_current/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL1:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL1;
			this_fl_str->cdev.brightness = (flash_mode_current_level1/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL2:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL2;
			this_fl_str->cdev.brightness = (flash_mode_current_level2/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL3:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL3;
			this_fl_str->cdev.brightness = (flash_mode_current_level3/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL4:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL4;
			this_fl_str->cdev.brightness = (flash_mode_current_level4/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL5:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL5;
			this_fl_str->cdev.brightness = (flash_mode_current_level5/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL6:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL6;
			this_fl_str->cdev.brightness = (flash_mode_current_level6/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;
	case FL_MODE_FLASH_LEVEL7:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1];
		if (cur_fled_data) {
			cur_fled_data->timer = this_fl_str->flash_sw_timeout_ms;
			this_fl_str->mode_status = FL_MODE_FLASH_LEVEL7;
			this_fl_str->cdev.brightness = (flash_mode_current_level7/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
		wake_lock(&this_fl_str->wake_lock);
	break;

	case FL_MODE_PRE_FLASH:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1];
		if (cur_fled_data) {
			this_fl_str->mode_status = FL_MODE_PRE_FLASH;
			this_fl_str->cdev.brightness = (pre_flash_current/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
	break;
	case FL_MODE_TORCH_LEVEL_1:
		cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1];
		if (cur_fled_data) {
			this_fl_str->mode_status = FL_MODE_TORCH_LEVEL_1;
			this_fl_str->cdev.brightness = (torch_level1_current/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
	break;
	case FL_MODE_TORCH_LEVEL_2:
		if (cur_fled_data) {
			cur_fled_data = &this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1];
			this_fl_str->mode_status = FL_MODE_TORCH_LEVEL_2;
			this_fl_str->cdev.brightness = (torch_level2_current/current_step-1);
			max8957_fled_set(cur_fled_data, this_fl_str->cdev.brightness);
		}
	break;

	default:
		printk(KERN_ERR "%s: unknown flash_light flags: %d\n",
							__func__, mode);
		ret = -EINVAL;
	break;
	}

	spin_unlock_irqrestore(&this_fl_str->spin_lock,
						this_fl_str->spinlock_flags);

	if (!cur_fled_data) {
		printk(KERN_ERR "%s: Failed to get cur_fled_data\n", __func__);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(max8957_flashlight_control);

static void led_flash_set(struct max8957_fled_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->dev);
	u8 reg_data;
	int rc;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: id=%d, name=%s, brightness=%d, cntrl_mode=%d\n", __func__, led->id, led->name, led->brightness, led->cntrl_mode);
#endif

	if (led->brightness == LED_OFF) {
		if (led->cntrl_mode == LED_CTRL_BY_FLASHSTB) {
#if MAXIM_LEDS_FLASH_DEBUG
			pr_err("%s: name=%s, LED OFF in LED_CTRL_BY_FLASHSTB mode\n", __func__, led->name);
#endif
			if (this_fl_str && this_fl_str->gpio_flash)
				gpio_set_value(this_fl_str->gpio_flash, 0);
		}

		/* Turn off FLASH */
#if MAXIM_LEDS_FLASH_DEBUG
		pr_err("%s: name=%s, LED OFF for all modes\n", __func__, led->name);
#endif
		reg_data = MAX8957_FLASH_OFF << led_en_shift[led->id];
		rc = max8957_set_bits(chip, I2C_REG_ADDR_FLASH_EN, led_en_mask[led->id], reg_data);
		if (unlikely(rc))
			pr_err("%s: can't set flash led%d level %d, rc=%d\n", __func__, led->id, led->brightness, rc);
#if MAXIM_LEDS_FLASH_DEBUG
		else
			pr_err("%s: id=%d, name=%s, addr=0x%x, data=0x%x, mask=0x%x\n", __func__, led->id, led->name, I2C_REG_ADDR_FLASH_EN, reg_data, led_en_mask[led->id]);
#endif
	} else {
		/* Config LED */
		rc = max8957_fled_setup(led);
		if (unlikely(rc)) {
			pr_err("%s: can't set flash led%d level %d, rc=%d\n", __func__, led->id, led->brightness, rc);
			return;
		}

		if (led->cntrl_mode == LED_CTRL_BY_I2C) {
#if MAXIM_LEDS_FLASH_DEBUG
			pr_err("%s: name=%s, LED ON in LED_CTRL_BY_I2C mode\n", __func__, led->name);
#endif
			/* Need to turn off LED before turn on again */
			/* Turn off LED */
			reg_data = MAX8957_FLASH_OFF << led_en_shift[led->id];
			rc = max8957_set_bits(chip, I2C_REG_ADDR_FLASH_EN, led_en_mask[led->id], reg_data);
			if (unlikely(rc))
				pr_err("%s: can't set flash led%d level %d, rc=%d\n", __func__, led->id, led->brightness, rc);
#if MAXIM_LEDS_FLASH_DEBUG
			else
				pr_err("%s: id=%d, name=%s, addr=0x%x, data=0x%x, mask=0x%x\n", __func__, led->id, led->name, I2C_REG_ADDR_FLASH_EN, reg_data, led_en_mask[led->id]);
#endif

			/* Turn on LED */
			reg_data = MAX8957_FLASH_BY_I2C << led_en_shift[led->id];
			rc = max8957_set_bits(chip, I2C_REG_ADDR_FLASH_EN, led_en_mask[led->id], reg_data);
			if (unlikely(rc))
				pr_err("%s: can't set flash led%d level %d, rc=%d\n", __func__, led->id, led->brightness, rc);
#if MAXIM_LEDS_FLASH_DEBUG
			else
				pr_err("%s: id=%d, name=%s, addr=0x%x, data=0x%x, mask=0x%x\n", __func__, led->id, led->name, I2C_REG_ADDR_FLASH_EN, reg_data, led_en_mask[led->id]);
#endif
		} else if (led->cntrl_mode == LED_CTRL_BY_FLASHSTB) {
#if MAXIM_LEDS_FLASH_DEBUG
			pr_err("%s: name=%s, LED ON in LED_CTRL_BY_FLASHSTB mode\n", __func__, led->name);
#endif
			if (this_fl_str && this_fl_str->gpio_flash) {
				gpio_set_value(this_fl_str->gpio_flash, 1);
				mdelay(3);
				/* Set GPIO gpio_flash to low in MAX8957_FLASH_MODE_ONESHOT mode */
				if (led->timer_mode == MAX8957_FLASH_MODE_ONESHOT)
					gpio_set_value(this_fl_str->gpio_flash, 0);
			}
		}
	}
}

static void max8957_fled_set(struct max8957_fled_data *led, enum led_brightness brightness)
{
	unsigned long flags;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: name=%s, brightness=%d\n", __func__, led->name, brightness);
#endif

	spin_lock_irqsave(&led->value_lock, flags);
	led->brightness = brightness;
	spin_unlock_irqrestore(&led->value_lock, flags);

	schedule_work(&led->work);
}

static void max8957_fled_work(struct work_struct *work)
{
	struct max8957_fled_data *led = container_of(work, struct max8957_fled_data, work);

	mutex_lock(&led->lock);
	led_flash_set(led);
	mutex_unlock(&led->lock);
}

static int max8957_fled_setup(struct max8957_fled_data *led)
{
	struct max8957_chip *chip = dev_get_drvdata(led->dev);
	int ret = 0;
	u8 reg_addr;
	u8 reg_data;
	u8 reg_mask;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: id=%d, name=%s, cntrl_mode=%d, timer=%d, timer_mode=%d, torch_timer_disable=%d\n",
		__func__, led->id, led->name, led->cntrl_mode, led->timer, led->timer_mode, led->torch_timer_disable);
#endif

	/* Set LED current */
	reg_addr = reg_led_current[led->id];
	reg_data = (led->brightness << led_current_shift[led->id]) & led_current_mask[led->id];
	max8957_write(chip, reg_addr, &reg_data, 1);

	/* Set control mode */
	if (led->cntrl_mode == LED_CTRL_BY_FLASHSTB) {
		reg_data = MAX8957_FLASH_BY_FLASHSTROB << led_en_shift[led->id];
		reg_addr = I2C_REG_ADDR_FLASH_EN;
		reg_mask = led_en_mask[led->id];
		ret |= max8957_set_bits(chip, reg_addr, reg_mask, reg_data);
	}

	/* Set TORCH_TIMER or FLASH_TIMER */
	reg_addr = reg_led_timer[led->id];
	reg_data = ((led->timer) | (led->torch_timer_disable) << MAX8957_DIS_TORCH_TMR_SHIFT |
			(led->timer_mode) << MAX8957_FLASH_TMR_MODE_SHIFT);
	ret |= max8957_write(chip, reg_addr, &reg_data, 1);

	return ret;
}

#ifdef MAX8957_LEDS_FLASH_TEST
static void flash_on_off(unsigned long brightness)
{
#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: brightness=%lu\n", __func__, brightness);
#endif

	if (this_fl_str) {
		/* Test for FLASH mode */
		this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1].brightness = LED_HALF;
		schedule_work(&this_fl_str->fled_data[MAX8957_ID_FLASH_LED_1].work);

		/* Test for TORCH mode */
		/*
		this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1].brightness = LED_HALF;
		schedule_work(&this_fl_str->fled_data[MAX8957_ID_TORCH_LED_1].work);
		*/
	}

	fl_timer.expires = jiffies + HZ*5;
	add_timer(&fl_timer);
}
#endif

static void max8957_fl_lcdev_brightness_set(struct led_classdev *led_cdev,
						enum led_brightness brightness)
{
	enum flashlight_mode_flags mode;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: name=%s, brightness=%d\n", __func__, led_cdev->name, brightness);
#endif

	if (brightness > 0 && brightness <= LED_HALF) {
		/* Torch mode */
		if (brightness == (LED_HALF - 2))
			mode = FL_MODE_TORCH_LEVEL_1;
		else if (brightness == (LED_HALF - 1))
			mode = FL_MODE_TORCH_LEVEL_2;
		/*
		else if (brightness == 1 && fl_str->led_count)
			mode = FL_MODE_TORCH_LED_A;
		else if (brightness == 2 && fl_str->led_count)
			mode = FL_MODE_TORCH_LED_B;
		*/
		else
			mode = FL_MODE_TORCH;
	} else if (brightness > LED_HALF && brightness <= LED_FULL) {
		/* Flashlight mode */
		if (brightness == (LED_HALF + 1))
			mode = FL_MODE_PRE_FLASH; /* pre-flash mode */
		else
			mode = FL_MODE_FLASH;
	} else
		/* off and else */
		mode = FL_MODE_OFF;

	max8957_flashlight_control(mode);

	return;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void flashlight_early_suspend(struct early_suspend *handler)
{
#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s:\n", __func__);
#endif

	if (this_fl_str != NULL && this_fl_str->mode_status) {
		spin_lock_irqsave(&this_fl_str->spin_lock, this_fl_str->spinlock_flags);
		flashlight_turn_off();
		spin_unlock_irqrestore(&this_fl_str->spin_lock,
						this_fl_str->spinlock_flags);
	}
}

static void flashlight_late_resume(struct early_suspend *handler)
{
#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s:\n", __func__);
#endif
}
#endif

static int flashlight_setup_gpio(struct max8957_fleds_platform_data *flashlight,
					struct max8957_flashlight_struct *fl_str)
{
	int ret = 0;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s:\n", __func__);
#endif

	if (flashlight->gpio_init)
		flashlight->gpio_init();

	if (flashlight->flash) {
#if MAXIM_LEDS_FLASH_DEBUG
		pr_err("%s: request flash pin(GPIO%d)\n", __func__, flashlight->flash);
#endif
		ret = gpio_request(flashlight->flash, "fl_flash");
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio_request(flash) failed\n", __func__);
			return ret;
		}
		fl_str->gpio_flash = flashlight->flash;
	}

	if (flashlight->pre_flash_duration_ms)
		fl_str->pre_flash_sw_timeout_ms = flashlight->pre_flash_duration_ms;
	else /* load default value */
		fl_str->pre_flash_sw_timeout_ms = FLASH_TIME_62P5MS;

	if (flashlight->flash_duration_ms)
		fl_str->flash_sw_timeout_ms = flashlight->flash_duration_ms;
	else /* load default value */
		fl_str->flash_sw_timeout_ms = FLASH_TIME_625MS;

	return ret;
}

static int flashlight_free_gpio(struct max8957_fleds_platform_data *flashlight,
					struct max8957_flashlight_struct *fl_str)
{
	int ret = 0;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s: request flash pin(GPIO%d)\n", __func__, flashlight->flash);
#endif

	if (fl_str->gpio_flash) {
#if MAXIM_LEDS_FLASH_DEBUG
		pr_err("%s: free flash pin(GPIO%d)\n", __func__, flashlight->flash);
#endif
		gpio_free(flashlight->flash);
		fl_str->gpio_flash = 0;
	}

	return ret;
}

static int max8957_fled_probe(struct platform_device *pdev)
{
	struct max8957_fleds_platform_data *pdata = pdev->dev.platform_data;
	struct max8957_chip *max8957_chip = dev_get_drvdata(pdev->dev.parent);
	struct max8957_fled_data *led_dat;
	struct max8957_fled *curr_led;
	struct max8957_flashlight_struct *fl_str;
	u8 reg_data;
	int rc, i = 0;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s:+\n", __func__);
#endif

	if (!max8957_chip) {
		pr_err("%s: failed to get max8957_chip\r\n", __func__);
		return -ENODEV;
	}

	fl_str = kzalloc(sizeof(struct max8957_flashlight_struct), GFP_KERNEL);
	if (!fl_str) {
		printk(KERN_ERR "%s: kzalloc fail !!!\n", __func__);
		return -ENOMEM;
	}

	rc = flashlight_setup_gpio(pdata, fl_str);
	if (rc < 0) {
		printk(KERN_ERR "%s: setup GPIO fail !!!\n", __func__);
		goto fail_free_mem;
	}

	spin_lock_init(&fl_str->spin_lock);
	wake_lock_init(&fl_str->wake_lock, WAKE_LOCK_SUSPEND, pdev->name);

	for (i = 0; i < pdata->num_leds; i++) {
		curr_led	= &pdata->leds[i];
		led_dat		= &fl_str->fled_data[curr_led->id];

		led_dat->id = curr_led->id;
		led_dat->name = curr_led->name;
		led_dat->cntrl_mode = curr_led->cntrl_mode;
		led_dat->timer = curr_led->timer;
		led_dat->timer_mode = curr_led->timer_mode;
		led_dat->torch_timer_disable = curr_led->torch_timer_disable;

		if (unlikely(led_dat->id >= MAX8957_ID_LED_MAX)) {
			dev_err(&pdev->dev, "invalid LED ID (%d) specified\n",
						 led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;
		}

		mutex_init(&led_dat->lock);
		spin_lock_init(&led_dat->value_lock);
		INIT_WORK(&led_dat->work, max8957_fled_work);

		/* initial setup */
		led_dat->dev = pdev->dev.parent;

		/* Don't need to setup FLED here but brfore turning it on */
		/*
		rc = max8957_fled_setup(led_dat);
		if (unlikely(rc)) {
		    dev_err(&pdev->dev, "failed to write\n");
		    goto err_reg_write;
		}
		*/
	}

	/* Set FLEDNUM to FLED1/FLED2 both with Fixed mode */
	/*
	reg_data = 0x84;
	*/
	/* Set FLEDNUM to FLED1 with Fixed mode (FLED2 is not used) */
	reg_data = 0x04;
	max8957_write(max8957_chip, I2C_REG_ADDR_VOUT_CNTL, &reg_data, 1);
	/* Set VOUT_FLASH to 5V */
	reg_data = 0x50;
	max8957_write(max8957_chip, I2C_REG_ADDR_VOUT_FLASH1, &reg_data, 1);

#ifdef MAX8957_LEDS_FLASH_TEST
	init_timer(&fl_timer);
	fl_timer.function = flash_on_off;
	fl_timer.data = 0;
	fl_timer.expires = jiffies + HZ*10;
	add_timer(&fl_timer);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	fl_str->early_suspend_flashlight.suspend = flashlight_early_suspend;
	fl_str->early_suspend_flashlight.resume = flashlight_late_resume;
	register_early_suspend(&fl_str->early_suspend_flashlight);
#endif

	/* Use FLASH as default LED */
	cur_fled_data = &fl_str->fled_data[MAX8957_ID_FLASH_LED_1];

	/* Register led_classdev for flashlight */
	fl_str->cdev.name = pdata->name;
	/*
	fl_str->cdev.default_trigger = cur_fled_data->default_trigger;
	*/
	fl_str->cdev.brightness_set = max8957_fl_lcdev_brightness_set;
	fl_str->cdev.brightness = LED_OFF;
	fl_str->cdev.max_brightness = LED_FULL;
	fl_str->cdev.flags = LED_CORE_SUSPENDRESUME;

	rc = led_classdev_register(&pdev->dev, &fl_str->cdev);
	if (unlikely(rc)) {
		dev_err(&pdev->dev, "unable to register led %s\n",
						fl_str->cdev.name);
		goto fail_led_register;
	}

	/* Set FL_MODE_OFF as default mode_status */
	fl_str->mode_status = FL_MODE_OFF;

	this_fl_str = fl_str;

#if MAXIM_LEDS_FLASH_DEBUG
	pr_err("%s:-\n", __func__);
#endif
	return 0;

fail_led_register:
	wake_lock_destroy(&fl_str->wake_lock);
	flashlight_free_gpio(pdata, fl_str);

fail_id_check:
fail_free_mem:
	kfree(fl_str);
	pr_err("%s:-, Failed, rc=%d\n", __func__, rc);

	return rc;
}

static int __devexit max8957_fled_remove(struct platform_device *pdev)
{
	int i;
	struct max8957_fleds_platform_data *pdata = pdev->dev.platform_data;

	if (this_fl_str) {
		flashlight_turn_off();
		unregister_early_suspend(&this_fl_str->early_suspend_flashlight);
		led_classdev_unregister(&this_fl_str->cdev);

		for (i = 0; i < pdata->num_leds; i++)
			cancel_work_sync(&this_fl_str->fled_data[i].work);

		wake_lock_destroy(&this_fl_str->wake_lock);
		flashlight_free_gpio(pdata, this_fl_str);
	}

	return 0;
}

static struct platform_driver max8957_fled_driver = {
	.probe		= max8957_fled_probe,
	.remove		= __devexit_p(max8957_fled_remove),
	.driver		=
	{
		.name	= "max8957-fled",
		.owner	= THIS_MODULE,
	},
};

static int __init max8957_fled_init(void)
{
	return platform_driver_register(&max8957_fled_driver);
}
module_init(max8957_fled_init);

static void __exit max8957_fled_exit(void)
{
	platform_driver_unregister(&max8957_fled_driver);
}
module_exit(max8957_fled_exit);

MODULE_AUTHOR("Clark Kim <clark.kim@maxim-ic.com>");
MODULE_DESCRIPTION("MAX8957 FLEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("2.1");
MODULE_ALIAS("platform:max8957-fled");
