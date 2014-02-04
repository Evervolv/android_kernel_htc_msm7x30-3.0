/* linux/arch/arm/mach-msm/board-kingdom-audio.c
 *
 * Copyright (C) 2010-2011 HTC Corporation.
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

#include <linux/android_pmem.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/marimba.h>
#include <linux/delay.h>
#include <linux/tpa2051d3.h>
#include <mach/gpio.h>
#include <mach/pmic.h>
#include <mach/dal.h>
#include "board-kingdom.h"
#include <mach/qdsp5v2/snddev_icodec.h>
#include <mach/qdsp5v2/snddev_ecodec.h>
#include <mach/qdsp5v2/audio_def.h>
#include <mach/qdsp5v2/voice.h>
#include <mach/htc_acoustic_7x30.h>
#include <mach/htc_acdb_7x30.h>
#include <linux/spi_aic3254.h>
#include <mach/board_htc.h>

static struct mutex bt_sco_lock;
static int curr_rx_mode;
static atomic_t aic3254_ctl = ATOMIC_INIT(0);
void kingdom_back_mic_enable(int);

#define BIT_SPEAKER	(1 << 0)
#define BIT_HEADSET	(1 << 1)
#define BIT_RECEIVER	(1 << 2)
#define BIT_FM_SPK	(1 << 3)
#define BIT_FM_HS	(1 << 4)

#define KINGDOM_ACDB_SMEM_SIZE        (0xE000)
#define KINGDOM_ACDB_RADIO_BUFFER_SIZE (1024 * 3072)

static struct q5v2_hw_info q5v2_audio_hw[Q5V2_HW_COUNT] = {
	[Q5V2_HW_HANDSET] = {
		.max_gain[VOC_NB_INDEX] = 0,
		.min_gain[VOC_NB_INDEX] = 0,
		.max_gain[VOC_WB_INDEX] = 0,
		.min_gain[VOC_WB_INDEX] = 0,
	},
	[Q5V2_HW_HEADSET] = {
		.max_gain[VOC_NB_INDEX] = 0,
		.min_gain[VOC_NB_INDEX] = 0,
		.max_gain[VOC_WB_INDEX] = 0,
		.min_gain[VOC_WB_INDEX] = 0,
	},
	[Q5V2_HW_SPEAKER] = {
		.max_gain[VOC_NB_INDEX] = 0,
		.min_gain[VOC_NB_INDEX] = 0,
		.max_gain[VOC_WB_INDEX] = 0,
		.min_gain[VOC_WB_INDEX] = 0,
	},
	[Q5V2_HW_BT_SCO] = {
		.max_gain[VOC_NB_INDEX] = 0,
		.min_gain[VOC_NB_INDEX] = -1500,
		.max_gain[VOC_WB_INDEX] = 0,
		.min_gain[VOC_WB_INDEX] = -1500,
	},
	[Q5V2_HW_TTY] = {
		.max_gain[VOC_NB_INDEX] = 0,
		.min_gain[VOC_NB_INDEX] = 0,
		.max_gain[VOC_WB_INDEX] = 0,
		.min_gain[VOC_WB_INDEX] = 0,
	},
	[Q5V2_HW_HS_SPKR] = {
		.max_gain[VOC_NB_INDEX] = -500,
		.min_gain[VOC_NB_INDEX] = -2000,
		.max_gain[VOC_WB_INDEX] = -500,
		.min_gain[VOC_WB_INDEX] = -2000,
	},
	[Q5V2_HW_USB_HS] = {
		.max_gain[VOC_NB_INDEX] = 1000,
		.min_gain[VOC_NB_INDEX] = -500,
		.max_gain[VOC_WB_INDEX] = 1000,
		.min_gain[VOC_WB_INDEX] = -500,
	},
	[Q5V2_HW_HAC] = {
		.max_gain[VOC_NB_INDEX] = 100,
		.min_gain[VOC_NB_INDEX] = -1900,
		.max_gain[VOC_WB_INDEX] = 100,
		.min_gain[VOC_WB_INDEX] = -1900,
	},
};

static unsigned aux_pcm_gpio_off[] = {
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_OUT, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_DOUT */
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_IN, 0, GPIO_CFG_INPUT, 
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_DIN  */
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_SYNC, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_SYNC */
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_CLK, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_CLK  */
};

