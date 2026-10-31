#ifndef __KEY_DRIVER_H__
#define __KEY_DRIVER_H__

#include "debug.h"
#include <stdbool.h>

/*=====================================================================
 *  按键（Key_Process 由 TIM6 每 40ms 调一次）
 *    按下为低电平、内部上拉；用一个 32 位移位寄存器记采样，
 *    数上升沿（松开次数）来分辨 长按 / 单击 / 双击 / 三击。
 *=====================================================================*/

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SINGLE,   /* 单击 */
    KEY_EVENT_DOUBLE,   /* 双击 */
    KEY_EVENT_TRIPLE,   /* 三击 */
    KEY_EVENT_LONG      /* 长按 */
} Key_Event_e;

typedef struct {
    /* 硬件绑定 */
    GPIO_TypeDef *GPIOx;
    uint16_t GPIO_Pin;

    /* 运行状态 */
    uint8_t  Sampling_Counter;
    uint32_t Sampling_Temp;
    bool     Previous_Key;
    bool     Now_Key;
    bool     Is_Sampling;
    bool     Start_Sampling;
    uint8_t  Key_Release_Counter;

    /* 输出 */
    Key_Event_e Event;
} Key_Dev_t;

extern volatile Key_Dev_t Key_1;   /* KEY1 = PC8  */
extern volatile Key_Dev_t Key_2;   /* KEY2 = PD2  */
extern volatile Key_Dev_t Key_3;   /* KEY3 = PB14 */

/* 采一个键，命中事件时写 btn->Event */
void Key_Process (volatile Key_Dev_t *btn);

/* 在主循环里调，把 Event 消费掉并执行动作 */
void Key_Event_Handler (void);

/* 三个按键脚配成上拉输入（GPIO_Setup 之后调） */
void KEY_Setup (void);

#endif /* __KEY_DRIVER_H__ */
