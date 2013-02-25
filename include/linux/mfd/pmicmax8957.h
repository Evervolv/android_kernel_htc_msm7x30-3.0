/* linux/mfd/pmicprxx.h
 *
 * Functions to access MAX8957 power management chip.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_PMICMAX8957_H
#define __LINUX_MFD_PMICMAX8957_H

#include <linux/irq.h>
#include <linux/mfd/core.h>

#define MAXIM_I2C_DEBUG				0
#define MAXIM_GRPB_INT_ENABLE		0
#define MAXIM_INT_DEBUG				0

/*===========================================================================

Definitions for Maxim MAX8957/MAX8967 Inuerrupts

============================================================================*/
/* Interrupts for SID1 */
#define MAX8957_REG_INTGRPA             0xEF
#define MAX8957_REG_INTGRPA_M           0xF0
#define MAX8957_INTGRPA_FLASHINT_MASK   0x02
#define MAX8957_INTGRPA_KBCINT_MASK     0x04
#define MAX8957_INTGRPA_GPIOINT_MASK    0x08
#define MAX8957_INTGRPA_WLEDINT_MASK    0x10
#define MAX8957_INTGRPA_FGINT_MASK      0x20
#define MAX8957_INTGRPA_LEDINT_MASK     0x40
#define MAX8957_INTGRPA_MICBINT_MASK    0x80

#define MAX8957_REG_FLASH_INT           0x0E
#define MAX8957_REG_FLASH_INT_MASK      0x0F
#define MAX8957_IRQ_FLED2_OPEN_MASK     0x01
#define MAX8957_IRQ_FLED2_SHORT_MASK    0x02
#define MAX8957_IRQ_FLED1_OPEN_MASK     0x04
#define MAX8957_IRQ_FLED1_SHORT_MASK    0x08
#define MAX8957_IRQ_MAX_FLASH_MASK      0x10

#define MAX8957_REG_KBCINT              0x69
#define MAX8957_KBCINT_MASK             0x01
#define MAX8957_REG_KBCCNFG             0x5D
#define MAX8957_KBCINTM_MASK            0x40

#define MAX8957_REG_GPIOINT0            0x6C
#define MAX8957_GPIO0_MASK              0x01
#define MAX8957_GPIO1_MASK              0x02
#define MAX8957_GPIO2_MASK              0x04
#define MAX8957_GPIO3_MASK              0x08
#define MAX8957_GPIO4_MASK              0x10
#define MAX8957_GPIO5_MASK              0x20
#define MAX8957_GPIO6_MASK              0x40
#define MAX8957_GPIO7_MASK              0x80

#define MAX8957_REG_GPIOINT1            0x6E
#define MAX8957_GPIO8_MASK              0x01
#define MAX8957_GPIO9_MASK              0x02
#define MAX8957_GPIO10_MASK             0x04
#define MAX8957_GPIO11_MASK             0x08
#define MAX8957_GPIO12_MASK             0x10
#define MAX8957_GPIO13_MASK             0x20
#define MAX8957_GPIO14_MASK             0x40
#define MAX8957_GPIO15_MASK             0x80

#define MAX8957_REG_GPIOINT2            0x70
#define MAX8957_GPIO16_MASK             0x01
#define MAX8957_GPIO17_MASK             0x02
#define MAX8957_GPIO18_MASK             0x04
#define MAX8957_GPIO19_MASK             0x08
#define MAX8957_GPIO20_MASK             0x10
#define MAX8957_GPIO21_MASK             0x20
#define MAX8957_GPIO22_MASK             0x40
#define MAX8957_GPIO23_MASK             0x80

#define MAX8957_REG_GPIOINT3            0x72
#define MAX8957_GPIO24_MASK             0x01
#define MAX8957_GPIO25_MASK             0x02
#define MAX8957_GPIO26_MASK             0x04
#define MAX8957_GPIO27_MASK             0x08
#define MAX8957_GPIO28_MASK             0x10
#define MAX8957_GPIO29_MASK             0x20
#define MAX8957_GPIO30_MASK             0x40
#define MAX8957_GPIO31_MASK             0x80

#define MAX8957_REG_WLED_INT            0x9B
#define MAX8957_REG_WLED_INT_M          0x99
#define MAX8957_WLEDOL_MASK             0x10
#define MAX8957_WLED2SHORT_MASK         0x20
#define MAX8957_WLED1SHORT_MASK         0x40
#define MAX8957_WLEDOVP_MASK            0x80

