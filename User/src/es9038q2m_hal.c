

#include <stddef.h>
#include "es9038q2m_hal.h"
#include "settings.h"

/* ========================================================================= */
/* 1) 移植层配置区 -- 对接用户自己的读写函数时只需修改本区                        */
/* ========================================================================= */

/* 库内部统一以 es9038_bus_xxx 为名调用总线; 私有宏把库内名字映射到外部
 * 函数名, 从而将库内对 es9038_bus_write 的所有调用替换为 I2C_Write 调用。
 * 移植时只需修改下面 3 行 #define 的右侧, 例如封装 STM32 HAL 的
 * HAL_I2C_Mem_Write 为 I2C_Write 后即可直接对接。                        */
#define es9038_bus_write I2C_Write
#define es9038_bus_read I2C_Read
#define es9038_bus_delay_ms Delay_ms

/* 外部必须实现的三个函数 (宏替换后实际声明/调用的是右侧名字):
 *   int  I2C_Write(uint8_t reg, uint8_t value);   返回 0 成功
 *   int  I2C_Read (uint8_t reg, uint8_t *value);  返回 0 成功, 单寄存器读
 *   void Delay_ms (uint32_t ms);                  毫秒延时
 * 读写均为单寄存器操作 (reg 为寄存器地址); 放在任意用户文件中链接即可。      */
extern int es9038_bus_write (uint8_t reg, uint8_t value);
extern int es9038_bus_read (uint8_t reg, uint8_t *value);
extern void es9038_bus_delay_ms (uint32_t ms);

/* 多字节寄存器字节序: 0 = 小端 (低字节存低地址, 默认, 依据手册 FIR 示例与
 * DPLL/ADC "LSB锁存" 说明); 1 = 大端。如实测相反, 改为 1 即可整体翻转。   */
#ifndef ES9038_REG_MSB_FIRST
#define ES9038_REG_MSB_FIRST 0
#endif

/* ========================================================================= */
/* 2) 内部辅助函数                                                            */
/* ========================================================================= */

/** 内部检查宏: 调用失败立即返回 ES9038_ERROR */
#define ES9038_CHK(call)           \
    do {                           \
        if ((call) != ES9038_OK) { \
            return ES9038_ERROR;   \
        }                          \
    } while (0)

/** 单寄存器写 */
static ES9038_StatusTypeDef es9038_wr (uint8_t reg, uint8_t val) {
    return (es9038_bus_write (reg, val) == 0) ? ES9038_OK : ES9038_ERROR;
}

/** 单寄存器读 */
static ES9038_StatusTypeDef es9038_rd (uint8_t reg, uint8_t *val) {
    if (val == NULL) {
        return ES9038_ERROR;
    }
    return (es9038_bus_read (reg, val) == 0) ? ES9038_OK : ES9038_ERROR;
}

/** 读改写: (old & ~mask) | (val & mask) */
static ES9038_StatusTypeDef es9038_mod (uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t old;

    ES9038_CHK (es9038_rd (reg, &old));
    old = (uint8_t)((old & (uint8_t)~mask) | (val & mask));
    return es9038_wr (reg, old);
}

/**
 * 多字节寄存器写: 依次写 base, base+1, ...
 * 字节顺序按 ES9038_REG_MSB_FIRST (0: 低字节在低地址)
 */
static ES9038_StatusTypeDef es9038_wr_multi (uint8_t base, uint32_t val, uint8_t bytes) {
    uint8_t i;

    for (i = 0; i < bytes; i++) {
        uint8_t shift = (ES9038_REG_MSB_FIRST != 0)
                            ? (uint8_t)(8U * (bytes - 1U - i))
                            : (uint8_t)(8U * i);
        ES9038_CHK (es9038_wr ((uint8_t)(base + i), (uint8_t)((val >> shift) & 0xFFU)));
    }
    return ES9038_OK;
}

/** 多字节寄存器读 (逐寄存器单字节读, 避开手册禁止的多字节连续读) */
static ES9038_StatusTypeDef es9038_rd_multi (uint8_t base, uint32_t *val, uint8_t bytes) {
    uint8_t i;
    uint32_t v = 0;

    if (val == NULL) {
        return ES9038_ERROR;
    }
    for (i = 0; i < bytes; i++) {
        uint8_t b;
        uint8_t shift = (ES9038_REG_MSB_FIRST != 0)
                            ? (uint8_t)(8U * (bytes - 1U - i))
                            : (uint8_t)(8U * i);
        ES9038_CHK (es9038_rd ((uint8_t)(base + i), &b));
        v |= ((uint32_t)b) << shift;
    }
    *val = v;
    return ES9038_OK;
}

/**
 * 当前软启动斜坡时间 (ms), 公式 (手册 Reg14): t = 4096 * 2^(time+1) / MCLK
 * 读取失败返回 0
 */
static uint32_t es9038_softstart_ms (void) {
    uint8_t t;
    uint64_t ticks;

    if (es9038_rd (ES9038_REG_SOFT_START, &t) != ES9038_OK) {
        return 0;
    }
    t &= ES9038_SOFT_START_TIME_Msk;
    if (t > 20U) {
        t = 20U;                   /* 手册: 有效范围 0~20 */
    }
    ticks = (4096ULL << (t + 1U)); /* 4096 * 2^(t+1) 个 MCLK 周期 */
    return (uint32_t)((ticks * 1000ULL) / (uint64_t)ES9038_MCLK_HZ);
}

