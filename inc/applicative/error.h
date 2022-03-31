/*
 * error.h
 *
 *  Created on: Mar 31, 2022
 *      Author: Ludo
 */

#ifndef ERROR_H
#define ERROR_H

// Peripherals.
#include "adc.h"
#include "iwdg.h"
#include "lptim.h"
#include "lpuart.h"
#include "nvm.h"
#include "rcc.h"
#include "rtc.h"
// Utils.
#include "math.h"
#include "parser.h"
#include "string.h"

typedef enum {
	UHFM_SUCCESS = 0,
	UHFM_ERROR_BASE_PARSER = 0x0100
} UHFM_status_t;

#endif /* ERROR_H */
