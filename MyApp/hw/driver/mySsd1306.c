#include "mySsd1306.h"
#include "i2c.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static uint8_t ssd1306_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8]; 

static const uint8_t font6x8[][6] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00, 0x00}, // '!'
    {0x00, 0x07, 0x00, 0x07, 0x00, 0x00}, // '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00}, // '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00}, // '$'
    {0x23, 0x13, 0x08, 0x64, 0x62, 0x00}, // '%'
    {0x36, 0x49, 0x55, 0x22, 0x50, 0x00}, // '&'
    {0x00, 0x05, 0x03, 0x00, 0x00, 0x00}, // '''
    {0x00, 0x1C, 0x22, 0x41, 0x00, 0x00}, // '('
    {0x00, 0x41, 0x22, 0x1C, 0x00, 0x00}, // ')'
    {0x08, 0x2A, 0x1C, 0x2A, 0x08, 0x00}, // '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08, 0x00}, // '+'
    {0x00, 0x50, 0x30, 0x00, 0x00, 0x00}, // ','
    {0x08, 0x08, 0x08, 0x08, 0x08, 0x00}, // '-'
    {0x00, 0x60, 0x60, 0x00, 0x00, 0x00}, // '.'
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x00}, // '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00}, // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31, 0x00}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03, 0x00}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E, 0x00}, // '9'
    {0x00, 0x36, 0x36, 0x00, 0x00, 0x00}, // ':'
    {0x00, 0x56, 0x36, 0x00, 0x00, 0x00}, // ';'
    {0x00, 0x08, 0x14, 0x22, 0x41, 0x00}, // '<'
    {0x14, 0x14, 0x14, 0x14, 0x14, 0x00}, // '='
    {0x00, 0x41, 0x22, 0x14, 0x08, 0x00}, // '>'
    {0x02, 0x01, 0x51, 0x09, 0x06, 0x00}, // '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E, 0x00}, // '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00}, // 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36, 0x00}, // 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22, 0x00}, // 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00}, // 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41, 0x00}, // 'E'
    {0x7F, 0x09, 0x09, 0x01, 0x01, 0x00}, // 'F'
    {0x3E, 0x41, 0x41, 0x51, 0x32, 0x00}, // 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00}, // 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00, 0x00}, // 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01, 0x00}, // 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41, 0x00}, // 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40, 0x00}, // 'L'
    {0x7F, 0x02, 0x04, 0x02, 0x7F, 0x00}, // 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00}, // 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00}, // 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06, 0x00}, // 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00}, // 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46, 0x00}, // 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31, 0x00}, // 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01, 0x00}, // 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00}, // 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00}, // 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F, 0x00}, // 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63, 0x00}, // 'X'
    {0x03, 0x04, 0x78, 0x04, 0x03, 0x00}, // 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43, 0x00}, // 'Z'
    {0x00, 0x7F, 0x41, 0x41, 0x00, 0x00}, // '['
    {0x02, 0x04, 0x08, 0x10, 0x20, 0x00}, // '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00, 0x00}, // ']'
    {0x04, 0x02, 0x01, 0x02, 0x04, 0x00}, // '^'
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x00}, // '_'
    {0x00, 0x01, 0x02, 0x04, 0x00, 0x00}, // '`'
    {0x20, 0x54, 0x54, 0x54, 0x78, 0x00}, // 'a'
    {0x7F, 0x48, 0x44, 0x44, 0x38, 0x00}, // 'b'
    {0x38, 0x44, 0x44, 0x44, 0x20, 0x00}, // 'c'
    {0x38, 0x44, 0x44, 0x48, 0x7F, 0x00}, // 'd'
    {0x38, 0x54, 0x54, 0x54, 0x18, 0x00}, // 'e'
    {0x08, 0x7E, 0x09, 0x01, 0x02, 0x00}, // 'f'
    {0x08, 0x14, 0x54, 0x54, 0x3C, 0x00}, // 'g'
    {0x7F, 0x08, 0x04, 0x04, 0x78, 0x00}, // 'h'
    {0x00, 0x44, 0x7D, 0x40, 0x00, 0x00}, // 'i'
    {0x20, 0x40, 0x44, 0x3D, 0x00, 0x00}, // 'j'
    {0x7F, 0x10, 0x28, 0x44, 0x00, 0x00}, // 'k'
    {0x00, 0x41, 0x7F, 0x40, 0x00, 0x00}, // 'l'
    {0x7C, 0x04, 0x18, 0x04, 0x78, 0x00}, // 'm'
    {0x7C, 0x08, 0x04, 0x04, 0x78, 0x00}, // 'n'
    {0x38, 0x44, 0x44, 0x44, 0x38, 0x00}, // 'o'
    {0x7C, 0x14, 0x14, 0x14, 0x08, 0x00}, // 'p'
    {0x08, 0x14, 0x14, 0x18, 0x7C, 0x00}, // 'q'
    {0x7C, 0x08, 0x04, 0x04, 0x08, 0x00}, // 'r'
    {0x48, 0x54, 0x54, 0x54, 0x20, 0x00}, // 's'
    {0x04, 0x3F, 0x44, 0x40, 0x20, 0x00}, // 't'
    {0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00}, // 'u'
    {0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00}, // 'v'
    {0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00}, // 'w'
    {0x44, 0x28, 0x10, 0x28, 0x44, 0x00}, // 'x'
    {0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00}, // 'y'
    {0x44, 0x64, 0x54, 0x4C, 0x44, 0x00}, // 'z'
    {0x00, 0x08, 0x36, 0x41, 0x00, 0x00}, // '{'
    {0x00, 0x00, 0x7F, 0x00, 0x00, 0x00}, // '|'
    {0x00, 0x41, 0x36, 0x08, 0x00, 0x00}, // '}'
    {0x08, 0x08, 0x2A, 0x1C, 0x08, 0x00}  // '~'
};
static const uint8_t icon6x8[ICON_COUNT][6] = {
    {0x06, 0x1F, 0x3E, 0x1F, 0x06, 0x00}, // HEART
    {0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00}, // STAR
    {0x08, 0x10, 0x08, 0x04, 0x02, 0x00}, // CHECK
    {0x22, 0x14, 0x08, 0x14, 0x22, 0x00}, // CROSS
    {0x10, 0x1E, 0x3F, 0x1E, 0x10, 0x00}, // BELL
    {0x20, 0x70, 0x30, 0x1F, 0x01, 0x00}, // MUSIC
    {0x3E, 0x22, 0x22, 0x3E, 0x1C, 0x00}, // BATTERY
    {0x20, 0x14, 0x0E, 0x05, 0x00, 0x00}  // LIGHTNING
};

