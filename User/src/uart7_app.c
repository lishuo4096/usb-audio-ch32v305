#include "uart7_app.h"
#include "settings.h"
#include "main.h"
#include "usbhs_device.h"

/*=====================================================================
 *  UART7 控制台：DMA 发送环 + 接收环
 *=====================================================================*/

#define RX_RING UART7_RX_RING_BYTES
#define RX_MASK (RX_RING - 1u)
#define TX_RING UART7_TX_RING_BYTES
#define TX_MASK (TX_RING - 1u)

static uint8_t rx_ring[RX_RING];
static volatile uint16_t rx_head;   /* 生产者：ISR / DMA 同步 */
static volatile uint16_t rx_tail;   /* 消费者：主循环 */
static uint16_t rx_dma_pos;

static uint8_t tx_ring[TX_RING];
static volatile uint16_t tx_head;   /* 生产者：UART7_Write */
static volatile uint16_t tx_tail;   /* 消费者：DMA 完成中断 */
static volatile uint16_t tx_chunk;  /* 当前这一次 DMA 搬了多少字节 */
static volatile uint8_t  tx_busy;

volatile uint32_t uart7_tx_drop;
volatile uint32_t uart7_idle_frames;

static uint32_t Uart7IrqLock (void) {
    uint32_t m=NVIC_GetStatusIRQ(DMA2_Channel8_IRQn);
    NVIC_DisableIRQ(DMA2_Channel8_IRQn);
    return m;
}

static void Uart7IrqUnlock (uint32_t m) {
    if(m) NVIC_EnableIRQ(DMA2_Channel8_IRQn);
}

static void uart7_tx_kick (void) {
    uint32_t irq = Uart7IrqLock ();
    uint16_t n;

    if (tx_busy || (tx_head == tx_tail)) {
        Uart7IrqUnlock (irq);
        return;
    }
    /* 一次只搬一段连续的，跨环尾就分两次（下一段由 TC 中断接着搬） */
    if (tx_head > tx_tail) {
        n = (uint16_t)(tx_head - tx_tail);
    } else {
        n = (uint16_t)(TX_RING - tx_tail);
    }

    DMA_Cmd (UART7_TX_DMA_CH, DISABLE);
    DMA_SetCurrDataCounter (UART7_TX_DMA_CH, n);
    UART7_TX_DMA_CH->MADDR = (uint32_t)&tx_ring[tx_tail];
    tx_chunk = n;
    tx_busy = 1u;
    DMA_Cmd (UART7_TX_DMA_CH, ENABLE);
    Uart7IrqUnlock (irq);
}

/* Claim DMA completion atomically with its IRQ; keep USB interrupts enabled. */
void UART7_TxPoll (void) {
    uint32_t irq = Uart7IrqLock ();

    if (!tx_busy) {
        if (tx_head != tx_tail) {
            uart7_tx_kick ();
        }
        Uart7IrqUnlock (irq);
        return;
    }
    if ((UART7_TX_DMA_CH->CFGR & 0x01u) == 0u) {   /* EN=0 → 本轮已结束 */
        DMA_ClearITPendingBit (DMA2_IT_TC8);
        tx_tail = (uint16_t)((tx_tail + tx_chunk) & TX_MASK);
        tx_busy = 0u;
        uart7_tx_kick ();
    }
    Uart7IrqUnlock (irq);
}

/* A diagnostic page may print >1KiB. Yield during formatting as well as while
 * waiting for TX space, so the five USB RX banks cannot fill behind printf.
 * PFIC GISR.GACTSTA excludes all interrupt contexts from main-loop PCM work. */
static void uart7_audio_poll(void) {
    static uint8_t chars;
    if (++chars < 32u) return;
    chars=0u;
    if (!(NVIC->GISR & 0x100u) && USBHS_DevConfig &&
        (USBHS_DevAltSetting[1] || USBHS_DevAltSetting[2])) USB_StreamService();
}

void UART7_Write (const uint8_t *p, uint16_t len) {
    uint16_t i;

    if (p == 0 || len == 0) {
        return;
    }
    for (i = 0; i < len; i++) {
        uint16_t next = (uint16_t)((tx_head + 1u) & TX_MASK);
        uint32_t guard = UART7_TX_SPIN_LIMIT;

        /* 环满：先推一把 DMA，最多等 guard 次；再满就丢，绝不无限阻塞 */
        while ((next == tx_tail) && (guard-- != 0u)) {
            UART7_TxPoll();
            uart7_audio_poll();
        }
        if (next == tx_tail) {
            uart7_tx_drop++;
            continue;
        }
        tx_ring[tx_head] = p[i];
        tx_head = next;
        if (!tx_busy) {
            uart7_tx_kick();
        }
        uart7_audio_poll();
    }
    uart7_tx_kick();
}

void UART7_Flush (void) {
    uint32_t guard = UART7_TX_SPIN_LIMIT;

    uart7_tx_kick();
    while ((tx_busy || (tx_head != tx_tail)) && (guard-- != 0u)) {
    }
}

void UART7_SendByte (uint8_t b) {
    UART7_Write (&b, 1);
}

void UART7_SendString (const char *s) {
    if (s == 0) {
        return;
    }
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c == '\n') {
            UART7_SendByte ('\r');
        }
        UART7_SendByte (c);
    }
}

void UART7_SendBuf (const uint8_t *p, uint16_t len) {
    UART7_Write (p, len);
}

/*---------------------------------------------------------------------
 *  DMA2 通道 8 = UART7_TX 完成中断
 *-------------------------------------------------------------------*/
void DMA2_Channel8_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA2_Channel8_IRQHandler(void) {
    if (DMA_GetITStatus (DMA2_IT_TC8) != RESET) {
        DMA_ClearITPendingBit (DMA2_IT_TC8);
        tx_tail = (uint16_t)((tx_tail + tx_chunk) & TX_MASK);
        tx_busy = 0u;
        uart7_tx_kick();  /* 环里还有就接着搬 */
    }
}

/*---------------------------------------------------------------------
 *  接收
 *-------------------------------------------------------------------*/
static void uart7_rx_sync (void) {
#if UART7_RX_USE_DMA
    uint16_t pos = (uint16_t)(RX_RING - DMA_GetCurrDataCounter (UART7_RX_DMA_CH));
    pos &= RX_MASK;
    rx_dma_pos = pos;
    rx_head = pos;
#else
    /* RXNE 中断自己维护 rx_head，这里无事可做 */
#endif
}

void UART7_IRQHandler (void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART7_IRQHandler (void) {
    if (USART_GetITStatus (UART7, USART_IT_RXNE) != RESET) {
#if !UART7_RX_USE_DMA
        uint8_t b = (uint8_t)(USART_ReceiveData (UART7) & 0xFFu);
        uint16_t next = (uint16_t)((rx_head + 1u) & RX_MASK);
        if (next != rx_tail) {  /* 满了就丢，绝不阻塞 */
            rx_ring[rx_head] = b;
            rx_head = next;
        }
#else
        (void)USART_ReceiveData (UART7);  /* 只清标志 */
#endif
    }

    if (USART_GetITStatus (UART7, USART_IT_IDLE) != RESET) {
        /* 清 IDLE 必须先读 STATR 再读 DATAR。但 RXNE 分支可能刚把数据取走，
         * 两次读之间若又到一个字节，无条件读 DATAR 会把它悄悄吞掉 ——
         * 所以先看 RXNE：真有字节就照常收进环，没有才纯读一次清标志。 */
        volatile uint32_t tmp = UART7->STATR;

        (void)tmp;
#if UART7_RX_USE_DMA
        (void)UART7->DATAR;                /* DMA 模式只借它清标志 */
#else
        if (USART_GetITStatus (UART7, USART_IT_RXNE) != RESET) {
            uint8_t b = (uint8_t)(USART_ReceiveData (UART7) & 0xFFu);
            uint16_t next = (uint16_t)((rx_head + 1u) & RX_MASK);
            if (next != rx_tail) {         /* 满了就丢，绝不阻塞 */
                rx_ring[rx_head] = b;
                rx_head = next;
            }
        } else {
            (void)UART7->DATAR;            /* 不读一次，IDLE 标志清不掉 */
        }
#endif
        uart7_rx_sync();
        uart7_idle_frames++;
    }
}

uint16_t UART7_Available (void) {
    uart7_rx_sync();
    return (uint16_t)((rx_head - rx_tail) & RX_MASK);
}

uint8_t UART7_GetByte (uint8_t *b) {
    if (b == 0) {
        return 0;
    }
    uart7_rx_sync();
    if (rx_head == rx_tail) {
        return 0;
    }
    *b = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & RX_MASK);
    return 1;
}

