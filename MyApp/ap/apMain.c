#include "apMain.h"
#include "myServo.h"
#include "myJoystick.h"
#include "myLaser.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include "myPiezo.h"
#define AIM_SPEED_DEG   1.5f    /* 50ms당 최대 이동 각도 */
static float panAngle  = 90.0f;
static float tiltAngle = 90.0f;

static float aimClamp(float deg)
{
    if (deg < 0.0f)                 deg = 0.0f;
    if (deg > (float)SERVO_MAX_DEG) deg = (float)SERVO_MAX_DEG;

    return deg;
}

static uint8_t btnPrev = 1;   /* 풀업이라 평소 High */

static uint8_t btnFirePressed(void)
{
    uint8_t btnNow = (uint8_t)HAL_GPIO_ReadPin(BTN_FIRE_GPIO_Port, BTN_FIRE_Pin);
    uint8_t edge   = 0;

    if (btnPrev == 1 && btnNow == 0)   /* High -> Low 하강 엣지 */
        edge = 1;

    btnPrev = btnNow;

    return edge;
}

void apInit(void)
{
    servoInit();
    joystickInit();
    laserInit();
}

void apMain(void)
{
    startRhythm();
    uint32_t tPrev20   = 0;
    uint32_t tPrev50   = 0;
    uint32_t tPrev100  = 0;
    uint32_t tPrev250  = 0;
    uint32_t tPrev1000 = 0;
    //테스트용 코드 (pan 각도 조절)
    // servoSetTarget(SERVO_PAN,  90.0);
    while (1)
    {
        // uint32_t tNow = HAL_GetTick();
        // if (tNow - tPrev20 >= 20)
        // {
        //     tPrev20 = tNow;

        //     laserUpdate();
        // }
        // /* 50ms : 조이스틱 읽기 -> 서보 각도 갱신 */
        // if (tNow - tPrev50 >= 50)
        // {
        //     tPrev50 = tNow;

        //     joystickUpdate();

        //     panAngle  += joystickGetRatio(JOY_AXIS_X) * AIM_SPEED_DEG;
        //     tiltAngle -= joystickGetRatio(JOY_AXIS_Y) * AIM_SPEED_DEG;

        //     panAngle  = aimClamp(panAngle);
        //     tiltAngle = aimClamp(tiltAngle);

        //     servoSetTarget(SERVO_PAN,  panAngle);
        //     servoSetTarget(SERVO_TILT, tiltAngle);

        //     servoUpdate();
        //     if (btnFirePressed())
        //         laserFire();
        // }

        // /* 100ms : 버튼 입력 처리 (시작 스위치 / 발사 버튼) */
        // if (tNow - tPrev100 >= 100)
        // {
        //     tPrev100 = tNow;

        //     /* buttonUpdate();  */
        //     /* laserUpdate();   */
        // }

        // /* 250ms : 디스플레이 갱신 */
        // if (tNow - tPrev250 >= 250)
        // {
        //     tPrev250 = tNow;

        //     /* displayUpdate(); */
        // }

        // /* 1000ms : 상태 확인용 하트비트 */
        // if (tNow - tPrev1000 >= 1000)
        // {
        //     tPrev1000 = tNow;

        //     //HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        // }
        
    }
}