static void writeCommand(uint8_t cmd) {
  HAL_I2C_Mem_Write(&hi2c1, SSD1306_I2C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd,  1, 10);
}
void ssd1306Clear(void){
    memset(ssd1306_buffer,0x00,sizeof(ssd1306_buffer));
}

void ssd1306LoadFrame(const uint8_t *frame){
    if(frame == NULL)
        return;

    memcpy(ssd1306_buffer, frame, sizeof(ssd1306_buffer));
}

void ssd1306Update(void){
    writeCommand(0x21); // set column addr
    writeCommand(0);
    writeCommand(SSD1306_WIDTH-1);

    writeCommand(0x22); // set page addr
    writeCommand(0);
    writeCommand((SSD1306_HEIGHT/8)-1);

    HAL_I2C_Mem_Write(&hi2c1, SSD1306_I2C_ADDR, 0x40,
                      I2C_MEMADD_SIZE_8BIT, ssd1306_buffer,
                      sizeof(ssd1306_buffer), 200);
}

void ssd1306DrawPixel(int16_t x, int16_t y, uint8_t color){
    if(x<0||y<0||x>=SSD1306_WIDTH||y>=SSD1306_HEIGHT)
    return;

    if(color==SSD1306_COLOR_WHITE){
        ssd1306_buffer[x + (y / 8) * SSD1306_WIDTH] |= (1U << (y % 8));
    }
    else{
        ssd1306_buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1U << (y % 8));
    }
}


