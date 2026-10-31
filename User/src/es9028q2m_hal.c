

#include <stddef.h>
#include "es9028q2m_hal.h"

/* ========================================================================= */
/* 1) 移植层配置区 -- 对接用户自己的读写函数时只需修改本区                        */
/* ========================================================================= */

/* 库内部统一以 es9028_bus_xxx 为名调用总线; 私有宏把库内名字映射到外部
 * 函数名, 从而将库内对 es9028_bus_write 的所有调用替换为 I2C_Write 调用。
 * 移植时只需修改下面 3 行 #define 的右侧, 例如封装 STM32 HAL 的
 * HAL_I2C_Mem_Write 为 I2C_Write 后即可直接对接。                        */
#define es9028_bus_write       I2C_Write
#define es9028_bus_read        I2C_Read
#define es9028_bus_delay_ms    Delay_ms

/* 外部必须实现的三个函数 (宏替换后实际声明/调用的是右侧名字):
 *   int  I2C_Write(uint8_t reg, uint8_t value);   返回 0 成功
 *   int  I2C_Read (uint8_t reg, uint8_t *value);  返回 0 成功, 单寄存器读
 *   void Delay_ms (uint32_t ms);                  毫秒延时
 * 读写均为单寄存器操作 (reg 为寄存器地址); 放在任意用户文件中链接即可。      */
extern int  es9028_bus_write(uint8_t reg, uint8_t value);
extern int  es9028_bus_read(uint8_t reg, uint8_t *value);
extern void es9028_bus_delay_ms(uint32_t ms);

/* 多字节寄存器字节序: 0 = 小端 (低字节存低地址, 默认)。ES9028Q2M 手册明确
 * 小端: "Reg 17 are the LSBs"、"Reg 66 contains the LSBs"、FIR 下载示例
 * 先写低字节 (reg27)。一般无需修改, 如遇例外可改为 1 整体翻转。          */
#ifndef ES9028_REG_MSB_FIRST
#define ES9028_REG_MSB_FIRST    0
#endif

/* ========================================================================= */
/* 2) 内部辅助函数                                                            */
/* ========================================================================= */

/** 内部检查宏: 调用失败立即返回 ES9028_ERROR */
#define ES9028_CHK(call)                                                     \
    do {                                                                    \
        if ((call) != ES9028_OK) { return ES9028_ERROR; }                   \
    } while (0)

/** 单寄存器写 */
static ES9028_StatusTypeDef es9028_wr(uint8_t reg, uint8_t val)
{
    return (es9028_bus_write(reg, val) == 0) ? ES9028_OK : ES9028_ERROR;
}

/** 单寄存器读 */
static ES9028_StatusTypeDef es9028_rd(uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return ES9028_ERROR;
    }
    return (es9028_bus_read(reg, val) == 0) ? ES9028_OK : ES9028_ERROR;
}

/** 读改写: (old & ~mask) | (val & mask) — 用于含保留位的寄存器 */
static ES9028_StatusTypeDef es9028_mod(uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t old;

    ES9028_CHK(es9028_rd(reg, &old));
    old = (uint8_t)((old & (uint8_t)~mask) | (val & mask));
    return es9028_wr(reg, old);
}

/**
 * 多字节寄存器写: 依次写 base, base+1, ...
 * 字节顺序按 ES9028_REG_MSB_FIRST (0: 低字节在低地址, 手册明确定义)
 */
static ES9028_StatusTypeDef es9028_wr_multi(uint8_t base, uint32_t val, uint8_t bytes)
{
    uint8_t i;

    for (i = 0; i < bytes; i++) {
        uint8_t shift = (ES9028_REG_MSB_FIRST != 0)
                        ? (uint8_t)(8U * (bytes - 1U - i))
                        : (uint8_t)(8U * i);
        ES9028_CHK(es9028_wr((uint8_t)(base + i), (uint8_t)((val >> shift) & 0xFFU)));
    }
    return ES9028_OK;
}

