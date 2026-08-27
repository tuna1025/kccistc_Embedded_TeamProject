#include "myServo.h"
#include "tim.h"
/* 기구 장착 방향에 맞춰 축별로 0(정방향) / 1(역방향) 지정 */
#define SERVO_PAN_INVERT    1
#define SERVO_TILT_INVERT   1
static float servoAngle[2] = {90.0f, 120.0f};
static const uint8_t servoInvert[2] = {SERVO_PAN_INVERT, SERVO_TILT_INVERT};
static const float servoMinDeg[2] = {SERVO_PAN_MIN_DEG, SERVO_TILT_MIN_DEG};
static const float servoMaxDeg[2] = {SERVO_PAN_MAX_DEG, SERVO_TILT_MAX_DEG};
static float servoTarget[2] = {90.0f, 120.0f};
//가동범위 제한 + 역방향 각도 전환
float servoClampAngle(uint8_t ch, float deg)
{
    if (deg < servoMinDeg[ch]) deg = servoMinDeg[ch];
    if (deg > servoMaxDeg[ch]) deg = servoMaxDeg[ch];

    return deg;
}
static uint16_t servoDegToUs(uint8_t ch, float deg)
{
    deg = servoClampAngle(ch, deg);

    if (servoInvert[ch] != 0)
        deg = (float)SERVO_MAX_DEG - deg;

    return (uint16_t)(SERVO_MIN_US +
           (deg * (SERVO_MAX_US - SERVO_MIN_US)) / SERVO_MAX_DEG);
}

void servoInit(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    servoSetAngle(SERVO_PAN,  90.0f);
    servoSetAngle(SERVO_TILT, 120.0f);

    servoSetTarget(SERVO_PAN,  90.0f);
    servoSetTarget(SERVO_TILT, 120.0f);
}
//서보모터 설정한 각도로 이동
void servoSetAngle(uint8_t ch, float deg)
{
    uint16_t us = servoDegToUs(ch,deg);

    if (ch == SERVO_PAN)
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, us);   //PWM을 통한 서보모터 제어
    else
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, us);

    servoAngle[ch] = deg;
}

float servoGetAngle(uint8_t ch)
{
    return servoAngle[ch];
}
//급발진을 막기 위해 목표 각도 설정
void servoSetTarget(uint8_t ch, float deg)
{
    servoTarget[ch] = servoClampAngle(ch, deg);
}
//목표 각도로 시행당 2도로 이동(SERVO_STEP_DEG만큼)
void servoUpdate(void)
{
    for (uint8_t ch = 0; ch < 2; ch++)
    {
        float diff = servoTarget[ch] - servoAngle[ch];

        if (diff > SERVO_STEP_DEG)
            servoSetAngle(ch, servoAngle[ch] + SERVO_STEP_DEG);
        else if (diff < -SERVO_STEP_DEG)
            servoSetAngle(ch, servoAngle[ch] - SERVO_STEP_DEG);
        else if (diff != 0.0f)
            servoSetAngle(ch, servoTarget[ch]);
    }
}
// servoUpdate()가 목표에 도달했는지 판정 -> Tracking에서 사용
uint8_t servoIsAtTarget(uint8_t ch)
{
    float diff = servoTarget[ch] - servoAngle[ch];

    if (diff < 0.0f)
        diff = -diff;

    return (diff < 0.01f) ? 1 : 0;
}