#include "apMain.h"
#include "myCds.h"
#include "myJoystick.h"
#include "myLaser.h"
#include "myLcd1602.h"
#include "myServo.h"
#include "mySsd1306.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AIM_SPEED_DEG                     1.5f
#define GAME_DURATION_MS                  30000U
#define GAME_COUNTDOWN_MS                 3000U
#define HIT_ANIMATION_MS                  500U
#define WAIT_BLINK_MS                     500U
#define GAME_TARGET_COUNT                 3U

/*
 * CDS 센서가 구현되기 전 UI 시험용 설정.
 * 1: 게임 중 발사 버튼을 누르면 임시로 명중 처리
 * 0: 발사만 하고 점수는 변경하지 않음
 */
#define GAME_SIMULATE_HIT_WITH_FIRE_BUTTON 1

typedef enum
{
    GAME_STATE_WAIT = 0,
    GAME_STATE_COUNTDOWN,
    GAME_STATE_PLAYING,
    GAME_STATE_HIT,
    GAME_STATE_OVER
} gameState_t;

static float panAngle  = 90.0f;
static float tiltAngle = 120.0f;

static bool lcdReady  = false;
static bool oledReady = false;

static gameState_t gameState = GAME_STATE_WAIT;
static uint32_t stateTick = 0;
static uint32_t gameStartTick = 0;
static uint32_t displayedSeconds = 0;
static uint8_t countdownValue = 3;
static uint8_t targetNumber = 1;
static uint16_t score = 0;
static bool waitPromptVisible = true;

static uint8_t btnPrev = 1;

static uint8_t btnFirePressed(void)
{
    uint8_t btnNow =
        (uint8_t)HAL_GPIO_ReadPin(BTN_FIRE_GPIO_Port, BTN_FIRE_Pin);
    uint8_t edge = 0;

    if (btnPrev == 1U && btnNow == 0U)
        edge = 1;

    btnPrev = btnNow;
    return edge;
}

static void lcdWriteLine(uint8_t row, const char *text)
{
    if (!lcdReady)
        return;

    char line[17];
    memset(line, ' ', 16);
    line[16] = '\0';

    if (text != NULL)
    {
        size_t length = strlen(text);
        if (length > 16U)
            length = 16U;
        memcpy(line, text, length);
    }

    lcd1602Cursor(row, 0);
    lcd1602Print(line);
}

static void oledDrawCentered(int16_t y, const char *text, uint8_t scale)
{
    if (text == NULL || scale == 0U)
        return;

    int16_t width = (int16_t)(strlen(text) * 6U * scale);
    int16_t x = (SSD1306_WIDTH - width) / 2;
    if (x < 0)
        x = 0;

    if (scale == 1U)
        ssd1306DrawString(x, y, text, SSD1306_COLOR_WHITE);
    else
        ssd1306DrawStringScaled(x, y, text, SSD1306_COLOR_WHITE, scale);
}

static void uiRenderWaitOled(bool showPrompt)
{
    if (!oledReady)
        return;

    ssd1306Clear();
    ssd1306DrawRect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT,
                    SSD1306_COLOR_WHITE);
    oledDrawCentered(8, "LASER", 2);
    oledDrawCentered(27, "TARGET", 2);

    if (showPrompt)
        oledDrawCentered(51, "PRESS START", 1);

    ssd1306Update();
}

static void uiRenderWait(void)
{
    lcdWriteLine(0, "TGT:-  SCORE:00");
    lcdWriteLine(1, "STATUS: READY");
    uiRenderWaitOled(true);
}

static void uiRenderCountdown(uint8_t value)
{
    char digitText[2] = {(char)('0' + value), '\0'};

    if (!oledReady)
        return;

    ssd1306Clear();
    oledDrawCentered(5, "GET READY", 1);
    oledDrawCentered(20, digitText, 4);
    ssd1306Update();
}

