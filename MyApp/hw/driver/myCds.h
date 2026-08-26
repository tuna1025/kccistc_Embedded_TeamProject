#pragma once
#include "main.h"
#include <cstdint>
#include <stdint.h>

void cdsInit(void);
uint32_t Adc_ch0(void);
uint32_t Adc_ch1(void);
uint32_t Adc_ch4(void);
void adcUpdate(void);
