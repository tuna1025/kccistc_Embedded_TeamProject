#pragma once
#include "main.h"
#include "stm32f411xe.h"
#include "stm32f4xx_hal_adc.h"
#include <stdint.h>
#include <stdlib.h>

#define SENSOR_COUNT 3
#define WAIT_TIME 1000
#define CDS_CALIBRATION_SAMPLES 32U
#define CDS_CALIBRATION_DELAY_MS 1000U
#define CDS_HIT_DELTA 30U
#define CDS_HIT_CONFIRM_COUNT 3U

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

/** @brief 부팅 시 측정한 센서별 주변광 평균값을 반환한다. */
uint32_t cdsGetBaseline(uint8_t index);
/** @brief 센서별 명중 임계값(주변광 평균 - CDS_HIT_DELTA)을 반환한다. */
uint32_t cdsGetThreshold(uint8_t index);
