/* myLaser.h */
#ifndef __MY_LASER_H
#define __MY_LASER_H

#include "main.h"
#define LASER_Pin           GPIO_PIN_2
#define LASER_GPIO_Port     GPIOB

#define BTN_FIRE_Pin        GPIO_PIN_0
#define BTN_FIRE_GPIO_Port  GPIOB

#define LASER_FIRE_MS    300    /* 1회 발사 유지 시간 */
#define LASER_COOL_MS    500    /* 발사 후 재발사 금지 시간 */

void    laserInit(void);
uint8_t laserFire(void);        /* 발사 성공하면 1, 쿨다운 중이면 0 */
void    laserUpdate(void);
uint8_t laserIsOn(void);

#endif