/* ========================================================================= */
/* 3) 低层寄存器访问 API                                                       */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_WriteReg (uint8_t RegAddr, uint8_t Value) {
    return es9038_wr (RegAddr, Value);
}

ES9038_StatusTypeDef ES9038_ReadReg (uint8_t RegAddr, uint8_t *Value) {
    return es9038_rd (RegAddr, Value);
}

ES9038_StatusTypeDef ES9038_ModifyReg (uint8_t RegAddr, uint8_t Mask, uint8_t Value) {
    return es9038_mod (RegAddr, Mask, Value);
}

/* ========================================================================= */
/* 整机初始化 / 复位                                                           */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_SoftReset (void) {
    uint8_t v;
    uint8_t n;


    (void)es9038_wr (ES9038_REG_SYSTEM, ES9038_SYS_SOFT_RESET_Msk);


    for (n = 0u; n < 50u; n++) {
        es9038_bus_delay_ms (2U);
        if (es9038_rd (ES9038_REG_SYSTEM, &v) == ES9038_OK) {
            if ((v & ES9038_SYS_SOFT_RESET_Msk) == 0u) {
                return ES9038_OK;
            }
        }
    }
    return ES9038_ERROR;
}


volatile uint8_t es9038_init_step;

#define ES9038_STEP(n, call)             \
    do {                                 \
        es9038_init_step = (uint8_t)(n); \
        if ((call) != ES9038_OK) {       \
            return ES9038_ERROR;         \
        }                                \
    } while (0)

ES9038_StatusTypeDef ES9038_Init (void) {
    ES9038_Clock_InitTypeDef clock = ES9038_CLOCK_DEFAULT_CONFIG;
    ES9038_SAI_InitTypeDef sai = ES9038_SAI_DEFAULT_CONFIG;
    ES9038_MasterMode_InitTypeDef master = ES9038_MASTER_DEFAULT_CONFIG;
    ES9038_ASRC_InitTypeDef asrc = ES9038_ASRC_DEFAULT_CONFIG;
    ES9038_Filter_InitTypeDef filter = ES9038_FILTER_DEFAULT_CONFIG;
    ES9038_DeEmphasis_InitTypeDef deemph = ES9038_DEEMPH_DEFAULT_CONFIG;
    ES9038_Volume_InitTypeDef volume = ES9038_VOLUME_DEFAULT_CONFIG;
    ES9038_Automute_InitTypeDef automute = ES9038_AUTOMUTE_DEFAULT_CONFIG;
    ES9038_SoftStart_InitTypeDef sstart = ES9038_SOFTSTART_DEFAULT_CONFIG;
    ES9038_Power_InitTypeDef power = ES9038_POWER_DEFAULT_CONFIG;
    ES9038_Mix_InitTypeDef mix = ES9038_MIX_DEFAULT_CONFIG;
    ES9038_SPDIF_InitTypeDef spdif = ES9038_SPDIF_DEFAULT_CONFIG;
    ES9038_GPIO_InitTypeDef gpio = ES9038_GPIO_DEFAULT_CONFIG;
    ES9038_StateTypeDef dev_ok = ES9038_DISABLE;

    /* 1. 软复位, 恢复全部上电默认值 */
    ES9038_STEP (1, ES9038_SoftReset());

    Delay_ms (1);

    /* 2. 芯片ID校验 (Reg64[7:2] 应为 0x1C) */
    ES9038_STEP (2, ES9038_CheckDevice (&dev_ok));
    if (dev_ok != ES9038_ENABLE) {
        return ES9038_ERROR;
    }

    /* 3. 时钟: 振荡器全偏置, MCLK=XI (本地晶振 98.304MHz, 不分频) */
    ES9038_STEP (3, ES9038_Clock_Config (&clock));

    /* 4. SAI: I2S / 32bit / 串行输入 / 关闭自动检测 (显式指定) */
    ES9038_STEP (4, ES9038_SAI_Config (&sai));

    /* 5. 主从: 从模式, BCLK=MCLK/2 (仅主模式生效), lock速度默认 */
    master.LockSpeed=DAC_LOCK_SPEED;
    ES9038_STEP (5, ES9038_MasterMode_Config (&master));

    /* 6. ASRC 使能 + DPLL 带宽默认 */
    asrc.SerialDpllBw=(DAC_DPLL_BW_SERIAL & 15u)<<4;
    ES9038_STEP (6, ES9038_ASRC_Config (&asrc));

    /* 7. 数字滤波器: 表偿快速衰减 (默认), 不旁路OSF */
    ES9038_STEP (7, ES9038_Filter_Config (&filter));

    /* 8. 去加重默认旁路; DoP 关闭 */
    ES9038_STEP (8, ES9038_DeEmphasis_Config (&deemph));
    ES9038_STEP (9, ES9038_DoP_Cmd (ES9038_DISABLE));

    /* 9. 音量: 斜坡速率默认, 系数实时锁存; 音量值保持上电默认 -40dB */
    volume.RampRate=DAC_VOLUME_RAMP_RATE;
    ES9038_STEP (10, ES9038_Volume_Config (&volume));

    /* 10. Automute 关闭 */
    ES9038_STEP (11, ES9038_Automute_Config (&automute));

    /* 11. 通道映射 交叉（DAC1<-ch2、DAC2<-ch1） */
    mix.Dac1Source=DAC_OUTPUT_CROSS ? ES9038_MIX_DAC1_FROM_CH2 : ES9038_MIX_DAC1_FROM_CH1;
    mix.Dac2Source=DAC_OUTPUT_CROSS ? ES9038_MIX_DAC2_FROM_CH1 : ES9038_MIX_DAC2_FROM_CH2;
    ES9038_STEP (12, ES9038_Mix_Config (&mix));

    /* 12. SPDIF 默认输入源 */
    ES9038_STEP (13, ES9038_SPDIF_Config (&spdif));

    /* 13. GPIO 默认功能 */
    ES9038_STEP (14, ES9038_GPIO_Config (&gpio));

    /* 14. 软启动: 输出 ramp 至 AVCC/2 (产生模拟输出) */
    ES9038_STEP (15, ES9038_SoftStart_Config (&sstart));

    /* 15. 电源: 打开耳机功放 (无外置功放的板子可在 Sleep/Power_Config 调整) */
    ES9038_STEP (16, ES9038_Power_Config (&power));

    /* 16. 解除静音 */
    ES9038_STEP (17, ES9038_MuteCmd (ES9038_DISABLE));

    /* External BCLK/WS slave: NCO mode belongs to internally generated clocks.
     * Keep reset value 0; nonzero also makes lock_status report a forced lock. */
    ES9038_STEP (18, es9038_wr_multi(ES9038_REG_NCO_1,0u,4u));
    /* ES9038Q2M 复位后 Reg15/Reg16（volume1/volume2）默认是
     * 8d80 = -40 dB（手册 Register 15-16: Volume Control原文
     * Default of 8'd80 (-40dB) */
    ES9038_Volume_Set (ES9038_CHANNEL_ALL, 0U);

    ES9038_MuteCmd (ES9038_DISABLE);

    es9038_init_step = 0u; /* 全部成功 */

    return ES9038_OK;
}

