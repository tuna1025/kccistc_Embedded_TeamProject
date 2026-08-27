#include "myGame.h"
#include "myCds.h"
#include "myGameUi.h"
#include "myJoystick.h"
#include "myLaser.h"
#include "myRc522.h"
#include "myRanking.h"
#include "myServo.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define AIM_SPEED_DEG                      1.5f
#define GAME_DURATION_MS                   30000U
#define GAME_COUNTDOWN_MS                  3000U
#define HIT_ANIMATION_MS                   500U
#define WAIT_BLINK_MS                      500U
#define GAME_TARGET_COUNT                  3U
#define GAME_SIMULATE_HIT_WITH_FIRE_BUTTON 1

typedef enum
{
    GAME_STATE_MAIN_MENU = 0,
    GAME_STATE_TAG_WAIT,
    GAME_STATE_PLAYER_READY,
    GAME_STATE_SCANNING,
    GAME_STATE_RANKING,
    GAME_STATE_COUNTDOWN,
    GAME_STATE_PLAYING,
    GAME_STATE_HIT,
    GAME_STATE_OVER
} gameState_t;

static float s_panAngle = 90.0f;
static float s_tiltAngle = 120.0f;
static gameState_t s_state = GAME_STATE_MAIN_MENU;
static uint32_t s_stateTick = 0;
static uint32_t s_gameStartTick = 0;
static uint32_t s_displayedSeconds = 0;
static uint32_t s_tick50 = 0;
static uint32_t s_tickRfid = 0;
static uint8_t s_countdownValue = 3;
static uint8_t s_targetNumber = 1;
static uint16_t s_score = 0;
static bool s_waitPromptVisible = true;
static bool s_rc522Ready = false;
static uint8_t s_btnPrev = 1;
static uint8_t s_playerUid[RANKING_UID_MAX_SIZE];
static uint8_t s_playerUidSize = 0;
static uint8_t s_mainMenuIndex = 0;
static uint8_t s_gameOverMenuIndex = 0;
static uint8_t s_rankingIndex = 0;
static bool s_menuAxisLatched = false;

static bool gameIsActive(void)
{
    return s_state == GAME_STATE_PLAYING || s_state == GAME_STATE_HIT;
}

static uint8_t fireButtonPressed(void)
{
    uint8_t now =
        (uint8_t)HAL_GPIO_ReadPin(BTN_FIRE_GPIO_Port, BTN_FIRE_Pin);
    uint8_t pressed = (s_btnPrev == 1U && now == 0U) ? 1U : 0U;
    s_btnPrev = now;
    return pressed;
}

static int8_t menuDirection(void)
{
    float value = joystickGetRatio(JOY_AXIS_Y);
    if (value > -0.30f && value < 0.30f)
    {
        s_menuAxisLatched = false;
        return 0;
    }
    if (s_menuAxisLatched)
        return 0;

    if (value > 0.65f)
    {
        s_menuAxisLatched = true;
        return -1;
    }
    if (value < -0.65f)
    {
        s_menuAxisLatched = true;
        return 1;
    }
    return 0;
}

static void renderRanking(void)
{
    rankingEntry_t entry;
    uint8_t count = rankingGetPlayerCount();
    if (count == 0U || !rankingGetEntryByRank(s_rankingIndex, &entry))
    {
        gameUiRenderRanking(NULL, 0, 0, 0, 0);
        return;
    }
    gameUiRenderRanking(entry.uid, entry.uidSize, entry.bestScore,
                        entry.rank, count);
}

static void enterMainMenu(void)
{
    s_state = GAME_STATE_MAIN_MENU;
    s_mainMenuIndex = 0;
    s_menuAxisLatched = false;
    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);
    gameUiRenderMainMenu(s_mainMenuIndex);
}

static uint32_t remainingSeconds(uint32_t now)
{
    uint32_t elapsed = now - s_gameStartTick;
    if (elapsed >= GAME_DURATION_MS)
        return 0;
    return (GAME_DURATION_MS - elapsed + 999U) / 1000U;
}

