#include <stdarg.h>
#include "myLcd1602.h"
#include "i2c.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdint.h>
#include <stdio.h>

static uint8_t lcd1602_addr=0x27<<1;
static bool backlight_state = true;
static bool lcd_ok = false;

static HAL_StatusTypeDef i2c_send(uint8_t *buf, uint16_t len){
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&hi2c1, lcd1602_addr, buf, len, 10);
    if(ret!=HAL_OK){

    }
    return ret;
}


/**
 * @brief  HD44780: 4비트 nibble 전송 + Enable 스트로브
 *         4바이트를 한 번에 전송하여 I2C 트랜잭션을 최소화
 */

static void lcd1602_write(uint8_t data, uint8_t rs)
{
  if (!lcd_ok) return;

  uint8_t bl   = backlight_state ? 0x08 : 0x00;
  uint8_t high = data & 0xF0;
  uint8_t low  = (data << 4) & 0xF0;

  uint8_t buf[4];
  buf[0] = high | bl | rs | 0x04; // High nibble EN=1
  buf[1] = high | bl | rs;        // High nibble EN=0
  buf[2] = low  | bl | rs | 0x04; // Low  nibble EN=1
  buf[3] = low  | bl | rs;        // Low  nibble EN=0

  if (i2c_send(buf, 4) != HAL_OK)
  {
    lcd_ok = false; // 재시도 후도 실패 → 이후 호출 전체 비활성화
  }
}

/* ----------------------------------------------------------------
 * 공개 API
 * ----------------------------------------------------------------*/

bool lcd1602Init(void)
{
  lcd_ok = false;

  HAL_Delay(50); // 전원 인가 후 대기 (>40ms)

  /* PCF8574 주소 자동 감지: 0x27 우선, 없으면 0x3F */
  if (HAL_I2C_IsDeviceReady(&hi2c1, 0x27 << 1, 2, 10) == HAL_OK)
  {
    lcd1602_addr = 0x27 << 1;
  }
  else if (HAL_I2C_IsDeviceReady(&hi2c1, 0x3F << 1, 2, 10) == HAL_OK)
  {
    lcd1602_addr = 0x3F << 1;
  }
  else
  {
    /* LCD가 버스에 없음: I2C 버스 상태만 복구하고 종료 */
    //i2cBusRecover();
    return false;
  }

  backlight_state = true;
  lcd_ok = true;

  uint8_t bl = 0x08;
  uint8_t cmd2[2];

  /* HD44780 초기화 시퀀스 (8비트 → 4비트 전환) */
  cmd2[0] = 0x30 | bl | 0x04;
  cmd2[1] = 0x30 | bl;
  HAL_I2C_Master_Transmit(&hi2c1, lcd1602_addr, cmd2, 2, 10);
  HAL_Delay(5);

  HAL_I2C_Master_Transmit(&hi2c1, lcd1602_addr, cmd2, 2, 10);
  HAL_Delay(1);

  HAL_I2C_Master_Transmit(&hi2c1, lcd1602_addr, cmd2, 2, 10);
  HAL_Delay(1);

  cmd2[0] = 0x20 | bl | 0x04; // 4비트 모드 전환
  cmd2[1] = 0x20 | bl;
  HAL_I2C_Master_Transmit(&hi2c1, lcd1602_addr, cmd2, 2, 10);
  HAL_Delay(1);

  /* 기능 설정 */
  lcd1602SendCommand(0x28); // 4-bit, 2라인, 5x8
  lcd1602SendCommand(0x0C); // Display ON, Cursor OFF
  lcd1602SendCommand(0x06); // Entry mode: 커서 우향
  lcd1602Clear();

  return lcd_ok;
}

void lcd1602SendCommand(uint8_t cmd){
    lcd1602_write(cmd, 0x00);
    if(cmd==0x01 || cmd == 0x02){
        HAL_Delay(2);
    }
}
void lcd1602SendData(uint8_t data){
    lcd1602_write(data, 0x01);
}
void lcd1602Clear(void){
    lcd1602SendCommand(0x01);
}

void lcd1602Cursor(uint8_t row, uint8_t col){
    uint8_t addr = (row==0) ? (0x00+col) : (0x40 + col);
    lcd1602SendCommand(0x80 | addr);
}

void lcd1602Print(const char *str){
    while(*str){
        lcd1602SendData((uint8_t)(*str++));
    }
}

void lcd1602Printf(const char *fmt, ...){
    char buf[33];
    va_list args;
    va_start(args,fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lcd1602Print(buf);
}

void lcd1602Backlight(bool on)
{
    backlight_state = on;

    if (!lcd_ok)
        return;

    uint8_t value = on ? 0x08 : 0x00;
    if (i2c_send(&value, 1) != HAL_OK)
        lcd_ok = false;
}