/* ========================================================================= */
/* 时钟块: Reg0 (振荡器偏置 / 内部时钟分频)                                    */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Clock_Config (const ES9038_Clock_InitTypeDef *Cfg) {
    static const ES9038_Clock_InitTypeDef def = ES9038_CLOCK_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg0 整字节写出: [7:4] osc_drv, [3:2] clk_gear, [1]保留=0, [0]软复位=0 */
    val = (uint8_t)((Cfg->OscDrive & ES9038_SYS_OSC_DRV_Msk) |
                    (Cfg->ClkGear & ES9038_SYS_CLK_GEAR_Msk));
    return es9038_wr (ES9038_REG_SYSTEM, val);
}

/* ========================================================================= */
/* SAI 串行音频接口: Reg1                                                     */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_SAI_Config (const ES9038_SAI_InitTypeDef *Cfg) {
    static const ES9038_SAI_InitTypeDef def = ES9038_SAI_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg1 整字节写出; 注意: InputSource 仅当 AutoSelect=DISABLE 时生效,
     * 主模式下手册要求显式选择输入源 (不可用自动检测)                       */
    val = (uint8_t)((Cfg->WordLength & ES9038_IN_SER_LEN_Msk) |
                    (Cfg->Format & ES9038_IN_SER_MODE_Msk) |
                    (Cfg->AutoSelect & ES9038_IN_AUTO_SEL_Msk) |
                    (Cfg->InputSource & ES9038_IN_SEL_Msk));
    return es9038_wr (ES9038_REG_INPUT_SEL, val);
}

/* ========================================================================= */
/* 主从模式与同步: Reg10                                                      */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_MasterMode_Config (const ES9038_MasterMode_InitTypeDef *Cfg) {
    static const ES9038_MasterMode_InitTypeDef def = ES9038_MASTER_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    val = (uint8_t)((Cfg->Mode != ES9038_DISABLE ? ES9038_MASTER_MODE_Msk : 0U) |
                    (Cfg->ClockDivider & ES9038_MASTER_DIV_Msk) |
                    (Cfg->Sync128Fs != ES9038_DISABLE ? ES9038_MASTER_128FS_MODE_Msk : 0U) |
                    (Cfg->LockSpeed & ES9038_LOCK_SPEED_Msk));
    return es9038_wr (ES9038_REG_MASTER_MODE, val);
}

/* ========================================================================= */
/* 主模式 NCO 采样率: Reg34-37                                                */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_SampleRate_Config (uint32_t SampleRateHz) {
    uint32_t nco;

    if ((SampleRateHz == 0U) || (SampleRateHz >= ES9038_MCLK_HZ)) {
        return ES9038_ERROR;
    }
    /* FSR = nco * MCLK / 2^32  =>  nco = FSR * 2^32 / MCLK (64位中间量) */
    nco = (uint32_t)(((uint64_t)SampleRateHz << 32) / (uint64_t)ES9038_MCLK_HZ);
    if (nco == 0U) {
        nco = 1U; /* 0 会使 NCO 模式被禁用 */
    }
    /* 注: NCO 要驱动引脚还需 Reg10[7] 主模式使能;
     * NCO 模式下 Reg10[6:5] master_div 被忽略                             */
    return es9038_wr_multi (ES9038_REG_NCO_1, nco, 4U);
}

