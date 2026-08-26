/* myJoystick.h */
#ifndef __MY_JOYSTICK_H
#define __MY_JOYSTICK_H

#include "main.h"
#define JOY_AXIS_X     0
#define JOY_AXIS_Y     1

#define JOY_DEADZONE   150      /* ADC LSB 기준 중립 무시 범위 */
#define JOY_FULLSCALE  2048

void  joystickInit(void);
void  joystickUpdate(void);
float joystickGetRatio(uint8_t axis);   /* -1.0 ~ +1.0 */

#endif