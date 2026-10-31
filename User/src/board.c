#include "board.h"
#include "ch32_bootloader.h"
#include "i2s_app.h"
#include "ws2812_driver.h"

void Board_EnterBootloader(void) {
    if (!ch32_bl_present()) {
        printf("ISP image missing: install first_install_128k.bin via SDI first\r\n");
        return;
    }
    printf("Entering WCH USB ISP\r\n");
    AMP_SetMute(Bit_RESET);
    USBHS_Device_Init(DISABLE);
    while(ws2812_busy) { IWDG_Feed(); }
    WS2812_Fill(0,0,0); WS2812_Show();
    Delay_Ms(50);
    ch32_bl_enter();
}
#include "settings.h"
#include "usbhs_device.h"
#include <string.h>

volatile uint32_t i2c_timeout_count;
volatile uint32_t i2c_recovery_count;
#if I2C_ENABLE
#define I2C_TIMEOUT I2C2_TIMEOUT_LOOPS
#define I2C_TIMEOUT_TICKS ((I2C2_TIMEOUT_US * 960u + 999u) / 1000u)

static uint8_t i2c_expired (uint16_t started) {
    if ((uint16_t)((uint16_t)TIM8->CNT - started) < I2C_TIMEOUT_TICKS)
        return 0u;
    i2c_timeout_count++;
    return 1u;
}

/* Runtime volume writes share the main loop with 125us audio service.
 * Transport never calls I2C; it can run while this master waits for SCL/ACK.
 * Leave event/DMA ownership in the USB ISR and only convert queued PCM here. */
static void i2c_stream_poll (void) {
    static uint16_t last;
    uint16_t now = (uint16_t)TIM8->CNT;
    if ((uint16_t)(now - last) < 16u)
        return;
    last = now;
    if (USBHS_DevConfig && (USBHS_DevAltSetting[1] || USBHS_DevAltSetting[2]))
        USB_StreamService();
}

static uint8_t i2c_wait (uint32_t event) {
    uint32_t n = I2C_TIMEOUT;
    uint16_t started = (uint16_t)TIM8->CNT;
    while (n--) {
        if (I2C_CheckEvent (I2C2, event)) {
            return 1;
        }
        i2c_stream_poll();
        if (i2c_expired (started))
            return 0u;
    }
    return 0;
}

static uint8_t i2c_wait_flag (uint32_t flag) {
    uint32_t n = I2C_TIMEOUT;
    uint16_t started = (uint16_t)TIM8->CNT;
    while (n--) {
        if (I2C_GetFlagStatus (I2C2, flag) != RESET) {
            return 1;
        }
        i2c_stream_poll();
        if (i2c_expired (started))
            return 0u;
    }
    return 0;
}

static uint8_t i2c_wait_idle (void) {
    uint32_t n = I2C_TIMEOUT;
    uint16_t started = (uint16_t)TIM8->CNT;

    while (n--) {
        if (I2C_GetFlagStatus (I2C2, I2C_FLAG_BUSY) == RESET) {
            return 1;
        }
        i2c_stream_poll();
        if (i2c_expired (started))
            return 0u;
    }
    return 0;
}

static void i2c_abort (uint8_t failed) {
    uint32_t n = I2C_TIMEOUT;
    uint16_t started = (uint16_t)TIM8->CNT;
    I2C_GenerateSTOP (I2C2, ENABLE);
    I2C_AcknowledgeConfig (I2C2, ENABLE);
    I2C_ClearFlag (I2C2, I2C_FLAG_AF | I2C_FLAG_ARLO | I2C_FLAG_BERR | I2C_FLAG_OVR);
    /* 等 STOP 真正生效（BUSY 清零）再返回。不等的话下一次 START 会被
     * 外设忽略，整条链上后面的探测全部超时 —— 这是「第一次探测失败、
     * 反复探测之后又突然好了」的成因。 */
    while ((I2C2->STAR2 & 0x0002u) && n--) {
        i2c_stream_poll();
        if (i2c_expired (started))
            break;
    }
    if (failed || (I2C2->STAR2 & 0x0002u)) {
        i2c_recovery_count++;
        I2C_Setup();
    }
}

/* 总线急救：从机被上次传输卡在「SDA 被拉低」状态时，MCU 复位也救不回来，
 * 只能手动打 9 个 SCL 让它把剩下的位吐完，再补一个 STOP。
 * 不做这一步的话，插着 DAC/ADC 冷启动时 I2C 会一直无应答（实测：
 * 上电后头 112 次探测全部失败，之后才突然能通）。 */