/** 多字节寄存器读 (逐寄存器单字节读) */
static ES9028_StatusTypeDef es9028_rd_multi(uint8_t base, uint32_t *val, uint8_t bytes)
{
    uint8_t i;
    uint32_t v = 0;

    if (val == NULL) {
        return ES9028_ERROR;
    }
    for (i = 0; i < bytes; i++) {
        uint8_t b;
        uint8_t shift = (ES9028_REG_MSB_FIRST != 0)
                        ? (uint8_t)(8U * (bytes - 1U - i))
                        : (uint8_t)(8U * i);
        ES9028_CHK(es9028_rd((uint8_t)(base + i), &b));
        v |= ((uint32_t)b) << shift;
    }
    *val = v;
    return ES9028_OK;
}

/**
 * 当前软启动斜坡时间 (ms), 公式 (手册 Reg14): t = 4096 * 2^(time+1) / MCLK
 * 读取失败返回 0
 */
static uint32_t es9028_softstart_ms(void)
{
    uint8_t t;
    uint64_t ticks;

    if (es9028_rd(ES9028_REG_SOFT_START, &t) != ES9028_OK) {
        return 0;
    }
    t &= ES9028_SOFT_START_TIME_Msk;
    if (t > 20U) {
        t = 20U;                    /* 手册: 有效范围 0~20 */
    }
    ticks = (4096ULL << (t + 1U));  /* 4096 * 2^(t+1) 个 MCLK 周期 */
    return (uint32_t)((ticks * 1000ULL) / (uint64_t)ES9028_MCLK_HZ);
}

/* ========================================================================= */
/* 3) 低层寄存器访问 API                                                       */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_WriteReg(uint8_t RegAddr, uint8_t Value)
{
    return es9028_wr(RegAddr, Value);
}

ES9028_StatusTypeDef ES9028_ReadReg(uint8_t RegAddr, uint8_t *Value)
{
    return es9028_rd(RegAddr, Value);
}

ES9028_StatusTypeDef ES9028_ModifyReg(uint8_t RegAddr, uint8_t Mask, uint8_t Value)
{
    return es9028_mod(RegAddr, Mask, Value);
}

/* ========================================================================= */
/* 整机初始化 / 复位                                                           */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_SoftReset(void)
{
    /* Reg0[0]=1 触发软复位 (该位自清零且恒读0); 其余位写回上电默认 (含
     * 保留位[3:1]=0, 手册要求保持 0) */
    ES9028_CHK(es9028_wr(ES9028_REG_SYSTEM, ES9028_SYS_SOFT_RESET_Msk));
    es9028_bus_delay_ms(1U);            /* 等待复位完成 */
    return ES9028_OK;
}

