#include "ws2812_driver.h"
#include "settings.h"
#include "main.h"

/*---------------------------------------------------------------------
 *  时序参数（TIM10 计数 = 122.88MHz，ARR = 152）
 *-------------------------------------------------------------------*/
#define WS_ARR (WS2812_TIM_ARR)             /* 152 -> 153 counts/bit */
#define WS_T0H (WS2812_T0H)                 /* 43  -> 350ns */
#define WS_T1H (WS2812_T1H)                 /* 98  -> 797ns */
#define WS_RESET_SLOTS (WS2812_RESET_SLOTS) /* 60  -> 74.7us */

#define BITS_PER_LED 24
#define WS_SLOTS (WS2812_LED_NUM * BITS_PER_LED + WS_RESET_SLOTS)

static uint8_t dma_buffer[WS_SLOTS];

CRGB leds[WS2812_LED_NUM];
volatile uint8_t ws2812_busy;

/*---------------------------------------------------------------------
 *  DMA2 通道 11 = TIM10_UP：搬完一轮就停定时器，把总线放开
 *-------------------------------------------------------------------*/
void DMA2_Channel11_IRQHandler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));

void DMA2_Channel11_IRQHandler (void) {
    if (DMA_GetITStatus (DMA2_IT_TC11) != RESET) {
        DMA_ClearITPendingBit (DMA2_IT_TC11);
        TIM_Cmd (TIM10, DISABLE);
        TIM_SetCompare1 (TIM10, 0); /* CCR = 0 -> CH1N 恒低，就是 RESET 电平 */
        ws2812_busy = 0u;
    }
}

void WS2812_Setup (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    ws2812_busy = 0u;

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM10,
                            ENABLE);
    RCC_AHBPeriphClockCmd (RCC_AHBPeriph_DMA2, ENABLE);

    /* PA5 -> TIM10_CH1N：remap_1 = 部分重映射 */
    GPIO_PinRemapConfig (GPIO_PartialRemap_TIM10, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOA, &GPIO_InitStructure);

    /* 时基：PSC = 0，ARR = 152，向上计数 */
    TIM_TimeBaseInitStructure.TIM_Period = WS_ARR;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit (TIM10, &TIM_TimeBaseInitStructure);

    /* CH1 本体不用（PA5 是 CH1N），只开互补输出
     *   TIM_OCNPolarity_Low -> CC1NP = 1 -> CH1N 相对 OC1REF 不再反相，
     *   于是 CCR 就是高电平宽度，语义和普通 PWM 一样。 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC1Init (TIM10, &TIM_OCInitStructure);

    /* TIM10 带 CH1N，主输出使能照参考工程一样打一下（没有 BDTR 时无害） */
    TIM_CtrlPWMOutputs (TIM10, ENABLE);
    TIM_OC1PreloadConfig (TIM10, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig (TIM10, ENABLE);

    /* 更新事件触发 DMA：每来一个周期把下一个 CCR 值搬进 CH1CVR */
    TIM_DMACmd (TIM10, TIM_DMA_Update, ENABLE);
    TIM_Cmd (TIM10, DISABLE); /* 等 DMA 装好、Show() 时再开 */

    /* DMA2 通道 11 = TIM10_UP
     *   外设 CH1CVR 是 16bit，内存是 8bit，库会把 8bit 填到低半字。 */
    DMA_DeInit (DMA2_Channel11);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&TIM10->CH1CVR);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dma_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = WS_SLOTS;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init (DMA2_Channel11, &DMA_InitStructure);

    DMA_ITConfig (DMA2_Channel11, DMA_IT_TC, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Channel11_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init (&NVIC_InitStructure);

    /* 上电先把灯串刷成全黑，免得残留随机颜色 */
    WS2812_Fill (0, 0, 0);
    WS2812_Show();
}

void WS2812_Show (void) {
    uint32_t pos = 0;
    uint32_t i, j;

    if (ws2812_busy) {
        return;
    }

    for (i = 0; i < WS2812_LED_NUM; i++) {
        /* WS2812 的线序是 G-R-B，高位先出 */
        uint32_t packed = ((uint32_t)leds[i].g << 16) |
                          ((uint32_t)leds[i].r << 8) |
                          ((uint32_t)leds[i].b);

        for (j = 0; j < BITS_PER_LED; j++) {
            dma_buffer[pos++] = ((packed << j) & 0x800000u) ? (uint8_t)WS_T1H
                                                            : (uint8_t)WS_T0H;
        }
    }
    for (i = 0; i < WS_RESET_SLOTS; i++) {
        dma_buffer[pos++] = 0; /* CCR = 0 -> 恒低，构成 RESET */
    }

    ws2812_busy = 1u;

    DMA_Cmd (DMA2_Channel11, DISABLE);
    DMA_SetCurrDataCounter (DMA2_Channel11, WS_SLOTS);
    DMA2_Channel11->MADDR = (uint32_t)dma_buffer;
    DMA_ClearITPendingBit (DMA2_IT_TC11);
    DMA_Cmd (DMA2_Channel11, ENABLE);

    TIM_SetCounter (TIM10, 0);
    TIM_SetCompare1 (TIM10, 0);
    TIM_Cmd (TIM10, ENABLE);
}

/* 按 WS2812_BRIGHTNESS 缩放一个通道 */
static uint8_t ws_scale (uint8_t v) {
    return (uint8_t)(((uint32_t)v * (uint32_t)WS2812_BRIGHTNESS) / 255u);
}

void WS2812_SetPixel (uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < WS2812_LED_NUM) {
        leds[idx].r = ws_scale (r);
        leds[idx].g = ws_scale (g);
        leds[idx].b = ws_scale (b);
    }
}

void WS2812_Fill (uint8_t r, uint8_t g, uint8_t b) {
    uint16_t i;
    for (i = 0; i < WS2812_LED_NUM; i++) {
        leds[i].r = ws_scale (r);
        leds[i].g = ws_scale (g);
        leds[i].b = ws_scale (b);
    }
}

/* h/s/v 0..255 -> r/g/b 0..255 */
void WS2812_HSV (uint8_t h, uint8_t s, uint8_t v,
                 uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t region, rem, p, q, t;
    uint32_t sv;

    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    region = (uint8_t)(h / 43); /* 6 段，每段 43 */
    rem = (uint8_t)((h - region * 43u) * 6u);

    sv = (uint32_t)v * (255u - s) / 255u;
    p = (uint8_t)sv;
    q = (uint8_t)((uint32_t)v * (255u - (uint32_t)s * rem / 255u) / 255u);
    t = (uint8_t)((uint32_t)v * (255u - (uint32_t)s * (255u - rem) / 255u) / 255u);

    switch (region) {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}
