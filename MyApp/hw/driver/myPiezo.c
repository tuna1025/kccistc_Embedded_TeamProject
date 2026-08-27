#include "main.h"
#include "stm32_hal_legacy.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"
#include "myPiezo.h"

static void buzzerStart(){
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}
static void buzzerStop(){
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}
//도레마파솔라시도 구현 코드
static void buzzer_tone(uint32_t freq){
    uint32_t period;
    buzzerStop();
    period = 1000000UL / freq;
    __HAL_TIM_SetAutoreload(&htim2, period -1);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, period / 2);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    buzzerStart();
}
void startRhythm(){
    buzzer_tone(784);
    HAL_Delay(500);

    buzzerStop();
}