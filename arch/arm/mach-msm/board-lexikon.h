/* linux/arch/arm/mach-msm/board-lexikon.h
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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_LEXIKON_H
#define __ARCH_ARM_MACH_MSM_BOARD_LEXIKON_H

#include <mach/board.h>

#define LEXIKON_GPIO_UART2_RX 	51
#define LEXIKON_GPIO_UART2_TX 	52

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)     (pm_gpio + NR_GPIO_IRQS)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio)    (sys_gpio - NR_GPIO_IRQS)
#define PM8058_MPP_BASE			   PM8058_GPIO_PM_TO_SYS(PM8058_GPIOS)
#define PM8058_MPP_PM_TO_SYS(pm_gpio)	   (pm_gpio + PM8058_MPP_BASE)
#define PM8058_IRQ_BASE			   (NR_MSM_IRQS + NR_GPIO_IRQS)

#define MSM_LINUX_BASE1		    0x04A00000
#define MSM_LINUX_SIZE1		    0x0B600000
#define MSM_LINUX_BASE2		    0x20000000
#define MSM_LINUX_SIZE2		    0x0C300000

#define MSM_RAM_CONSOLE_BASE	0x00500000
#define MSM_RAM_CONSOLE_SIZE	0x00100000

#define MSM_PMEM_ADSP_BASE	    0x2C300000
#define MSM_PMEM_ADSP_SIZE	    0x01D00000

#define MSM_PMEM_SF_BASE	    0x2E100000
#define MSM_PMEM_SF_SIZE	    0x01700000

#define PMEM_KERNEL_EBI0_SIZE   0x00600000

#define MSM_FB_SIZE		0x00500000

#ifdef CONFIG_ION_MSM
#define MSM_ION_HEAP_NUM	3
#define MSM_ION_CAMERA_SIZE	MSM_PMEM_ADSP_SIZE
#define MSM_ION_SF_BASE		MSM_PMEM_SF_BASE
#define MSM_ION_SF_SIZE		MSM_PMEM_SF_SIZE
#endif

/* GPIO definition */
#define PMIC_GPIO_INT		(27)

#define LEXIKON_GPIO_WIFI_IRQ             147
#define LEXIKON_GPIO_WIFI_SHUTDOWN_N       39

#define LEXIKON_GPIO_KEYPAD_POWER_KEY		46
//#define LEXIKON_GPIO_KEYPAD_MENU_KEY		113
//#define LEXIKON_GPIO_KEYPAD_HOME_KEY		18
//#define LEXIKON_GPIO_KEYPAD_BACK_KEY		19

#define LEXIKON_GPIO_FLASH_EN			97
#define LEXIKON_GPIO_TORCH_EN			98/* for XA board */

#define LEXIKON_GPIO_UP_RESET_N			43
#define LEXIKON_GPIO_UP_INT_N			142

#define LEXIKON_GPIO_TP_INT_N			20
#define LEXIKON_GPIO_TP_EN			105

#define LEXIKON_GPIO_COMPASS_INT		42
#define LEXIKON_GPIO_GSENSOR_INT_N 180
#define	LEXIKON_GPIO_CHG_INT			180
#define LEXIKON_LAYOUTS			{ \
		{ {0,  1, 0}, { 1, 0,  0}, {0, 0,  -1} }, \
		{ { 0, -1, 0}, { 1,  0,  0}, {0, 0, -1} }, \
		{ { 1, 0, 0}, { 0,  -1,  0}, {0, 0,  -1} }, \
		{ {-1,  0, 0}, { 0,  0, -1}, {0, 1,  0} }  \
					}
#define LEXIKON_MDDI_TE			(30)
#define LEXIKON_LCD_RSTz		(126)
#define LEXIKON_LCD_ID1			(128)
#define LEXIKON_LCD_ID0			(129)
#define LEXIKON_LCD_PCLK               (90)
#define LEXIKON_LCD_DE                 (91)
#define LEXIKON_LCD_VSYNC              (92)
#define LEXIKON_LCD_HSYNC              (93)
#define LEXIKON_LCD_G2                 (94)
#define LEXIKON_LCD_G3                 (95)
#define LEXIKON_LCD_G4                 (96)
#define LEXIKON_LCD_G5                 (97)
#define LEXIKON_LCD_G6                 (98)
#define LEXIKON_LCD_G7                 (99)
#define LEXIKON_LCD_B3                 (100)
#define LEXIKON_LCD_B4                 (101)
#define LEXIKON_LCD_B5                 (102)
#define LEXIKON_LCD_B6                 (103)
#define LEXIKON_LCD_B7                 (104)
#define LEXIKON_LCD_R3                 (105)
#define LEXIKON_LCD_R4                 (106)
#define LEXIKON_LCD_R5                 (107)
#define LEXIKON_LCD_R6                 (108)
#define LEXIKON_LCD_R7                 (109)

