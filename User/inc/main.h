#ifndef __MAIN_H__
#define __MAIN_H__


#include "debug.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"

#include "settings.h"
#include "board.h"
#include "key_driver.h"
#include "usbhs_device.h"
#include "i2s_app.h"
#include "uart7_app.h"
#include "tim6_app.h"
extern uint32_t system_reset_flags;
extern volatile uint32_t i2c_timeout_count;
extern volatile uint32_t i2c_recovery_count;

/*--------------------------------------------------------------------
 *  GPIO_Desc：一个引脚 = (端口, 掩码)
 *  WCH 库的 GPIO_Pin_x 就是位掩码，直接用。
 *--------------------------------------------------------------------*/
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET = 1
} GPIO_PinState;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} GPIO_Desc;

/* 按 (模式, 速度) 初始化一个描述符描述的引脚 —— 相当于 HAL_GPIO_Init */
static inline void GPIO_Desc_Mode (GPIO_Desc desc, GPIOMode_TypeDef mode, GPIOSpeed_TypeDef speed) {
    GPIO_InitTypeDef s;
    s.GPIO_Pin = desc.pin;
    s.GPIO_Mode = mode;
    s.GPIO_Speed = speed;
    GPIO_Init (desc.port, &s);
}

static inline void GPIO_WritePin (GPIO_Desc desc, GPIO_PinState state) {
    GPIO_WriteBit (desc.port, desc.pin, (state == GPIO_PIN_SET) ? Bit_SET : Bit_RESET);
}

static inline GPIO_PinState GPIO_ReadPin (GPIO_Desc desc) {
    return (GPIO_ReadInputDataBit (desc.port, desc.pin) == Bit_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static inline void GPIO_TogglePin (GPIO_Desc desc) {
    if (GPIO_ReadOutputDataBit (desc.port, desc.pin) == Bit_SET) {
        GPIO_ResetBits (desc.port, desc.pin);
    } else {
        GPIO_SetBits (desc.port, desc.pin);
    }
}

/*=====================================================================
 *  板级引脚表（按原理图）
 *=====================================================================*/

/* --- 状态灯：板上没有独立 LED，只有两颗 WS2812（PA5 = TIM10_CH1N） --- */

/* --- 按键：PD2 / PC8 / PB14，按下为低（内部上拉） --- */
#define KEY1_PIN ((GPIO_Desc){.port = GPIOC, .pin = GPIO_Pin_8})
#define KEY2_PIN ((GPIO_Desc){.port = GPIOD, .pin = GPIO_Pin_2})
#define KEY3_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_14})

/* --- USB：PB7 = DP / PB6 = DM --- */
#define USB2_DP_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_7})
#define USB2_DM_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_6})

/* --- I2S3 播放（master TX） --- */
#define I2S3_SDATA_PIN ((GPIO_Desc){.port = GPIOC, .pin = GPIO_Pin_12})
#define I2S3_BCLK_PIN ((GPIO_Desc){.port = GPIOC, .pin = GPIO_Pin_10})
#define I2S3_LRCK_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_4})
#define I2S3_MCLK_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_8})

/* --- I2S2 录音（master RX） --- */
#define I2S2_SDATA_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_15})
#define I2S2_BCLK_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_13})
#define I2S2_LRCK_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_12})

/* --- UART7 --- */
#define UART7_TX_PIN ((GPIO_Desc){.port = GPIOC, .pin = GPIO_Pin_2})
#define UART7_RX_PIN ((GPIO_Desc){.port = GPIOC, .pin = GPIO_Pin_3})

/* --- I2C2 --- */
#define I2C2_SCL_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_10})
#define I2C2_SDA_PIN ((GPIO_Desc){.port = GPIOB, .pin = GPIO_Pin_11})

/* --- 外设控制 --- */
#define WS2812_DIN_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_5})
#define DAC_RST_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_6})
/* 功放静音（零检测静音就拉这个脚） */
#define HAMP_MUTE_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_7})
#define OAMP_PWR_EN_PIN ((GPIO_Desc){.port = GPIOC, .pin = GPIO_Pin_11})

/* --- 调试口 --- */
#define SWCLK_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_14})
#define SWDIO_PIN ((GPIO_Desc){.port = GPIOA, .pin = GPIO_Pin_13})

/* 静音脚便捷宏 */
#define HAMP_MUTE_ON() GPIO_WritePin (HAMP_MUTE_PIN, (ZERO_MUTE_ACTIVE_LEVEL) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define HAMP_MUTE_OFF() GPIO_WritePin (HAMP_MUTE_PIN, (ZERO_MUTE_ACTIVE_LEVEL) ? GPIO_PIN_RESET : GPIO_PIN_SET)

/*=====================================================================
 *  板级初始化（定义在 main.c）
 *=====================================================================*/
void GPIO_Setup (void);


#endif /* __MAIN_H__ */
