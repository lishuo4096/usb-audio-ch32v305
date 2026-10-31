#ifndef __WS2812_DRIVER_H__
#define __WS2812_DRIVER_H__

#include "debug.h"
#include <stdint.h>
#include "settings.h"


typedef struct {
    uint8_t g; /* WS2812 的线序是 GRB */
    uint8_t r;
    uint8_t b;
} CRGB;

/* 灯珠数量（本板只有 2 颗） */
extern CRGB leds[WS2812_LED_NUM];

/* 一次 Show() 还没结束时为 1，此时再调 Show() 直接返回 */
extern volatile uint8_t ws2812_busy;

/* 时钟 / 引脚 / 定时器 / DMA / 中断，一次配好 */
void WS2812_Setup (void);

/* 把 leds[] 推给灯串（非阻塞，靠 DMA 完成中断收尾） */
void WS2812_Show (void);

/* 写一颗灯（越界忽略） */
/* 全局亮度 0..255：SetPixel/Fill 的 r/g/b 会按这个比例缩放。
 * 255 = 全亮；觉得刺眼就调小，觉得暗就保持 255 并把调用处的 r/g/b 写大。 */
#ifndef WS2812_BRIGHTNESS
#define WS2812_BRIGHTNESS 255u
#endif

void WS2812_SetPixel (uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
/* 便捷：一次设置全部灯珠 */
void WS2812_Fill (uint8_t r, uint8_t g, uint8_t b);

/* 整条灯串单一颜色 */
void WS2812_Fill (uint8_t r, uint8_t g, uint8_t b);

/* 色相环：h/s/v 都是 0..255 */
void WS2812_HSV (uint8_t h, uint8_t s, uint8_t v,
                 uint8_t *r, uint8_t *g, uint8_t *b);

#endif /* __WS2812_DRIVER_H__ */