/* BT */
#define LEXIKON_GPIO_BT_UART1_RTS      (134)
#define LEXIKON_GPIO_BT_UART1_CTS      (135)
#define LEXIKON_GPIO_BT_UART1_RX       (136)
#define LEXIKON_GPIO_BT_UART1_TX       (137)
#define LEXIKON_GPIO_BT_PCM_OUT        (138)
#define LEXIKON_GPIO_BT_PCM_IN         (139)
#define LEXIKON_GPIO_BT_PCM_SYNC       (140)
#define LEXIKON_GPIO_BT_PCM_CLK        (141)
#define LEXIKON_GPIO_BT_RESET_N        (41)
#define LEXIKON_GPIO_BT_HOST_WAKE      (44)
#define LEXIKON_GPIO_BT_CHIP_WAKE      (50)
#define LEXIKON_GPIO_BT_SHUTDOWN_N     (38)

/* USB */
#define LEXIKON_GPIO_USB_ID_PIN			(49)
#define LEXIKON_GPIO_USB_ID1_PIN			(145)
#define LEXIKON_AUDIOz_UART_SW			(95)
#define LEXIKON_USBz_AUDIO_SW				(96)

#define LEXIKON_SPI_CS2                (87)
#define LEXIKON_SPI_DO                 (47)
#define LEXIKON_SPI_DI                 (48)
#define LEXIKON_SPI_CLK                (45)

#define LEXIKON_GPIO_PS_HOLD	(29)

#define LEXIKON_SLIDING_INTZ	(18)

/* 35mm headset */
#define LEXIKON_GPIO_35MM_HEADSET_DET	(26)

/* EMMC */
#define LEXIKON_GPIO_EMMC_RST		   (88)

/* PMIC GPIO */
#define PMGPIO(x) (x-1)
#define LEXIKON_VOL_UP			PMGPIO(16)
#define LEXIKON_VOL_DN			PMGPIO(17)
#define LEXIKON_AUD_SPK_ENO		PMGPIO(18)
#define LEXIKON_AUD_HANDSET_ENO		PMGPIO(19)
#define LEXIKON_GPIO_PS_EN		PMGPIO(20)
#define LEXIKON_TP_RSTz			PMGPIO(21)
#define LEXIKON_GPIO_PS_INT_N		PMGPIO(22)
#define LEXIKON_GPIO_GSENSOR_INT_N_XB PMGPIO(23)
#define LEXIKON_GREEN_LED		PMGPIO(24)
#define LEXIKON_AMBER_LED		PMGPIO(25)
#define LEXIKON_KEYPAD_LED		PMGPIO(26)
#define LEXIKON_GPIO_SDMC_CD_N		PMGPIO(34)
#define LEXIKON_GPIO_LS_EN		PMGPIO(35)
#define LEXIKON_GPIO_WIFI_BT_SLEEP_CLK_EN	PMGPIO(38)
#define LEXIKON_GPIO_uP_RST_XB PMGPIO(36)
#define LEXIKON_GPIO_COMPASS_INT_N_XB PMGPIO(37)
/*Camera*/
#define LEXIKON_CAM_RST (31)
#define LEXIKON_CAM_PWD (34)

/*display*/
extern struct platform_device msm_device_mdp;
extern struct platform_device msm_device_mddi0;
extern unsigned long msm_fb_base;
extern int panel_type;

int lexikon_init_mmc(unsigned int sys_rev);
void __init lexikon_audio_init(void);
int __init lexikon_init_keypad(void);
int __init lexikon_wifi_init(void);
void __init lexikon_microp_init(void);
int __init lexikon_init_panel(unsigned int sys_rev);
#endif /* __ARCH_ARM_MACH_MSM_BOARD_LEXIKON_H */