ES9028_StatusTypeDef ES9028_Init(void)
{
    ES9028_Clock_InitTypeDef        clock    = ES9028_CLOCK_DEFAULT_CONFIG;
    ES9028_SAI_InitTypeDef          sai      = ES9028_SAI_DEFAULT_CONFIG;
    ES9028_MasterMode_InitTypeDef   master   = ES9028_MASTER_DEFAULT_CONFIG;
    ES9028_DPLL_InitTypeDef         dpll     = ES9028_DPLL_DEFAULT_CONFIG;
    ES9028_Filter_InitTypeDef       filter   = ES9028_FILTER_DEFAULT_CONFIG;
    ES9028_DeEmphasis_InitTypeDef   deemph   = ES9028_DEEMPH_DEFAULT_CONFIG;
    ES9028_Automute_InitTypeDef     automute = ES9028_AUTOMUTE_DEFAULT_CONFIG;
    ES9028_Mix_InitTypeDef          mix      = ES9028_MIX_DEFAULT_CONFIG;
    ES9028_GPIO_InitTypeDef         gpio     = ES9028_GPIO_DEFAULT_CONFIG;
    ES9028_SoftStart_InitTypeDef    sstart   = ES9028_SOFTSTART_DEFAULT_CONFIG;
    ES9028_StateTypeDef             dev_ok   = ES9028_DISABLE;

    /* 1. 软复位, 恢复全部上电默认值 */
    ES9028_CHK(ES9028_SoftReset());

    /* 2. 芯片ID校验 (Reg64[4:2] 应为 3'd1) */
    ES9028_CHK(ES9028_CheckDevice(&dev_ok));
    if (dev_ok != ES9028_ENABLE) {
        return ES9028_ERROR;
    }

    /* 3. 时钟: 振荡器全偏置 (本地晶振 24.576MHz; 本芯片无 clk_gear) */
    ES9028_CHK(ES9028_Clock_Config(&clock));

    /* 4. SAI: I2S / 32bit / 串行输入 / 关闭自动检测 (显式指定) */
    ES9028_CHK(ES9028_SAI_Config(&sai));

    /* 5. 主从: 从模式, BCLK=MCLK/4 (仅主模式生效), 锁定速度默认 */
    ES9028_CHK(ES9028_MasterMode_Config(&master));

    /* 6. DPLL 带宽默认 (I2S=5, DSD=10) */
    ES9028_CHK(ES9028_DPLL_Config(&dpll));

    /* 7. 数字滤波器: 快速衰减 + IIR 标准带宽, 不旁路 */
    ES9028_CHK(ES9028_Filter_Config(&filter));

    /* 8. 去加重默认旁路; 音量斜坡速率默认 */
    ES9028_CHK(ES9028_DeEmphasis_Config(&deemph));
    ES9028_CHK(ES9028_VolumeRamp_Config(ES9028_VOLUME_RATE_DEFAULT));

    /* 9. Automute 关闭 */
    ES9028_CHK(ES9028_Automute_Config(&automute));

    /* 10. 通道映射: 通道1<-左, 通道2<-右; SPDIF 源 = DATA_CLK */
    ES9028_CHK(ES9028_Mix_Config(&mix));
    ES9028_CHK(ES9028_SPDIF_Select(ES9028_SPDIF_IN_DATA_CLK));

    /* 11. GPIO 默认功能 (双输入, InputSel 选择 I2S) */
    ES9028_CHK(ES9028_GPIO_Config(&gpio));

    /* 12. 软启动: 输出 ramp 至 AVCC/2 (产生模拟输出) */
    ES9028_CHK(ES9028_SoftStart_Config(&sstart));

    /* 13. 解除双通道静音 */
    ES9028_CHK(ES9028_MuteCmd(ES9028_CHANNEL_1, ES9028_DISABLE));
    ES9028_CHK(ES9028_MuteCmd(ES9028_CHANNEL_2, ES9028_DISABLE));

    return ES9028_OK;
}

/* ========================================================================= */
/* 时钟块: Reg0 (振荡器偏置)                                                   */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_Clock_Config(const ES9028_Clock_InitTypeDef *Cfg)
{
    static const ES9028_Clock_InitTypeDef def = ES9028_CLOCK_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg0 整字节写出: [7:4] osc_drv, [3:1]保留=0 (手册要求), [0]软复位=0 */
    val = (uint8_t)(Cfg->OscDrive & ES9028_SYS_OSC_DRV_Msk);
    return es9028_wr(ES9028_REG_SYSTEM, val);
}

/* ========================================================================= */
/* SAI 串行音频接口: Reg1                                                     */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_SAI_Config(const ES9028_SAI_InitTypeDef *Cfg)
{
    static const ES9028_SAI_InitTypeDef def = ES9028_SAI_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg1 整字节写出; 注意: InputSource 仅当 AutoSelect=DISABLE 时生效;
     * 本芯片仅支持 I2S 与左对齐格式                                          */
    val = (uint8_t)((Cfg->WordLength  & ES9028_IN_I2S_LEN_Msk)  |
                    (Cfg->Format      & ES9028_IN_I2S_MODE_Msk) |
                    (Cfg->AutoSelect  & ES9028_IN_AUTO_SEL_Msk) |
                    (Cfg->InputSource & ES9028_IN_SEL_Msk));
    return es9028_wr(ES9028_REG_INPUT_SEL, val);
}

/* ========================================================================= */
/* 主模式: Reg10                                                              */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_MasterMode_Config(const ES9028_MasterMode_InitTypeDef *Cfg)
{
    static const ES9028_MasterMode_InitTypeDef def = ES9028_MASTER_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg10 整字节均为功能位; 主模式仅支持 I2S + 32bit (手册要求),
     * 帧时钟 = BCLK/64; FSR>96kHz 时 StopDiv 应设为 ES9028_STOP_DIV_16384  */
    val = (uint8_t)((Cfg->Mode         != ES9028_DISABLE ? ES9028_MASTER_MODE_Msk : 0U) |
                    (Cfg->ClockDivider & ES9028_CLOCK_DIV_Msk)                         |
                    (Cfg->Sync128Fs    != ES9028_DISABLE ? ES9028_SYNC_MODE_Msk : 0U)  |
                    (Cfg->StopDiv      & ES9028_STOP_DIV_Msk));
    return es9028_wr(ES9028_REG_MASTER_MODE, val);
}

