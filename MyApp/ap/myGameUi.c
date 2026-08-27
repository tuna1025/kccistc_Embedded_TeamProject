#include "myGameUi.h"
#include "myLcd1602.h"
#include "mySsd1306.h"
#include <stdio.h>
#include <string.h>

#define GAME_DURATION_SECONDS 30U

static bool s_lcdReady = false;
static bool s_oledReady = false;

static void lcdWriteLine(uint8_t row, const char *text)
{
    if (!s_lcdReady)
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

void gameUiInit(void)
{
    s_lcdReady = lcd1602Init();
    s_oledReady = ssd1306Init();
}

void gameUiRenderMainMenu(uint8_t selected)
{
    static const char *items[4] = {
        "NORMAL", "GUEST", "SCANNING", "RANKING"
    };
    char line[17];

    snprintf(line, sizeof(line), ">%s", items[selected % 4U]);
    lcdWriteLine(0, "SELECT MODE");
    lcdWriteLine(1, line);

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(0, "MODE SELECT", 1);
    for (uint8_t i = 0; i < 4U; i++)
    {
        snprintf(line, sizeof(line), "%c %s",
                 i == selected ? '>' : ' ', items[i]);
        ssd1306DrawString(18, (int16_t)(15 + i * 12), line,
                          SSD1306_COLOR_WHITE);
    }
    ssd1306Update();
}

void gameUiRenderGuest(void)
{
    lcdWriteLine(0, "PLAYER: GUEST");
    lcdWriteLine(1, "NO RANKING SAVE");

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(8, "GUEST MODE", 2);
    oledDrawCentered(40, "PRESS START", 1);
    ssd1306Update();
}

void gameUiRenderScanning(void)
{
    lcdWriteLine(0, "SCANNING MODE");
    lcdWriteLine(1, "COMING SOON");
    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(12, "SCANNING", 2);
    oledDrawCentered(43, "BTN: BACK", 1);
    ssd1306Update();
}

void gameUiRenderRanking(const uint8_t *uid, uint8_t uidSize,
                         uint16_t bestScore, uint8_t rank,
                         uint8_t playerCount)
{
    char line[17] = "ID:";
    size_t length = 3U;
    if (uid != NULL)
    {
        for (uint8_t i = 0; i < uidSize && length + 2U < sizeof(line); i++)
        {
            int written = snprintf(&line[length], sizeof(line) - length,
                                   "%02X", uid[i]);
            if (written > 0)
                length += (size_t)written;
        }
    }
    lcdWriteLine(0, playerCount == 0U ? "NO RANKING DATA" : line);
    snprintf(line, sizeof(line), "RANK:%u BEST:%02u", rank,
             (unsigned int)(bestScore % 100U));
    lcdWriteLine(1, playerCount == 0U ? "BTN: BACK" : line);

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(2, "RANKING", 1);
    if (playerCount == 0U)
    {
        oledDrawCentered(24, "NO DATA", 2);
    }
    else
    {
        snprintf(line, sizeof(line), "%u / %u", rank, playerCount);
        oledDrawCentered(20, line, 2);
        snprintf(line, sizeof(line), "BEST SCORE %u", bestScore);
        oledDrawCentered(48, line, 1);
    }
    ssd1306Update();
}

void gameUiRenderTagWait(bool showPrompt)
{
    lcdWriteLine(0, "  TAG YOUR CARD");
    lcdWriteLine(1, "PLAYER LOGIN");

    if (!s_oledReady)
        return;
    ssd1306Clear();
    ssd1306DrawRect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT,
                    SSD1306_COLOR_WHITE);
    oledDrawCentered(10, "TAG CARD", 2);
    if (showPrompt)
        oledDrawCentered(46, "TO LOGIN", 1);
    ssd1306Update();
}

void gameUiRenderPlayer(const uint8_t *uid, uint8_t uidSize,
                        uint16_t bestScore, uint8_t rank)
{
    char uidLine[17] = "ID:";
    char scoreLine[17];
    size_t length = 3U;

    if (uid != NULL)
    {
        for (uint8_t i = 0; i < uidSize && length + 2U < sizeof(uidLine); i++)
        {
            int written = snprintf(&uidLine[length], sizeof(uidLine) - length,
                                   "%02X", uid[i]);
            if (written > 0)
                length += (size_t)written;
        }
    }

    lcdWriteLine(0, uidLine);
    snprintf(scoreLine, sizeof(scoreLine), "BEST:%02u RANK:%u",
             (unsigned int)(bestScore % 100U), rank);
    lcdWriteLine(1, scoreLine);

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(7, "PLAYER READY", 1);
    oledDrawCentered(25, "PRESS START", 1);
    ssd1306Update();
}

