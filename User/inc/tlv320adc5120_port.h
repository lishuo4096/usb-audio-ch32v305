


#ifndef TLV320ADC5120_PORT_H
#define TLV320ADC5120_PORT_H

#include <stdint.h>

/* board.h 里的实现（返回 0 成功，-1 失败） */
int  I2C_Write_Buf (uint8_t dev7, uint8_t reg, const uint8_t *data, uint16_t len);
int  I2C_Read_Buf  (uint8_t dev7, uint8_t reg, uint8_t *data, uint16_t len);
void Delay_ms      (uint32_t ms);

#define adc5120_bus_write   I2C_Write_Buf
#define adc5120_bus_read    I2C_Read_Buf
#define adc5120_delay_ms    Delay_ms

/* 本板 ADC 复位走软件复位（ADC5120_RESET_SOFTWARE），不用硬件复位脚 */
/* #define adc5120_hw_reset()  YourHwResetFunction */

#endif /* TLV320ADC5120_PORT_H */
