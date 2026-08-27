#include "myRanking.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>
#include <string.h>

#define RANKING_FLASH_ADDRESS 0x08060000UL
#define RANKING_FLASH_SECTOR  FLASH_SECTOR_7
#define RANKING_MAGIC         0x52414E4BUL
#define RANKING_VERSION       1U
#define RANKING_NO_PLAYER     0xFFU

typedef struct
{
    uint8_t uid[RANKING_UID_MAX_SIZE];
    uint8_t uidSize;
    uint8_t reserved;
    uint16_t bestScore;
} rankingRecord_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    rankingRecord_t players[RANKING_MAX_PLAYERS];
    uint32_t checksum;
} rankingStorage_t;

_Static_assert((sizeof(rankingStorage_t) % 4U) == 0U,
               "ranking storage must be word aligned");

static rankingStorage_t s_storage;
static uint8_t s_currentPlayer = RANKING_NO_PLAYER;

static uint32_t rankingChecksum(const rankingStorage_t *storage)
{
    const uint8_t *data = (const uint8_t *)storage;
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < offsetof(rankingStorage_t, checksum); i++)
    {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

static bool rankingSave(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sectorError = 0;
    HAL_StatusTypeDef status;

    s_storage.checksum = rankingChecksum(&s_storage);
    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = RANKING_FLASH_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    status = HAL_FLASHEx_Erase(&erase, &sectorError);

    if (status == HAL_OK)
    {
        const uint32_t *words = (const uint32_t *)&s_storage;
        for (size_t i = 0; i < sizeof(s_storage) / sizeof(uint32_t); i++)
        {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                       RANKING_FLASH_ADDRESS + i * 4U,
                                       words[i]);
            if (status != HAL_OK)
                break;
        }
    }

    HAL_FLASH_Lock();
    if (status != HAL_OK)
        return false;

    return memcmp((const void *)RANKING_FLASH_ADDRESS,
                  &s_storage, sizeof(s_storage)) == 0;
}

void rankingInit(void)
{
    const rankingStorage_t *flash =
        (const rankingStorage_t *)RANKING_FLASH_ADDRESS;

    if (flash->magic == RANKING_MAGIC &&
        flash->version == RANKING_VERSION &&
        flash->count <= RANKING_MAX_PLAYERS &&
        flash->checksum == rankingChecksum(flash))
    {
        memcpy(&s_storage, flash, sizeof(s_storage));
    }
    else
    {
        memset(&s_storage, 0, sizeof(s_storage));
        s_storage.magic = RANKING_MAGIC;
        s_storage.version = RANKING_VERSION;
    }
    s_currentPlayer = RANKING_NO_PLAYER;
}

bool rankingSelectPlayer(const uint8_t *uid, uint8_t uidSize)
{
    if (uid == NULL || uidSize == 0U || uidSize > RANKING_UID_MAX_SIZE)
        return false;

    for (uint8_t i = 0; i < s_storage.count; i++)
    {
        rankingRecord_t *player = &s_storage.players[i];
        if (player->uidSize == uidSize &&
            memcmp(player->uid, uid, uidSize) == 0)
        {
            s_currentPlayer = i;
            return true;
        }
    }

    if (s_storage.count >= RANKING_MAX_PLAYERS)
        return false;

    rankingRecord_t *player = &s_storage.players[s_storage.count];
    memset(player, 0, sizeof(*player));
    memcpy(player->uid, uid, uidSize);
    player->uidSize = uidSize;
    s_currentPlayer = (uint8_t)s_storage.count;
    s_storage.count++;
    if (!rankingSave())
    {
        s_storage.count--;
        memset(player, 0, sizeof(*player));
        s_currentPlayer = RANKING_NO_PLAYER;
        return false;
    }
    return true;
}

uint16_t rankingGetCurrentBest(void)
{
    if (s_currentPlayer == RANKING_NO_PLAYER)
        return 0;
    return s_storage.players[s_currentPlayer].bestScore;
}

uint8_t rankingGetCurrentRank(void)
{
    if (s_currentPlayer == RANKING_NO_PLAYER)
        return 0;

    uint16_t score = s_storage.players[s_currentPlayer].bestScore;
    uint8_t rank = 1;
    for (uint8_t i = 0; i < s_storage.count; i++)
    {
        if (s_storage.players[i].bestScore > score)
            rank++;
    }
    return rank;
}

bool rankingSubmitScore(uint16_t score)
{
    if (s_currentPlayer == RANKING_NO_PLAYER)
        return false;

    rankingRecord_t *player = &s_storage.players[s_currentPlayer];
    if (score <= player->bestScore)
        return true;

    player->bestScore = score;
    return rankingSave();
}

uint8_t rankingGetPlayerCount(void)
{
    return (uint8_t)s_storage.count;
}

bool rankingGetEntryByRank(uint8_t rankIndex, rankingEntry_t *entry)
{
    if (entry == NULL || rankIndex >= s_storage.count)
        return false;

    uint8_t order[RANKING_MAX_PLAYERS];
    for (uint8_t i = 0; i < s_storage.count; i++)
        order[i] = i;

    for (uint8_t i = 0; i < s_storage.count; i++)
    {
        for (uint8_t j = (uint8_t)(i + 1U); j < s_storage.count; j++)
        {
            if (s_storage.players[order[j]].bestScore >
                s_storage.players[order[i]].bestScore)
            {
                uint8_t temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }

    const rankingRecord_t *player = &s_storage.players[order[rankIndex]];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->uid, player->uid, player->uidSize);
    entry->uidSize = player->uidSize;
    entry->bestScore = player->bestScore;
    entry->rank = (uint8_t)(rankIndex + 1U);
    return true;
}