void gameUiRenderWaitPrompt(bool showPrompt)
{
    if (!s_oledReady)
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

void gameUiRenderWait(bool showPrompt)
{
    lcdWriteLine(0, "TGT:-  SCORE:00");
    lcdWriteLine(1, "STATUS: READY");
    gameUiRenderWaitPrompt(showPrompt);
}

void gameUiRenderCountdown(uint8_t value)
{
    if (!s_oledReady)
        return;
    char digitText[2] = {(char)('0' + value), '\0'};
    ssd1306Clear();
    oledDrawCentered(5, "GET READY", 1);
    oledDrawCentered(20, digitText, 4);
    ssd1306Update();
}

void gameUiRenderPlaying(uint8_t target, uint16_t score,
                         uint32_t remainingSeconds, const char *status)
{
    char line[24];
    snprintf(line, sizeof(line), "TGT:%u SCORE:%02u", target,
             (unsigned int)(score % 100U));
    lcdWriteLine(0, line);
    snprintf(line, sizeof(line), "STATUS: %s", status);
    lcdWriteLine(1, line);

    if (!s_oledReady)
        return;

    char timerText[16];
    uint32_t minutes = remainingSeconds / 60U;
    uint32_t seconds = remainingSeconds % 60U;
    int16_t barWidth =
        (int16_t)((remainingSeconds * 104U) / GAME_DURATION_SECONDS);
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

void gameUiRenderHit(uint8_t target, uint16_t score)
{
    char line[24];
    snprintf(line, sizeof(line), "TGT:%u SCORE:%02u", target,
             (unsigned int)(score % 100U));
    lcdWriteLine(0, line);
    lcdWriteLine(1, "STATUS: HIT");

    if (!s_oledReady)
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

void gameUiRenderGameOver(uint16_t score, uint16_t bestScore, uint8_t rank)
{
    char line[24];
    snprintf(line, sizeof(line), "SCORE:%02u BEST:%02u",
             (unsigned int)(score % 100U),
             (unsigned int)(bestScore % 100U));
    lcdWriteLine(0, line);
    snprintf(line, sizeof(line), "RANK:%u BTN=NEXT", rank);
    lcdWriteLine(1, line);

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(12, "TIME UP", 2);
    oledDrawCentered(45, "PRESS START", 1);
    ssd1306Update();
}

void gameUiRenderGameOverMenu(uint16_t score, uint16_t bestScore,
                              uint8_t rank, uint8_t selected)
{
    char line[24];
    snprintf(line, sizeof(line), "S:%02u B:%02u R:%u",
             (unsigned int)(score % 100U),
             (unsigned int)(bestScore % 100U), rank);
    lcdWriteLine(0, line);
    lcdWriteLine(1, selected == 0U ? ">RETRY   MENU" : " RETRY  >MENU");

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(1, "GAME OVER", 1);
    snprintf(line, sizeof(line), "SCORE %u", score);
    oledDrawCentered(16, line, 1);
    ssd1306DrawString(15, 36, selected == 0U ? "> RETRY" : "  RETRY",
                      SSD1306_COLOR_WHITE);
    ssd1306DrawString(70, 36, selected == 1U ? "> MENU" : "  MENU",
                      SSD1306_COLOR_WHITE);
    ssd1306Update();
}

void gameUiRenderGuestGameOverMenu(uint16_t score, uint8_t selected)
{
    char line[20];
    snprintf(line, sizeof(line), "GUEST SCORE:%02u",
             (unsigned int)(score % 100U));
    lcdWriteLine(0, line);
    lcdWriteLine(1, selected == 0U ? ">RETRY   MENU" : " RETRY  >MENU");

    if (!s_oledReady)
        return;
    ssd1306Clear();
    oledDrawCentered(1, "GUEST OVER", 1);
    snprintf(line, sizeof(line), "SCORE %u", score);
    oledDrawCentered(16, line, 1);
    ssd1306DrawString(15, 36, selected == 0U ? "> RETRY" : "  RETRY",
                      SSD1306_COLOR_WHITE);
    ssd1306DrawString(70, 36, selected == 1U ? "> MENU" : "  MENU",
                      SSD1306_COLOR_WHITE);
    ssd1306Update();
}

void gameUiRenderRfidUid(const uint8_t *uid, uint8_t size)
{
    if (uid == NULL)
        return;

    char line[17] = "UID:";
    size_t length = 4U;
    for (uint8_t i = 0; i < size && length + 2U < sizeof(line); i++)
    {
        int written = snprintf(&line[length], sizeof(line) - length,
                               "%02X", uid[i]);
        if (written > 0)
            length += (size_t)written;
    }
    lcdWriteLine(0, "RFID DETECTED");
    lcdWriteLine(1, line);
}

void gameUiRenderRfidError(void)
{
    lcdWriteLine(1, "RC522 ERROR");
}

void gameUiRenderRankingFull(void)
{
    lcdWriteLine(0, "RANKING FULL");
    lcdWriteLine(1, "MAX 8 PLAYERS");
}