#define MAX8957_REG_FG_INT              0xA7
#define MAX8957_REG_FG_INT_M            0xA8
#define MAX8957_TMN_MASK                0x01
#define MAX8957_TMX_MASK                0x02
#define MAX8957_VMN_MASK                0x04
#define MAX8957_VMX_MASK                0x08
#define MAX8957_SMN_MASK                0x10
#define MAX8957_SMX_MASK                0x20

#define MAX8957_REG_LED_INT             0xC6
#define MAX8957_LEDINT_MASK             0x01
#define MAX8957_LEDINT_SHIFT            0
#define MAX8957_LEDINTM_MASK            0x80
#define MAX8957_LEDINTM_SHIFT           7

#define MAX8957_REG_MICBINT             0xA4
#define MAX8957_MICBINT_MASK            0x01
#define MAX8957_REG_MICBCNFG2           0x9F
#define MAX8957_MICBINTM_MASK           0x02

/* Interrupts for SID3 */
#define MAX8957_REG_INTGRPB             0xA4
#define MAX8957_REG_INTGRPB_M           0xA5
#define MAX8957_INTGRPB_CHGINT_MASK     0x04
#define MAX8957_INTGRPB_RTCINT_MASK     0x08
#define MAX8957_INTGRPB_ADCINT_MASK     0x10
#define MAX8957_INTGRPB_BUCKINT_MASK    0x20
#define MAX8957_INTGRPB_LDOINT_MASK     0x40
#define MAX8957_INTGRPB_TOPSYSINT_MASK  0x80

#define MAX8957_REG_CHG_INT             0xB0
#define MAX8957_REG_CHG_INT_MASK        0xB1
#define MAX8957_REG_CHG_INT_OK      	0xB2
#define MAX8957_BYP_I_MASK              (1 << 0)
#define MAX8957_THM_I_MASK              (1 << 2)
#define MAX8957_BAT_I_MASK              (1 << 3)
#define MAX8957_CHG_I_MASK              (1 << 4)
#define MAX8957_CHGIN_I_MASK            (1 << 6)

#define MAX8957_REG_RTCINT              0x00
#define MAX8957_REG_RTCINTM             0x01
#define MAX8957_RTC60_MASK              0x01
#define MAX8957_RTCA1_MASK              0x02
#define MAX8957_RTCA2_MASK              0x04
#define MAX8957_SMPL_MASK               0x08
#define MAX8957_RTC1S_MASK              0x10

#define MAX8957_REG_ADCINT              0x2E
#define MAX8957_REG_ADCINTM             0x2F
#define MAX8957_ADCINT_MASK             0x01

#define MAX8957_REG_BUCKINT             0x45
#define MAX8957_REG_BUCKINTM            0x46
#define MAX8957_BUCK7_OUT_F_MASK        0x02
#define MAX8957_BUCK6_OUT_F_MASK        0x04
#define MAX8957_BUCK5_OUT_F_MASK        0x08
#define MAX8957_BUCK4_OUT_F_MASK        0x10
#define MAX8957_BUCK3_OUT_F_MASK        0x20
#define MAX8957_BUCK2_OUT_F_MASK        0x40
#define MAX8957_BUCK1_OUT_F_MASK        0x80

#define MAX8957_REG_LDO_INT1            0x76
#define MAX8957_REG_LDO_INTM1           0x7B
#define MAX8957_L01_INT_MASK            0x01
#define MAX8957_L02_INT_MASK            0x02
#define MAX8957_L03_INT_MASK            0x04
#define MAX8957_L04_INT_MASK            0x08
#define MAX8957_L05_INT_MASK            0x10
#define MAX8957_L06_INT_MASK            0x20
#define MAX8957_L07_INT_MASK            0x40
#define MAX8957_L08_INT_MASK            0x80

#define MAX8957_REG_LDO_INT2            0x78
#define MAX8957_REG_LDO_INTM2           0x7C
#define MAX8957_L09_INT_MASK            0x01
#define MAX8957_L10_INT_MASK            0x02
#define MAX8957_L11_INT_MASK            0x04
#define MAX8957_L12_INT_MASK            0x08
#define MAX8957_L13_INT_MASK            0x10
#define MAX8957_L14_INT_MASK            0x20
#define MAX8957_L15_INT_MASK            0x40
#define MAX8957_L16_INT_MASK            0x80

#define MAX8957_REG_LDO_INT3            0x7A
#define MAX8957_REG_LDO_INTM3           0x7D
#define MAX8957_L17_INT_MASK            0x01
#define MAX8957_L18_INT_MASK            0x02
#define MAX8957_L19_INT_MASK            0x04

