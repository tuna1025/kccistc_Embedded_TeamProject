/* myServo.h */
#ifndef __MY_SERVO_H
#define __MY_SERVO_H

#include "main.h"

#define SERVO_PAN    0
#define SERVO_TILT   1
#define SERVO_STEP_DEG   2.0f   /* 1회 갱신당 최대 이동 각도 */
/* 서보의 전체 기계적 범위 (펄스폭 환산 기준, 건드리지 말 것) */
#define SERVO_MAX_DEG      180

/* 기구 간섭을 피한 실제 허용 가동 범위 */
#define SERVO_PAN_MIN_DEG    0.0f
#define SERVO_PAN_MAX_DEG  180.0f
#define SERVO_TILT_MIN_DEG 100.0f
#define SERVO_TILT_MAX_DEG 180.0f

#define SERVO_MIN_US   500
#define SERVO_MAX_US   2500
#define SERVO_MAX_DEG  180

void  servoInit(void);
void  servoSetAngle(uint8_t ch, float deg);
float servoGetAngle(uint8_t ch);

void servoSetTarget(uint8_t ch, float deg);
void servoUpdate(void);
float servoClampAngle(uint8_t ch, float deg);
#endif