static void uiRenderPlayingLcd(const char *status)
{
    char line[24];

    snprintf(line, sizeof(line), "TGT:%u SCORE:%02u",
             targetNumber, (unsigned int)(score % 100U));
    lcdWriteLine(0, line);

    snprintf(line, sizeof(line), "STATUS: %s", status);
    lcdWriteLine(1, line);
}

static void uiRenderPlayingOled(uint32_t remainingSeconds)
{
    if (!oledReady)
        return;

    char timerText[16];
    uint32_t minutes = remainingSeconds / 60U;
    uint32_t seconds = remainingSeconds % 60U;
    uint32_t totalSeconds = GAME_DURATION_MS / 1000U;
    int16_t barWidth = (int16_t)((remainingSeconds * 104U) / totalSeconds);

    snprintf(timerText, sizeof(timerText), "%02lu:%02lu",
             (unsigned long)minutes, (unsigned long)seconds);

    ssd1306Clear();
    oledDrawCentered(3, "TIME LEFT", 1);
    oledDrawCentered(20, timerText, 2);
    ssd1306DrawRect(10, 49, 108, 10, SSD1306_COLOR_WHITE);
    if (barWidth > 0)
        ssd1306FillRect(12, 51, barWidth, 6, SSD1306_COLOR_WHITE);
    ssd1306Update();
}

static void uiRenderHit(void)
{
    uiRenderPlayingLcd("HIT");

    if (!oledReady)
        return;

    ssd1306Clear();
    ssd1306DrawRect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT,
                    SSD1306_COLOR_WHITE);
    ssd1306DrawRect(3, 3, SSD1306_WIDTH - 6, SSD1306_HEIGHT - 6,
                    SSD1306_COLOR_WHITE);
    oledDrawCentered(8, "HIT!", 3);
    oledDrawCentered(42, "+1", 2);
    ssd1306Update();
}

static void uiRenderGameOver(void)
{
    char line[24];

    lcdWriteLine(0, "  TIME IS UP!");
    snprintf(line, sizeof(line), "FINAL SCORE:%02u",
             (unsigned int)(score % 100U));
    lcdWriteLine(1, line);

    if (!oledReady)
        return;

    ssd1306Clear();
    oledDrawCentered(12, "TIME UP", 2);
    oledDrawCentered(45, "PRESS START", 1);
    ssd1306Update();
}

static uint32_t gameGetRemainingSeconds(uint32_t now)
{
    uint32_t elapsed = now - gameStartTick;

    if (elapsed >= GAME_DURATION_MS)
        return 0;

    return (GAME_DURATION_MS - elapsed + 999U) / 1000U;
}

static bool gameIsActive(void)
{
    return gameState == GAME_STATE_PLAYING || gameState == GAME_STATE_HIT;
}

static void gameEnterWait(uint32_t now)
{
    gameState = GAME_STATE_WAIT;
    stateTick = now;
    score = 0;
    targetNumber = 1;
    waitPromptVisible = true;
    uiRenderWait();
}

static void gameStartCountdown(uint32_t now)
{
    gameState = GAME_STATE_COUNTDOWN;
    stateTick = now;
    score = 0;
    targetNumber = 1;
    countdownValue = 3;
    uiRenderCountdown(countdownValue);
}

static void gameEnterPlaying(uint32_t now)
{
    gameState = GAME_STATE_PLAYING;
    gameStartTick = now;
    displayedSeconds = gameGetRemainingSeconds(now);
    uiRenderPlayingLcd("AIMING");
    uiRenderPlayingOled(displayedSeconds);
}

static void gameEnterOver(uint32_t now)
{
    gameState = GAME_STATE_OVER;
    stateTick = now;
    HAL_GPIO_WritePin(LASER_GPIO_Port, LASER_Pin, GPIO_PIN_RESET);
    uiRenderGameOver();
}

/*
 * 추후 CDS 드라이버에서 명중이 확정되면 이 함수를 호출하면 된다.
 */
