#pragma once

/** @brief 게임, UI, 입력장치, RFID 및 랭킹 모듈을 초기화한다. */
void gameInit(void);

/** @brief 입력 확인, 상태 전환, 화면 갱신을 한 번 수행한다. 메인 루프에서 반복 호출한다. */
void gameUpdate(void);

/** @brief 현재 타겟의 명중을 처리하고 점수를 1점 증가시킨다. */
void gameHitDetected(void);
