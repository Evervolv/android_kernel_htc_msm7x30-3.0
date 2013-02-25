/*
 *  Max8957 MICB driver
 *
 *  Copyright (c) 2011 Maxim Integrated Product
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#ifndef __HTC_HEADSET_MAX8957_H__
#define __HTC_HEADSET_MAX8957_H__

#define MAXIM_HEADSET_DEBUG	1

/*===========================================================================

Definitions for Maxim MAX8957/MAX8967 MICB

============================================================================*/
#define MAX8957_MICBCNFG1	0x9E
#define MAX8957_MICBCNFG2	0x9F
#define MAX8957_MICBFIFOH	0xA1
#define MAX8957_MICBFIFOL	0xA2
#define MAX8957_MICBINT		0xA4
#define MAX8957_MICBSTTS	0xA5

#define MAX8957_ADCCNTL		0x26
#define MAX8957_ADCSEL0		0x28
#define MAX8957_ADCSEL1		0x29
#define MAX8957_ADCCHSEL	0x2A
#define MAX8957_ADCDATAL	0x2B
#define MAX8957_ADCDATAH	0x2C
#define MAX8957_ADCINT		0x2E

#define MAX8957_JDEB		0xF8
#define MAX8957_VREF		0x07
#define MAX8957_MICBINTM	0x02
#define MAX8957_CMP_EN		0x01
#define MAX8957_MICB_INT	0x01
#define MAX8957_FIFO_STTS	0x01
#define MAX8957_FIFOL		0xFF
#define MAX8957_FIFOH		0x0F
#define MAX8957_FIFO_PRESS	0x80

#define MAX8957_MICB_BIAS_VOLTAGE 2500
#define MAX8957_MICB_FIFO_LEN 3

#define DRIVER_HS_MAX8957_RPC_MIC_STATUS	(1 << 0)
#define DRIVER_HS_MAX8957_USE_SID1		(1 << 1)
#define DRIVER_HS_MAX8957_USE_SID3		(1 << 2)


enum max8957_micb_vref {
	MAX8957_MICB_VREF_350MV,
	MAX8957_MICB_VREF_450MV,
	MAX8957_MICB_VREF_550MV,
	MAX8957_MICB_VREF_650MV,
	MAX8957_MICB_VREF_750MV,
	MAX8957_MICB_VREF_850MV,
	MAX8957_MICB_VREF_950MV,
	MAX8957_MICB_VREF_1050MV,
};

enum max8957_micb_jdeb {
	MAX8957_MICB_JDEB_0MS,
	MAX8957_MICB_JDEB_10MS,
	MAX8957_MICB_JDEB_20MS,
	MAX8957_MICB_JDEB_30MS,
	MAX8957_MICB_JDEB_40MS,
	MAX8957_MICB_JDEB_50MS,
	MAX8957_MICB_JDEB_60MS,
	MAX8957_MICB_JDEB_70MS,
	MAX8957_MICB_JDEB_80MS,
	MAX8957_MICB_JDEB_90MS,
	MAX8957_MICB_JDEB_100MS,
	MAX8957_MICB_JDEB_110MS,
	MAX8957_MICB_JDEB_120MS,
	MAX8957_MICB_JDEB_130MS,
	MAX8957_MICB_JDEB_140MS,
	MAX8957_MICB_JDEB_150MS,
	MAX8957_MICB_JDEB_160MS,
	MAX8957_MICB_JDEB_170MS,
	MAX8957_MICB_JDEB_180MS,
	MAX8957_MICB_JDEB_190MS,
	MAX8957_MICB_JDEB_200MS,
};

struct htc_headset_max8957_platform_data {
	unsigned int driver_flag;
	/* ADC tables */
	uint32_t adc_mic;
	uint32_t adc_mic_bias[2];
	uint32_t adc_remote[6];
	/* Remote key detection */
	unsigned int remote_irq;
	enum max8957_micb_vref vref;
	enum max8957_micb_jdeb jdeb;
};

int max8957_micbint_enable(int enable);

#endif  /* __HTC_HEADSET_MAX8957_H__ */
