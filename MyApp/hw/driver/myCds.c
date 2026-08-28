#include "myCds.h"
#include "adc.h"
#include "main.h"
#include "myLaser.h"
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
static uint8_t scanMode = 0;
static uint8_t ledLit[SENSOR_COUNT] = {0, 0, 0};
static ledPair pairs[SENSOR_COUNT]= {{0,GPIOC,GPIO_PIN_7},{0,GPIOA,GPIO_PIN_9},{0,GPIOA,GPIO_PIN_8}};
static uint32_t baseline[SENSOR_COUNT] = {0};
static uint32_t threshold[SENSOR_COUNT] = {0};
static uint8_t hitConfirmCount = 0;
static int hitEvent = CDS_NONE;

static void ledOn(int index){
    HAL_GPIO_WritePin(pairs[index].ledPort, pairs[index].ledPin , GPIO_PIN_SET);
    ledLit[index] = 1;
}
static void ledOff(int index){
    HAL_GPIO_WritePin(pairs[index].ledPort, pairs[index].ledPin , GPIO_PIN_RESET);
    ledLit[index] = 0;
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

static void captureBaselines(uint32_t sampleCount){
    for(int sensor = 0; sensor < SENSOR_COUNT; sensor++){
        uint32_t sum = 0;

        for(uint32_t sample = 0; sample < sampleCount; sample++){
            sum += readSensor(sensor);
            HAL_Delay(2);
        }

        baseline[sensor] = sum / sampleCount;
        threshold[sensor] = (baseline[sensor] > CDS_HIT_DELTA)
                          ? baseline[sensor] - CDS_HIT_DELTA
                          : 0U;
    }
}

static void calibrateSensors(void){
    ledAllOff();
    /* 전원과 센서 출력이 안정된 뒤 주변광 기준값을 측정한다. */
    HAL_Delay(CDS_CALIBRATION_DELAY_MS);
    captureBaselines(CDS_CALIBRATION_SAMPLES);
}

void cdsInit(){
    ledAllOff();
    calibrateSensors();
    state = CDS_SELECT;
    activeSens = -1;
    prevSens = -1;
    waitStart = 0;
    scanMode = 0;
    hitConfirmCount = 0;
    hitEvent = CDS_NONE;
    srand(HAL_GetTick());
}
void cdsUpdate(void){
    /* 스캐닝 중에는 LED 3개가 모두 켜져 있어야 하므로 랜덤 선택을 멈춘다 */
    if(scanMode){
        return;
    }

    switch (state) {
        case CDS_SELECT:
            activeSens = selectRandom(prevSens);
            ledAllOff();
            ledOn(activeSens);
            hitConfirmCount = 0;
            state = CDS_ACTIVE;
            break;
        case CDS_ACTIVE:
            pairs[activeSens].adcVal = readSensor(activeSens);

            /* 레이저가 꺼져 있을 때 주변광 변화를 천천히 기준값에 반영한다. */
            if(!laserIsOn()){
                baseline[activeSens] =
                    (baseline[activeSens] * 31U + pairs[activeSens].adcVal) / 32U;
                threshold[activeSens] =
                    (baseline[activeSens] > CDS_HIT_DELTA)
                    ? baseline[activeSens] - CDS_HIT_DELTA
                    : 0U;
                hitConfirmCount = 0;
            }
            else if(pairs[activeSens].adcVal < threshold[activeSens]){
                if(hitConfirmCount < CDS_HIT_CONFIRM_COUNT){
                    hitConfirmCount++;
                }
            }
            else{
                hitConfirmCount = 0;
            }

            if(hitConfirmCount >= CDS_HIT_CONFIRM_COUNT){
                /* LED 소등과 상태 전환이 확정된 명중만 게임에 전달한다. */
                hitEvent = activeSens;
                ledOff(activeSens);
                startRhythm();
                prevSens = activeSens;
                waitStart = HAL_GetTick();
                hitConfirmCount = 0;
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

int cdsTakeHit(void){
    int hit = hitEvent;
    hitEvent = CDS_NONE;
    return hit;
}

int cdsGetActive(void){
    if(scanMode || state != CDS_ACTIVE){
        return CDS_NONE;
    }
    return activeSens;
}

/* --- 스캐닝 모드 --- */

void cdsScanBegin(void){
    scanMode = 1;
    for(int i=0; i<SENSOR_COUNT; i++){
        ledOn(i);
    }

    /* LED 3개를 동시에 켜면서 발생하는 전원/주변광 변화를 스캔 기준값에
       포함한다. 이 함수가 끝난 뒤 trackingStart()가 레이저를 켠다. */
    HAL_Delay(CDS_SCAN_CALIBRATION_DELAY_MS);
    captureBaselines(CDS_SCAN_CALIBRATION_SAMPLES);
}

int cdsScanCheckHit(void){
    uint32_t sum[SENSOR_COUNT] = {0};
    uint32_t bestDrop = 0U;
    uint32_t secondDrop = 0U;
    int bestIndex = CDS_NONE;

    /* 순간적인 전원 흔들림을 줄이기 위해 모든 센서를 여러 번 교차 측정한다. */
    for(uint32_t sample=0; sample<CDS_SCAN_AVERAGE_COUNT; sample++){
        for(int i=0; i<SENSOR_COUNT; i++){
            if(ledLit[i] != 0){
                sum[i] += readSensor(i);
            }
        }
        HAL_Delay(1);
    }

    for(int i=0; i<SENSOR_COUNT; i++){
        if(ledLit[i] == 0){
            continue;                       /* 이미 찾은 타겟은 건너뛴다 */
        }

        pairs[i].adcVal = sum[i] / CDS_SCAN_AVERAGE_COUNT;
        uint32_t drop = (baseline[i] > pairs[i].adcVal)
                      ? baseline[i] - pairs[i].adcVal
                      : 0U;

        if(drop > bestDrop){
            secondDrop = bestDrop;
            bestDrop = drop;
            bestIndex = i;
        }
        else if(drop > secondDrop){
            secondDrop = drop;
        }
    }

    /* 전원 강하는 여러 채널이 비슷하게 떨어지므로, 한 센서만 다른
       센서보다 충분히 크게 변했을 때 레이저 명중으로 확정한다. */
    if(bestIndex != CDS_NONE &&
       bestDrop >= CDS_HIT_DELTA &&
       bestDrop >= secondDrop + CDS_SCAN_WIN_MARGIN){
        return bestIndex;
    }

    return CDS_NONE;
}

void cdsScanLedOff(int index){
    if(index < 0 || index >= SENSOR_COUNT){
        return;
    }
    ledOff(index);
    startRhythm();
}

void cdsScanEnd(void){
    scanMode = 0;
    ledAllOff();
    state = CDS_SELECT;
    activeSens = -1;
    prevSens = -1;
    hitConfirmCount = 0;
    hitEvent = CDS_NONE;
}

uint32_t cdsGetBaseline(uint8_t index){
    return (index < SENSOR_COUNT) ? baseline[index] : 0U;
}

uint32_t cdsGetThreshold(uint8_t index){
    return (index < SENSOR_COUNT) ? threshold[index] : 0U;
}