static void i2c_bus_recover (void) {
    GPIO_InitTypeDef gi = {0};
    uint8_t i;

    gi.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gi.GPIO_Mode = GPIO_Mode_Out_OD;
    gi.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOB, &gi);

    GPIO_WriteBit (GPIOB, GPIO_Pin_10, Bit_SET);
    GPIO_WriteBit (GPIOB, GPIO_Pin_11, Bit_SET);
    Delay_Us (20);

    for (i = 0u; i < 9u; i++) {
        GPIO_WriteBit (GPIOB, GPIO_Pin_10, Bit_RESET);
        Delay_Us (10);
        GPIO_WriteBit (GPIOB, GPIO_Pin_10, Bit_SET);
        Delay_Us (10);
        i2c_stream_poll();
        if (GPIO_ReadInputDataBit (GPIOB, GPIO_Pin_11) != 0) {
            break; /* SDA 已经回到高，从机松手了 */
        }
    }

    /* 手动补 STOP：SCL 保持高，SDA 由低跳高 */
    GPIO_WriteBit (GPIOB, GPIO_Pin_10, Bit_RESET);
    Delay_Us (10);
    GPIO_WriteBit (GPIOB, GPIO_Pin_11, Bit_RESET);
    Delay_Us (10);
    GPIO_WriteBit (GPIOB, GPIO_Pin_10, Bit_SET);
    Delay_Us (10);
    GPIO_WriteBit (GPIOB, GPIO_Pin_11, Bit_SET);
    Delay_Us (20);
}

void I2C_Setup (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef I2C_InitStructure = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_I2C2, ENABLE);

    I2C_Cmd (I2C2, DISABLE);
    i2c_bus_recover(); /* 先把可能卡在低电平的总线救回来 */

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOB, &GPIO_InitStructure);

    I2C_DeInit (I2C2);
    I2C_InitStructure.I2C_ClockSpeed = I2C2_CLOCK_SPEED;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init (I2C2, &I2C_InitStructure);
    I2C_Cmd (I2C2, ENABLE);
}

/* 只发地址不发数据，1 = 有 ACK。返回 0 = 应答，-1 = 无应答。 */
int I2C_Probe (uint8_t dev7) {
    uint8_t ok = 0;

    if (!i2c_wait_idle()) {
        i2c_abort (1u);
        return -1;
    }
    I2C_GenerateSTART (I2C2, ENABLE);
    if (i2c_wait (I2C_EVENT_MASTER_MODE_SELECT)) {
        I2C_Send7bitAddress (I2C2, (uint8_t)(dev7 << 1), I2C_Direction_Transmitter);
        ok = i2c_wait (I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);
    }
    i2c_abort ((uint8_t)!ok);
    return ok ? 0 : -1;
}

