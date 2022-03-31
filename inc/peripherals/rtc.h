/*
 * rtc.h
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#ifndef RTC_H
#define RTC_H

#include "mode.h"

/*** RTC macros ***/

// RTC wake-up timer period.
// Warning: this value must be lower than the watchdog period = 25s.
 #define RTC_WAKEUP_PERIOD_SECONDS	5

/*** RTC functions ***/

void RTC_reset(void);
void RTC_init(void);
void RTC_start_wakeup_timer(unsigned int delay_seconds);
void RTC_stop_wakeup_timer(void);
volatile unsigned char RTC_get_wakeup_timer_flag(void);
void RTC_clear_wakeup_timer_flag(void);

#endif /* RTC_H */
