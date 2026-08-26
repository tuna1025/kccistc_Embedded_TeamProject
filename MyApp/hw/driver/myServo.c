#include "myServo.h"
#include "tim.h"

static float servoAngle[2] = {90.0f, 90.0f};
//가동범위 제한
static uint16_t servoDegToUs(float deg)
{
    if (deg < 0.0f)                deg = 0.0f;
    if (deg > (float)SERVO_MAX_DEG) deg = (float)SERVO_MAX_DEG;

    return (uint16_t)(SERVO_MIN_US +
           (deg * (SERVO_MAX_US - SERVO_MIN_US)) / SERVO_MAX_DEG);
}

void servoInit(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    //시작 각도 0, 90
    servoSetTarget(SERVO_PAN,  90.0f);
    servoSetTarget(SERVO_TILT, 90.0f);
}
//서보모터 설정한 각도로 이동
void servoSetAngle(uint8_t ch, float deg)
{
    uint16_t us = servoDegToUs(deg);

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
static float servoTarget[2] = {90.0f, 90.0f};
//급발진을 막기 위해 목표 각도 설정
void servoSetTarget(uint8_t ch, float deg)
{
    if (deg < 0.0f)                 deg = 0.0f;
    if (deg > (float)SERVO_MAX_DEG) deg = (float)SERVO_MAX_DEG;

    servoTarget[ch] = deg;
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