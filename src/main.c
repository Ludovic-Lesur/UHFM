/*
 * main.c
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

// Peripherals.
#include "adc.h"
#include "aes.h"
#include "dma.h"
#include "exti.h"
#include "gpio.h"
#include "iwdg.h"
#include "lptim.h"
#include "lpuart.h"
#include "mapping.h"
#include "nvic.h"
#include "nvm.h"
#include "pwr.h"
#include "rcc.h"
#include "spi.h"
#include "rtc.h"
// Components.
#include "s2lp.h"
#include "sigfox_types.h"
#include "sigfox_api.h"
// Applicative.
#include "at.h"
#include "mode.h"
#include "sigfox_api.h"

/* MAIN FUNCTION.
 * @param: 	None.
 * @return: 0.
 */
int main (void) {
	// Init memory.
	NVIC_init();
	// Init power and clock modules.
	PWR_init();
	RCC_init();
	RCC_enable_lsi();
	// Init watchdog.
#ifndef DEBUG
	IWDG_init();
#endif
	// Init GPIOs.
	GPIO_init();
	EXTI_init();
	// Init RTC.
	RTC_reset();
	RCC_enable_lse();
	RTC_init();
	// Init peripherals.
	LPTIM1_init();
	LPUART1_init();
	ADC1_init();
	SPI1_init();
	// Init components.
	S2LP_init();
	// Init AT interface.
	AT_init();
	// Main loop.
	while (1) {
		IWDG_reload();
		// Enter stop mode.
		PWR_enter_stop_mode();
		// Wake-up: perform AT task.
		AT_task();
	}
}
