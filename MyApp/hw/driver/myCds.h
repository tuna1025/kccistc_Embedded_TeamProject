#pragma once
#include "main.h"
#include "stm32f411xe.h"
#include "stm32f4xx_hal_adc.h"
#include <stdint.h>
#include <stdlib.h>

#define SENSOR_COUNT 3
#define WAIT_TIME 1000
#define LIGHT_THRESHOLD 250

typedef enum{
    CDS_SELECT,
    CDS_ACTIVE,
    CDS_WAIT
} cdsState;
typedef struct{
    uint32_t adcVal;
    GPIO_TypeDef *ledPort;
    uint16_t ledPin;
}ledPair;
void cdsInit();
void cdsUpdate(void);
uint32_t readADC(uint32_t channel);