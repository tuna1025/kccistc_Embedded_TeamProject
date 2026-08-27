#ifndef AP_MAIN_H
#define AP_MAIN_H

/** @brief 애플리케이션에서 사용하는 게임 및 하드웨어 모듈을 초기화한다. */
void apInit(void);

/** @brief 메인 반복문에서 게임 업데이트를 계속 실행한다. 반환하지 않는다. */
void apMain(void);

#endif /* AP_MAIN_H */