/* ========================================================================= */
/* ASRC / DPLL 带宽: Reg12 + Reg27[7]                                        */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_ASRC_Config (const ES9038_ASRC_InitTypeDef *Cfg) {
    static const ES9038_ASRC_InitTypeDef def = ES9038_ASRC_DEFAULT_CONFIG;
    uint8_t bw;
    uint8_t gen;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg12: [7:4] I2S模式带宽, [3:0] DSD模式带宽 (整字节均为功能位) */
    bw = (uint8_t)((Cfg->SerialDpllBw & ES9038_DPLL_BW_SERIAL_Msk) |
                   (Cfg->DsdDpllBw & ES9038_DPLL_BW_DSD_Msk));
    ES9038_CHK (es9038_wr (ES9038_REG_ASRC_DPLL_BW, bw));

    /* Reg27 为读改写 (保留位[6:5]、[4]保持上电默认) */
    gen = (Cfg->AsrcEnable != ES9038_DISABLE) ? ES9038_GEN_ASRC_EN_Msk : 0U;
    return es9038_mod (ES9038_REG_GENERAL_CFG, ES9038_GEN_ASRC_EN_Msk, gen);
}

/* ========================================================================= */
/* 数字滤波器: Reg7 (保留 mute 位)                                             */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Filter_Config (const ES9038_Filter_InitTypeDef *Cfg) {
    static const ES9038_Filter_InitTypeDef def = ES9038_FILTER_DEFAULT_CONFIG;
    uint8_t val;
    uint8_t mask;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    mask = (uint8_t)(ES9038_FILTER_SHAPE_Msk | ES9038_FILTER_BYPASS_OSF_Msk);
    val = (uint8_t)((Cfg->Shape & ES9038_FILTER_SHAPE_Msk) |
                    (Cfg->BypassOsf != ES9038_DISABLE ? ES9038_FILTER_BYPASS_OSF_Msk : 0U));
    return es9038_mod (ES9038_REG_FILTER_MUTE, mask, val);
}

/* ========================================================================= */
/* 去加重: Reg6[7:4] / DoP: Reg6[3] / 音量斜坡速率: Reg6[2:0]                  */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_DeEmphasis_Config (const ES9038_DeEmphasis_InitTypeDef *Cfg) {
    static const ES9038_DeEmphasis_InitTypeDef def = ES9038_DEEMPH_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    mask = (uint8_t)(ES9038_DEEMPH_AUTO_Msk | ES9038_DEEMPH_BYPASS_Msk |
                     ES9038_DEEMPH_SEL_Msk);
    val = (uint8_t)((Cfg->AutoEnable != ES9038_DISABLE ? ES9038_DEEMPH_AUTO_Msk : 0U) |
                    (Cfg->Bypass != ES9038_DISABLE ? ES9038_DEEMPH_BYPASS_Msk : 0U) |
                    (Cfg->Select & ES9038_DEEMPH_SEL_Msk));
    return es9038_mod (ES9038_REG_DEEMPH, mask, val);
}

ES9038_StatusTypeDef ES9038_DoP_Cmd (ES9038_StateTypeDef State) {
    return es9038_mod (ES9038_REG_DEEMPH, ES9038_DOP_ENABLE_Msk,
                       (State != ES9038_DISABLE) ? ES9038_DOP_ENABLE_Msk : 0U);
}

/* ========================================================================= */
/* 音量: Reg6[2:0] / Reg15-16 / Reg17-20 / Reg27[3:0]                         */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Volume_Config (const ES9038_Volume_InitTypeDef *Cfg) {
    static const ES9038_Volume_InitTypeDef def = ES9038_VOLUME_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg6[2:0] volume_rate */
    ES9038_CHK (es9038_mod (ES9038_REG_DEEMPH, (uint8_t)ES9038_VOLUME_RATE_Msk,
                            (uint8_t)(Cfg->RampRate & ES9038_VOLUME_RATE_Msk)));

    /* Reg27: 通道共用 / 系数锁存 / +18dB增益 (读改写保留 ASRC 等位) */
    mask = (uint8_t)(ES9038_GEN_CH_VOLUME_Msk | ES9038_GEN_LATCH_VOLUME_Msk |
                     ES9038_GEN_18DB_GAIN_Msk);
    val = (uint8_t)((Cfg->ChannelShare != ES9038_DISABLE ? ES9038_GEN_CH_VOLUME_Msk : 0U) |
                    (Cfg->Latch != ES9038_DISABLE ? ES9038_GEN_LATCH_VOLUME_Msk : 0U) |
                    (Cfg->Gain18 & ES9038_GEN_18DB_GAIN_Msk));
    return es9038_mod (ES9038_REG_GENERAL_CFG, mask, val);
}


