#include "myCds.h"
#include "adc.h"
#include "main.h"
#include "myPiezo.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>
#include <stdlib.h>

static cdsState state = CDS_SELECT;
static int activeSens = -1;
static int prevSens = -1;
static uint32_t waitStart = 0;
static ledPair pairs[SENSOR_COUNT]= {{0,GPIOC,GPIO_PIN_7},{0,GPIOA,GPIO_PIN_9},{0,GPIOA,GPIO_PIN_8}};

static void ledOn(int index){
    HAL_GPIO_WritePin(pairs[index].ledPort, pairs[index].ledPin , GPIO_PIN_SET);
}
static void ledOff(int index){
    HAL_GPIO_WritePin(pairs[index].ledPort, pairs[index].ledPin , GPIO_PIN_RESET);
}
static void ledAllOff(void){
    for(int i=0; i<SENSOR_COUNT; i++){
        ledOff(i);
    }
}
static int selectRandom(int prev){
    int selected;
    do{
        selected = rand()%SENSOR_COUNT;
    }while(selected == prev);
    return selected;
}
 uint32_t readADC(uint32_t channel){
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if(HAL_ADC_ConfigChannel(&hadc1, &sConfig)){
        Error_Handler();
    }
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}
static uint32_t readSensor(int index){
    switch (index) {
        case 0:
            return readADC(ADC_CHANNEL_4);
        case 1:
            return readADC(ADC_CHANNEL_11);
        case 2:
            return readADC(ADC_CHANNEL_10);
        default:
            return 0;
    }
}
void cdsInit(){
    ledAllOff();
    state = CDS_SELECT;
    activeSens = -1;
    prevSens = -1;
    waitStart = 0;
    srand(HAL_GetTick());
}
void cdsUpdate(void){
    switch (state) {
        case CDS_SELECT:
            activeSens = selectRandom(prevSens);
            ledAllOff();
            ledOn(activeSens);
            state = CDS_ACTIVE;
            break;
        case CDS_ACTIVE:
            pairs[activeSens].adcVal = readSensor(activeSens);
            if(pairs[activeSens].adcVal < LIGHT_THRESHOLD){
                ledOff(activeSens);
                startRhythm();
                prevSens = activeSens;
                waitStart = HAL_GetTick();
                state = CDS_WAIT;
            }
            break;
        case CDS_WAIT:
            if(HAL_GetTick() - waitStart >= WAIT_TIME){
                state = CDS_SELECT;
            }
            break;
        default:
            state = CDS_SELECT;
            break;
    }
}