/*
 * dma.h
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#ifndef DMA_H
#define DMA_H

/*** DMA functions ***/

void DMA1_init_channel3(void);
void DMA1_start_channel3(void);
void DMA1_stop_channel3(void);
void DMA1_set_channel3_source_addr(unsigned int source_buf_addr, unsigned short source_buf_size);
unsigned char DMA1_get_channel3_status(void);
void DMA1_disable(void);

#endif /* DMA_H */