bool ssd1306Init(void) {
  // 디바이스 응답 확인
  if (HAL_I2C_IsDeviceReady(&hi2c1, SSD1306_I2C_ADDR, 2, 10) != HAL_OK) {
    return false;
  }

  HAL_Delay(10);

  // SSD1306 초기화 시퀀스
  writeCommand(0xAE); // Display OFF

  writeCommand(0x20); // Set Memory Addressing Mode
  writeCommand(0x00); // 00: Horizontal Addressing Mode

  writeCommand(0xB0); // Set Page Start Address for Page Addressing Mode,0-7

  writeCommand(0xC8); // Set COM Output Scan Direction (Reversed)
  writeCommand(0x00); // Set low column address
  writeCommand(0x10); // Set high column address

  writeCommand(0x40); // Set start line address

  writeCommand(0x81); // Set contrast control register
  writeCommand(0xFF);

  writeCommand(0xA1); // Set Segment Re-map (0 to 127)

  writeCommand(0xA6); // Set Normal display (0xA7: Inverse)

  writeCommand(0xA8); // Set multiplex ratio(1 to 64)
  writeCommand(0x3F); // 1/64 duty

  writeCommand(
      0xA4); // 0xA4: Output follows RAM content; 0xA5: Entire display ON

  writeCommand(0xD3); // Set display offset
  writeCommand(0x00); // Not offset

  writeCommand(0xD5); // Set display clock divide ratio/oscillator frequency
  writeCommand(0xF0); // Set divide ratio

  writeCommand(0xD9); // Set pre-charge period
  writeCommand(0x22);

  writeCommand(0xDA); // Set com pins hardware configuration
  writeCommand(0x12);

  writeCommand(0xDB); // Set vcomh
  writeCommand(0x20); // 0x20: 0.77xVcc

  writeCommand(0x8D); // Set DC-DC enable
  writeCommand(0x14); // Enable charge pump

  writeCommand(0xAF); // Display ON

  ssd1306Clear();
  ssd1306Update();

  return true;
}

void ssd1306DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     uint8_t color) {
  int16_t dx = abs(x1 - x0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0);
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  int16_t e2;

  while (1) {
    ssd1306DrawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}
void ssd1306DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color){
    // 위, 아래
    ssd1306DrawLine(x, y, x + w - 1, y, color);
    ssd1306DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);

    // 왼쪽, 오른쪽
    ssd1306DrawLine(x, y, x, y + h - 1, color);
    ssd1306DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);

}

void ssd1306FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color){
    for(int16_t i=x; i<x+w; i++){
        for(int16_t j=y; j<y+h; j++){
            ssd1306DrawPixel(i, j, color);
        }
    }
}

void ssd1306DrawChar(int16_t x, int16_t y, char ch, uint8_t color)
{
    if (ch < ' ' || ch > '~')
        ch = '?';

    uint8_t index = ch-' ';

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t line = font6x8[index][i];
        for (uint8_t j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                ssd1306DrawPixel(x + i, y + j, color);
            } else {
                ssd1306DrawPixel(x + i, y + j, !color);
            }
        }
    }
}

void ssd1306DrawIcon(int16_t x, int16_t y,ssd1306Icon_t icon,uint8_t color){
    if (icon >= ICON_COUNT)
        return;

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t line = icon6x8[icon][i];
        for (uint8_t j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                ssd1306DrawPixel(x + i, y + j, color);
            } else {
                ssd1306DrawPixel(x + i, y + j, !color);
            }
        }
    }
}