static unsigned aux_pcm_gpio_on[] = {
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_OUT, 1, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA),   /* PCM_DOUT */
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_IN, 1, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA),   /* PCM_DIN  */
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_SYNC, 1, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA),   /* PCM_SYNC */
	GPIO_CFG(KINGDOM_GPIO_BT_PCM_CLK, 1, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA),   /* PCM_CLK  */
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("[CAM] %s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

void kingdom_snddev_poweramp_on(int en)
{
	pr_aud_info("%s %d\n", __func__, en);
	if (en) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_SPK_EN), 1);
		mdelay(30);
		if (!atomic_read(&aic3254_ctl))
			curr_rx_mode |= BIT_SPEAKER;
	} else {
		/* Reset AIC3254 */
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_SPK_EN), 0);
		if (!atomic_read(&aic3254_ctl))
			curr_rx_mode &= ~BIT_SPEAKER;
	}
}

void kingdom_snddev_hsed_pamp_on(int en)
{
	pr_aud_info("%s %d\n", __func__, en);
	if (en) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_HP_EN), 1);
		mdelay(30);
		set_headset_amp(1);
		if (!atomic_read(&aic3254_ctl))
			curr_rx_mode |= BIT_HEADSET;
	} else {
		set_headset_amp(0);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_HP_EN), 0);
		if (!atomic_read(&aic3254_ctl))
			curr_rx_mode &= ~BIT_HEADSET;
	}
}

void kingdom_snddev_hs_spk_pamp_on(int en)
{
	kingdom_snddev_poweramp_on(en);
	kingdom_snddev_hsed_pamp_on(en);
}

void kingdom_snddev_bt_sco_pamp_on(int en)
{
	static int bt_sco_refcount;
	pr_aud_info("%s %d\n", __func__, en);
	mutex_lock(&bt_sco_lock);
	if (en) {
		if (++bt_sco_refcount == 1)
			config_gpio_table(aux_pcm_gpio_on,
					ARRAY_SIZE(aux_pcm_gpio_on));
	} else {
		if (--bt_sco_refcount == 0) {
			config_gpio_table(aux_pcm_gpio_off,
					ARRAY_SIZE(aux_pcm_gpio_off));
			gpio_set_value(KINGDOM_GPIO_BT_PCM_OUT, 0);
			gpio_set_value(KINGDOM_GPIO_BT_PCM_SYNC, 0);
			gpio_set_value(KINGDOM_GPIO_BT_PCM_CLK, 0);
		}
	}
	mutex_unlock(&bt_sco_lock);
}

void kingdom_snddev_receiver_pamp_on(int en)
{
	pr_aud_info("%s %d\n", __func__, en);
	if (en) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_EP_EN), 1);
		mdelay(20);
		set_handset_amp(1);
		if (!atomic_read(&aic3254_ctl))
			curr_rx_mode |= BIT_RECEIVER;
	} else {
		set_handset_amp(0);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_EP_EN), 0);
		if (!atomic_read(&aic3254_ctl))
			curr_rx_mode &= ~BIT_RECEIVER;
	}
}

void kingdom_snddev_imic_pamp_on(int en)
{
	pr_aud_info("%s: %d\n", __func__, en);

	if (en) {
		pmic_hsed_enable(PM_HSED_CONTROLLER_0, PM_HSED_ENABLE_ALWAYS);
		kingdom_back_mic_enable(1);
	} else {
		pmic_hsed_enable(PM_HSED_CONTROLLER_0, PM_HSED_ENABLE_OFF);
		kingdom_back_mic_enable(0);
	}
}

void kingdom_snddev_emic_pamp_on(int en)
{
	pr_aud_info("%s %d\n", __func__, en);
	if (en) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL), 1);
	} else
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL), 0);
}

void kingdom_back_mic_enable(int en)
{
	pr_aud_info("%s %d\n", __func__, en);
	if (en) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL),
			0);
		pmic_hsed_enable(PM_HSED_CONTROLLER_1, PM_HSED_ENABLE_ALWAYS);
	} else {
		pmic_hsed_enable(PM_HSED_CONTROLLER_1, PM_HSED_ENABLE_OFF);
	}
}

int kingdom_get_rx_vol(uint8_t hw, int network, int level)
{
	struct q5v2_hw_info *info;
	int vol, maxv, minv;

	info = &q5v2_audio_hw[hw];
	maxv = info->max_gain[network];
	minv = info->min_gain[network];
	vol = minv + ((maxv - minv) * level) / 100;
	pr_aud_info("%s(%d, %d, %d) => %d\n", __func__, hw, network, level, vol);
	return vol;
}

void kingdom_mic_bias_enable(int en, int shift)
{
	pr_aud_info("%s: %d\n", __func__, en);

	if (en)
		pmic_hsed_enable(PM_HSED_CONTROLLER_2, PM_HSED_ENABLE_ALWAYS);
	else
		pmic_hsed_enable(PM_HSED_CONTROLLER_2, PM_HSED_ENABLE_OFF);
}

