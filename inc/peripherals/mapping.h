/*
 * mapping.h
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#ifndef MAPPING_H
#define MAPPING_H

#include "gpio.h"
#include "gpio_reg.h"

// Test points.
static const GPIO_pin_t GPIO_TP1 =					(GPIO_pin_t) {GPIOA, 0, 0, 0};
static const GPIO_pin_t GPIO_TP2 =					(GPIO_pin_t) {GPIOA, 0, 5, 0};
static const GPIO_pin_t GPIO_TP3 =					(GPIO_pin_t) {GPIOA, 0, 12, 0};
// LPUART.
static const GPIO_pin_t GPIO_LPUART1_TX =			(GPIO_pin_t) {GPIOA, 0, 2, 6};
static const GPIO_pin_t GPIO_LPUART1_RX =			(GPIO_pin_t) {GPIOA, 0, 3, 6};
static const GPIO_pin_t GPIO_LPUART1_DE =			(GPIO_pin_t) {GPIOB, 1, 1, 4};
static const GPIO_pin_t GPIO_LPUART1_NRE =			(GPIO_pin_t) {GPIOB, 1, 2, 0};
// Analog inputs.
static const GPIO_pin_t GPIO_ADC1_IN7 =				(GPIO_pin_t) {GPIOA, 0, 7, 0};
// TCXO power control.
static const GPIO_pin_t GPIO_TCXO_POWER_ENABLE =	(GPIO_pin_t) {GPIOA, 0, 8, 0};
// S2LP GPIOs.
static const GPIO_pin_t GPIO_S2LP_SDN =				(GPIO_pin_t) {GPIOA, 0, 9, 0};
static const GPIO_pin_t GPIO_S2LP_GPIO0 =			(GPIO_pin_t) {GPIOA, 0, 11, 0};
// Programming.
static const GPIO_pin_t GPIO_SWDIO =				(GPIO_pin_t) {GPIOA, 0, 13, 0};
static const GPIO_pin_t GPIO_SWCLK =				(GPIO_pin_t) {GPIOA, 0, 14, 0};
// SPI1.
static const GPIO_pin_t GPIO_SPI1_SCK = 			(GPIO_pin_t) {GPIOB, 1, 3, 0};
static const GPIO_pin_t GPIO_SPI1_MISO = 			(GPIO_pin_t) {GPIOB, 1, 4, 0};
static const GPIO_pin_t GPIO_SPI1_MOSI = 			(GPIO_pin_t) {GPIOB, 1, 5, 0};
static const GPIO_pin_t GPIO_S2LP_CS = 				(GPIO_pin_t) {GPIOA, 0, 15, 0};
// RF power enable.
static const GPIO_pin_t GPIO_RF_POWER_ENABLE =		(GPIO_pin_t) {GPIOB, 1, 8, 0};
static const GPIO_pin_t GPIO_RF_TX_ENABLE =			(GPIO_pin_t) {GPIOB, 1, 7, 0};
static const GPIO_pin_t GPIO_RF_RX_ENABLE =			(GPIO_pin_t) {GPIOB, 1, 6, 0};

#endif /* MAPPING_H */
