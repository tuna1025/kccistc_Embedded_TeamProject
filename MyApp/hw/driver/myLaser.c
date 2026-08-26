/* myLaser.c */
#include "myLaser.h"
#include "stm32f4xx_hal_gpio.h"

typedef enum {
    LASER_IDLE = 0,
    LASER_ON
} LaserState_t;

static LaserState_t laserState = LASER_IDLE;

void laserInit(void)
{
    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);

    laserState = LASER_IDLE;
}
//Laser GPIO port Write
uint8_t laserFire(void)
{
    if (laserState == LASER_IDLE)
    {
        HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_SET);
        laserState = LASER_ON;
    }
    else
    {
        HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);
        laserState = LASER_IDLE;
    }  
    return 1;
}
//LaserState에 따라 Pin 상태 갱신 -> 토글 방식은 필요 없음
void laserUpdate(void)
{
    //uint32_t tNow = HAL_GetTick();

    switch (laserState)
    {
        case LASER_ON:
                HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);
                //laserTick  = tNow;
            break;

        default:
            break;
    }
}

uint8_t laserIsOn(void)
{
    return (laserState == LASER_ON) ? 1 : 0;
}