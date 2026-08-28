/* myTracking.h */
#ifndef __MY_TRACKING_H
#define __MY_TRACKING_H

#include "main.h"

#define TRK_TARGET_MAX      3       /* myCds.h의 SENSOR_COUNT와 같아야 한다 */

/* 스캔 범위 - 실측 후 조정 */
#define TRK_PAN_MIN_DEG     60.0f
#define TRK_PAN_MAX_DEG    120.0f
#define TRK_TILT_MIN_DEG   110.0f
#define TRK_TILT_MAX_DEG   140.0f

#define TRK_PAN_STEP         2.0f
#define TRK_TILT_STEP        3.0f

#define TRK_SETTLE_MS        120     /* 스캔 지점 도달 후 조도 안정화 */

/* 재방문 */
#define TRK_BACKOFF_DEG      8.0f    /* 접근 전 물러날 각도 */
#define TRK_RV_SETTLE_MS     200     /* 재방문 도달 후 안정화 */
#define TRK_FIRE_WAIT_MS     1500    /* 발사 유지 + 판정 여유 */

typedef enum {
    TRK_IDLE = 0,
    TRK_SCAN_MOVE,
    TRK_SCAN_SETTLE,
    TRK_SCAN_MEASURE,
    TRK_RV_BACKOFF,
    TRK_RV_APPROACH,
    TRK_RV_SETTLE,
    TRK_RV_FIRE,
    TRK_SCAN_DONE,
    TRK_DONE
} TrkState_t;

typedef struct {
    float   pan;
    float   tilt;
    uint8_t valid;
} TrkPoint_t;

/** @brief 지정 범위를 훑으며 각 타겟의 좌표를 찾는다. */
void trackingStart(void);

/** @brief 스캔이나 재방문을 즉시 중단하고 레이저를 끈다. */
void trackingAbort(void);

/** @brief 상태 머신을 한 번 진행시킨다. 메인 루프에서 반복 호출한다. */
void trackingUpdate(void);

/** @brief 저장된 좌표로 재방문 후 발사를 시작한다. 좌표가 없으면 0을 반환한다. */
uint8_t trackingAimAt(uint8_t idx);

/** @brief 해당 타겟의 좌표가 저장되어 있으면 1 */
uint8_t trackingHasPoint(uint8_t idx);

/** @brief 스캔 또는 재방문이 진행 중이면 1 */
uint8_t trackingIsBusy(void);

TrkState_t trackingGetState(void);
uint8_t    trackingGetFoundCount(void);

#endif
