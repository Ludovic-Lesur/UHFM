/*
 * at.h
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#ifndef AT_H
#define AT_H

#include "mode.h"

/*** AT functions ***/

void AT_init(void);
void AT_task(void);
void AT_fill_rx_buffer(unsigned char rx_byte);
void AT_print_test_result(unsigned char status, int rssi);

#endif /* AT_H */
