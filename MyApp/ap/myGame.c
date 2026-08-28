#include "myGame.h"
#include "myCds.h"
#include "myGameUi.h"
#include "myJoystick.h"
#include "myLaser.h"
#include "myRc522.h"
#include "myRanking.h"
#include "myServo.h"
#include "myTracking.h"
#include "usart.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define AIM_SPEED_DEG                      1.5f
#define GAME_DURATION_MS                   60000U
#define GAME_COUNTDOWN_MS                  3000U
#define HIT_ANIMATION_MS                   500U
#define WAIT_BLINK_MS                      500U
#define GAME_TARGET_COUNT                  3U
#define CDS_HIT_CHECK_MS                   20U
#define CDS_DEBUG_PRINT_MS                 500U
#define MAIN_MENU_COUNT                    5U

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
static uint32_t s_tickCdsHit = 0;
static uint32_t s_tickCdsDebug = 0;
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
static bool s_guestMode = false;
static bool s_testMode = false;
static bool s_autoMode = false;

static bool gameIsActive(void)
{
    return s_state == GAME_STATE_PLAYING || s_state == GAME_STATE_HIT;
}

/*
 * CDS 드라이버의 내부 상태는 건드리지 않고 현재 켜진 타겟 LED로
 * 활성 센서를 알아낸다. 레이저가 켜져 있고 해당 CDS 값이 임계값보다
 * 낮아졌을 때만 명중으로 판정한다.
 */
static bool gameIsCdsHit(void)
{
    uint32_t channel;

    if (!laserIsOn())
        return false;

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7) == GPIO_PIN_SET)
        channel = ADC_CHANNEL_4;
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET)
        channel = ADC_CHANNEL_11;
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET)
        channel = ADC_CHANNEL_10;
    else
        return false;

    return readADC(channel) < LIGHT_THRESHOLD;
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
        return 1;
    }
    if (value < -0.65f)
    {
        s_menuAxisLatched = true;
        return -1;
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
    s_guestMode = false;
    s_testMode = false;
    s_autoMode = false;
    trackingAbort();
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
    s_guestMode = false;
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
    if (s_testMode)
        gameUiRenderTestPlaying(s_targetNumber, s_score);
    else
        gameUiRenderPlaying(s_targetNumber, s_score, s_displayedSeconds,
                            "AIMING");
}

static void enterGameOver(uint32_t now)
{
    s_state = GAME_STATE_OVER;
    s_stateTick = now;
    trackingAbort();
    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);
    s_gameOverMenuIndex = 0;
    s_menuAxisLatched = false;
    if (s_guestMode)
    {
        gameUiRenderGuestGameOverMenu(s_score, s_gameOverMenuIndex);
    }
    else
    {
        (void)rankingSubmitScore(s_score);
        gameUiRenderGameOverMenu(s_score, rankingGetCurrentBest(),
                                 rankingGetCurrentRank(),
                                 s_gameOverMenuIndex);
    }
}

void gameHitDetected(void)
{
    if (s_state != GAME_STATE_PLAYING)
        return;

    s_score++;
    if (s_autoMode)
        trackingAbort();          /* 현재 재방문을 끝내고 다음 타겟을 기다린다 */
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
            s_guestMode = true;
            s_state = GAME_STATE_PLAYER_READY;
            gameUiRenderGuest();
        }
        else if (s_mainMenuIndex == 2U)
        {
            s_guestMode = true;
            s_testMode = true;
            s_state = GAME_STATE_PLAYER_READY;
            gameUiRenderTest();
        }
        else if (s_mainMenuIndex == 3U)
        {
            s_state = GAME_STATE_SCANNING;
            gameUiRenderScanning();
            cdsScanBegin();
            trackingStart();
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

    if (s_state == GAME_STATE_SCANNING)
    {
        trackingAbort();
        cdsScanEnd();
        enterMainMenu();
        return;
    }

    if (s_state == GAME_STATE_RANKING)
    {
        enterMainMenu();
        return;
    }

    if (s_state == GAME_STATE_PLAYING && !s_autoMode && laserFire())
    {
        /* 버튼은 레이저만 ON/OFF하며 점수는 CDS가 실제 감지했을 때 올라간다. */
    }
}

