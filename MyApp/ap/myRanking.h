#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RANKING_MAX_PLAYERS 8U
#define RANKING_UID_MAX_SIZE 10U

/** @brief 랭킹 화면에 전달하는 플레이어 정보 구조체. */
typedef struct
{
    uint8_t uid[RANKING_UID_MAX_SIZE]; /**< RFID UID 데이터 */
    uint8_t uidSize;                  /**< UID 유효 길이 */
    uint16_t bestScore;               /**< 저장된 최고점 */
    uint8_t rank;                     /**< 점수순 순위(1부터 시작) */
} rankingEntry_t;

/** @brief Flash에서 랭킹 데이터를 검사하고 RAM으로 불러온다. */
void rankingInit(void);

/**
 * @brief UID에 해당하는 플레이어를 선택하며, 신규 UID이면 Flash에 등록한다.
 * @return 선택 또는 신규 등록 성공 시 true
 */
bool rankingSelectPlayer(const uint8_t *uid, uint8_t uidSize);

/** @return 현재 선택된 플레이어의 최고점. 선택된 플레이어가 없으면 0 */
uint16_t rankingGetCurrentBest(void);

/** @return 현재 선택된 플레이어의 순위. 선택된 플레이어가 없으면 0 */
uint8_t rankingGetCurrentRank(void);

/**
 * @brief 현재 플레이어의 최고점을 갱신하고 필요한 경우 Flash에 저장한다.
 * @return 기존 점수 이하이거나 저장 성공 시 true, 저장 실패 시 false
 */
bool rankingSubmitScore(uint16_t score);

/** @return Flash 랭킹에 등록된 플레이어 수 */
uint8_t rankingGetPlayerCount(void);

/**
 * @brief 점수순 위치에 해당하는 플레이어 정보를 가져온다.
 * @param rankIndex 0부터 시작하는 랭킹 목록 인덱스
 * @param entry 결과를 저장할 구조체
 * @return 유효한 항목을 가져오면 true
 */
bool rankingGetEntryByRank(uint8_t rankIndex, rankingEntry_t *entry);