#define MAX8957_REG_TOPSYSINT           0x9D
#define MAX8957_REG_TOPSYSINTM          0x9E
#define MAX8957_140C_MASK               0x01
#define MAX8957_120C_MASK               0x02
#define MAX8957_LOWSYS_MASK             0x04
#define MAX8957_ONKEYOFF_MASK           0x08
#define MAX8957_ONKEYON_MASK            0x10
#define MAX8957_RESET_MASK              0x20
#define MAX8957_32KINT_MASK             0x40

/* 2nd level interrupts at block level */
/* Group A interrupts */
#define MAX8957_NUM_FLASH_IRQS  5   /* FLASH_INT(0x0E) */
#define MAX8957_NUM_KBC_IRQS    1   /* KBCINT(0x69) */
#define MAX8957_NUM_GPIO_IRQS   32  /* GPIOINT0(0x6C), GPIOINT1(0x6E), GPIOINT2(0x70), GPIOINT3(0x72) */
#define MAX8957_NUM_WLED_IRQS   4   /* WLED_INT(0x9B) */
#define MAX8957_NUM_FG_IRQS     6   /* FG_INT(0xA7) */
#define MAX8957_NUM_LED_IRQS    1   /* LEDINT(0xC6) */
#define MAX8957_NUM_MICB_IRQS   1   /* MICBINT(0xA4) */

#define MAX8957_NUM_GRPA_IRQS	(MAX8957_NUM_FLASH_IRQS + \
								MAX8957_NUM_KBC_IRQS + \
								MAX8957_NUM_GPIO_IRQS + \
								MAX8957_NUM_WLED_IRQS + \
								MAX8957_NUM_FG_IRQS + \
								MAX8957_NUM_LED_IRQS + \
								MAX8957_NUM_MICB_IRQS)         /* 50 */

/* be careful if you change this since this is used to map irq <-> gpio */
#define MAX8957_FIRST_FLASH_IRQ	0
#define MAX8957_FIRST_KBC_IRQ	(MAX8957_FIRST_FLASH_IRQ + \
								MAX8957_NUM_FLASH_IRQS)
#define MAX8957_FIRST_GPIO_IRQ	(MAX8957_FIRST_KBC_IRQ + \
								MAX8957_NUM_KBC_IRQS)
#define MAX8957_FIRST_WLED_IRQ	(MAX8957_FIRST_GPIO_IRQ + \
								MAX8957_NUM_GPIO_IRQS)
#define MAX8957_FIRST_FG_IRQ	(MAX8957_FIRST_WLED_IRQ + \
								MAX8957_NUM_WLED_IRQS)
#define MAX8957_FIRST_LED_IRQ	(MAX8957_FIRST_FG_IRQ + \
								MAX8957_NUM_FG_IRQS)
#define MAX8957_FIRST_MICB_IRQ	(MAX8957_FIRST_LED_IRQ + \
								MAX8957_NUM_LED_IRQS)

/* each interrupts */
/* FLASH_INT(0x0E) */
#define MAX8957_IRQ_FLED2_OPEN      (MAX8957_FIRST_FLASH_IRQ + 0)
#define MAX8957_IRQ_FLED2_SHORT     (MAX8957_FIRST_FLASH_IRQ + 1)
#define MAX8957_IRQ_FLED1_OPEN      (MAX8957_FIRST_FLASH_IRQ + 2)
#define MAX8957_IRQ_FLED1_SHORT     (MAX8957_FIRST_FLASH_IRQ + 3)
#define MAX8957_IRQ_MAX_FLASH       (MAX8957_FIRST_FLASH_IRQ + 4)

/* KBCINT(0x69) */
#define MAX8957_IRQ_KBC             MAX8957_FIRST_KBC_IRQ