/* ========================================================================= */
/* DPLL 带宽: Reg12                                                           */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_DPLL_Config(const ES9028_DPLL_InitTypeDef *Cfg)
{
    static const ES9028_DPLL_InitTypeDef def = ES9028_DPLL_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg12 整字节均为功能位: [7:4] I2S/SPDIF 带宽, [3:0] DSD 带宽 */
    val = (uint8_t)((Cfg->I2sDpllBw & ES9028_DPLL_BW_I2S_Msk) |
                    (Cfg->DsdDpllBw & ES9028_DPLL_BW_DSD_Msk));
    return es9028_wr(ES9028_REG_DPLL_BW, val);
}

/* ========================================================================= */
/* 数字滤波器: Reg7[6:2] + Reg21[2],[0]                                        */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_Filter_Config(const ES9028_Filter_InitTypeDef *Cfg)
{
    static const ES9028_Filter_InitTypeDef def = ES9028_FILTER_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg7: 读改写保留 [7]、[4] (保持默认电平) 与 [1:0] 静音位 */
    mask = (uint8_t)(ES9028_FILTER_SHAPE_Msk | ES9028_IIR_BW_Msk);
    val  = (uint8_t)((Cfg->Shape    & ES9028_FILTER_SHAPE_Msk) |
                     (Cfg->IirBandwidth & ES9028_IIR_BW_Msk));
    ES9028_CHK(es9028_mod(ES9028_REG_GENERAL_CFG, mask, val));

    /* Reg21: 读改写保留 [3]、[1] 与 GPIO 输入选择位 */
    mask = (uint8_t)(ES9028_IIR_BYPASS_Msk | ES9028_OSF_BYPASS_Msk);
    val  = (uint8_t)((Cfg->BypassIir != ES9028_DISABLE ? ES9028_IIR_BYPASS_Msk : 0U) |
                     (Cfg->BypassOsf != ES9028_DISABLE ? ES9028_OSF_BYPASS_Msk : 0U));
    return es9028_mod(ES9028_REG_GPIO_INPUT_SEL, mask, val);
}

/* ========================================================================= */
/* 去加重: Reg6[7:4] / 音量斜坡速率: Reg6[2:0]                                 */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_DeEmphasis_Config(const ES9028_DeEmphasis_InitTypeDef *Cfg)
{
    static const ES9028_DeEmphasis_InitTypeDef def = ES9028_DEEMPH_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg6: 读改写 [7:4], 保留 [3] 必须保持 1, vol_rate [2:0] 不动 */
    mask = (uint8_t)(ES9028_DEEMPH_AUTO_Msk | ES9028_DEEMPH_BYPASS_Msk |
                     ES9028_DEEMPH_SEL_Msk);
    val  = (uint8_t)((Cfg->AutoEnable != ES9028_DISABLE ? ES9028_DEEMPH_AUTO_Msk : 0U) |
                     (Cfg->Bypass     != ES9028_DISABLE ? ES9028_DEEMPH_BYPASS_Msk : 0U) |
                     (Cfg->Select     & ES9028_DEEMPH_SEL_Msk));
    return es9028_mod(ES9028_REG_DEEMPH, mask, val);
}

ES9028_StatusTypeDef ES9028_VolumeRamp_Config(uint32_t RampRate)
{
    if (RampRate > ES9028_VOLUME_RATE_Msk) {
        return ES9028_ERROR;
    }
    /* Reg6[2:0]: 斜坡速率 = 0.0078125*fs / 2^(rate-5) dB/s */
    return es9028_mod(ES9028_REG_DEEMPH, (uint8_t)ES9028_VOLUME_RATE_Msk,
                      (uint8_t)RampRate);
}

/* ========================================================================= */
/* 音量 / Master Trim: Reg15-20                                               */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_Volume_Set(uint8_t Channel, uint8_t Attenuation05)
{
    uint8_t reg;

    switch (Channel) {
    case ES9028_CHANNEL_1:
        reg = ES9028_REG_VOLUME1;
        break;
    case ES9028_CHANNEL_2:
        reg = ES9028_REG_VOLUME2;
        break;
    default:
        return ES9028_ERROR;
    }
    return es9028_wr(reg, Attenuation05);
}

