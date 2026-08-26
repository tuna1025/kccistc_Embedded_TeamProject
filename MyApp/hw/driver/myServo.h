/* myServo.h */
#ifndef __MY_SERVO_H
#define __MY_SERVO_H

#include "main.h"

#define SERVO_PAN    0
#define SERVO_TILT   1
#define SERVO_STEP_DEG   2.0f   /* 1회 갱신당 최대 이동 각도 */

#define SERVO_MIN_US   500
#define SERVO_MAX_US   2500
#define SERVO_MAX_DEG  180

void  servoInit(void);
void  servoSetAngle(uint8_t ch, float deg);
float servoGetAngle(uint8_t ch);

void servoSetTarget(uint8_t ch, float deg);
void servoUpdate(void);
#endif