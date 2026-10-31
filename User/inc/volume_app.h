#ifndef __VOLUME_APP_H__
#define __VOLUME_APP_H__

#include "debug.h"
#include "settings.h"

/* ---- DAC 侧（ES9038Q2M Reg15/16 数字音量）----------------------------
 * 手册：0x00 = 0 dB，每步 0.5 dB，0xFF = -127.5 dB（复位默认 0x50 = -40dB）。
 * 所以对外只广告 0 dB ~ -127.5 dB，步进 0.5 dB。 */
#define VOL_DAC_MAX_M256     0        /* +0.00 dB   */
#define VOL_DAC_MIN_M256     (-32640) /* -127.50 dB */
#define VOL_DAC_RES_M256     128      /* 0.50 dB    */

#define VOL_ADC_MAX_M256     3072     /* +12.00 dB */
#define VOL_ADC_MIN_M256     (-25600) /* -100.00 dB */
#define VOL_ADC_RES_M256     128      /* 0.50 dB    */
#define VOL_ADC_PGA_MAX_DB   42

/* -∞（0x8000）：UAC2 要求必须实现，统一映射成最大衰减 + 静音 */
#define VOL_M256_NEG_INF     ((int16_t)0x8000)

/*--------------------------------------------------------------------
 *  生命周期
 *--------------------------------------------------------------------*/
/* 上电调用一次（必须在 I2C_Setup + DAC_ADC_Setup 之后、USB 初始化之前）：
 * 从 Flash 取回上次的音量并立刻落一次硬件。 */
void Volume_Init (void);

/* 主循环每轮调用：脏了才写 I2C；顺便做存盘防抖。 */
void Volume_Handler (void);

/*--------------------------------------------------------------------
 *  设置（USB 中断与串口共用；只改 RAM + 置脏标志，不动硬件）
 *  ch: 0 = master（两声道同值），1 = 左，2 = 右
 *--------------------------------------------------------------------*/
void    Volume_SetDac (uint8_t ch, int16_t m256);
void    Volume_SetAdc (uint8_t ch, int16_t m256);
void    Volume_SetDacMute (uint8_t on);
void    Volume_SetAdcMute (uint8_t on);

/* 读回（GET_CUR 用） */
int16_t Volume_GetDac (uint8_t ch);
int16_t Volume_GetAdc (uint8_t ch);
uint8_t Volume_GetDacMute (void);
uint8_t Volume_GetAdcMute (void);

/* 立刻存盘。
 *   allow_erase = 1：普通路径（电源正常）——槽用满时可以整片擦除（16~20ms）；
 *   allow_erase = 0：**掉电中断里必须传 0** ——只编程不擦除，写不下就丢这一次。
 * 时序依据见 fu_store.h 顶部（tprog=2ms / terase=16~20ms）。 */
void Volume_SaveNow (uint8_t allow_erase);


void Volume_SettingsChanged(void);

/* 芯片被重新初始化（重新跑 DAC_ADC_Setup / ES9038_Init）之后调用：
 * 寄存器回到上电默认值，必须把影子作废并重写一次，否则"重新初始化后
 * 音量突然变成 -40dB / 0dB"这种坑会反复出现。 */
void Volume_Reapply (void);

/* 是否有未落盘的改动 */
uint8_t Volume_IsDirty (void);

/* 把 1/256 dB 打印成 "-6.50" 这样的字符串（buf 至少 10 字节）。
 * 不引入浮点：本项目 -mabi=ilp32（无 F 扩展），%f 既编不出来也没开
 * -lprintfloat。 */
void Volume_FormatDb (char *buf, int16_t m256);

#endif /* __VOLUME_APP_H__ */