/* GPIOINT0(0x6C), GPIOINT1(0x6E), GPIOINT2(0x70), GPIOINT3(0x72) */
#define MAX8957_IRQ_GPIO0           (MAX8957_FIRST_GPIO_IRQ + 0)
#define MAX8957_IRQ_GPIO1           (MAX8957_FIRST_GPIO_IRQ + 1)
#define MAX8957_IRQ_GPIO2           (MAX8957_FIRST_GPIO_IRQ + 2)
#define MAX8957_IRQ_GPIO3           (MAX8957_FIRST_GPIO_IRQ + 3)
#define MAX8957_IRQ_GPIO4           (MAX8957_FIRST_GPIO_IRQ + 4)
#define MAX8957_IRQ_GPIO5           (MAX8957_FIRST_GPIO_IRQ + 5)
#define MAX8957_IRQ_GPIO6           (MAX8957_FIRST_GPIO_IRQ + 6)
#define MAX8957_IRQ_GPIO7           (MAX8957_FIRST_GPIO_IRQ + 7)
#define MAX8957_IRQ_GPIO8           (MAX8957_FIRST_GPIO_IRQ + 8)
#define MAX8957_IRQ_GPIO9           (MAX8957_FIRST_GPIO_IRQ + 9)
#define MAX8957_IRQ_GPIO10          (MAX8957_FIRST_GPIO_IRQ + 10)
#define MAX8957_IRQ_GPIO11          (MAX8957_FIRST_GPIO_IRQ + 11)
#define MAX8957_IRQ_GPIO12          (MAX8957_FIRST_GPIO_IRQ + 12)
#define MAX8957_IRQ_GPIO13          (MAX8957_FIRST_GPIO_IRQ + 13)
#define MAX8957_IRQ_GPIO14          (MAX8957_FIRST_GPIO_IRQ + 14)
#define MAX8957_IRQ_GPIO15          (MAX8957_FIRST_GPIO_IRQ + 15)
#define MAX8957_IRQ_GPIO16          (MAX8957_FIRST_GPIO_IRQ + 16)
#define MAX8957_IRQ_GPIO17          (MAX8957_FIRST_GPIO_IRQ + 17)
#define MAX8957_IRQ_GPIO18          (MAX8957_FIRST_GPIO_IRQ + 18)
#define MAX8957_IRQ_GPIO19          (MAX8957_FIRST_GPIO_IRQ + 19)
#define MAX8957_IRQ_GPIO20          (MAX8957_FIRST_GPIO_IRQ + 20)
#define MAX8957_IRQ_GPIO21          (MAX8957_FIRST_GPIO_IRQ + 21)
#define MAX8957_IRQ_GPIO22          (MAX8957_FIRST_GPIO_IRQ + 22)
#define MAX8957_IRQ_GPIO23          (MAX8957_FIRST_GPIO_IRQ + 23)
#define MAX8957_IRQ_GPIO24          (MAX8957_FIRST_GPIO_IRQ + 24)
#define MAX8957_IRQ_GPIO25          (MAX8957_FIRST_GPIO_IRQ + 25)
#define MAX8957_IRQ_GPIO26          (MAX8957_FIRST_GPIO_IRQ + 26)
#define MAX8957_IRQ_GPIO27          (MAX8957_FIRST_GPIO_IRQ + 27)
#define MAX8957_IRQ_GPIO28          (MAX8957_FIRST_GPIO_IRQ + 28)
#define MAX8957_IRQ_GPIO29          (MAX8957_FIRST_GPIO_IRQ + 29)
#define MAX8957_IRQ_GPIO30          (MAX8957_FIRST_GPIO_IRQ + 30)
#define MAX8957_IRQ_GPIO31          (MAX8957_FIRST_GPIO_IRQ + 31)

#define MAX8957_GPIO_IRQ_BASE   	MAX8957_IRQ_BASE + NR_MAX8957_TOP_IRQS + MAX8957_FIRST_GPIO_IRQ
#define MAX8957_GPIO_IRQ(base, gpio)	(base + gpio)
#define MAX8957_GPIO_TO_INT(gpio)	MAX8957_GPIO_IRQ(MAX8957_GPIO_IRQ_BASE, gpio)

/* WLED_INT(0x9B) */
#define MAX8957_IRQ_WLEDOL          (MAX8957_FIRST_WLED_IRQ + 0)
#define MAX8957_IRQ_WLED2SHORT      (MAX8957_FIRST_WLED_IRQ + 1)
#define MAX8957_IRQ_WLED1SHORT      (MAX8957_FIRST_WLED_IRQ + 2)
#define MAX8957_IRQ_WLEDOVP         (MAX8957_FIRST_WLED_IRQ + 3)

/* FG_INT(0xA7) */
#define MAX8957_IRQ_TMN             (MAX8957_FIRST_FG_IRQ + 0)
#define MAX8957_IRQ_TMX             (MAX8957_FIRST_FG_IRQ + 1)
#define MAX8957_IRQ_VMN             (MAX8957_FIRST_FG_IRQ + 2)
#define MAX8957_IRQ_VMX             (MAX8957_FIRST_FG_IRQ + 3)
#define MAX8957_IRQ_SMN             (MAX8957_FIRST_FG_IRQ + 4)
#define MAX8957_IRQ_SMX             (MAX8957_FIRST_FG_IRQ + 5)
/*
#define MAX8957_IRQ_BR              (MAX8957_FIRST_FG_IRQ + 6)
#define MAX8957_IRQ_BI              (MAX8957_FIRST_FG_IRQ + 7)
*/

