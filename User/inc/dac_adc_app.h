#ifndef __DAC_ADC_APP_H__
#define __DAC_ADC_APP_H__

#include "debug.h"

/*=====================================================================
 *  ES9038Q2M（DAC，7 位地址 0x48）+ TLV320ADC5120（ADC，7 位地址 0x4E）
 *  的上电流程。总线本身由 board.c 提供（I2C_Write / I2C_Write_Buf ...），
 *  这里只负责时序与两个库的初始化调用。
 *=====================================================================*/

/* I2C_Setup -> DAC_RST 释放 -> ES9038_Init -> ADC5120_Init
 * 返回 0 全失败；bit0 = DAC 成功，bit1 = ADC 成功（是"成功"位，不是失败位） */
uint8_t DAC_ADC_Setup (uint32_t sample_rate);

/* 运行期访问 TLV320ADC5120 的 Page0 单字节寄存器（录音音量 / PGA 用）。
 * Init 成功之前调用直接返回 0 —— 不会在总线上空打。
 * 返回 1 = 成功，0 = 失败 / 未就绪。 */
uint8_t DAC_ADC_AdcWrite (uint8_t reg, uint8_t value);
uint8_t DAC_ADC_AdcRead (uint8_t reg, uint8_t *value);

/* 只初始化 ADC5120（录音通路），不碰 DAC。返回 0 成功，1 失败。
 * ADC 上电默认停在 SLEEP，不写 I2C 就完全不出数据。 */
uint8_t ADC_Setup (uint32_t sample_rate);

/* ADC5120_Init() 的实际返回码：0=OK 1=总线/参数 2=探测无应答 3=状态未达预期 */
extern volatile uint8_t adc_init_status;

/* 初始化后回读的 ASI_CFG0（期望 0x70 = I2S + 32bit 槽 + TX 不填充）。
 * 用来抓"ADC 只在槽高位驱动数据"这类 ASI 配置没落地的情形。 */
extern volatile uint8_t adc_asi_cfg0;

/* 只探测，不写配置：返回同上 */
uint8_t DAC_ADC_Probe (void);

/* 运行期改 DAC 采样率（I2S 换率时联动） */
void DAC_SetSampleRate (uint32_t sample_rate);
uint8_t ADC_PrepareRate(uint32_t rate);
uint8_t ADC_FinishRate(void);

#endif /* __DAC_ADC_APP_H__ */