static void enterTagWait(uint32_t now)
{
    s_state = GAME_STATE_TAG_WAIT;
    s_stateTick = now;
    s_score = 0;
    s_targetNumber = 1;
    s_waitPromptVisible = true;
    s_playerUidSize = 0;
    gameUiRenderTagWait(true);
}

static void startCountdown(uint32_t now)
{
    s_state = GAME_STATE_COUNTDOWN;
    s_stateTick = now;
    s_score = 0;
    s_targetNumber = 1;
    s_countdownValue = 3;
    gameUiRenderCountdown(s_countdownValue);
}

static void enterPlaying(uint32_t now)
{
    s_state = GAME_STATE_PLAYING;
    s_gameStartTick = now;
    s_displayedSeconds = remainingSeconds(now);
    gameUiRenderPlaying(s_targetNumber, s_score, s_displayedSeconds,
                        "AIMING");
}

static void enterGameOver(uint32_t now)
{
    s_state = GAME_STATE_OVER;
    s_stateTick = now;
    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);
    (void)rankingSubmitScore(s_score);
    s_gameOverMenuIndex = 0;
    s_menuAxisLatched = false;
    gameUiRenderGameOverMenu(s_score, rankingGetCurrentBest(),
                             rankingGetCurrentRank(), s_gameOverMenuIndex);
}

void gameHitDetected(void)
{
    if (s_state != GAME_STATE_PLAYING)
        return;

    s_score++;
    s_state = GAME_STATE_HIT;
    s_stateTick = HAL_GetTick();
    gameUiRenderHit(s_targetNumber, s_score);
}

static void handleButton(uint32_t now)
{
    if (s_state == GAME_STATE_MAIN_MENU)
    {
        if (s_mainMenuIndex == 0U)
            enterTagWait(now);
        else if (s_mainMenuIndex == 1U)
        {
            s_state = GAME_STATE_SCANNING;
            gameUiRenderScanning();
        }
        else
        {
            s_state = GAME_STATE_RANKING;
            s_rankingIndex = 0;
            renderRanking();
        }
        return;
    }

    if (s_state == GAME_STATE_PLAYER_READY)
    {
        startCountdown(now);
        return;
    }

    if (s_state == GAME_STATE_OVER)
    {
        if (s_gameOverMenuIndex == 0U)
            startCountdown(now);
        else
            enterMainMenu();
        return;
    }

    if (s_state == GAME_STATE_SCANNING || s_state == GAME_STATE_RANKING)
    {
        enterMainMenu();
        return;
    }

    if (s_state == GAME_STATE_PLAYING && laserFire())
    {
#if GAME_SIMULATE_HIT_WITH_FIRE_BUTTON
        gameHitDetected();
#endif
    }
}

static void updateState(uint32_t now)
{
    if (gameIsActive() && now - s_gameStartTick >= GAME_DURATION_MS)
    {
        enterGameOver(now);
        return;
    }

    switch (s_state)
    {
        case GAME_STATE_MAIN_MENU:
        case GAME_STATE_SCANNING:
        case GAME_STATE_RANKING:
            break;

        case GAME_STATE_TAG_WAIT:
            if (now - s_stateTick >= WAIT_BLINK_MS)
            {
                s_stateTick = now;
                s_waitPromptVisible = !s_waitPromptVisible;
                gameUiRenderTagWait(s_waitPromptVisible);
            }
            break;

        case GAME_STATE_PLAYER_READY:
            break;

        case GAME_STATE_COUNTDOWN:
        {
            uint32_t elapsed = now - s_stateTick;
            if (elapsed >= GAME_COUNTDOWN_MS)
            {
                enterPlaying(now);
                break;
            }

            uint8_t value = (uint8_t)(3U - elapsed / 1000U);
            if (value != s_countdownValue)
            {
                s_countdownValue = value;
                gameUiRenderCountdown(value);
            }
            break;
        }

        case GAME_STATE_PLAYING:
        {
            uint32_t seconds = remainingSeconds(now);
            if (seconds != s_displayedSeconds)
            {
                s_displayedSeconds = seconds;
                gameUiRenderPlaying(s_targetNumber, s_score, seconds,
                                    "AIMING");
            }
            break;
        }

        case GAME_STATE_HIT:
            if (now - s_stateTick >= HIT_ANIMATION_MS)
            {
                s_targetNumber =
                    (uint8_t)((s_targetNumber % GAME_TARGET_COUNT) + 1U);
                s_state = GAME_STATE_PLAYING;
                s_displayedSeconds = remainingSeconds(now);
                gameUiRenderPlaying(s_targetNumber, s_score,
                                    s_displayedSeconds, "AIMING");
            }
            break;

        case GAME_STATE_OVER:
        default:
            break;
    }
}

