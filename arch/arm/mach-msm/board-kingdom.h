/* linux/arch/arm/mach-msm/board-kingdom.h
 * Copyright (C) 2007-2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __ARCH_ARM_MACH_MSM_BOARD_KINGDOM_H
#define __ARCH_ARM_MACH_MSM_BOARD_KINGDOM_H

#include <mach/board.h>

#define KINGDOM_GPIO_UART2_RX 	51
#define KINGDOM_GPIO_UART2_TX 	52

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)     (pm_gpio + NR_GPIO_IRQS)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio)    (sys_gpio - NR_GPIO_IRQS)

#define MSM_LINUX_BASE1         0x05000000
#define MSM_LINUX_SIZE1         0x1B000000
#define MSM_LINUX_BASE2         0x20000000
#define MSM_LINUX_SIZE2         0x09700000

#define MSM_RAM_CONSOLE_BASE    0x00500000
#define MSM_RAM_CONSOLE_SIZE    0x000F0000

#define MSM_PMEM_ADSP_BASE	0x29700000
#define MSM_PMEM_ADSP_SIZE      0x03A00000

#define MSM_PMEM_SF_BASE	0x2D100000
#define MSM_PMEM_SF_SIZE        0x01700000

#define MSM_FB_SIZE             0x00800000

#define PMEM_KERNEL_EBI0_SIZE   0x00900000

#define KINGDOM_GPIO_KEYPAD_POWER_KEY	(46)

#define KINGDOM_GPIO_FLASHLIGHT_FLASH   (97)
#define KINGDOM_GPIO_FLASHLIGHT_TORCH   (96)
#define KINGDOM_GPIO_UP_RESET_N		(54)

#define KINGDOM_GPIO_TP_EN		(105)

#define KINGDOM_LAYOUTS			{ \
		{ {0,  1, 0}, { -1, 0,  0}, {0, 0, 1} }, \
		{ { 0, -1, 0}, { 1,  0,  0}, {0, 0, -1} }, \
		{ {-1, 0, 0}, {  0, -1, 0}, {0, 0, 1} }, \
		{ {-1,  0, 0}, { 0,  0, -1}, {0, 1,  0} }  \
					}

/* display */
#define KINGDOM_MDDI_TE			(30)
#define V_LCM_3V_EN						(102)
#define KINGDOM_LCD_RSTz		(126)
#define KINGDOM_LCD_ID1			(128)
#define KINGDOM_LCD_ID0			(129)
/* Wifi */
#define KINGDOM_GPIO_WIFI_IRQ       (147)
#define KINGDOM_GPIO_WIFI_SHUTDOWN_N    (36)
/* BT */
#define KINGDOM_GPIO_BT_UART1_RTS	(134)
#define KINGDOM_GPIO_BT_UART1_CTS	(135)
#define KINGDOM_GPIO_BT_UART1_RX		(136)
#define KINGDOM_GPIO_BT_UART1_TX		(137)
#define KINGDOM_GPIO_BT_PCM_OUT          (138)
#define KINGDOM_GPIO_BT_PCM_IN           (139)
#define KINGDOM_GPIO_BT_PCM_SYNC         (140)
#define KINGDOM_GPIO_BT_PCM_CLK          (141)
#define KINGDOM_GPIO_BT_RESET_N		(37)
#define KINGDOM_GPIO_BT_HOST_WAKE	(44)
#define KINGDOM_GPIO_BT_CHIP_WAKE	(50)
#define KINGDOM_GPIO_BT_SHUTDOWN_N	(35)

/* WiMax */
#define KINGDOM_GPIO_V_WIMAX_1V2_RF_EN	(93)
#define KINGDOM_GPIO_WIMAX_EXT_RST		(21)
#define KINGDOM_GPIO_V_WIMAX_DVDD_EN	(92)
#define KINGDOM_GPIO_V_WIMAX_PVDD_EN	(90)
#define KINGDOM_GPIO_WIMAX_SDIO_D0		(43)
#define KINGDOM_GPIO_WIMAX_SDIO_D1		(42)
#define KINGDOM_GPIO_WIMAX_SDIO_D2		(41)
#define KINGDOM_GPIO_WIMAX_SDIO_D3		(40)
#define KINGDOM_GPIO_WIMAX_SDIO_CMD		(39)
#define KINGDOM_GPIO_WIMAX_SDIO_CLK_CPU	(38)
#define KINGDOM_CPU_WIMAX_SW				(98)