ES9028_StatusTypeDef ES9028_MasterTrim_Set(uint32_t Trim)
{
    /* 32位有符号, 满刻度 0x7FFFFFFF (手册: 不得超过) */
    if (Trim > ES9028_MASTER_TRIM_FULL_SCALE) {
        return ES9028_ERROR;
    }
    /* 手册: Reg17 = LSB, Reg20 = MSB (小端) */
    return es9028_wr_multi(ES9028_REG_MASTER_TRIM1, Trim, 4U);
}

/* ========================================================================= */
/* 静音: Reg7[1:0] (按通道独立)                                                */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_MuteCmd(uint8_t Channel, ES9028_StateTypeDef State)
{
    uint8_t bit;

    switch (Channel) {
    case ES9028_CHANNEL_1:
        bit = ES9028_MUTE_CH1_Msk;
        break;
    case ES9028_CHANNEL_2:
        bit = ES9028_MUTE_CH2_Msk;
        break;
    default:
        return ES9028_ERROR;
    }
    return es9028_mod(ES9028_REG_GENERAL_CFG, bit,
                      (State != ES9028_DISABLE) ? bit : 0U);
}

/* ========================================================================= */
/* Automute: Reg4 / Reg5                                                      */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_Automute_Config(const ES9028_Automute_InitTypeDef *Cfg)
{
    static const ES9028_Automute_InitTypeDef def = ES9028_AUTOMUTE_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg4 触发时间 (0 = 关闭 automute) */
    ES9028_CHK(es9028_wr(ES9028_REG_AUTOMUTE_TIME, Cfg->Time));

    /* Reg5 整字节均为功能位: [7] 触发动作, [6:0] 门限 */
    val = (uint8_t)((Cfg->Action & ES9028_AUTOMUTE_LOOPBACK_Msk) |
                    (Cfg->Level  & ES9028_AUTOMUTE_LEVEL_Msk));
    return es9028_wr(ES9028_REG_AUTOMUTE_LEVEL, val);
}

/* ========================================================================= */
/* 软启动: Reg14                                                              */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_SoftStart_Config(const ES9028_SoftStart_InitTypeDef *Cfg)
{
    static const ES9028_SoftStart_InitTypeDef def = ES9028_SOFTSTART_DEFAULT_CONFIG;
    uint8_t time;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    time = Cfg->Time;
    if (time > 20U) {
        time = 20U;                     /* 手册: 有效范围 0~20 */
    }
    /* Reg14 整字节均为功能位 */
    val = (uint8_t)((Cfg->OutputEnable   != ES9028_DISABLE ? ES9028_SOFT_START_Msk : 0U) |
                    (Cfg->ForceLowOnLock != ES9028_DISABLE ? ES9028_SOFT_START_ON_LOCK_Msk : 0U) |
                    (Cfg->MuteOnLock     != ES9028_DISABLE ? ES9028_MUTE_ON_LOCK_Msk : 0U)        |
                    (time & ES9028_SOFT_START_TIME_Msk));
    return es9028_wr(ES9028_REG_SOFT_START, val);
}

/* ========================================================================= */
/* GPIO: Reg8 + Reg21[7:4]                                                    */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_GPIO_Config(const ES9028_GPIO_InitTypeDef *Cfg)
{
    static const ES9028_GPIO_InitTypeDef def = ES9028_GPIO_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg8 整字节均为功能位: [7:4] gpio2_cfg, [3:0] gpio1_cfg */
    val = (uint8_t)(((Cfg->Gpio2Func & 0x0FU) << 4) | (Cfg->Gpio1Func & 0x0FU));
    ES9028_CHK(es9028_wr(ES9028_REG_GPIO_CFG, val));

    /* Reg21: [7:6] 高电平输入源, [5:4] 低电平输入源 (读改写保留旁路/保留位) */
    val = (uint8_t)(((Cfg->GpioHighLevelSel & 0x03U) << 6) |
                    ((Cfg->GpioLowLevelSel  & 0x03U) << 4));
    return es9028_mod(ES9028_REG_GPIO_INPUT_SEL,
                      (uint8_t)(ES9028_GPIO_IN_SEL1_Msk | ES9028_GPIO_IN_SEL2_Msk),
                      val);
}