void apGameHitDetected(void)
{
    if (gameState != GAME_STATE_PLAYING)
        return;

    score++;
    gameState = GAME_STATE_HIT;
    stateTick = HAL_GetTick();
    uiRenderHit();
}

static void gameHandleButton(uint32_t now)
{
    if (gameState == GAME_STATE_WAIT || gameState == GAME_STATE_OVER)
    {
        gameStartCountdown(now);
        return;
    }

    if (gameState == GAME_STATE_PLAYING && laserFire())
    {
#if GAME_SIMULATE_HIT_WITH_FIRE_BUTTON
        /* TODO: CDS 판정 구현 후 이 호출은 제거한다. */
        apGameHitDetected();
#endif
    }
}

static void gameUpdate(uint32_t now)
{
    if (gameIsActive() && (now - gameStartTick >= GAME_DURATION_MS))
    {
        gameEnterOver(now);
        return;
    }

    switch (gameState)
    {
        case GAME_STATE_WAIT:
            if (now - stateTick >= WAIT_BLINK_MS)
            {
                stateTick = now;
                waitPromptVisible = !waitPromptVisible;
                uiRenderWaitOled(waitPromptVisible);
            }
            break;

        case GAME_STATE_COUNTDOWN:
        {
            uint32_t elapsed = now - stateTick;

            if (elapsed >= GAME_COUNTDOWN_MS)
            {
                gameEnterPlaying(now);
                break;
            }

            uint8_t value = (uint8_t)(3U - (elapsed / 1000U));
            if (value != countdownValue)
            {
                countdownValue = value;
                uiRenderCountdown(value);
            }
            break;
        }

        case GAME_STATE_PLAYING:
        {
            uint32_t remaining = gameGetRemainingSeconds(now);
            if (remaining != displayedSeconds)
            {
                displayedSeconds = remaining;
                uiRenderPlayingOled(remaining);
            }
            break;
        }

        case GAME_STATE_HIT:
            if (now - stateTick >= HIT_ANIMATION_MS)
            {
                targetNumber = (uint8_t)((targetNumber % GAME_TARGET_COUNT) + 1U);
                gameState = GAME_STATE_PLAYING;
                displayedSeconds = gameGetRemainingSeconds(now);
                uiRenderPlayingLcd("AIMING");
                uiRenderPlayingOled(displayedSeconds);
            }
            break;

        case GAME_STATE_OVER:
        default:
            break;
    }
}

void apInit(void)
{
    servoInit();
    joystickInit();
    laserInit();
    cdsInit();

    lcdReady = lcd1602Init();
    oledReady = ssd1306Init();

    gameEnterWait(HAL_GetTick());
}

void apMain(void)
{
    uint32_t tPrev20 = 0;
    uint32_t tPrev50 = 0;

    servoSetTarget(SERVO_PAN, 90.0f);
    servoSetTarget(SERVO_TILT, 120.0f);
    servoUpdate();
    while (1)
    {
        cdsUpdate();
        uint32_t tNow = HAL_GetTick();

        if (tNow - tPrev20 >= 20U)
        {
            tPrev20 = tNow;
        }

        if (tNow - tPrev50 >= 50U)
        {
            tPrev50 = tNow;

            if (btnFirePressed())
                gameHandleButton(tNow);

            if (gameIsActive())
            {
                joystickUpdate();

                panAngle += joystickGetRatio(JOY_AXIS_X) * AIM_SPEED_DEG;
                tiltAngle -= joystickGetRatio(JOY_AXIS_Y) * AIM_SPEED_DEG;

                panAngle = servoClampAngle(SERVO_PAN, panAngle);
                tiltAngle = servoClampAngle(SERVO_TILT, tiltAngle);

                servoSetTarget(SERVO_PAN, panAngle);
                servoSetTarget(SERVO_TILT, tiltAngle);
                servoUpdate();
            }
        }

        gameUpdate(tNow);
    }
}