#define MAX8957_FG_IRQ_BASE   		MAX8957_IRQ_BASE + NR_MAX8957_TOP_IRQS + MAX8957_FIRST_FG_IRQ
#define MAX8957_FG_IRQ(base, fg_int)	(base + fg_int)
#define MAX8957_FG_TO_INT(fg_int)	MAX8957_FG_IRQ(MAX8957_FG_IRQ_BASE, fg_int)

/* LEDINT(0xC6) */
#define MAX8957_IRQ_LEDINT          MAX8957_FIRST_LED_IRQ

/* MICBINT(0xA4) */
#define MAX8957_IRQ_MICBINT         MAX8957_FIRST_MICB_IRQ

/* Group B interrupts (SID3) */
#define MAX8957_NUM_CHG_IRQS    5   /* CHG_INT(0xB0) */
#define MAX8957_NUM_RTC_IRQS    5   /* RTCINT(0x00) */
#define MAX8957_NUM_ADC_IRQS    1   /* ADCINT(0x2E) */
#define MAX8957_NUM_BUCK_IRQS   7   /* BUCKINT(0x45) */
#define MAX8957_NUM_LDO_IRQS    19  /* LDO_INT1(0x76), LDO_INT2(0x78), LDO_INT3(0x7A) */
#define MAX8957_NUM_TOPSYS_IRQS 7   /* TOPSYSINT(0x9D) */

#define MAX8957_NUM_GRPB_IRQS	(MAX8957_NUM_CHG_IRQS + \
								MAX8957_NUM_RTC_IRQS + \
								MAX8957_NUM_ADC_IRQS + \
								MAX8957_NUM_BUCK_IRQS + \
								MAX8957_NUM_LDO_IRQS + \
								MAX8957_NUM_TOPSYS_IRQS)   /* 44 */

/* be careful if you change this since this is used to map irq <-> gpio */
#define MAX8957_FIRST_CHG_IRQ	(MAX8957_FIRST_MICB_IRQ + \
								MAX8957_NUM_MICB_IRQS)
#define MAX8957_FIRST_RTC_IRQ	(MAX8957_FIRST_CHG_IRQ + \
								MAX8957_NUM_CHG_IRQS)
#define MAX8957_FIRST_ADC_IRQ	(MAX8957_FIRST_RTC_IRQ + \
								MAX8957_NUM_RTC_IRQS)
#define MAX8957_FIRST_BUCK_IRQ	(MAX8957_FIRST_ADC_IRQ + \
								MAX8957_NUM_ADC_IRQS)
#define MAX8957_FIRST_LDO_IRQ	(MAX8957_FIRST_BUCK_IRQ + \
								MAX8957_NUM_BUCK_IRQS)
#define MAX8957_FIRST_TOPSYS_IRQ	(MAX8957_FIRST_LDO_IRQ + \
									MAX8957_NUM_LDO_IRQS)
#define MAX8957_LAST_IRQ		(MAX8957_FIRST_TOPSYS_IRQ + \
								MAX8957_NUM_TOPSYS_IRQS)

/* each interrupts */
/* CHG_INT(0xB0, SID3) */
#define MAX8957_IRQ_BYP_I           (MAX8957_FIRST_CHG_IRQ + 0)
#define MAX8957_IRQ_THM_I           (MAX8957_FIRST_CHG_IRQ + 1)
#define MAX8957_IRQ_BAT_I           (MAX8957_FIRST_CHG_IRQ + 2)
#define MAX8957_IRQ_CHG_I           (MAX8957_FIRST_CHG_IRQ + 3)
#define MAX8957_IRQ_CHGIN_I         (MAX8957_FIRST_CHG_IRQ + 4)
#define MAX8957_CHG_IRQ_BASE   		MAX8957_IRQ_BASE + NR_MAX8957_TOP_IRQS + MAX8957_FIRST_CHG_IRQ
#define MAX8957_CHG_IRQ(base, chg_int)	(base + chg_int)

