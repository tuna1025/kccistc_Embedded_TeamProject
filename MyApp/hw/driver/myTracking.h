/* myTracking.h */
#ifndef __MY_TRACKING_H
#define __MY_TRACKING_H

#include "main.h"

#define TRK_TARGET_MAX      3

/* 스캔 범위 — 실측 후 조정 */
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
#define TRK_FIRE_WAIT_MS     3200    /* 발사 유지 + 판정 여유 */

typedef enum {
    TRK_IDLE = 0,
    TRK_SCAN_MOVE,
    TRK_SCAN_SETTLE,
    TRK_SCAN_MEASURE,
    TRK_RV_BACKOFF,
    TRK_RV_APPROACH,
    TRK_RV_SETTLE,
    TRK_RV_FIRE,
    TRK_DONE
} TrkState_t;

typedef struct {
    float   pan;
    float   tilt;
    uint8_t valid;
} TrkPoint_t;

void       trackingStart(void);
void       trackingAbort(void);
void       trackingUpdate(void);
TrkState_t trackingGetState(void);
uint8_t    trackingGetFoundCount(void);

#endif