ES9038_StatusTypeDef ES9038_Volume_Set (uint8_t Channel, uint8_t Attenuation05) {
    // 通道到寄存器地址的映射表，下标对应 Channel 枚举值
    static const uint8_t reg_map[] = {
        [ES9038_CHANNEL_1] = ES9038_REG_VOLUME1,
        [ES9038_CHANNEL_2] = ES9038_REG_VOLUME2,
    };

    if (Channel == ES9038_CHANNEL_ALL) {
        // 所有通道：依次写入两个寄存器
        if (es9038_wr (ES9038_REG_VOLUME1, Attenuation05) != ES9038_OK) {
            return ES9038_ERROR;
        }
        return es9038_wr (ES9038_REG_VOLUME2, Attenuation05);
    }

    // 单个通道：查表获取寄存器地址
    if (Channel >= sizeof (reg_map) / sizeof (reg_map[0])) {
        return ES9038_ERROR;  // 非法通道号
    }

    return es9038_wr (reg_map[Channel], Attenuation05);
}

ES9038_StatusTypeDef ES9038_MasterTrim_Set (uint32_t Trim) {
    /* 32位有符号, 满刻度 0x7FFFFFFF (手册: 不得超过) */
    if (Trim > ES9038_MASTER_TRIM_FULL_SCALE) {
        return ES9038_ERROR;
    }
    return es9038_wr_multi (ES9038_REG_MASTER_TRIM1, Trim, 4U);
}

/* ========================================================================= */
/* 静音: Reg7[0]                                                              */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_MuteCmd (ES9038_StateTypeDef State) {
    return es9038_mod (ES9038_REG_FILTER_MUTE, ES9038_FILTER_MUTE_Msk,
                       (State != ES9038_DISABLE) ? ES9038_FILTER_MUTE_Msk : 0U);
}

/* ========================================================================= */
/* Automute: Reg2[7:6] / Reg4 / Reg5[6:0]                                     */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Automute_Config (const ES9038_Automute_InitTypeDef *Cfg) {
    static const ES9038_Automute_InitTypeDef def = ES9038_AUTOMUTE_DEFAULT_CONFIG;
    uint8_t level;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg2[7:6] 触发动作 (读改写, 保留通道映射位) */
    ES9038_CHK (es9038_mod (ES9038_REG_MIXING, (uint8_t)ES9038_MIX_AUTOMUTE_CFG_Msk,
                            (uint8_t)(Cfg->Action & ES9038_MIX_AUTOMUTE_CFG_Msk)));

    /* Reg4 触发时间 (0 = 关闭 automute) */
    ES9038_CHK (es9038_wr (ES9038_REG_AUTOMUTE_TIME, Cfg->Time));

    /* Reg5[6:0] 触发门限 (位7保留, 读改写) */
    level = (uint8_t)(Cfg->Level & ES9038_AUTOMUTE_LEVEL_Msk);
    return es9038_mod (ES9038_REG_AUTOMUTE_LEVEL, (uint8_t)ES9038_AUTOMUTE_LEVEL_Msk, level);
}

/* ========================================================================= */
/* 软启动: Reg14                                                              */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_SoftStart_Config (const ES9038_SoftStart_InitTypeDef *Cfg) {
    static const ES9038_SoftStart_InitTypeDef def = ES9038_SOFTSTART_DEFAULT_CONFIG;
    uint8_t time;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    time = Cfg->Time;
    if (time > 20U) {
        time = 20U; /* 手册: 有效范围 0~20 */
    }
    /* Reg14: [7] soft_start, [6] soft_start_on_lock, [4:0] time;
     * [5] 保留恒0, 整字节写出安全 */
    val = (uint8_t)((Cfg->OutputEnable != ES9038_DISABLE ? ES9038_SOFT_START_Msk : 0U) |
                    (Cfg->StartOnLock != ES9038_DISABLE ? ES9038_SOFT_START_ON_LOCK_Msk : 0U) |
                    (time & ES9038_SOFT_START_TIME_Msk));
    return es9038_wr (ES9038_REG_SOFT_START, val);
}

/* ========================================================================= */
/* 电源/功放: Reg39 + Reg30/31 (电荷泵时钟)                                   */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Power_Config (const ES9038_Power_InitTypeDef *Cfg) {
    static const ES9038_Power_InitTypeDef def = ES9038_POWER_DEFAULT_CONFIG;
    uint8_t amp;
    uint16_t cp;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg39: [7] amp_pdb_ss, [6] amp_pdb, [5:2]保留(默认0), [1:0] sw_ctrl_en */
    amp = (uint8_t)((Cfg->AmpEnable != ES9038_DISABLE ? ES9038_AMP_PDB_Msk : 0U) |
                    (Cfg->AmpAutoDown != ES9038_DISABLE ? ES9038_AMP_PDB_SS_Msk : 0U) |
                    (Cfg->SwitchCtrl & ES9038_SW_CTRL_EN_Msk));
    ES9038_CHK (es9038_wr (ES9038_REG_GENERAL_CFG_2, amp));

    /* Reg30-31 组成 16 位电荷泵时钟域 (见多字节字节序说明) */
    cp = (uint16_t)((Cfg->CpClkSource & ES9038_CP_CLK_SEL_Msk) |
                    (Cfg->CpClkEnable & ES9038_CP_CLK_EN_Msk) |
                    (Cfg->CpClkDiv & ES9038_CP_CLK_DIV_Msk));
    return es9038_wr_multi (ES9038_REG_CP_CLK_1, (uint32_t)cp, 2U);
}