/* ========================================================================= */
/* SPDIF 输入源: Reg11[6:4]                                                    */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_SPDIF_Select(uint32_t Source)
{
    /* Reg11: 读改写 [6:4], 保留 [7] 与通道映射位不动 */
    return es9028_mod(ES9028_REG_CHANNEL_MAP, (uint8_t)ES9028_MAP_SPDIF_SEL_Msk,
                      (uint8_t)(Source & ES9028_MAP_SPDIF_SEL_Msk));
}

/* ========================================================================= */
/* 通道映射: Reg11[3:0]                                                       */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_Mix_Config(const ES9028_Mix_InitTypeDef *Cfg)
{
    static const ES9028_Mix_InitTypeDef def = ES9028_MIX_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg11: [3:2] 模拟交换, [1] 通道2源, [0] 通道1源 (读改写保留 [7]/SPDIF) */
    val = (uint8_t)(((Cfg->Dac1Source       & 0x01U) << 0) |
                    ((Cfg->Dac2Source       & 0x01U) << 1) |
                    (Cfg->Dac1AnalogSwap != ES9028_DISABLE ? ES9028_CH1_ANALOG_SWAP_Msk : 0U) |
                    (Cfg->Dac2AnalogSwap != ES9028_DISABLE ? ES9028_CH2_ANALOG_SWAP_Msk : 0U));
    return es9028_mod(ES9028_REG_CHANNEL_MAP,
                      (uint8_t)(ES9028_CH1_SEL_Msk | ES9028_CH2_SEL_Msk |
                                ES9028_CH1_ANALOG_SWAP_Msk | ES9028_CH2_ANALOG_SWAP_Msk),
                      val);
}

/* ========================================================================= */
/* THD 补偿: Reg13 / Reg22-25 / Reg34-38                                       */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_THD_Config(const ES9028_THD_InitTypeDef *Cfg)
{
    static const ES9028_THD_InitTypeDef def = ES9028_THD_DEFAULT_CONFIG;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg13[6] bypass_thd 位语义反转: 0=使能, 1=禁用 (保留位读改写) */
    ES9028_CHK(es9028_mod(ES9028_REG_THD_BYPASS, ES9028_THD_BYPASS_Msk,
                          (Cfg->Enable != ES9028_DISABLE) ? ES9028_THD_ENABLE
                                                          : ES9028_THD_DISABLE));

    /* 左/共享系数: 手册 Reg22=LSB, Reg23=MSB (小端) */
    ES9028_CHK(es9028_wr_multi(ES9028_REG_THD_COMP_C2_1,
                               (uint32_t)(uint16_t)Cfg->CoefC2, 2U));
    ES9028_CHK(es9028_wr_multi(ES9028_REG_THD_COMP_C3_1,
                               (uint32_t)(uint16_t)Cfg->CoefC3, 2U));

    /* Reg38: 左右独立使能 (保留位读改写) */
    ES9028_CHK(es9028_mod(ES9028_REG_THD_SEPARATE, ES9028_THD_SEPARATE_Msk,
                          (Cfg->SeparateRight != ES9028_DISABLE)
                              ? ES9028_THD_SEPARATE_Msk : 0U));

    /* 右声道系数: 手册 Reg34=LSB, Reg35=MSB (小端) */
    ES9028_CHK(es9028_wr_multi(ES9028_REG_THD_COMP_C2_R1,
                               (uint32_t)(uint16_t)Cfg->CoefC2Right, 2U));
    return es9028_wr_multi(ES9028_REG_THD_COMP_C3_R1,
                           (uint32_t)(uint16_t)Cfg->CoefC3Right, 2U);
}

