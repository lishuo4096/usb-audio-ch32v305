#ifndef __BOARD_H__
#define __BOARD_H__

#include "debug.h"
#include "main.h"

/* 时钟 + 全部板级 GPIO（功放静音、DAC_RST、功放供电、按键、WS2812 空闲电平） */
void GPIO_Setup (void);

void I2C_Setup (void);

int I2C_Probe (uint8_t dev7);
int I2C_Write_Buf (uint8_t dev7, uint8_t reg, const uint8_t *data, uint16_t len);
int I2C_Read_Buf (uint8_t dev7, uint8_t reg, uint8_t *data, uint16_t len);

int I2C_Write (uint8_t reg, uint8_t value); /* DAC @ 0x48 */
int I2C_Read (uint8_t reg, uint8_t *value); /* DAC @ 0x48 */

/* 两个音频库把毫秒延时映射成 Delay_ms，这里转给 WCH 的 Delay_Ms，
 * 这样 es9028/es9038/tlv320 三个库一行都不用改。 */
void Delay_ms (uint32_t ms);

/* ---- 电源/复位时序（DAC_RST = PA6, OAMP_PWR_EN = PC11） ---- */
void DAC_Reset (void);
void OAMP_Power (bool on);

/* ---- 独立看门狗（约 2s），由 TIM6 的 20ms 心跳喂狗 ---- */
void IWDG_Setup (void);
void IWDG_Feed (void);

void Board_EnterBootloader(void);
#endif /* __BOARD_H__ */
