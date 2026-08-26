/* myLaser.c */
#include "myLaser.h"

typedef enum {
    LASER_IDLE = 0,
    LASER_ON,
    LASER_COOL
} LaserState_t;

static LaserState_t laserState = LASER_IDLE;
static uint32_t     laserTick  = 0;

void laserInit(void)
{
    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);

    laserState = LASER_IDLE;
}

uint8_t laserFire(void)
{
    if (laserState != LASER_IDLE)
        return 0;

    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_SET);

    laserState = LASER_ON;
    laserTick  = HAL_GetTick();

    return 1;
}

void laserUpdate(void)
{
    uint32_t tNow = HAL_GetTick();

    switch (laserState)
    {
        case LASER_ON:
            if (tNow - laserTick >= LASER_FIRE_MS)
            {
                HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);

                laserState = LASER_COOL;
                laserTick  = tNow;
            }
            break;

        case LASER_COOL:
            if (tNow - laserTick >= LASER_COOL_MS)
                laserState = LASER_IDLE;
            break;

        default:
            break;
    }
}

uint8_t laserIsOn(void)
{
    return (laserState == LASER_ON) ? 1 : 0;
}