/* ========================================================================= */
/* SLIMbus: Reg43 (保留位读改写)                                               */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_SLIMbus_Config(const ES9028_SLIMbus_InitTypeDef *Cfg)
{
    static const ES9028_SLIMbus_InitTypeDef def = ES9028_SLIMBUS_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    mask = (uint8_t)(ES9028_SB_AUX_WK_EN_Msk | ES9028_SB_AUX_WK_OVERRIDE_Msk |
                     ES9028_SB_DRV_STRENGTH_EN_Msk | ES9028_SB_DRV_STRENGTH_OVR_Msk);
    val  = (uint8_t)((Cfg->AuxWeakEnable       != ES9028_DISABLE ? ES9028_SB_AUX_WK_EN_Msk : 0U)       |
                     (Cfg->AuxWeakOverride     != ES9028_DISABLE ? ES9028_SB_AUX_WK_OVERRIDE_Msk : 0U) |
                     (Cfg->DrvStrengthEnable   != ES9028_DISABLE ? ES9028_SB_DRV_STRENGTH_EN_Msk : 0U) |
                     (Cfg->DrvStrengthOverride != ES9028_DISABLE ? ES9028_SB_DRV_STRENGTH_OVR_Msk : 0U));
    return es9028_mod(ES9028_REG_SLIMBUS, mask, val);
}

/* ========================================================================= */
/* 可编程 FIR: Reg26-30 (与手册 Example Source Code 一致)                      */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_FIR_Load(const int32_t *Coef, uint16_t Count)
{
    uint16_t i;
    uint8_t  stage;

    if (Coef == NULL) {
        return ES9028_ERROR;
    }
    /* 128 系数 -> stage1; 16 系数 -> stage2 (手册约定, stage 位于 Reg26[7]) */
    if (Count == 128U) {
        stage = ES9028_FIR_STAGE_1;
    } else if (Count == 16U) {
        stage = ES9028_FIR_STAGE_2;
    } else {
        return ES9028_ERROR;
    }

    for (i = 0; i < Count; i++) {
        uint32_t c = (uint32_t)Coef[i] & 0x00FFFFFFUL;   /* 24位有符号量化值 */

        /* Reg26: [7] stage, [6:0] 系数地址 */
        ES9028_CHK(es9028_wr(ES9028_REG_PROG_FIR_ADDR,
                             (uint8_t)(stage | (i & ES9028_FIR_ADDR_Msk))));

        /* Reg27-29: 系数数据 (手册示例: reg27 写低字节, 小端) */
        ES9028_CHK(es9028_wr(ES9028_REG_PROG_FIR_DATA_1, (uint8_t)(c & 0xFFU)));
        ES9028_CHK(es9028_wr(ES9028_REG_PROG_FIR_DATA_2, (uint8_t)((c >> 8) & 0xFFU)));
        ES9028_CHK(es9028_wr(ES9028_REG_PROG_FIR_DATA_3, (uint8_t)((c >> 16) & 0xFFU)));

        /* Reg30: 置 prog_coeff_we 完成一次写入 (写期间自定义滤波器被旁路) */
        ES9028_CHK(es9028_wr(ES9028_REG_PROG_FIR_CFG, ES9028_FIR_CFG_PROG_WE_Msk));
    }
    /* 写完仅关闭 prog_coeff_we, 保留 prog_coeff_en / even_stage2 配置 */
    return es9028_mod(ES9028_REG_PROG_FIR_CFG, ES9028_FIR_CFG_PROG_WE_Msk, 0U);
}

ES9028_StatusTypeDef ES9028_FIR_Cmd(ES9028_StateTypeDef State)
{
    return es9028_mod(ES9028_REG_PROG_FIR_CFG, ES9028_FIR_CFG_PROG_EN_Msk,
                      (State != ES9028_DISABLE) ? ES9028_FIR_CFG_PROG_EN_Msk : 0U);
}

/* ========================================================================= */
/* 睡眠 / 唤醒                                                                */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_Sleep(void)
{
    uint32_t ramp_ms;

    /* 1. 双通道软静音, 防止 ramp 过程中的咔哒声 */
    ES9028_CHK(ES9028_MuteCmd(ES9028_CHANNEL_1, ES9028_ENABLE));
    ES9028_CHK(ES9028_MuteCmd(ES9028_CHANNEL_2, ES9028_ENABLE));

    /* 2. 模拟输出 ramp 到地 (soft_start=0) */
    ES9028_CHK(es9028_mod(ES9028_REG_SOFT_START, ES9028_SOFT_START_Msk, 0U));

    /* 3. 等待 ramp 到地完成 (时长 = 软启动斜坡时间) */
    ramp_ms = es9028_softstart_ms();
    if (ramp_ms > 0U) {
        es9028_bus_delay_ms(ramp_ms);
    }
    return ES9028_OK;
}

