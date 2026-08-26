#include "myServo.h"
#include "tim.h"

static float servoAngle[2] = {90.0f, 90.0f};

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

    servoSetAngle(SERVO_PAN,  0.0f);
    servoSetAngle(SERVO_TILT, 0.0f);
}

void servoSetAngle(uint8_t ch, float deg)
{
    uint16_t us = servoDegToUs(deg);

    if (ch == SERVO_PAN)
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, us);
    else
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, us);

    servoAngle[ch] = deg;
}

float servoGetAngle(uint8_t ch)
{
    return servoAngle[ch];
}
static float servoTarget[2] = {90.0f, 90.0f};

void servoSetTarget(uint8_t ch, float deg)
{
    if (deg < 0.0f)                 deg = 0.0f;
    if (deg > (float)SERVO_MAX_DEG) deg = (float)SERVO_MAX_DEG;

    servoTarget[ch] = deg;
}

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