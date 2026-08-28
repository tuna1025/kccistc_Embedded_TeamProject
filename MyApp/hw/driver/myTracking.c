/* myTracking.c */
#include "myTracking.h"
#include "myServo.h"
#include "myLaser.h"
#include "myCds.h"

static TrkState_t trkState = TRK_IDLE;
static TrkPoint_t trkPoint[TRK_TARGET_MAX];   /* 첨자 = CDS 센서 index */
static uint8_t    trkFound = 0;
static uint8_t    trkIdx   = 0;
static uint32_t   trkTick  = 0;

static float  trkPan  = 0.0f;
static float  trkTilt = 0.0f;
static int8_t trkDir  = 1;

static float trkPanMin,  trkPanMax;
static float trkTiltMin, trkTiltMax;

/* laserFire()는 토글이므로 원하는 상태가 아닐 때만 호출한다 */
static void trkLaser(uint8_t on)
{
    if (laserIsOn() != on)
        laserFire();
}

/* 스캔 범위를 서보 물리 가동범위와 교집합으로 계산 */
static void trkCalcRange(void)
{
    trkPanMin  = servoClampAngle(SERVO_PAN,  TRK_PAN_MIN_DEG);
    trkPanMax  = servoClampAngle(SERVO_PAN,  TRK_PAN_MAX_DEG);
    trkTiltMin = servoClampAngle(SERVO_TILT, TRK_TILT_MIN_DEG);
    trkTiltMax = servoClampAngle(SERVO_TILT, TRK_TILT_MAX_DEG);
}

/* 종료 처리 - 레이저 소등 경로를 한 곳으로 모은다 */
static void trkFinish(TrkState_t next)
{
    trkLaser(0);

    trkState = next;
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

/* 현재 조준점에서 감지된 타겟이 있으면 좌표를 저장한다 */
static void trkMeasure(void)
{
    int hit = cdsScanCheckHit();

    if (hit == CDS_NONE)
        return;

    trkPoint[hit].pan   = trkPan;
    trkPoint[hit].tilt  = trkTilt;
    trkPoint[hit].valid = 1;
    trkFound++;

    cdsScanLedOff(hit);      /* 찾은 타겟은 소등해서 중복 검출을 막는다 */
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

    trkLaser(1);             /* 스캔 중에는 계속 점등 */

    trkState = TRK_SCAN_MOVE;
}

void trackingAbort(void)
{
    trkLaser(0);

    trkState = TRK_IDLE;
}

uint8_t trackingAimAt(uint8_t idx)
{
    if (idx >= TRK_TARGET_MAX || trkPoint[idx].valid == 0)
        return 0;

    trkIdx = idx;

    /* 항상 같은 방향에서 접근하도록 목표보다 못 미친 지점으로 먼저 간다 */
    servoSetTarget(SERVO_PAN,  trkPoint[idx].pan - TRK_BACKOFF_DEG);
    servoSetTarget(SERVO_TILT, trkPoint[idx].tilt);

    trkState = TRK_RV_BACKOFF;

    return 1;
}

uint8_t trackingHasPoint(uint8_t idx)
{
    if (idx >= TRK_TARGET_MAX)
        return 0;

    return trkPoint[idx].valid;
}

uint8_t trackingIsBusy(void)
{
    return (trkState != TRK_IDLE &&
            trkState != TRK_DONE &&
            trkState != TRK_SCAN_DONE) ? 1 : 0;
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
            trkMeasure();

            if (trkFound >= TRK_TARGET_MAX)      /* 전부 찾으면 조기 종료 */
                trkFinish(TRK_SCAN_DONE);
            else if (trkNextPoint() == 0)
                trkFinish(TRK_SCAN_DONE);
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
                trkLaser(1);

                trkTick  = tNow;
                trkState = TRK_RV_FIRE;
            }
            break;

        case TRK_RV_FIRE:
            /* 명중 판정은 myGame이 하고, 여기서는 시간이 지나면 끝낸다 */
            if (tNow - trkTick >= TRK_FIRE_WAIT_MS)
                trkFinish(TRK_DONE);
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