ES9028_StatusTypeDef ES9028_Wake(void)
{
    uint32_t ramp_ms;

    /* 1. 模拟输出 ramp 到 AVCC/2 (soft_start=1, 恢复模拟输出) */
    ES9028_CHK(es9028_mod(ES9028_REG_SOFT_START, ES9028_SOFT_START_Msk,
                          ES9028_SOFT_START_Msk));

    /* 2. 解除双通道静音 (保持睡眠前的音量寄存器值不变) */
    ES9028_CHK(ES9028_MuteCmd(ES9028_CHANNEL_1, ES9028_DISABLE));
    ES9028_CHK(ES9028_MuteCmd(ES9028_CHANNEL_2, ES9028_DISABLE));

    /* 3. 等待 ramp 到 AVCC/2 完成 */
    ramp_ms = es9028_softstart_ms();
    if (ramp_ms > 0U) {
        es9028_bus_delay_ms(ramp_ms);
    }
    return ES9028_OK;
}

/* ========================================================================= */
/* 状态回读                                                                    */
/* ========================================================================= */

ES9028_StatusTypeDef ES9028_GetChipId(uint8_t *ChipStatus)
{
    return es9028_rd(ES9028_REG_CHIP_STATUS, ChipStatus);
}

ES9028_StatusTypeDef ES9028_CheckDevice(ES9028_StateTypeDef *Result)
{
    uint8_t id;

    if (Result == NULL) {
        return ES9028_ERROR;
    }
    *Result = ES9028_DISABLE;
    ES9028_CHK(es9028_rd(ES9028_REG_CHIP_STATUS, &id));
    if (((id & ES9028_CHIP_ID_Msk) >> ES9028_CHIP_ID_Pos) == ES9028_CHIP_ID_VALUE) {
        *Result = ES9028_ENABLE;
    }
    return ES9028_OK;
}

ES9028_StatusTypeDef ES9028_GetLockStatus(ES9028_StateTypeDef *State)
{
    uint8_t id;

    if (State == NULL) {
        return ES9028_ERROR;
    }
    ES9028_CHK(es9028_rd(ES9028_REG_CHIP_STATUS, &id));
    *State = ((id & ES9028_STATUS_LOCK_Msk) != 0U) ? ES9028_ENABLE : ES9028_DISABLE;
    return ES9028_OK;
}

ES9028_StatusTypeDef ES9028_GetAutomuteStatus(ES9028_StateTypeDef *State)
{
    uint8_t id;

    if (State == NULL) {
        return ES9028_ERROR;
    }
    ES9028_CHK(es9028_rd(ES9028_REG_CHIP_STATUS, &id));
    *State = ((id & ES9028_STATUS_AUTOMUTE_Msk) != 0U) ? ES9028_ENABLE : ES9028_DISABLE;
    return ES9028_OK;
}

ES9028_StatusTypeDef ES9028_WaitForLock(uint32_t TimeoutMs)
{
    uint32_t elapsed = 0U;

    for (;;) {
        ES9028_StateTypeDef lock = ES9028_DISABLE;

        ES9028_CHK(ES9028_GetLockStatus(&lock));
        if (lock == ES9028_ENABLE) {
            return ES9028_OK;
        }
        if (elapsed >= TimeoutMs) {
            return ES9028_ERROR;
        }
        es9028_bus_delay_ms(1U);
        elapsed++;
    }
}

ES9028_StatusTypeDef ES9028_GetDpllNumber(uint32_t *Number)
{
    /* 手册: "read from LSB to MSB as it is latched on the LSBs (register 66)"
     * 故必须先读 Reg66 触发锁存 */
    return es9028_rd_multi(ES9028_REG_DPLL_NUM_1, Number, 4U);
}

ES9028_StatusTypeDef ES9028_GetGpioLevel(uint8_t *Level)
{
    uint8_t v;

    if (Level == NULL) {
        return ES9028_ERROR;
    }
    ES9028_CHK(es9028_rd(ES9028_REG_GPIO_STATUS, &v));
    *Level = (uint8_t)(v & 0x03U);
    return ES9028_OK;
}