/* ========================================================================= */
/* 睡眠 / 唤醒                                                                */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Sleep (void) {
    uint32_t ramp_ms;

    /* 1. 软静音, 防止 ramp 过程中的咔哒声 */
    ES9038_CHK (ES9038_MuteCmd (ES9038_ENABLE));

    /* 2. 模拟输出 ramp 到地 (soft_start=0) */
    ES9038_CHK (es9038_mod (ES9038_REG_SOFT_START, ES9038_SOFT_START_Msk, 0U));

    /* 3. 关断耳机功放 (Reg39[6]) */
    ES9038_CHK (es9038_mod (ES9038_REG_GENERAL_CFG_2, ES9038_AMP_PDB_Msk, 0U));

    /* 4. 等待 ramp 到地完成 (时长 = 软启动斜坡时间) */
    ramp_ms = es9038_softstart_ms();
    if (ramp_ms > 0U) {
        es9038_bus_delay_ms (ramp_ms);
    }
    return ES9038_OK;
}

ES9038_StatusTypeDef ES9038_Wake (void) {
    uint32_t ramp_ms;

    /* 1. 打开耳机功放 */
    ES9038_CHK (es9038_mod (ES9038_REG_GENERAL_CFG_2, ES9038_AMP_PDB_Msk, ES9038_AMP_PDB_Msk));

    /* 2. 模拟输出 ramp 到 AVCC/2 (soft_start=1, 恢复模拟输出) */
    ES9038_CHK (es9038_mod (ES9038_REG_SOFT_START, ES9038_SOFT_START_Msk, ES9038_SOFT_START_Msk));

    /* 3. 解除静音 (保持睡眠前的音量寄存器值不变) */
    ES9038_CHK (ES9038_MuteCmd (ES9038_DISABLE));

    /* 4. 等待 ramp 到 AVCC/2 完成 */
    ramp_ms = es9038_softstart_ms();
    if (ramp_ms > 0U) {
        es9038_bus_delay_ms (ramp_ms);
    }
    return ES9038_OK;
}

/* ========================================================================= */
/* GPIO: Reg8 / Reg21 / Reg29                                                 */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_GPIO_Config (const ES9038_GPIO_InitTypeDef *Cfg) {
    static const ES9038_GPIO_InitTypeDef def = ES9038_GPIO_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg8: [7:4] gpio2_cfg, [3:0] gpio1_cfg (整字节均为功能位) */
    val = (uint8_t)(((Cfg->Gpio2Func & 0x0FU) << 4) | (Cfg->Gpio1Func & 0x0FU));
    ES9038_CHK (es9038_wr (ES9038_REG_GPIO12_CFG, val));

    /* Reg21: [7:6] gpio_sel2, [5:4] gpio_sel1, [3:0] 保留 (读改写) */
    val = (uint8_t)(((Cfg->Gpio2InputSel & 0x03U) << 6) |
                    ((Cfg->Gpio1InputSel & 0x03U) << 4));
    ES9038_CHK (es9038_mod (ES9038_REG_GPIO_INPUT_SEL, (uint8_t)(ES9038_GPIO1_IN_SEL_Msk | ES9038_GPIO2_IN_SEL_Msk),
                            val));

    /* Reg29: [7:6] invert_gpio, [5:0] 保留 (读改写) */
    val = (uint8_t)((Cfg->Invert & 0x03U) << 6);
    return es9038_mod (ES9038_REG_GPIO_CFG, (uint8_t)ES9038_GPIO_INVERT_Msk, val);
}

/* ========================================================================= */
/* SPDIF: Reg3[3:1] / Reg11[7:4]                                              */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_SPDIF_Config (const ES9038_SPDIF_InitTypeDef *Cfg) {
    static const ES9038_SPDIF_InitTypeDef def = ES9038_SPDIF_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg3: [7:4] 保留(默认4), 只改功能位 */
    mask = (uint8_t)(ES9038_SPDIF_USER_BITS_Msk | ES9038_SPDIF_IGNORE_DATA_Msk |
                     ES9038_SPDIF_IGNORE_VALID_Msk);
    val = (uint8_t)((Cfg->UserBits != ES9038_DISABLE ? ES9038_SPDIF_USER_BITS_Msk : 0U) |
                    (Cfg->IgnoreData != ES9038_DISABLE ? ES9038_SPDIF_IGNORE_DATA_Msk : 0U) |
                    (Cfg->IgnoreValid != ES9038_DISABLE ? ES9038_SPDIF_IGNORE_VALID_Msk : 0U));
    ES9038_CHK (es9038_mod (ES9038_REG_SPDIF_CFG, mask, val));

    /* Reg11: [7:4] spdif_sel, [3:0] 保留 */
    val = (uint8_t)(Cfg->Source & ES9038_SPDIF_SEL_Msk);
    return es9038_mod (ES9038_REG_SPDIF_SELECT, (uint8_t)ES9038_SPDIF_SEL_Msk, val);
}

/* ========================================================================= */
/* 通道映射: Reg2[3:0]                                                        */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Mix_Config (const ES9038_Mix_InitTypeDef *Cfg) {
    static const ES9038_Mix_InitTypeDef def = ES9038_MIX_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    mask = (uint8_t)(ES9038_MIX_CH1_SEL_Msk | ES9038_MIX_CH2_SEL_Msk);
    val = (uint8_t)((Cfg->Dac1Source & ES9038_MIX_CH1_SEL_Msk) |
                    (Cfg->Dac2Source & ES9038_MIX_CH2_SEL_Msk));
    return es9038_mod (ES9038_REG_MIXING, mask, val);
}

