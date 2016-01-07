/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef _BOARD_H_
#define _BOARD_H_

/*
 * Setup for the Olimex STM32-P103 proto board.
 */

/*
 * Board identifier.
 */
#define BOARD_MSDB_STM32F103C8T6
#define BOARD_NAME              "MSDB_STM32F103C8T6"

/*
 * Board frequencies.
 */
#define STM32_LSECLK            32768
#define STM32_HSECLK            8000000

/*
 * MCU type, supported types are defined in ./os/hal/platforms/hal_lld.h.
 */
 #define STM32F10X_MD

/*
 * IO pins assignments.
 */

/* on-board */
#define GPIOC_LED               13

/* out-board */
#define GPIOA_BT1               0
#define GPIOA_SHUTTER           1

#define GPIOA_MB1               2
#define GPIOA_MB2               3
#define GPIOA_MB3               4
#define GPIOA_MB4               5

#define GPIOA_MC1               6
#define GPIOA_MC2               7
#define GPIOA_MC3               8
#define GPIOA_MC4               9

#define GPIOB_MA_CLK            0
#define GPIOB_MA_DIR            1
#define GPIOB_MA_EN             3

#define GPIOB_SPI2NSS           12

/*
 * I/O ports initial setup, this configuration is established soon after reset
 * in the initialization code.
 *
 * The digits have the following meaning:
 *   0 - Analog input.
 *   1 - Push Pull output 10MHz.
 *   2 - Push Pull output 2MHz.
 *   3 - Push Pull output 50MHz.
 *   4 - Digital input.
 *   5 - Open Drain output 10MHz.
 *   6 - Open Drain output 2MHz.
 *   7 - Open Drain output 50MHz.
 *   8 - Digital input with PullUp or PullDown resistor depending on ODR.
 *   9 - Alternate Push Pull output 10MHz.
 *   A - Alternate Push Pull output 2MHz.
 *   B - Alternate Push Pull output 50MHz.
 *   C - Reserved.
 *   D - Alternate Open Drain output 10MHz.
 *   E - Alternate Open Drain output 2MHz.
 *   F - Alternate Open Drain output 50MHz.
 * Please refer to the STM32 Reference Manual for details.
 */

/*
 * Port A setup.
 * Everything input with pull-up except:
 * PA0  - Normal input      (BT1)
 * PA1  - Push Pull output  (SHUTTER).
 * PA2  - Push Pull output  (MB1).
 * PA3  - Push Pull output  (MB2).
 * PA4  - Push Pull output  (MB3).
 * PA5  - Push Pull output  (MB4).
 * PA6  - Push Pull output  (MC1).
 * PA7  - Push Pull output  (MC2).
 * PA8  - Push Pull output  (MC3).
 * PA9  - Alternate output  (USART1 TX).
 * PA10 - Normal input      (USART1 RX).
 * PA11 - Normal input      (USBDM).
 * PA12 - Normal input      (USBDP).
 * PA13 - Normal input      (DIO).
 * PA14 - Normal input      (DCLK).
 * PA15 - Push Pull output  (MC4).
 */
#define VAL_GPIOACRL            0x33333338      /*  PA7...PA0 */
#define VAL_GPIOACRH            0x388444B3      /* PA15...PA8 */
#define VAL_GPIOAODR            0xFFFFFFFF

/*
 * Port B setup.
 * PB0  - Push Pull output  (TB6569_CLK).
 * PB1  - Push Pull output  (TB6569_DIR).
 * PB3  - Push Pull output  (TB6569_EN).
 * PB12 - Push Pull output 2MHz (SPI enable).
 * PB13 - Alternate output  (MMC SPI2 SCK).
 * PB14 - Normal input      (MMC SPI2 MISO).
 * PB15 - Alternate output  (MMC SPI2 MOSI).
 */
#define VAL_GPIOBCRL            0x88883833      /*  PB7...PB0 */
#define VAL_GPIOBCRH            0xB4B28888      /* PB15...PB8 */
#define VAL_GPIOBODR            0xFFFFFFFF

/*
 * Port C setup.
 * Everything input with pull-up except:
 * PC13 - Push Pull output (LED).
 */
#define VAL_GPIOCCRL            0x88888888      /*  PC7...PC0 */
#define VAL_GPIOCCRH            0x88388888      /* PC15...PC8 */
#define VAL_GPIOCODR            0xFFFFFFFF

/*
 * Port D setup.
 * Everything input with pull-up except:
 * PD0  - Normal input (XTAL).
 * PD1  - Normal input (XTAL).
 */
#define VAL_GPIODCRL            0x88888844      /*  PD7...PD0 */
#define VAL_GPIODCRH            0x88888888      /* PD15...PD8 */
#define VAL_GPIODODR            0xFFFFFFFF

/*
 * Port E setup.
 * Everything input with pull-up except:
 */
#define VAL_GPIOECRL            0x88888888      /*  PE7...PE0 */
#define VAL_GPIOECRH            0x88888888      /* PE15...PE8 */
#define VAL_GPIOEODR            0xFFFFFFFF

/*
 * USB bus activation macro, required by the USB driver.
 */
#define usb_lld_connect_bus(usbp) palClearPad(GPIOC, GPIOC_USB_DISC)

/*
 * USB bus de-activation macro, required by the USB driver.
 */
#define usb_lld_disconnect_bus(usbp) palSetPad(GPIOC, GPIOC_USB_DISC)

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */
