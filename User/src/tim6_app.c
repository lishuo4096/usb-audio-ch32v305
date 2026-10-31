#include "main.h"
#include "tim6_app.h"
#include "settings.h"
#include "board.h"
#include "key_driver.h"

volatile uint32_t tim6_tick;
volatile bool Tim6_Sync = false;

/* TIM6CLK = PCLK1 = 122.88MHz。TIM6_PRESCALER(12287) 分到 10kHz，
 * 再数 200 次正好 20.00ms。 */
#define TIM6_TICK_HZ 10000UL
#define TIM6_CNT (TIM6_TICK_HZ * TIM6_PERIOD_MS / 1000UL)

static uint8_t key_div;

void TIM6_Setup (void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    tim6_tick = 0;
    key_div = 0;

    RCC_APB1PeriphClockCmd (RCC_TIM6EN, ENABLE);

    TIM_TimeBaseInitStructure.TIM_Prescaler = TIM6_PRESCALER;
    TIM_TimeBaseInitStructure.TIM_Period = (uint16_t)(TIM6_CNT - 1u);
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit (TIM6, &TIM_TimeBaseInitStructure);

    TIM_ClearFlag (TIM6, TIM_FLAG_Update);
    TIM_ITConfig (TIM6, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init (&NVIC_InitStructure);

    TIM_Cmd (TIM6, ENABLE);
}

void TIM6_IRQHandler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));

void TIM6_IRQHandler (void) {
    if (TIM_GetITStatus (TIM6, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit (TIM6, TIM_IT_Update);
        tim6_tick++;
        Tim6_Sync = true;
    }
}

uint32_t TIM6_Elapsed (uint32_t *last) {
    uint32_t now = tim6_tick;
    uint32_t d = now - *last;

    *last = now;
    return d;
}