/* ========================================================================= */
/* THD 补偿: Reg13[6] / Reg22-25                                              */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_THD_Config (const ES9038_THD_InitTypeDef *Cfg) {
    static const ES9038_THD_InitTypeDef def = ES9038_THD_DEFAULT_CONFIG;
    uint8_t enb;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg13[6] thd_enb 位语义反转: 0=使能, 1=禁用 */
    enb = (Cfg->Enable != ES9038_DISABLE) ? ES9038_THD_ENABLE : ES9038_THD_DISABLE;
    ES9038_CHK (es9038_mod (ES9038_REG_THD_BYPASS, 0x40U, enb));

    /* 系数为 16 位有符号, 按多字节字节序写出 */
    ES9038_CHK (es9038_wr_multi (ES9038_REG_THD_COMP_C2_1,
                                 (uint32_t)(uint16_t)Cfg->CoefC2, 2U));
    return es9038_wr_multi (ES9038_REG_THD_COMP_C3_1,
                            (uint32_t)(uint16_t)Cfg->CoefC3, 2U);
}

/* ========================================================================= */
/* 自动增益校准: Reg45 (读改写, [3:1]保留)                                     */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_Calibration_Config (const ES9038_Calibration_InitTypeDef *Cfg) {
    static const ES9038_Calibration_InitTypeDef def = ES9038_CALIB_DEFAULT_CONFIG;
    uint8_t mask;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    mask = (uint8_t)(ES9038_CALIB_EN_Msk | ES9038_CALIB_LATCH_Msk | ES9038_BIAS_CTRL_Msk);
    val = (uint8_t)((Cfg->Enable != ES9038_DISABLE ? ES9038_CALIB_EN_Msk : 0U) |
                    (Cfg->Latch != ES9038_DISABLE ? ES9038_CALIB_LATCH_Msk : 0U) |
                    (Cfg->BiasOut != ES9038_DISABLE ? ES9038_BIAS_CTRL_Msk : 0U));
    return es9038_mod (ES9038_REG_LOW_POWER, mask, val);
}

/* ========================================================================= */
/* 校准 ADC: Reg46 / Reg47-52                                                 */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_ADC_Config (const ES9038_ADC_InitTypeDef *Cfg) {
    static const ES9038_ADC_InitTypeDef def = ES9038_ADC_DEFAULT_CONFIG;
    uint8_t val;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    /* Reg46: [7][3][1]保留恒0, 整字节写出安全 */
    val = (uint8_t)((Cfg->Enable != ES9038_DISABLE ? ES9038_ADC_PDB_Msk : 0U) |
                    (Cfg->SecondOrder != ES9038_DISABLE ? ES9038_ADC_ORDER_Msk : 0U) |
                    (Cfg->ClockDiv & ES9038_ADC_CLK_Msk) |
                    (Cfg->DitherEnable != ES9038_DISABLE ? 0U : ES9038_ADC_DITHERB_Msk));
    return es9038_wr (ES9038_REG_ADC_CFG, val);
}

ES9038_StatusTypeDef ES9038_ADCFilter_Config (const ES9038_ADCFilter_InitTypeDef *Cfg) {
    static const ES9038_ADCFilter_InitTypeDef def = ES9038_ADCFILTER_DEFAULT_CONFIG;

    if (Cfg == NULL) {
        Cfg = &def;
    }
    ES9038_CHK (es9038_wr_multi (ES9038_REG_ADC_FILTER_1, Cfg->FtrScale, 2U));
    ES9038_CHK (es9038_wr_multi (ES9038_REG_ADC_FILTER_3, Cfg->FbqScale1, 2U));
    return es9038_wr_multi (ES9038_REG_ADC_FILTER_5, Cfg->FbqScale2, 2U);
}

/* ========================================================================= */
/* 中断屏蔽: Reg33                                                            */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_InterruptMask_Config (ES9038_StateTypeDef AutomuteMask,
                                                  ES9038_StateTypeDef LockMask) {
    uint8_t val = ES9038_IRQ_RESERVED_Msk; /* [5:2] 保留, 保持上电默认 1 */

    if (AutomuteMask != ES9038_DISABLE) {
        val |= ES9038_IRQ_AUTOMUTE_MASK_Msk;
    }
    if (LockMask != ES9038_DISABLE) {
        val |= ES9038_IRQ_LOCK_MASK_Msk;
    }
    return es9038_wr (ES9038_REG_INTERRUPT_MASK, val);
}

