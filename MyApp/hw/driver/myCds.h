#pragma once
#include "main.h"
#include "stm32f411xe.h"
#include "stm32f4xx_hal_adc.h"
#include <stdint.h>
#include <stdlib.h>

#define SENSOR_COUNT 3
#define WAIT_TIME 1000
#define LIGHT_THRESHOLD 700

#define CDS_NONE (-1)

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

/** @brief 현재 점등된 타겟 index. 없으면 CDS_NONE */
int  cdsGetActive(void);

/* --- 스캐닝 모드 --- */

/** @brief LED 3개를 모두 켜고 일반 상태머신을 정지시킨다. */
void cdsScanBegin(void);

/** @brief 아직 켜져 있는 LED 중 레이저가 감지된 index. 없으면 CDS_NONE */
int  cdsScanCheckHit(void);

/** @brief 해당 타겟 LED를 소등한다(스캔 성공 표시). */
void cdsScanLedOff(int index);

/** @brief 스캐닝 모드를 끝내고 일반 상태머신으로 복귀한다. */
void cdsScanEnd(void);
