#ifndef __UART7_APP_H__
#define __UART7_APP_H__

#include "debug.h"


void UART7_Setup (void);

/* 非阻塞发送：拷进发送环，返回立刻返回。环满的部分丢弃并计入
 * uart7_tx_drop。printf 也走这条路。 */
void UART7_Write     (const uint8_t *p, uint16_t len);
void UART7_TxPoll    (void);
void UART7_SendByte  (uint8_t b);
void UART7_SendString(const char *s);
void UART7_SendBuf   (const uint8_t *p, uint16_t len);

/* 阻塞等到发送环排空（只在上电横幅、复位前用，别在音频路径里调）。 */
void UART7_Flush (void);

/* 接收环里还有多少字节 */
uint16_t UART7_Available (void);

/* 最多取 max 个字节，返回实际取到的个数 */
uint16_t UART7_Read (uint8_t *dst, uint16_t max);

/* 取一个字节，返回 1 表示取到 */
uint8_t  UART7_GetByte (uint8_t *b);

extern volatile uint32_t uart7_tx_drop;   /* 发送环溢出丢弃的字节数 */
extern volatile uint32_t uart7_idle_frames;

#endif /* __UART7_APP_H__ */
