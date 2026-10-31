#ifndef __TIM6_APP_H__
#define __TIM6_APP_H__

#include "debug.h"

/*=====================================================================
 *  TIM6：20ms 系统心跳。只做两件最轻的事：
 *    1) 每 KEY_SCAN_DIV 个 tick 置位一次Tim6同步flag
 *    2) 给上层提供 tim6_tick 计时基准
 *  音频、WS2812、控制台都不在这里做 —— 它们各自有中断/DMA。
 *=====================================================================*/
void TIM6_Setup (void);

/* 每 20ms +1 的自由运行计数 */
extern volatile uint32_t tim6_tick;

extern volatile bool Tim6_Sync;

/* 距离上次调用过了多少个 tick（调用后 last 被更新） */
uint32_t TIM6_Elapsed (uint32_t *last);

#endif /* __TIM6_APP_H__ */