static void updateState(uint32_t now)
{
    if (!s_testMode && gameIsActive() &&
        now - s_gameStartTick >= GAME_DURATION_MS)
    {
        enterGameOver(now);
        return;
    }

    switch (s_state)
    {
        case GAME_STATE_MAIN_MENU:
        case GAME_STATE_RANKING:
            break;

        case GAME_STATE_SCANNING:
            if (trackingGetState() == TRK_SCAN_DONE)
            {
                cdsScanEnd();
                if (trackingGetFoundCount() > 0U)
                {
                    /* 스캔 성공 -> 자동 조준으로 60초 게임 진행 */
                    s_autoMode = true;
                    s_guestMode = true;
                    startCountdown(now);
                }
                else
                {
                    enterMainMenu();
                }
            }
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
            if (s_testMode)
                break;
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
                if (s_testMode)
                    gameUiRenderTestPlaying(s_targetNumber, s_score);
                else
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
    bool cdsHit = false;

    if (now - s_tickCdsHit >= CDS_HIT_CHECK_MS)
    {
        s_tickCdsHit = now;
        if (s_state == GAME_STATE_PLAYING)
            cdsHit = gameIsCdsHit();

        /* 게임 판정 직후 같은 주기에서 드라이버의 LED/부저 상태를 갱신한다.
           스캐닝 중에는 LED 3개가 모두 켜져 있어야 하므로 건드리지 않는다. */
        if (s_state != GAME_STATE_SCANNING)
            cdsUpdate();
    }

    if (s_testMode && gameIsActive() &&
        now - s_tickCdsDebug >= CDS_DEBUG_PRINT_MS)
    {
        char debugLine[64];
        uint32_t cds1 = readADC(ADC_CHANNEL_4);
        uint32_t cds2 = readADC(ADC_CHANNEL_11);
        uint32_t cds3 = readADC(ADC_CHANNEL_10);
        int length = snprintf(debugLine, sizeof(debugLine),
                              "CDS1:%lu CDS2:%lu CDS3:%lu TH:%u\r\n",
                              (unsigned long)cds1, (unsigned long)cds2,
                              (unsigned long)cds3,
                              (unsigned int)LIGHT_THRESHOLD);
        s_tickCdsDebug = now;
        if (length > 0)
            HAL_UART_Transmit(&huart2, (uint8_t *)debugLine,
                              (uint16_t)length, 20U);
    }

    if (cdsHit)
    {
        /* 명중 후 레이저를 꺼 중복 득점을 방지한다. */
        if (laserIsOn())
            (void)laserFire();
        gameHitDetected();
    }

    if (s_state == GAME_STATE_SCANNING || s_autoMode)
        trackingUpdate();

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
                s_mainMenuIndex =
                    (uint8_t)((s_mainMenuIndex + direction +
                               MAIN_MENU_COUNT) % MAIN_MENU_COUNT);
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
                if (s_guestMode)
                    gameUiRenderGuestGameOverMenu(s_score,
                                                  s_gameOverMenuIndex);
                else
                    gameUiRenderGameOverMenu(s_score,
                                             rankingGetCurrentBest(),
                                             rankingGetCurrentRank(),
                                             s_gameOverMenuIndex);
            }
        }

        if (fireButtonPressed())
            handleButton(now);

        if (gameIsActive())
        {
            if (s_autoMode)
            {
                /* 켜진 LED의 저장 좌표로 재방문한다. 진행 중이면 건드리지 않는다. */
                if (s_state == GAME_STATE_PLAYING && !trackingIsBusy())
                {
                    int active = cdsGetActive();
                    if (active != CDS_NONE)
                        (void)trackingAimAt((uint8_t)active);
                }
            }
            else
            {
                s_panAngle += joystickGetRatio(JOY_AXIS_X) * AIM_SPEED_DEG;
                s_tiltAngle -= joystickGetRatio(JOY_AXIS_Y) * AIM_SPEED_DEG;
                s_panAngle = servoClampAngle(SERVO_PAN, s_panAngle);
                s_tiltAngle = servoClampAngle(SERVO_TILT, s_tiltAngle);
                servoSetTarget(SERVO_PAN, s_panAngle);
                servoSetTarget(SERVO_TILT, s_tiltAngle);
            }
            servoUpdate();
        }
        else if (s_state == GAME_STATE_SCANNING)
        {
            servoUpdate();
        }
    }

    updateState(now);
}
