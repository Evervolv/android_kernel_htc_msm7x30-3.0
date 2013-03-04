/* linux/arch/arm/mach-msm/board-speedy.h
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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_SPEEDY_H
#define __ARCH_ARM_MACH_MSM_BOARD_SPEEDY_H

#include <mach/board.h>

#define SPEEDY_GPIO_UART2_RX	51
#define SPEEDY_GPIO_UART2_TX	52

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)     (pm_gpio + NR_GPIO_IRQS)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio)    (sys_gpio - NR_GPIO_IRQS)
#define PM8058_MPP_BASE			   PM8058_GPIO_PM_TO_SYS(PM8058_GPIOS)
#define PM8058_MPP_PM_TO_SYS(pm_gpio)	   (pm_gpio + PM8058_MPP_BASE)
#define PM8058_IRQ_BASE			   (NR_MSM_IRQS + NR_GPIO_IRQS)

#define MSM_LINUX_BASE1		0x04000000
#define MSM_LINUX_SIZE1		0x0C000000
#define MSM_LINUX_BASE2		0x20000000
#define MSM_LINUX_SIZE2		0x0C100000

#define MSM_GPU_MEM_BASE	0x00100000
#define MSM_GPU_MEM_SIZE	0x003f0000

#define MSM_RAM_CONSOLE_BASE	0x00500000
#define MSM_RAM_CONSOLE_SIZE    0x000F0000

#define MSM_PMEM_ADSP_BASE	0x2C100000
#define MSM_PMEM_ADSP_SIZE	0x01D00000

#define PMEM_KERNEL_EBI1_BASE   0x2DE00000
#define PMEM_KERNEL_EBI1_SIZE   0x00600000

#define MSM_PMEM_SF_BASE	0x2E400000
#define MSM_PMEM_SF_SIZE	0x01700000

#define MSM_FB_BASE		0x2FB00000
#define MSM_FB_SIZE		0x00500000

/* GPIO definition */
#define PMIC_GPIO_INT		(27)

#define SPEEDY_GPIO_WIFI_IRQ		(147)
#define SPEEDY_GPIO_WIFI_SHUTDOWN_N	(36)

#define SPEEDY_GPIO_KEYPAD_POWER_KEY	(46)

#define SPEEDY_GPIO_FLASH_EN		(97)
#define SPEEDY_GPIO_UP_RESET_N		(54)

#define SPEEDY_GPIO_TP_INT_N		(20)
#define SPEEDY_GPIO_TP_EN		(105)

#define SPEEDY_LAYOUTS			{ \
		{ {0,  1, 0}, { 1,  0,  0}, {0, 0, -1} }, \
		{ {0, -1, 0}, { -1, 0,  0}, {0, 0, 1} }, \
		{ {1,  0, 0}, { 0,  -1, 0}, {0, 0, -1} }, \
		{ {1,  0, 0}, { 0,  0,  1}, {0, 1, 0} }  \
					}

#define SPEEDY_MDDI_TE			(30)
#define SPEEDY_LCD_RSTz			(126)
#define SPEEDY_LCD_ID1			(128)
#define SPEEDY_LCD_ID0			(129)
/* BT */
#define SPEEDY_GPIO_BT_UART1_RTS	(134)
#define SPEEDY_GPIO_BT_UART1_CTS	(135)
#define SPEEDY_GPIO_BT_UART1_RX		(136)
#define SPEEDY_GPIO_BT_UART1_TX		(137)
#define SPEEDY_GPIO_BT_PCM_OUT          (138)
#define SPEEDY_GPIO_BT_PCM_IN           (139)
#define SPEEDY_GPIO_BT_PCM_SYNC         (140)
#define SPEEDY_GPIO_BT_PCM_CLK          (141)
#define SPEEDY_GPIO_BT_RESET_N		(37)
#define SPEEDY_GPIO_BT_HOST_WAKE	(44)
#define SPEEDY_GPIO_BT_CHIP_WAKE	(50)
#define SPEEDY_GPIO_BT_SHUTDOWN_N	(35)

