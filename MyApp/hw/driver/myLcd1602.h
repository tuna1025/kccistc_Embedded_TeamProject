#pragma once
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

bool lcd1602Init(void);
void lcd1602SendCommand(uint8_t cmd);
void lcd1602SendData(uint8_t data);

void lcd1602Clear(void);
void lcd1602Cursor(uint8_t row, uint8_t col);
void lcd1602Print(const char *str);
void lcd1602Printf(const char *fmt, ...);
void lcd1602Backlight(bool on);
