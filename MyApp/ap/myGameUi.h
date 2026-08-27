#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @brief LCD1602와 SSD1306 OLED를 초기화한다. */
void gameUiInit(void);

/** @brief 메인 모드 선택 메뉴를 표시한다. @param selected 선택 항목(0: 일반, 1: 스캐닝, 2: 랭킹) */
void gameUiRenderMainMenu(uint8_t selected);

/** @brief 아직 구현되지 않은 스캐닝 모드 안내 화면을 표시한다. */
void gameUiRenderScanning(void);

/**
 * @brief 랭킹 목록에서 플레이어 한 명의 UID와 최고점을 표시한다.
 * @param uid 플레이어 UID 바이트 배열
 * @param uidSize UID 길이(4, 7 또는 10바이트)
 * @param bestScore 플레이어 최고점
 * @param rank 현재 순위(1부터 시작)
 * @param playerCount 저장된 전체 플레이어 수
 */
void gameUiRenderRanking(const uint8_t *uid, uint8_t uidSize,
                         uint16_t bestScore, uint8_t rank,
                         uint8_t playerCount);

/** @brief RFID 태그 대기 화면을 표시한다. @param showPrompt 깜빡이는 안내 문구 표시 여부 */
void gameUiRenderTagWait(bool showPrompt);

/**
 * @brief 태그가 인식된 플레이어의 UID, 최고점, 순위를 표시한다.
 * @param uid 플레이어 UID 바이트 배열
 * @param uidSize UID 길이
 * @param bestScore 저장된 최고점
 * @param rank 현재 순위
 */
void gameUiRenderPlayer(const uint8_t *uid, uint8_t uidSize,
                        uint16_t bestScore, uint8_t rank);

/** @brief 기본 게임 대기 화면을 표시한다. @param showPrompt 시작 안내 문구 표시 여부 */
void gameUiRenderWait(bool showPrompt);

/** @brief 대기 화면의 OLED 시작 안내 문구만 갱신한다. @param showPrompt 안내 문구 표시 여부 */
void gameUiRenderWaitPrompt(bool showPrompt);

/** @brief 게임 시작 전 카운트다운 숫자를 OLED에 표시한다. @param value 표시할 숫자 */
void gameUiRenderCountdown(uint8_t value);

/**
 * @brief 게임 진행 중 타겟, 점수, 남은 시간과 상태를 표시한다.
 * @param target 현재 타겟 번호
 * @param score 현재 점수
 * @param remainingSeconds 남은 시간(초)
 * @param status LCD에 표시할 상태 문자열
 */
void gameUiRenderPlaying(uint8_t target, uint16_t score,
                         uint32_t remainingSeconds, const char *status);

/** @brief 명중 애니메이션과 갱신된 점수를 표시한다. */
void gameUiRenderHit(uint8_t target, uint16_t score);

/** @brief 게임 종료 점수, 최고점 및 순위를 표시한다. */
void gameUiRenderGameOver(uint16_t score, uint16_t bestScore, uint8_t rank);

/**
 * @brief 게임 종료 후 다시하기/메인 메뉴 선택 화면을 표시한다.
 * @param selected 선택 항목(0: 다시하기, 1: 메인 메뉴)
 */
void gameUiRenderGameOverMenu(uint16_t score, uint16_t bestScore,
                              uint8_t rank, uint8_t selected);

/** @brief 시험용으로 RFID UID를 LCD에 표시한다. */
void gameUiRenderRfidUid(const uint8_t *uid, uint8_t size);

/** @brief RC522 초기화 또는 통신 실패 메시지를 표시한다. */
void gameUiRenderRfidError(void);

/** @brief 등록 가능한 최대 플레이어 수를 초과했다는 메시지를 표시한다. */
void gameUiRenderRankingFull(void);
