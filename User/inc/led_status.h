#ifndef __LED_STATUS_H__
#define __LED_STATUS_H__

/* 两颗 WS2812 分别指示播放 / 录音当前采样率。
 * 主循环每轮调用一次，内部只在速率变化时才真正刷灯。 */
void Led_RateUpdate (void);

#endif