void ssd1306DrawString(int16_t x, int16_t y, const char *str, uint8_t color){
    while(*str){
        ssd1306DrawChar(x, y, *str, color);
        x+=6;
        str++;
    }
}

void ssd1306DrawCharScaled(int16_t x, int16_t y, char ch, uint8_t color,
                           uint8_t scale)
{
    if (scale == 0)
        return;

    if (ch < ' ' || ch > '~')
        ch = '?';

    uint8_t index = (uint8_t)(ch - ' ');

    for (uint8_t i = 0; i < 6; i++)
    {
        uint8_t line = font6x8[index][i];

        for (uint8_t j = 0; j < 8; j++)
        {
            uint8_t pixel_color = (line & (1U << j)) ? color : !color;
            ssd1306FillRect(x + (int16_t)i * scale,
                            y + (int16_t)j * scale,
                            scale, scale, pixel_color);
        }
    }
}

void ssd1306DrawStringScaled(int16_t x, int16_t y, const char *str,
                             uint8_t color, uint8_t scale)
{
    if (str == NULL || scale == 0)
        return;

    while (*str)
    {
        ssd1306DrawCharScaled(x, y, *str, color, scale);
        x += (int16_t)(6U * scale);
        str++;
    }
}


static void ssd1306FillCircle(int16_t center_x, int16_t center_y,
                              int16_t radius, uint8_t color)
{
    int32_t radius_squared = (int32_t)radius * radius;

    for (int16_t y = -radius; y <= radius; y++) {
        int16_t half_width = radius;

        while (half_width > 0 &&
               ((int32_t)half_width * half_width + (int32_t)y * y) >
                   radius_squared) {
            half_width--;
        }

        ssd1306DrawLine(center_x - half_width, center_y + y,
                        center_x + half_width, center_y + y, color);
    }
}




void ssd1306DrawBomb(uint8_t color)
{
    uint8_t highlight_color = (color == SSD1306_COLOR_WHITE)
                                  ? SSD1306_COLOR_BLACK
                                  : SSD1306_COLOR_WHITE;

    /* Round body: occupies most of the left and lower part of the display. */
    ssd1306FillCircle(37, 35, 28, color);

    /* Neck and fuse. Draw nearby parallel lines to give them some thickness. */
    ssd1306DrawLine(53, 16, 62, 7, color);
    ssd1306DrawLine(54, 17, 63, 8, color);
    ssd1306DrawLine(55, 18, 64, 9, color);

    ssd1306DrawLine(62, 7, 81, 3, color);
    ssd1306DrawLine(63, 8, 81, 4, color);
    ssd1306DrawLine(81, 3, 94, 10, color);
    ssd1306DrawLine(81, 4, 94, 11, color);

    /* Spark at the end of the fuse. */
    ssd1306DrawLine(105, 7, 105, 0, color);
    ssd1306DrawLine(105, 15, 105, 23, color);
    ssd1306DrawLine(101, 11, 92, 11, color);
    ssd1306DrawLine(109, 11, 123, 11, color);
    ssd1306DrawLine(102, 8, 95, 1, color);
    ssd1306DrawLine(108, 8, 115, 1, color);
    ssd1306DrawLine(102, 14, 95, 21, color);
    ssd1306DrawLine(108, 14, 115, 21, color);
    ssd1306DrawRect(103, 9, 5, 5, color);

    /* Small reflected highlight on the bomb body. */
    ssd1306DrawLine(24, 24, 28, 19, highlight_color);
    ssd1306DrawLine(28, 19, 35, 16, highlight_color);
    ssd1306DrawLine(25, 25, 29, 20, highlight_color);
}

void ssd1306Test(void)
{
    ssd1306Clear();
    ssd1306DrawRect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT,
                    SSD1306_COLOR_WHITE);
    ssd1306DrawString(19, 28, "SSD1306 TEST", SSD1306_COLOR_WHITE);
    ssd1306Update();
}