/* RTCINT(0x00) */
#define MAX8957_IRQ_RTC60           (MAX8957_FIRST_RTC_IRQ + 0)
#define MAX8957_IRQ_RTCA1           (MAX8957_FIRST_RTC_IRQ + 1)
#define MAX8957_IRQ_RTCA2           (MAX8957_FIRST_RTC_IRQ + 2)
#define MAX8957_IRQ_SMPL            (MAX8957_FIRST_RTC_IRQ + 3)
#define MAX8957_IRQ_RTC1S           (MAX8957_FIRST_RTC_IRQ + 4)

/* ADCINT(0x2E) */
#define MAX8957_IRQ_ADCINT          MAX8957_FIRST_ADC_IRQ

/* BUCKINT(0x45) */
#define MAX8957_IRQ_BUCK7_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 0)
#define MAX8957_IRQ_BUCK6_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 1)
#define MAX8957_IRQ_BUCK5_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 2)
#define MAX8957_IRQ_BUCK4_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 3)
#define MAX8957_IRQ_BUCK3_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 4)
#define MAX8957_IRQ_BUCK2_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 5)
#define MAX8957_IRQ_BUCK1_OUT_F     (MAX8957_FIRST_BUCK_IRQ + 6)

/* LDO_INT1(0x76), LDO_INT2(0x78), LDO_INT3(0x7A) */
#define MAX8957_IRQ_LO1_INT         (MAX8957_FIRST_LDO_IRQ + 0)
#define MAX8957_IRQ_LO2_INT         (MAX8957_FIRST_LDO_IRQ + 1)
#define MAX8957_IRQ_LO3_INT         (MAX8957_FIRST_LDO_IRQ + 2)
#define MAX8957_IRQ_LO4_INT         (MAX8957_FIRST_LDO_IRQ + 3)
#define MAX8957_IRQ_LO5_INT         (MAX8957_FIRST_LDO_IRQ + 4)
#define MAX8957_IRQ_LO6_INT         (MAX8957_FIRST_LDO_IRQ + 5)
#define MAX8957_IRQ_LO7_INT         (MAX8957_FIRST_LDO_IRQ + 6)
#define MAX8957_IRQ_LO8_INT         (MAX8957_FIRST_LDO_IRQ + 7)
#define MAX8957_IRQ_LO9_INT         (MAX8957_FIRST_LDO_IRQ + 8)
#define MAX8957_IRQ_LO10_INT        (MAX8957_FIRST_LDO_IRQ + 9)
#define MAX8957_IRQ_LO11_INT        (MAX8957_FIRST_LDO_IRQ + 10)
#define MAX8957_IRQ_LO12_INT        (MAX8957_FIRST_LDO_IRQ + 11)
#define MAX8957_IRQ_LO13_INT        (MAX8957_FIRST_LDO_IRQ + 12)
#define MAX8957_IRQ_LO14_INT        (MAX8957_FIRST_LDO_IRQ + 13)
#define MAX8957_IRQ_LO15_INT        (MAX8957_FIRST_LDO_IRQ + 14)
#define MAX8957_IRQ_LO16_INT        (MAX8957_FIRST_LDO_IRQ + 15)
#define MAX8957_IRQ_LO17_INT        (MAX8957_FIRST_LDO_IRQ + 16)
#define MAX8957_IRQ_LO18_INT        (MAX8957_FIRST_LDO_IRQ + 17)
#define MAX8957_IRQ_LO19_INT        (MAX8957_FIRST_LDO_IRQ + 18)

/* TOPSYSINT(0x9D) */
#define MAX8957_IRQ_140C            (MAX8957_FIRST_TOPSYS_IRQ + 0)
#define MAX8957_IRQ_120C            (MAX8957_FIRST_TOPSYS_IRQ + 1)
#define MAX8957_IRQ_LOWSYS          (MAX8957_FIRST_TOPSYS_IRQ + 2)
#define MAX8957_IRQ_ONKEYOFF        (MAX8957_FIRST_TOPSYS_IRQ + 3)
#define MAX8957_IRQ_ONKEYON         (MAX8957_FIRST_TOPSYS_IRQ + 4)
#define MAX8957_IRQ_RESET           (MAX8957_FIRST_TOPSYS_IRQ + 5)
#define MAX8957_IRQ_32KINT          (MAX8957_FIRST_TOPSYS_IRQ + 6)


#define	MAX8957_NUM_IRQS	(MAX8957_NUM_GRPA_IRQS + MAX8957_NUM_GRPB_IRQS)
#define	MAX8957_NUM_BLOCKS	MAX8957_NR_INTS
#define	MAX8957_NUM_GROUPS  2

#define MAX8967_VREG_SD1H		0x02
#define MAX8967_VREG_SD1L		0x03
#define MAX8967_VREG_SD1H_CFG	0x04
#define MAX8967_VREG_SD1L_CFG	0x05