/* USB */
#define KINGDOM_GPIO_USB_ID_PIN		(49)
#define KINGDOM_GPIO_USB_ID1_PIN		(145)

/* EMMC */
#define KINGDOM_GPIO_EMMC_RST	(88)

#define KINGDOM_SPI_DO			(47)
#define KINGDOM_SPI_DI			(48)
#define KINGDOM_SPI_CLK			(45)

#define KINGDOM_GPIO_PS_HOLD		(29)

/* Audio */
#define KINGDOM_AUD_CODEC_RST          (25)
#define KINGDOM_WCA_MCLK               (103)
#define KINGDOM_WCA_DATA_SD0           (121)
#define KINGDOM_WCA_DATA_SD1           (122)
#define KINGDOM_WCA_DATA_SD2           (123)
#define KINGDOM_WCF_I2S_WS             (144)
#define KINGDOM_WCF_I2S_CLK            (145)
#define KINGDOM_WCF_I2S_DATA           (146)

#define KINGDOM_SYN_TP_INT_N           (18)
#define KINGDOM__SYN_TP_RSTz           (99)

/* PMIC GPIO */
#define PMGPIO(x) (x-1)
#define KINGDOM_GPIO_TP_INT_N		PMGPIO(1)
#define KINGDOM_GPIO_TP_INT_N_CPU	PMGPIO(2)
#define KINGDOM_VOL_UP			PMGPIO(3)
#define KINGDOM_VOL_DN			PMGPIO(4)
#define KINGDOM_AUD_REMO_PRES		PMGPIO(6)
#define KINGDOM_AUD_MICPATH_SEL		PMGPIO(7)
#define KINGDOM_AUD_REMO_EN		PMGPIO(8)
#define SHOOTER_WIMAX_DEBUG12      	PMGPIO(16)
#define SHOOTER_WIMAX_DEBUG14      	PMGPIO(15)
#define SHOOTER_WIMAX_DEBUG15      	PMGPIO(14)
#define KINGDOM_WiMAX_HOST_WAKEUP    	PMGPIO(17)
#define KINGDOM_AUD_HP_EN		PMGPIO(18)
#define KINGDOM_AUD_EP_EN		PMGPIO(18)
#define KINGDOM_AUD_SPK_EN		PMGPIO(19)
#define KINGDOM_GPIO_GSENSOR_INT	PMGPIO(20)
#define KINGDOM_TP_RSTz			PMGPIO(21)
#define KINGDOM_GPIO_PS_INT_N		PMGPIO(22)
#define KINGDOM_AUD_HP_DETz		PMGPIO(23)
#define KINGDOM_GREEN_LED		PMGPIO(24)
#define KINGDOM_AMBER_LED		PMGPIO(25)
#define KINGDOM_KEYPAD_LED		PMGPIO(26)
#define KINGDOM_GPIO_SDMC_CD_N		PMGPIO(34)
#define KINGDOM_GPIO_CHG_INT		PMGPIO(35)
#define KINGDOM_GPIO_uP_RST_XB	PMGPIO(36)
#define KINGDOM_GPIO_COMPASS_INT	PMGPIO(37)
#define KINGDOM_WIFI_BT_SLEEP_CLK_EN	PMGPIO(38)

/*display*/
extern struct platform_device msm_device_mdp;
extern struct platform_device msm_device_mddi0;
extern unsigned long msm_fb_base;
extern int panel_type;

int kingdom_init_mmc(unsigned int sys_rev);
void __init kingdom_audio_init(void);
int kingdom_init_keypad(void);
int __init kingdom_wifi_init(void);

/*int __init kingdom_init_panel(unsigned int sys_rev);*/
#endif /* __ARCH_ARM_MACH_MSM_BOARD_KINGDOM_H */