uint16_t UART7_Read (uint8_t *dst, uint16_t max) {
    uint16_t n = 0;
    while ((n < max) && UART7_GetByte (&dst[n])) {
        n++;
    }
    return n;
}

/*---------------------------------------------------------------------
 *  初始化
 *-------------------------------------------------------------------*/
void UART7_Setup (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};

    rx_head = 0;
    rx_tail = 0;
    rx_dma_pos = 0;
    tx_head = 0;
    tx_tail = 0;
    tx_chunk = 0;
    tx_busy = 0;
    uart7_tx_drop = 0;
    uart7_idle_frames = 0;

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd (RCC_USART7EN, ENABLE);
    RCC_AHBPeriphClockCmd (RCC_AHBPeriph_DMA2, ENABLE);

    /* PC2 = TX（复用推挽），PC3 = RX（上拉输入） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init (GPIOC, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = UART7_BAUD;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init (UART7, &USART_InitStructure);

    /* ---- TX：DMA2 通道 8，内存 -> 外设，8bit，单次 ----
     * 优先级 Medium：I2S3_TX 是 VeryHigh，WS2812 是 High，控制台排它们后面。 */
    DMA_DeInit (UART7_TX_DMA_CH);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&UART7->DATAR);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)tx_ring;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 1;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init (UART7_TX_DMA_CH, &DMA_InitStructure);
    DMA_ITConfig (UART7_TX_DMA_CH, DMA_IT_TC, ENABLE);
    USART_DMACmd (UART7, USART_DMAReq_Tx, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Channel8_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init (&NVIC_InitStructure);

    /* ---- RX ---- */
#if UART7_RX_USE_DMA
    DMA_DeInit (UART7_RX_DMA_CH);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&UART7->DATAR);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)rx_ring;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = RX_RING;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init (UART7_RX_DMA_CH, &DMA_InitStructure);
    DMA_Cmd (UART7_RX_DMA_CH, ENABLE);
    USART_DMACmd (UART7, USART_DMAReq_Rx, ENABLE);
#else
    USART_ITConfig (UART7, USART_IT_RXNE, ENABLE);
#endif

    /* IDLE 中断：两条接收路径都用它切帧 */
    USART_ITConfig (UART7, USART_IT_IDLE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = UART7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init (&NVIC_InitStructure);

    USART_Cmd (UART7, ENABLE);
}

/*---------------------------------------------------------------------
 *  printf 重定向：\n 自动补 \r，写进发送环，DMA 搬
 *-------------------------------------------------------------------*/
int _write (int fd, char *buf, int len) {
    int i;
    uint8_t c;

    (void)fd;
    for (i = 0; i < len; i++) {
        c = (uint8_t)buf[i];
        if (c == '\n' && (i == 0 || (uint8_t)buf[i - 1] != '\r')) {
            UART7_SendByte ('\r');
        }
        UART7_SendByte (c);
    }
    return len;
}
