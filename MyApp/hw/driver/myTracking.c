/* myTracking.c */
#include "myTracking.h"
#include "myServo.h"
#include "myLaser.h"

static TrkState_t trkState = TRK_IDLE;
static TrkPoint_t trkPoint[TRK_TARGET_MAX];
static uint8_t    trkFound = 0;
static uint8_t    trkIdx   = 0;
static uint32_t   trkTick  = 0;

static float  trkPan  = 0.0f;
static float  trkTilt = 0.0f;
static int8_t trkDir  = 1;

static float trkPanMin,  trkPanMax;
static float trkTiltMin, trkTiltMax;

/* 스캔 범위를 서보 물리 가동범위와 교집합으로 계산 */
static void trkCalcRange(void)
{
    trkPanMin  = servoClampAngle(SERVO_PAN,  TRK_PAN_MIN_DEG);
    trkPanMax  = servoClampAngle(SERVO_PAN,  TRK_PAN_MAX_DEG);
    trkTiltMin = servoClampAngle(SERVO_TILT, TRK_TILT_MIN_DEG);
    trkTiltMax = servoClampAngle(SERVO_TILT, TRK_TILT_MAX_DEG);
}

/* 종료 처리 — 레이저 소등 경로를 한 곳으로 모은다 */
static void trkFinish(void)
{
    laserFire();

    trkState = TRK_DONE;
}

/* --- 스캔 --- */

/* 다음 스캔 지점으로 목표 설정. 전체 완료면 0 */
static uint8_t trkNextPoint(void)
{
    trkPan += TRK_PAN_STEP * (float)trkDir;

    if (trkPan > trkPanMax || trkPan < trkPanMin)
    {
        if (trkPan > trkPanMax) trkPan = trkPanMax;
        else                    trkPan = trkPanMin;

        trkDir = -trkDir;

        trkTilt += TRK_TILT_STEP;

        if (trkTilt > trkTiltMax)
            return 0;
    }

    servoSetTarget(SERVO_PAN,  trkPan);
    servoSetTarget(SERVO_TILT, trkTilt);

    return 1;
}

/* --- 재방문 --- */

/* trkIdx 타겟의 백오프 지점으로 목표 설정 */
static void trkAimBackoff(void)
{
    servoSetTarget(SERVO_PAN,  trkPoint[trkIdx].pan - TRK_BACKOFF_DEG);
    servoSetTarget(SERVO_TILT, trkPoint[trkIdx].tilt);
}

/* 다음 유효 타겟으로 이동. 남은 타겟이 없으면 0 */
static uint8_t trkNextTarget(void)
{
    trkIdx++;

    while (trkIdx < TRK_TARGET_MAX && trkPoint[trkIdx].valid == 0)
        trkIdx++;

    if (trkIdx >= TRK_TARGET_MAX)
        return 0;

    trkAimBackoff();

    return 1;
}

/* 스캔 종료 후 재방문으로 자동 전환 */
static void trkEnterRevisit(void)
{
    laserFire();

    trkIdx = 0;

    while (trkIdx < TRK_TARGET_MAX && trkPoint[trkIdx].valid == 0)
        trkIdx++;

    if (trkIdx >= TRK_TARGET_MAX)
    {
        trkFinish();            /* 하나도 못 찾음 */
        return;
    }

    trkAimBackoff();

    trkState = TRK_RV_BACKOFF;
}

/* --- 공개 함수 --- */

void trackingStart(void)
{
    trkCalcRange();

    trkFound = 0;
    trkIdx   = 0;

    for (uint8_t i = 0; i < TRK_TARGET_MAX; i++)
        trkPoint[i].valid = 0;

    trkPan  = trkPanMin;
    trkTilt = trkTiltMin;
    trkDir  = 1;

    servoSetTarget(SERVO_PAN,  trkPan);
    servoSetTarget(SERVO_TILT, trkTilt);

    laserFire();

    trkState = TRK_SCAN_MOVE;
}

void trackingAbort(void)
{
    laserFire();

    trkState = TRK_IDLE;
}

void trackingUpdate(void)
{
    uint32_t tNow = HAL_GetTick();

    switch (trkState)
    {
        case TRK_SCAN_MOVE:
            if (servoIsAtTarget(SERVO_PAN) && servoIsAtTarget(SERVO_TILT))
            {
                trkTick  = tNow;
                trkState = TRK_SCAN_SETTLE;
            }
            break;

        case TRK_SCAN_SETTLE:
            if (tNow - trkTick >= TRK_SETTLE_MS)
                trkState = TRK_SCAN_MEASURE;
            break;

        case TRK_SCAN_MEASURE:
            /* 조도센서 판정 — 팀원 모듈 확정 후 채운다
            if (sensorHit() && trkFound < TRK_TARGET_MAX)
            {
                trkPoint[trkFound].pan   = trkPan;
                trkPoint[trkFound].tilt  = trkTilt;
                trkPoint[trkFound].valid = 1;
                trkFound++;
            }
            */

            if (trkNextPoint() == 0)
                trkEnterRevisit();
            else
                trkState = TRK_SCAN_MOVE;
            break;

        case TRK_RV_BACKOFF:
            if (servoIsAtTarget(SERVO_PAN) && servoIsAtTarget(SERVO_TILT))
            {
                /* 항상 각도가 증가하는 방향으로만 목표에 접근 */
                servoSetTarget(SERVO_PAN, trkPoint[trkIdx].pan);

                trkState = TRK_RV_APPROACH;
            }
            break;

        case TRK_RV_APPROACH:
            if (servoIsAtTarget(SERVO_PAN))
            {
                trkTick  = tNow;
                trkState = TRK_RV_SETTLE;
            }
            break;

        case TRK_RV_SETTLE:
            if (tNow - trkTick >= TRK_RV_SETTLE_MS)
            {
                laserFire();

                trkTick  = tNow;
                trkState = TRK_RV_FIRE;
            }
            break;

        case TRK_RV_FIRE:
            if (tNow - trkTick >= TRK_FIRE_WAIT_MS)
            {
                /* 득점 판정은 조도센서 모듈에서 처리 */

                if (trkNextTarget() == 0)
                    trkFinish();
                else
                    trkState = TRK_RV_BACKOFF;
            }
            break;

        default:
            break;
    }
}

TrkState_t trackingGetState(void)
{
    return trkState;
}

uint8_t trackingGetFoundCount(void)
{
    return trkFound;
}