#define MAX8967_VREG_SD2H		0x06
#define MAX8967_VREG_SD2L		0x07
#define MAX8967_VREG_SD2H_CFG	0x08
#define MAX8967_VREG_SD2L_CFG	0x09

#define MAX8967_VREG_LDO1		0x0E
#define MAX8967_VREG_LDO1_CFG2	0x0F
#define MAX8967_VREG_LDO2		0x10
#define MAX8967_VREG_LDO2_CFG2	0x11
#define MAX8967_VREG_LDO3		0x12
#define MAX8967_VREG_LDO3_CFG2	0x13
#define MAX8967_VREG_LDO4		0x14
#define MAX8967_VREG_LDO4_CFG2	0x15
#define MAX8967_VREG_LDO5		0x16
#define MAX8967_VREG_LDO5_CFG2	0x17
#define MAX8967_VREG_LDO6		0x18
#define MAX8967_VREG_LDO6_CFG2	0x19

#define MAX8967_VREG_TYPE_SD    0X00
#define MAX8967_VREG_TYPE_LDO   0X01

#define MAX8967_SD_MODE_M		0x30
#define MAX8967_SD_MODE_SHIFT   4
#define MAX8967_LDO_MODE_M      0xC0
#define MAX8967_LDO_MODE_SHIFT  6
#define MAX8967_LDO_VOLT_M      0x3F

#define MAX8967_POWER_OFF_MODE			0x00
#define MAX8967_DYNAMIC_LOW_POWER_MODE  0x01
#define MAX8967_LOW_POWER_MODE          0x10
#define MAX8967_NORMAL_MODE             0x11

/* 1st lev IRQ definitions at top level*/
enum {
	MAX8957_INT_GRPA_START,
	MAX8957_INT_GRPA_FLASHINT = MAX8957_INT_GRPA_START,
	MAX8957_INT_GRPA_KBCINT,
	MAX8957_INT_GRPA_GPIOINT,
	MAX8957_INT_GRPA_WLEDINT,
	MAX8957_INT_GRPA_FGINT,
	MAX8957_INT_GRPA_LEDINT,
	MAX8957_INT_GRPA_MICBINT,
	MAX8957_INT_GRPA_END = MAX8957_INT_GRPA_MICBINT,

	MAX8957_INT_GRPB_START,
	MAX8957_INT_GRPB_CHGINT = MAX8957_INT_GRPB_START,
	MAX8957_INT_GRPB_RTCINT,
	MAX8957_INT_GRPB_ADCINT,
	MAX8957_INT_GRPB_BUCKINT,
	MAX8957_INT_GRPB_LDOINT,
	MAX8957_INT_GRPB_TOPSYSINT,
	MAX8957_INT_GRPB_END = MAX8957_INT_GRPB_TOPSYSINT,
	MAX8957_NR_INTS,        /* 13 */
};

enum {
    MAX8957_INT_FLED2_OPEN,
    MAX8957_INT_FLED2_SHORT,
    MAX8957_INT_FLED1_OPEN,
    MAX8957_INT_FLED1_SHORT,
    MAX8957_INT_MAX_FLASH,

    MAX8957_INT_KBC,

    MAX8957_INT_GPIO0,
    MAX8957_INT_GPIO1,
    MAX8957_INT_GPIO2,
    MAX8957_INT_GPIO3,
    MAX8957_INT_GPIO4,
    MAX8957_INT_GPIO5,
    MAX8957_INT_GPIO6,
    MAX8957_INT_GPIO7,
    MAX8957_INT_GPIO8,
    MAX8957_INT_GPIO9,
    MAX8957_INT_GPIO10,
    MAX8957_INT_GPIO11,
    MAX8957_INT_GPIO12,
    MAX8957_INT_GPIO13,
    MAX8957_INT_GPIO14,
    MAX8957_INT_GPIO15,
    MAX8957_INT_GPIO16,
    MAX8957_INT_GPIO17,
    MAX8957_INT_GPIO18,
    MAX8957_INT_GPIO19,
    MAX8957_INT_GPIO20,
    MAX8957_INT_GPIO21,
    MAX8957_INT_GPIO22,
    MAX8957_INT_GPIO23,
    MAX8957_INT_GPIO24,
    MAX8957_INT_GPIO25,
    MAX8957_INT_GPIO26,
    MAX8957_INT_GPIO27,
    MAX8957_INT_GPIO28,
    MAX8957_INT_GPIO29,
    MAX8957_INT_GPIO30,
    MAX8957_INT_GPIO31,