void kingdom_rx_amp_enable(int en)
{
	if (curr_rx_mode != 0) {
		atomic_set(&aic3254_ctl, 1);
		pr_aud_info("%s: curr_rx_mode 0x%x, en %d\n",
			__func__, curr_rx_mode, en);
		if (curr_rx_mode & BIT_SPEAKER)
			kingdom_snddev_poweramp_on(en);
		if (curr_rx_mode & BIT_HEADSET)
			kingdom_snddev_hsed_pamp_on(en);
		if (curr_rx_mode & BIT_RECEIVER)
			kingdom_snddev_receiver_pamp_on(en);
		atomic_set(&aic3254_ctl, 0);;
	}
}

uint32_t kingdom_get_smem_size(void)
{
	return KINGDOM_ACDB_SMEM_SIZE;
}

uint32_t kingdom_get_acdb_radio_buffer_size(void)
{
	return KINGDOM_ACDB_RADIO_BUFFER_SIZE;
}

int kingdom_support_aic3254(void)
{
	return 1;
}

int kingdom_support_back_mic(void)
{
	return 1;
}

void kingdom_get_acoustic_tables(struct acoustic_tables *tb)
{
	strcpy(tb->aic3254,
			"AIC3254_REG_DualMic.csv\0");
}

static struct q5v2audio_icodec_ops iops = {
	.support_aic3254 = kingdom_support_aic3254,
};

static struct acdb_ops acdb = {
	.get_acdb_radio_buffer_size = kingdom_get_acdb_radio_buffer_size,
};

static struct q5v2audio_analog_ops ops = {
	.speaker_enable	= kingdom_snddev_poweramp_on,
	.headset_enable	= kingdom_snddev_hsed_pamp_on,
	.handset_enable	= kingdom_snddev_receiver_pamp_on,
	.bt_sco_enable = kingdom_snddev_bt_sco_pamp_on,
	.headset_speaker_enable = kingdom_snddev_hs_spk_pamp_on,
	.int_mic_enable = kingdom_snddev_imic_pamp_on,
	.ext_mic_enable = kingdom_snddev_emic_pamp_on,
	.fm_headset_enable = kingdom_snddev_hsed_pamp_on,
	.fm_speaker_enable = kingdom_snddev_poweramp_on,
};

static struct q5v2audio_ecodec_ops eops = {
	.bt_sco_enable  = kingdom_snddev_bt_sco_pamp_on,
};

static struct q5v2voice_ops vops = {
	.get_rx_vol = kingdom_get_rx_vol,
};

static struct acoustic_ops acoustic = {
	.enable_mic_bias = kingdom_mic_bias_enable,
	.support_aic3254 = kingdom_support_aic3254,
	.support_back_mic = kingdom_support_back_mic,
	.enable_back_mic =  kingdom_back_mic_enable,
	.get_acoustic_tables = kingdom_get_acoustic_tables
};

static struct aic3254_ctl_ops cops = {
	.rx_amp_enable = kingdom_rx_amp_enable,
};


void __init kingdom_audio_init(void)
{
	mutex_init(&bt_sco_lock);
#ifdef CONFIG_MSM7KV2_AUDIO
	htc_7x30_register_analog_ops(&ops);
	htc_7x30_register_icodec_ops(&iops);
	htc_7x30_register_ecodec_ops(&eops);
	htc_7x30_register_voice_ops(&vops);
	acoustic_register_ops(&acoustic);
	acdb_register_ops(&acdb);
#endif
	aic3254_register_ctl_ops(&cops);

	gpio_request(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_SPK_EN), "AMP_SPK_EN");
	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_SPK_EN), 1);
	gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_SPK_EN), 0);

	gpio_request(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_HP_EN), "AMP_HP_EN");
	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_HP_EN), 1);
	gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL), 0);

	gpio_request(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL),
						"aud_mic_sel");
	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL),
						1);
	gpio_set_value(PM8058_GPIO_PM_TO_SYS(KINGDOM_AUD_MICPATH_SEL), 0);

	mutex_lock(&bt_sco_lock);
	config_gpio_table(aux_pcm_gpio_off, ARRAY_SIZE(aux_pcm_gpio_off));
	gpio_set_value(KINGDOM_GPIO_BT_PCM_OUT, 0);
	gpio_set_value(KINGDOM_GPIO_BT_PCM_SYNC, 0);
	gpio_set_value(KINGDOM_GPIO_BT_PCM_CLK, 0);
	mutex_unlock(&bt_sco_lock);
}