/* ========================================================================= */
/* 可编程 FIR: Reg40-44 (与手册 Example Source Code 一致)                      */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_FIR_Load (const int32_t *Coef, uint16_t Count) {
    uint16_t i;
    uint8_t stage;

    if (Coef == NULL) {
        return ES9038_ERROR;
    }
    /* 128 系数 -> stage1; 16 系数 -> stage2 (手册约定) */
    if (Count == 128U) {
        stage = ES9038_FIR_STAGE_1;
    } else if (Count == 16U) {
        stage = ES9038_FIR_STAGE_2;
    } else {
        return ES9038_ERROR;
    }

    for (i = 0; i < Count; i++) {
        uint32_t c = (uint32_t)Coef[i] & 0x00FFFFFFUL; /* 24位有符号量化值 */

        /* Reg40: [7] stage, [6:0] 系数地址 */
        ES9038_CHK (es9038_wr (ES9038_REG_PROG_FIR_ADDR,
                               (uint8_t)(stage | (i & ES9038_FIR_ADDR_Msk))));

        /* Reg41-43: 系数数据 (手册示例按低字节先写) */
        ES9038_CHK (es9038_wr (ES9038_REG_PROG_FIR_DATA_1, (uint8_t)(c & 0xFFU)));
        ES9038_CHK (es9038_wr (ES9038_REG_PROG_FIR_DATA_2, (uint8_t)((c >> 8) & 0xFFU)));
        ES9038_CHK (es9038_wr (ES9038_REG_PROG_FIR_DATA_3, (uint8_t)((c >> 16) & 0xFFU)));

        /* Reg44: 置 prog_we 完成一次写入 */
        ES9038_CHK (es9038_wr (ES9038_REG_PROG_FIR_CFG, ES9038_FIR_CFG_PROG_WE_Msk));
    }
    /* 写完关闭 prog_we, 保留 stage2 对称性配置为默认 */
    return es9038_wr (ES9038_REG_PROG_FIR_CFG, 0U);
}

ES9038_StatusTypeDef ES9038_FIR_Cmd (ES9038_StateTypeDef State) {
    return es9038_mod (ES9038_REG_PROG_FIR_CFG, ES9038_FIR_CFG_PROG_EN_Msk,
                       (State != ES9038_DISABLE) ? ES9038_FIR_CFG_PROG_EN_Msk : 0U);
}

/* ========================================================================= */
/* 状态回读                                                                    */
/* ========================================================================= */

ES9038_StatusTypeDef ES9038_GetChipId (uint8_t *ChipId) {
    return es9038_rd (ES9038_REG_CHIP_ID, ChipId);
}

ES9038_StatusTypeDef ES9038_CheckDevice (ES9038_StateTypeDef *Result) {
    uint8_t id;

    if (Result == NULL) {
        return ES9038_ERROR;
    }
    *Result = ES9038_DISABLE;
    ES9038_CHK (es9038_rd (ES9038_REG_CHIP_ID, &id));
    if ((uint8_t)(id >> 2) == ES9038_CHIP_ID_VALUE) {
        *Result = ES9038_ENABLE;
    }
    return ES9038_OK;
}

ES9038_StatusTypeDef ES9038_GetLockStatus (ES9038_StateTypeDef *State) {
    uint8_t id;

    if (State == NULL) {
        return ES9038_ERROR;
    }
    ES9038_CHK (es9038_rd (ES9038_REG_CHIP_ID, &id));
    *State = ((id & ES9038_STATUS_LOCK_Msk) != 0U) ? ES9038_ENABLE : ES9038_DISABLE;
    return ES9038_OK;
}

ES9038_StatusTypeDef ES9038_GetAutomuteStatus (ES9038_StateTypeDef *State) {
    uint8_t id;

    if (State == NULL) {
        return ES9038_ERROR;
    }
    ES9038_CHK (es9038_rd (ES9038_REG_CHIP_ID, &id));
    *State = ((id & ES9038_STATUS_AUTOMUTE_Msk) != 0U) ? ES9038_ENABLE : ES9038_DISABLE;
    return ES9038_OK;
}

ES9038_StatusTypeDef ES9038_WaitForLock (uint32_t TimeoutMs) {
    uint32_t elapsed = 0U;

    for (;;) {
        ES9038_StateTypeDef lock = ES9038_DISABLE;

        ES9038_CHK (ES9038_GetLockStatus (&lock));
        if (lock == ES9038_ENABLE) {
            return ES9038_OK;
        }
        if (elapsed >= TimeoutMs) {
            return ES9038_ERROR;
        }
        es9038_bus_delay_ms (1U);
        elapsed++;
    }
}

ES9038_StatusTypeDef ES9038_GetDpllNumber (uint32_t *Number) {
    /* 手册: 必须先读 Reg66 (触发锁存), 再依次读后续字节 */
    return es9038_rd_multi (ES9038_REG_DPLL_NUM_1, Number, 4U);
}

ES9038_StatusTypeDef ES9038_GetInputStatus (uint8_t *Status) {
    return es9038_rd (ES9038_REG_INPUT_STATUS, Status);
}

ES9038_StatusTypeDef ES9038_GetGpioLevel (uint8_t *Level) {
    uint8_t v;

    if (Level == NULL) {
        return ES9038_ERROR;
    }
    ES9038_CHK (es9038_rd (ES9038_REG_GPIO_READBACK, &v));
    *Level = (uint8_t)(v & 0x03U);
    return ES9038_OK;
}

ES9038_StatusTypeDef ES9038_GetAdcValue (uint32_t *Value) {
    /* 手册: 读 Reg100 (LSB) 触发锁存, 故从 Reg100 顺序读起 */
    return es9038_rd_multi (ES9038_REG_ADC_READBACK_1, Value, 3U);
}
