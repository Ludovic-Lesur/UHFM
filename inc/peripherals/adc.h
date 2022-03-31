/*
 * adc.h
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#ifndef ADC_H
#define ADC_H

/*** ADC structures ***/

typedef enum {
	ADC_DATA_IDX_VPV_MV = 0,
	ADC_DATA_IDX_VOUT_MV,
	ADC_DATA_IDX_IOUT_UA,
	ADC_DATA_IDX_VMCU_MV,
	ADC_DATA_IDX_LAST
} ADC_data_index_t;

/*** ADC functions ***/

void ADC1_init(void);
void ADC1_disable(void);
void ADC1_perform_measurements(void);
void ADC1_get_data(ADC_data_index_t data_idx, unsigned int* data);
void ADC1_get_tmcu_comp2(signed char* tmcu_degrees);
void ADC1_get_tmcu_comp1(unsigned char* tmcu_degrees);

#endif /* ADC_H */