/* WiMax */
#define SPEEDY_GPIO_V_WIMAX_1V2_RF_EN	(93)
#define SPEEDY_GPIO_WIMAX_EXT_RST	(21)
#define SPEEDY_GPIO_V_WIMAX_DVDD_EN	(92)
#define SPEEDY_GPIO_V_WIMAX_PVDD_EN	(90)
#define SPEEDY_GPIO_WIMAX_SDIO_D0	(43)
#define SPEEDY_GPIO_WIMAX_SDIO_D1	(42)
#define SPEEDY_GPIO_WIMAX_SDIO_D2	(41)
#define SPEEDY_GPIO_WIMAX_SDIO_D3	(40)
#define SPEEDY_GPIO_WIMAX_SDIO_CMD	(39)
#define SPEEDY_GPIO_WIMAX_SDIO_CLK_CPU	(38)
#define SPEEDY_CPU_WIMAX_SW		(98)

/* USB */
#define SPEEDY_GPIO_USB_ID_PIN		(49)
#define SPEEDY_GPIO_USB_ID1_PIN		(145)

#define SPEEDY_SPI_DO			(47)
#define SPEEDY_SPI_DI			(48)
#define SPEEDY_SPI_CLK			(45)

#define SPEEDY_GPIO_PS_HOLD		(29)

#define SPEEDY_SLIDING_INTZ		(18)

/* 35mm headset */
#define SPEEDY_GPIO_35MM_HEADSET_DET	(26)

/* Camera */
#define SPEEDY_CAM_RST			(31)
#define SPEEDY_CAM_PWD			(34)

#define SPEEDY_GPIO_EMMC_RST		  (88)

/* PMIC GPIO */
#define PMGPIO(x) (x-1)
#define SPEEDY_KYPD_SNS1		PMGPIO(1)
#define SPEEDY_KYPD_SNS2		PMGPIO(2)
#define SPEEDY_KYPD_SNS3		PMGPIO(3)
#define SPEEDY_KYPD_SNS4		PMGPIO(4)
#define SPEEDY_KYPD_SNS5		PMGPIO(5)
#define SPEEDY_KYPD_SNS6		PMGPIO(6)
#define SPEEDY_KYPD_SNS7		PMGPIO(7)
#define SPEEDY_WiMAX_HOST_WAKEUP	PMGPIO(17)
#define SPEEDY_AUD_SPK_ENO		PMGPIO(18)
#define SPEEDY_AUD_HANDSET_ENO		PMGPIO(19)
#define SPEEDY_GPIO_PS_EN		PMGPIO(20)
#define SPEEDY_GPIO_GSENSOR_INT_N_XB	PMGPIO(20)
#define SPEEDY_TP_RSTz			PMGPIO(21)
#define SPEEDY_GPIO_PS_INT_N		PMGPIO(22)
#define SPEEDY_GPIO_UP_INT_N		PMGPIO(23)
#define SPEEDY_GREEN_LED		PMGPIO(24)
#define SPEEDY_AMBER_LED		PMGPIO(25)
#define SPEEDY_KEYPAD_LED		PMGPIO(26)
#define SPEEDY_VOL_UP			PMGPIO(27)
#define SPEEDY_VOL_DN			PMGPIO(29)
#define SPEEDY_GPIO_SDMC_CD_N		PMGPIO(34)
#define SPEEDY_GPIO_LS_EN		PMGPIO(35)
#define SPEEDY_GPIO_CHG_INT		PMGPIO(35)
#define SPEEDY_GPIO_uP_RST_XB		PMGPIO(36)
#define SPEEDY_GPIO_GSENSOR_INT_N_XA	PMGPIO(37)
#define SPEEDY_GPIO_COMPASS_INT_N_XB	PMGPIO(37)
#define SPEEDY_WIFI_BT_SLEEP_CLK_EN	PMGPIO(38)

/*display*/
extern struct platform_device msm_device_mdp;
extern struct platform_device msm_device_mddi0;
extern int panel_type;

unsigned int speedy_get_engineerid(void);
int __init speedy_init_mmc(unsigned int sys_rev);
void __init speedy_audio_init(void);
int __init speedy_init_keypad(void);
int __init speedy_wifi_init(void);
void __init speedy_microp_init(void);

int __init speedy_init_panel(unsigned int sys_rev);
#endif /* __ARCH_ARM_MACH_MSM_BOARD_SPEEDY_H */
