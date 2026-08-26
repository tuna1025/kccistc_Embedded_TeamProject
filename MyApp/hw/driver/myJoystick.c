/* myJoystick.c */
#include "myJoystick.h"
#include "adc.h"

static const uint32_t joyChannel[2] = {ADC_CHANNEL_0, ADC_CHANNEL_1};

static uint16_t joyCenter[2] = {2048, 2048};
static float    joyRatio[2]  = {0.0f, 0.0f};

static uint16_t joystickReadRaw(uint8_t axis)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t value = 0;

    sConfig.Channel      = joyChannel[axis];
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}

void joystickInit(void)
{
    uint32_t sum[2] = {0, 0};

    /* 부팅 시 손을 뗀 상태의 중립값을 실측해서 기준으로 삼는다 */
    for (uint8_t i = 0; i < 16; i++)
    {
        sum[JOY_AXIS_X] += joystickReadRaw(JOY_AXIS_X);
        sum[JOY_AXIS_Y] += joystickReadRaw(JOY_AXIS_Y);
        HAL_Delay(5);
    }

    joyCenter[JOY_AXIS_X] = (uint16_t)(sum[JOY_AXIS_X] / 16);
    joyCenter[JOY_AXIS_Y] = (uint16_t)(sum[JOY_AXIS_Y] / 16);
}

void joystickUpdate(void)
{
    for (uint8_t axis = 0; axis < 2; axis++)
    {
        int32_t diff = (int32_t)joystickReadRaw(axis) - (int32_t)joyCenter[axis];
        float   ratio;

        if (diff > -JOY_DEADZONE && diff < JOY_DEADZONE)
        {
            joyRatio[axis] = 0.0f;
            continue;
        }

        /* 데드존 경계에서 값이 튀지 않도록 데드존만큼 빼고 시작 */
        if (diff > 0) diff -= JOY_DEADZONE;
        else          diff += JOY_DEADZONE;

        ratio = (float)diff / (float)(JOY_FULLSCALE - JOY_DEADZONE);

        if (ratio >  1.0f) ratio =  1.0f;
        if (ratio < -1.0f) ratio = -1.0f;

        joyRatio[axis] = ratio;
    }
}

float joystickGetRatio(uint8_t axis)
{
    return joyRatio[axis];
}