void gameInit(void)
{
    servoInit();
    joystickInit();
    laserInit();
    cdsInit();
    gameUiInit();
    rankingInit();

    servoSetTarget(SERVO_PAN, s_panAngle);
    servoSetTarget(SERVO_TILT, s_tiltAngle);
    servoUpdate();

    enterMainMenu();
    s_rc522Ready = rc522Init();
    if (!s_rc522Ready)
        gameUiRenderRfidError();
}

void gameUpdate(void)
{
    uint32_t now = HAL_GetTick();
    cdsUpdate();

    if (s_rc522Ready && now - s_tickRfid >= 200U)
    {
        rc522Uid_t uid;
        s_tickRfid = now;
        if (s_state == GAME_STATE_TAG_WAIT && rc522ReadUid(&uid))
        {
            if (rankingSelectPlayer(uid.bytes, uid.size))
            {
                memcpy(s_playerUid, uid.bytes, uid.size);
                s_playerUidSize = uid.size;
                s_state = GAME_STATE_PLAYER_READY;
                gameUiRenderPlayer(s_playerUid, s_playerUidSize,
                                   rankingGetCurrentBest(),
                                   rankingGetCurrentRank());
            }
            else
            {
                gameUiRenderRankingFull();
            }
        }
    }

    if (now - s_tick50 >= 50U)
    {
        s_tick50 = now;
        joystickUpdate();

        int8_t direction = menuDirection();
        if (direction != 0)
        {
            if (s_state == GAME_STATE_MAIN_MENU)
            {
                s_mainMenuIndex = (uint8_t)((s_mainMenuIndex + direction + 3) % 3);
                gameUiRenderMainMenu(s_mainMenuIndex);
            }
            else if (s_state == GAME_STATE_RANKING)
            {
                uint8_t count = rankingGetPlayerCount();
                if (count > 0U)
                {
                    s_rankingIndex =
                        (uint8_t)((s_rankingIndex + direction + count) % count);
                    renderRanking();
                }
            }
            else if (s_state == GAME_STATE_OVER)
            {
                s_gameOverMenuIndex ^= 1U;
                gameUiRenderGameOverMenu(s_score, rankingGetCurrentBest(),
                                         rankingGetCurrentRank(),
                                         s_gameOverMenuIndex);
            }
        }

        if (fireButtonPressed())
            handleButton(now);

        if (gameIsActive())
        {
            s_panAngle += joystickGetRatio(JOY_AXIS_X) * AIM_SPEED_DEG;
            s_tiltAngle -= joystickGetRatio(JOY_AXIS_Y) * AIM_SPEED_DEG;
            s_panAngle = servoClampAngle(SERVO_PAN, s_panAngle);
            s_tiltAngle = servoClampAngle(SERVO_TILT, s_tiltAngle);
            servoSetTarget(SERVO_PAN, s_panAngle);
            servoSetTarget(SERVO_TILT, s_tiltAngle);
            servoUpdate();
        }
    }

    updateState(now);
}