    MAX8957_INT_WLEDOL,
    MAX8957_INT_WLED2SHORT,
    MAX8957_INT_WLED1SHORT,
    MAX8957_INT_WLEDOVP,

    MAX8957_INT_TMN,
    MAX8957_INT_TMX,
    MAX8957_INT_VMN,
    MAX8957_INT_VMX,
    MAX8957_INT_SMN,
    MAX8957_INT_SMX,

    MAX8957_INT_LEDINT,

    MAX8957_INT_MICBINT,

	/* GROUP B */
    MAX8957_INT_BYP_I,
    MAX8957_INT_THM_I,
    MAX8957_INT_BAT_I,
    MAX8957_INT_CHG_I,
    MAX8957_INT_CHGIN_I,

    MAX8957_INT_RTC60,
    MAX8957_INT_RTCA1,
    MAX8957_INT_RTCA2,
    MAX8957_INT_SMPL,
    MAX8957_INT_RTC1S,

    MAX8957_INT_ADCINT,

    MAX8957_INT_BUCK7_OUT_F,
    MAX8957_INT_BUCK6_OUT_F,
    MAX8957_INT_BUCK5_OUT_F,
    MAX8957_INT_BUCK4_OUT_F,
    MAX8957_INT_BUCK3_OUT_F,
    MAX8957_INT_BUCK2_OUT_F,
    MAX8957_INT_BUCK1_OUT_F,

    MAX8957_INT_L01_INT,
    MAX8957_INT_L02_INT,
    MAX8957_INT_L03_INT,
    MAX8957_INT_L04_INT,
    MAX8957_INT_L05_INT,
    MAX8957_INT_L06_INT,
    MAX8957_INT_L07_INT,
    MAX8957_INT_L08_INT,
    MAX8957_INT_L09_INT,
    MAX8957_INT_L10_INT,
    MAX8957_INT_L11_INT,
    MAX8957_INT_L12_INT,
    MAX8957_INT_L13_INT,
    MAX8957_INT_L14_INT,
    MAX8957_INT_L15_INT,
    MAX8957_INT_L16_INT,
    MAX8957_INT_L17_INT,
    MAX8957_INT_L18_INT,
    MAX8957_INT_L19_INT,

    MAX8957_INT_140C,
    MAX8957_INT_120C,
    MAX8957_INT_LOWSYS,
    MAX8957_INT_ONKEYOFF,
    MAX8957_INT_ONKEYON,
    MAX8957_INT_RESET,
    MAX8957_INT_32KINT,
};

enum {
    MAX8957_PDATA_SID1,
    MAX8957_PDATA_SID3,
    MAX8957_PDATA_SID4,
};

struct max8957_platform_data {
    /* This table is only needed for misc interrupts. */
    int     irq_base;
    int     pdata_number;
    int		num_subdevs;
    struct mfd_cell *sub_devices;
};

struct max8957_chip {
    struct max8957_platform_data pdata;
    struct i2c_client   *dev;
    struct mutex    pm_lock;
    struct mutex    irq_lock;
    u8	irqs_1st_allowed[MAX8957_NUM_BLOCKS];   /* top level */
    u8  irqs_2nd_allowed[MAX8957_NUM_IRQS];     /* 2nd level(block level) */
};

#if defined(CONFIG_DEBUG_FS)
struct max8957_dbg_device {
    struct mutex        dbg_mutex;
    struct max8957_chip  *pm_chip;
    struct dentry       *dent;
    int	addr;
};
#endif

int max8957_read(struct max8957_chip *chip, u8 addr, u8 *values, unsigned int len);
int max8957_write(struct max8957_chip *chip, u8 addr, u8 *values, unsigned int len);
int max8957_set_bits(struct max8957_chip *chip, u8 addr, u8 mask, u8 value);
/* Disable I2C functions without lock */
/*
int max8957_read_no_lock(struct max8957_chip *chip, u8 addr, u8 *values, unsigned int len);
int max8957_write_no_lock(struct max8957_chip *chip, u8 addr, u8 *values, unsigned int len);
int max8957_set_bits_no_lock(struct max8957_chip *chip, u8 addr, u8 mask, u8 value);
*/
extern int max8957_device_init(struct max8957_chip *chip,
				struct max8957_platform_data *pdata);
extern int max8957_request_irq(struct max8957_chip *chip, int irq,
			    struct max8957_platform_data *pdata);
extern void max8957_device_exit(struct max8957_chip *chip);
#endif