int I2C_Write_Buf (uint8_t dev7, uint8_t reg, const uint8_t *data, uint16_t len) {
    uint16_t i;

    if (!i2c_wait_idle()) {
        i2c_abort (1u);
        return -1;
    }
    I2C_GenerateSTART (I2C2, ENABLE);
    if (!i2c_wait (I2C_EVENT_MASTER_MODE_SELECT)) {
        i2c_abort (1u);
        return -1;
    }
    I2C_Send7bitAddress (I2C2, (uint8_t)(dev7 << 1), I2C_Direction_Transmitter);
    if (!i2c_wait (I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        i2c_abort (1u);
        return -1;
    }

    I2C_SendData (I2C2, reg);
    if (!i2c_wait (I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
        i2c_abort (1u);
        return -1;
    }

    for (i = 0; i < len; i++) {
        I2C_SendData (I2C2, data[i]);
        if (!i2c_wait (I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
            i2c_abort (1u);
            return -1;
        }
    }

    /* 等 BTF，保证最后一个字节真的从移位器出去了 */
    if (!i2c_wait (I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        i2c_abort (1u);
        return -1;
    }
    I2C_GenerateSTOP (I2C2, ENABLE);
    return 0;
}

int I2C_Read_Buf (uint8_t dev7, uint8_t reg, uint8_t *data, uint16_t len) {
    uint16_t i;

    if (len == 0) {
        return 0;
    }

    /* 第一段：只把寄存器索引写进去 */
    if (!i2c_wait_idle()) {
        i2c_abort (1u);
        return -1;
    }
    I2C_GenerateSTART (I2C2, ENABLE);
    if (!i2c_wait (I2C_EVENT_MASTER_MODE_SELECT)) {
        i2c_abort (1u);
        return -1;
    }
    I2C_Send7bitAddress (I2C2, (uint8_t)(dev7 << 1), I2C_Direction_Transmitter);
    if (!i2c_wait (I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        i2c_abort (1u);
        return -1;
    }
    I2C_SendData (I2C2, reg);
    if (!i2c_wait (I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        i2c_abort (1u);
        return -1;
    }

    /* 第二段：重复起始条件，读 len 个字节 */
    I2C_GenerateSTART (I2C2, ENABLE);
    if (!i2c_wait (I2C_EVENT_MASTER_MODE_SELECT)) {
        i2c_abort (1u);
        return -1;
    }
    I2C_Send7bitAddress (I2C2, (uint8_t)(dev7 << 1), I2C_Direction_Receiver);
    if (!i2c_wait (I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) {
        i2c_abort (1u);
        return -1;
    }

    for (i = 0; i < len; i++) {
        if (i == (uint16_t)(len - 1)) {
            I2C_AcknowledgeConfig (I2C2, DISABLE);
            I2C_GenerateSTOP (I2C2, ENABLE);
        }
        if (!i2c_wait_flag (I2C_FLAG_RXNE)) {
            i2c_abort (1u);
            return -1;
        }
        data[i] = I2C_ReceiveData (I2C2);
    }

    I2C_AcknowledgeConfig (I2C2, ENABLE);
    return 0;
}

/* ES9028Q2M / ES9038Q2M 库要的单寄存器接口，从地址写在 settings.h 里。
 * 手册明确禁止多字节连续读（会让 I2C 解码器挂死到复位），这里也强制单字节。 */
int I2C_Write (uint8_t reg, uint8_t value) {
    return I2C_Write_Buf (DAC_I2C_ADDR7, reg, &value, 1);
}

int I2C_Read (uint8_t reg, uint8_t *value) {
    return I2C_Read_Buf (DAC_I2C_ADDR7, reg, value, 1);
}

#else
void I2C_Setup (void) { }

int I2C_Probe (uint8_t d) {
    (void)d;
    return 1;
}

int I2C_Write_Buf (uint8_t d, uint8_t r, const uint8_t *p, uint16_t n) {
    (void)d;
    (void)r;
    (void)p;
    (void)n;
    return 1;
}

int I2C_Read_Buf (uint8_t d, uint8_t r, uint8_t *p, uint16_t n) {
    (void)d;
    (void)r;
    (void)p;
    (void)n;
    return 1;
}

int I2C_Write (uint8_t r, uint8_t v) {
    (void)r;
    (void)v;
    return 1;
}

int I2C_Read (uint8_t r, uint8_t *v) {
    (void)r;
    (void)v;
    return 1;
}
#endif

void Delay_ms (uint32_t ms) {
    Delay_Ms (ms);
}

/*=====================================================================
 *  板级 GPIO
 *=====================================================================*/
void GPIO_Setup (void) {
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                                RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                                RCC_APB2Periph_AFIO,
                            ENABLE);

    /* 功放静音：必须是第一个动作，任何电源起来之前先压住 */
    GPIO_Desc_Mode (HAMP_MUTE_PIN, GPIO_Mode_Out_PP, GPIO_Speed_2MHz);
    HAMP_MUTE_ON();

    /* DAC 复位：平时保持高（释放），真正复位由 DAC_Reset() 拉一下 */
    GPIO_Desc_Mode (DAC_RST_PIN, GPIO_Mode_Out_PP, GPIO_Speed_2MHz);
    GPIO_WritePin (DAC_RST_PIN, GPIO_PIN_SET);

    /* 运放供电：默认关，完成初始化之后再开 */
    GPIO_Desc_Mode (OAMP_PWR_EN_PIN, GPIO_Mode_Out_PP, GPIO_Speed_2MHz);
    GPIO_WritePin (OAMP_PWR_EN_PIN, GPIO_PIN_RESET);

    /* WS2812 数据线：先当普通推挽拉低，真正的 AF 复用留给 WS2812_Setup()
     * 打开（TIM10 部分重映射之后这个脚才变成 CH1N）。 */
    GPIO_Desc_Mode (WS2812_DIN_PIN, GPIO_Mode_Out_PP, GPIO_Speed_50MHz);
    GPIO_WritePin (WS2812_DIN_PIN, GPIO_PIN_RESET);

    /* 按键：按下为低，内部上拉 */
    GPIO_Desc_Mode (KEY1_PIN, GPIO_Mode_IPU, GPIO_Speed_50MHz);
    GPIO_Desc_Mode (KEY2_PIN, GPIO_Mode_IPU, GPIO_Speed_50MHz);
    GPIO_Desc_Mode (KEY3_PIN, GPIO_Mode_IPU, GPIO_Speed_50MHz);


    /* USB2 DP/DM（PB6/PB7）由 USBHS 外设驱动，这里不能配。 */
}

void OAMP_Power (bool on) {
    GPIO_WritePin (OAMP_PWR_EN_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void DAC_Reset (void) {
    /* ES9038Q2M 要求 RESETB 低电平 >= 1us，释放后等内部稳压稳定。*/
    GPIO_WritePin (DAC_RST_PIN, GPIO_PIN_RESET);
    Delay_Us (100);
    GPIO_WritePin (DAC_RST_PIN, GPIO_PIN_SET);
    Delay_Ms (1);
}

/*=====================================================================
 *  IWDG
 *=====================================================================*/
void IWDG_Setup (void) {
#if IWDG_ENABLE
    IWDG_WriteAccessCmd (IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler (IWDG_PRESCALER);
    IWDG_SetReload (IWDG_RELOAD);
    IWDG_ReloadCounter();
    IWDG_Enable();
#endif
}

void IWDG_Feed (void) {
#if IWDG_ENABLE
    IWDG_ReloadCounter();
#endif
}
