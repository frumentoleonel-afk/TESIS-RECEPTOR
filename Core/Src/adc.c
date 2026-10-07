/*
 * adc.c
 *
 *  Created on: Jul 7, 2025
 *      Author: Leonel
 */
#include "adc.h"

extern ADC_HandleTypeDef hadc1;

uint16_t Get_ADC_Average(uint8_t samples)
{
    uint32_t sum = 0;

    for (uint8_t i = 0; i < samples; i++)
    {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }

    return (uint16_t)(sum / samples);  // devuelve promedio en pasos (0–4095)
}
