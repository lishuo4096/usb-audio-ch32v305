

#ifndef ES9038Q2M_HAL_H
#define ES9038Q2M_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* 版本                                                                      */
/* ========================================================================= */
#define ES9038_HAL_VERSION_MAJOR 1U
#define ES9038_HAL_VERSION_MINOR 0U

/* ========================================================================= */
/* 通用类型                                                                   */
/* ========================================================================= */
/** 函数返回状态 */
typedef enum {
    ES9038_OK = 0,   /**< 操作成功 */
    ES9038_ERROR = 1 /**< I2C 失败或参数错误 */
} ES9038_StatusTypeDef;

/** 通用使能状态 */
typedef enum {
    ES9038_DISABLE = 0,
    ES9038_ENABLE = 1
} ES9038_StateTypeDef;

/* ========================================================================= */
/* 库内部固定参数 (可在包含本头文件之前用编译器宏覆盖)                         */
/* ========================================================================= */
#ifndef ES9038_MCLK_HZ
#define ES9038_MCLK_HZ 98304000U /**< 本地时钟: 98.304 MHz 无源晶振 (XI) */
#endif

#define ES9038_CHIP_ID_VALUE 0x1CU    /**< Reg64[7:2] 期望芯片 ID */
#define ES9038_I2C_ADDR_7BIT_A0 0x48U /**< ADDR=0, 8位写地址 0x90 */
#define ES9038_I2C_ADDR_7BIT_A1 0x49U /**< ADDR=1, 8位写地址 0x92 */

/* 通道选择 (ES9038_Volume_Set 等) */
#define ES9038_CHANNEL_1 1U
#define ES9038_CHANNEL_2 2U
#define ES9038_CHANNEL_ALL 3U

/* ========================================================================= */
/* 寄存器地址 (手册 REGISTER SETTINGS 章节编号 -> 8位地址)                     */
/* ========================================================================= */
/* --- 可写寄存器 --- */
#define ES9038_REG_SYSTEM 0x00U          /**< Reg0  : 振荡器驱动/时钟分频/软复位 */
#define ES9038_REG_INPUT_SEL 0x01U       /**< Reg1  : 输入选择/SAI格式/字长 */
#define ES9038_REG_MIXING 0x02U          /**< Reg2  : 通道映射/Automute动作 */
#define ES9038_REG_SPDIF_CFG 0x03U       /**< Reg3  : SPDIF 配置 */
#define ES9038_REG_AUTOMUTE_TIME 0x04U   /**< Reg4  : Automute 时间 */
#define ES9038_REG_AUTOMUTE_LEVEL 0x05U  /**< Reg5  : Automute 电平 */
#define ES9038_REG_DEEMPH 0x06U          /**< Reg6  : 去加重/DoP/音量斜坡速率 */
#define ES9038_REG_FILTER_MUTE 0x07U     /**< Reg7  : 滤波器形状/OSF旁路/静音 */
#define ES9038_REG_GPIO12_CFG 0x08U      /**< Reg8  : GPIO1/2 功能配置 */
#define ES9038_REG_RESERVED_09 0x09U     /**< Reg9  : 保留 */
#define ES9038_REG_MASTER_MODE 0x0AU     /**< Reg10 : 主从模式/同步配置 */
#define ES9038_REG_SPDIF_SELECT 0x0BU    /**< Reg11 : SPDIF 输入源选择 */
#define ES9038_REG_ASRC_DPLL_BW 0x0CU    /**< Reg12 : ASRC/DPLL 带宽 */
#define ES9038_REG_THD_BYPASS 0x0DU      /**< Reg13 : THD 补偿使能 */
#define ES9038_REG_SOFT_START 0x0EU      /**< Reg14 : 软启动配置 */
#define ES9038_REG_VOLUME1 0x0FU         /**< Reg15 : 通道1 音量 */
#define ES9038_REG_VOLUME2 0x10U         /**< Reg16 : 通道2 音量 */
#define ES9038_REG_MASTER_TRIM1 0x11U    /**< Reg17 : Master Trim 字节0 (低字节在低地址) */
#define ES9038_REG_MASTER_TRIM2 0x12U    /**< Reg18 : Master Trim 字节1 */
#define ES9038_REG_MASTER_TRIM3 0x13U    /**< Reg19 : Master Trim 字节2 */
#define ES9038_REG_MASTER_TRIM4 0x14U    /**< Reg20 : Master Trim 字节3 (高字节) */
#define ES9038_REG_GPIO_INPUT_SEL 0x15U  /**< Reg21 : GPIO 输入源选择 */
#define ES9038_REG_THD_COMP_C2_1 0x16U   /**< Reg22 : THD C2 系数 字节0 (低字节) */
#define ES9038_REG_THD_COMP_C2_2 0x17U   /**< Reg23 : THD C2 系数 字节1 (高字节) */
#define ES9038_REG_THD_COMP_C3_1 0x18U   /**< Reg24 : THD C3 系数 字节0 (低字节) */
#define ES9038_REG_THD_COMP_C3_2 0x19U   /**< Reg25 : THD C3 系数 字节1 (高字节) */
#define ES9038_REG_RESERVED_1A 0x1AU     /**< Reg26 : 保留 (默认 0x62) */
#define ES9038_REG_GENERAL_CFG 0x1BU     /**< Reg27 : ASRC使能/音量锁存/18dB增益 */
#define ES9038_REG_RESERVED_1C 0x1CU     /**< Reg28 : 保留 (默认 0xF0) */
#define ES9038_REG_GPIO_CFG 0x1DU        /**< Reg29 : GPIO 输出反相 */
#define ES9038_REG_CP_CLK_1 0x1EU        /**< Reg30 : 电荷泵时钟 字节0 (低字节) */
#define ES9038_REG_CP_CLK_2 0x1FU        /**< Reg31 : 电荷泵时钟 字节1 (高字节) */
#define ES9038_REG_RESERVED_20 0x20U     /**< Reg32 : 保留 */
#define ES9038_REG_INTERRUPT_MASK 0x21U  /**< Reg33 : 中断屏蔽 */
#define ES9038_REG_NCO_1 0x22U           /**< Reg34 : NCO 字节0 (低字节) */
#define ES9038_REG_NCO_2 0x23U           /**< Reg35 : NCO 字节1 */
#define ES9038_REG_NCO_3 0x24U           /**< Reg36 : NCO 字节2 */
#define ES9038_REG_NCO_4 0x25U           /**< Reg37 : NCO 字节3 (高字节) */
#define ES9038_REG_RESERVED_26 0x26U     /**< Reg38 : 保留 */
#define ES9038_REG_GENERAL_CFG_2 0x27U   /**< Reg39 : 功放电源/开关控制 */
#define ES9038_REG_PROG_FIR_ADDR 0x28U   /**< Reg40 : 可编程FIR地址 */
#define ES9038_REG_PROG_FIR_DATA_1 0x29U /**< Reg41 : FIR系数 [7:0]   */
#define ES9038_REG_PROG_FIR_DATA_2 0x2AU /**< Reg42 : FIR系数 [15:8]  */
#define ES9038_REG_PROG_FIR_DATA_3 0x2BU /**< Reg43 : FIR系数 [23:16] */
#define ES9038_REG_PROG_FIR_CFG 0x2CU    /**< Reg44 : FIR 配置 */
#define ES9038_REG_LOW_POWER 0x2DU       /**< Reg45 : 自动校准/BIAS */
#define ES9038_REG_ADC_CFG 0x2EU         /**< Reg46 : ADC 配置 */
#define ES9038_REG_ADC_FILTER_1 0x2FU    /**< Reg47 : ADC ftr_scale 字节0 (低字节) */
#define ES9038_REG_ADC_FILTER_2 0x30U    /**< Reg48 : ADC ftr_scale 字节1 (高字节) */
#define ES9038_REG_ADC_FILTER_3 0x31U    /**< Reg49 : ADC fbq_scale1 字节0 */
#define ES9038_REG_ADC_FILTER_4 0x32U    /**< Reg50 : ADC fbq_scale1 字节1 */
#define ES9038_REG_ADC_FILTER_5 0x33U    /**< Reg51 : ADC fbq_scale2 字节0 */
#define ES9038_REG_ADC_FILTER_6 0x34U    /**< Reg52 : ADC fbq_scale2 字节1 */

/* --- 只读寄存器 --- */
#define ES9038_REG_CHIP_ID 0x40U          /**< Reg64 : 芯片ID/状态 (chip_id[7:2]) */
#define ES9038_REG_GPIO_READBACK 0x41U    /**< Reg65 : GPIO 电平回读 */
#define ES9038_REG_DPLL_NUM_1 0x42U       /**< Reg66 : DPLL NUM [7:0]   (读序: 先读此寄存器) */
#define ES9038_REG_DPLL_NUM_2 0x43U       /**< Reg67 : DPLL NUM [15:8]  */
#define ES9038_REG_DPLL_NUM_3 0x44U       /**< Reg68 : DPLL NUM [23:16] */
#define ES9038_REG_DPLL_NUM_4 0x45U       /**< Reg69 : DPLL NUM [31:24] */
#define ES9038_REG_SPDIF_STATUS_1 0x46U   /**< Reg70 : SPDIF 通道状态/User (70-93) */
#define ES9038_REG_SPDIF_STATUS_END 0x5DU /**< Reg93 : SPDIF 状态结束 */
#define ES9038_REG_INPUT_STATUS 0x60U     /**< Reg96 : 输入选择与Automute状态 */
#define ES9038_REG_ADC_READBACK_1 0x64U   /**< Reg100: ADC 回读 [7:0]   (LSB, 读此寄存器锁存) */
#define ES9038_REG_ADC_READBACK_2 0x65U   /**< Reg101: ADC 回读 [15:8]  */
#define ES9038_REG_ADC_READBACK_3 0x66U   /**< Reg102: ADC 回读 [23:16] */

/* ========================================================================= */
/* 寄存器位域宏                                                               */
/* 每个配置宏均已按寄存器位位置移位, 结构体字段可直接按位或写入                    */
/* ========================================================================= */

/* ---- Reg0 0x00: System -------------------------------------------------- */
#define ES9038_SYS_OSC_DRV_Pos (4U)
#define ES9038_SYS_OSC_DRV_Msk (0xFUL << ES9038_SYS_OSC_DRV_Pos)
#define ES9038_OSC_DRIVE_FULL_BIAS (0x0UL << ES9038_SYS_OSC_DRV_Pos) /**< 全偏置 (默认) */
#define ES9038_OSC_DRIVE_3_4_BIAS (0x8UL << ES9038_SYS_OSC_DRV_Pos)  /**< 3/4 偏置 */
#define ES9038_OSC_DRIVE_1_2_BIAS (0xCUL << ES9038_SYS_OSC_DRV_Pos)  /**< 1/2 偏置 (省电) */
#define ES9038_OSC_DRIVE_1_4_BIAS (0xEUL << ES9038_SYS_OSC_DRV_Pos)  /**< 1/4 偏置 (省电) */
#define ES9038_OSC_DRIVE_SHUTDOWN (0xFUL << ES9038_SYS_OSC_DRV_Pos)  /**< 关断振荡器 */

#define ES9038_SYS_CLK_GEAR_Pos (2U)
#define ES9038_SYS_CLK_GEAR_Msk (0x3UL << ES9038_SYS_CLK_GEAR_Pos)
#define ES9038_CLK_GEAR_MCLK_EQ_XI (0x0UL << ES9038_SYS_CLK_GEAR_Pos)      /**< MCLK = XI (默认) */
#define ES9038_CLK_GEAR_MCLK_EQ_XI_DIV2 (0x1UL << ES9038_SYS_CLK_GEAR_Pos) /**< MCLK = XI/2 */
#define ES9038_CLK_GEAR_MCLK_EQ_XI_DIV4 (0x2UL << ES9038_SYS_CLK_GEAR_Pos) /**< MCLK = XI/4 */
#define ES9038_CLK_GEAR_MCLK_EQ_XI_DIV8 (0x3UL << ES9038_SYS_CLK_GEAR_Pos) /**< MCLK = XI/8 */

#define ES9038_SYS_SOFT_RESET_Msk (0x01U)                                  /**< 写1触发软复位, 自清零且恒读0 */

/* ---- Reg1 0x01: Input Selection ---------------------------------------- */
#define ES9038_IN_SER_LEN_Pos (6U)
#define ES9038_IN_SER_LEN_Msk (0x3UL << ES9038_IN_SER_LEN_Pos)
#define ES9038_SAI_WORD_16BIT (0x0UL << ES9038_IN_SER_LEN_Pos) /**< 16-bit 数据字 */
#define ES9038_SAI_WORD_24BIT (0x1UL << ES9038_IN_SER_LEN_Pos) /**< 24-bit 数据字 */
#define ES9038_SAI_WORD_32BIT (0x3UL << ES9038_IN_SER_LEN_Pos) /**< 32-bit 数据字 (默认) */

#define ES9038_IN_SER_MODE_Pos (4U)
#define ES9038_IN_SER_MODE_Msk (0x3UL << ES9038_IN_SER_MODE_Pos)
#define ES9038_SAI_FORMAT_I2S (0x0UL << ES9038_IN_SER_MODE_Pos)        /**< I2S (默认) */
#define ES9038_SAI_FORMAT_LEFT_JUST (0x1UL << ES9038_IN_SER_MODE_Pos)  /**< 左对齐 */
#define ES9038_SAI_FORMAT_RIGHT_JUST (0x3UL << ES9038_IN_SER_MODE_Pos) /**< 右对齐 */

#define ES9038_IN_AUTO_SEL_Pos (2U)
#define ES9038_IN_AUTO_SEL_Msk (0x3UL << ES9038_IN_AUTO_SEL_Pos)
#define ES9038_AUTOSEL_DISABLE (0x0UL << ES9038_IN_AUTO_SEL_Pos) /**< 关闭自动选择, 用 InputSource */
#define ES9038_AUTOSEL_DSD (0x1UL << ES9038_IN_AUTO_SEL_Pos)     /**< 自动: DSD / 串行 */
#define ES9038_AUTOSEL_SPDIF (0x2UL << ES9038_IN_AUTO_SEL_Pos)   /**< 自动: SPDIF / 串行 */
#define ES9038_AUTOSEL_ALL (0x3UL << ES9038_IN_AUTO_SEL_Pos)     /**< 自动: DSD/SPDIF/串行 (默认) */

#define ES9038_IN_SEL_Pos (0U)
#define ES9038_IN_SEL_Msk (0x3UL << ES9038_IN_SEL_Pos)
#define ES9038_INPUT_SERIAL (0x0UL << ES9038_IN_SEL_Pos) /**< 串行 PCM (默认) */
#define ES9038_INPUT_SPDIF (0x1UL << ES9038_IN_SEL_Pos)  /**< SPDIF */
#define ES9038_INPUT_DSD (0x3UL << ES9038_IN_SEL_Pos)    /**< DSD */

/* ---- Reg2 0x02: Mixing & Automute Config -------------------------------- */
#define ES9038_MIX_AUTOMUTE_CFG_Pos (6U)
#define ES9038_MIX_AUTOMUTE_CFG_Msk (0x3UL << ES9038_MIX_AUTOMUTE_CFG_Pos)
#define ES9038_AUTOMUTE_ACTION_NONE (0x0UL << ES9038_MIX_AUTOMUTE_CFG_Pos) /**< 无动作 (默认) */
#define ES9038_AUTOMUTE_ACTION_MUTE (0x1UL << ES9038_MIX_AUTOMUTE_CFG_Pos) /**< 软静音 */
#define ES9038_AUTOMUTE_ACTION_RAMP (0x2UL << ES9038_MIX_AUTOMUTE_CFG_Pos) /**< 输出 ramp 到地 (省电) */
#define ES9038_AUTOMUTE_ACTION_BOTH (0x3UL << ES9038_MIX_AUTOMUTE_CFG_Pos) /**< 先静音再 ramp 到地 */

#define ES9038_MIX_CH2_SEL_Pos (2U)
#define ES9038_MIX_CH2_SEL_Msk (0x3UL << ES9038_MIX_CH2_SEL_Pos)
#define ES9038_MIX_DAC2_FROM_CH1 (0x0UL << ES9038_MIX_CH2_SEL_Pos) /**< DAC2 <- ch1 */
#define ES9038_MIX_DAC2_FROM_CH2 (0x1UL << ES9038_MIX_CH2_SEL_Pos) /**< DAC2 <- ch2 (默认) */

#define ES9038_MIX_CH1_SEL_Pos (0U)
#define ES9038_MIX_CH1_SEL_Msk (0x3UL << ES9038_MIX_CH1_SEL_Pos)
#define ES9038_MIX_DAC1_FROM_CH1 (0x0UL << ES9038_MIX_CH1_SEL_Pos) /**< DAC1 <- ch1 (默认) */
#define ES9038_MIX_DAC1_FROM_CH2 (0x1UL << ES9038_MIX_CH1_SEL_Pos) /**< DAC1 <- ch2 */

/* ---- Reg3 0x03: SPDIF Config -------------------------------------------- */
#define ES9038_SPDIF_USER_BITS_Msk (0x08U)    /**< 1: 回读接口呈现 user bits */
#define ES9038_SPDIF_IGNORE_DATA_Msk (0x04U)  /**< 1: 忽略 channel status 的 data 标志 */
#define ES9038_SPDIF_IGNORE_VALID_Msk (0x02U) /**< 1: 忽略 valid 标志 */

/* ---- Reg4 0x04: Automute Time ------------------------------------------- */
#define ES9038_AUTOMUTE_TIME_DISABLE (0x00U) /**< 0 = 关闭 automute (默认) */
/* 时间(s) = 2096896 / (automute_time * FSR), 非0有效 */

/* ---- Reg5 0x05: Automute Level ------------------------------------------ */
#define ES9038_AUTOMUTE_LEVEL_Msk (0x7FU)
#define ES9038_AUTOMUTE_LEVEL_DEFAULT (104U) /**< 默认 -104dB (数值 = |dB|) */

/* ---- Reg6 0x06: De-emphasis / DoP / Volume Ramp -------------------------- */
#define ES9038_DEEMPH_AUTO_Msk (0x80U)   /**< 1: 依 SPDIF channel status 自动去加重 */
#define ES9038_DEEMPH_BYPASS_Msk (0x40U) /**< 1: 旁路(禁用)去加重滤波器 (默认) */
#define ES9038_DEEMPH_SEL_Pos (4U)
#define ES9038_DEEMPH_SEL_Msk (0x3UL << ES9038_DEEMPH_SEL_Pos)
#define ES9038_DEEMPH_32KHZ (0x0UL << ES9038_DEEMPH_SEL_Pos) /**< 32kHz (默认) */
#define ES9038_DEEMPH_44KHZ (0x1UL << ES9038_DEEMPH_SEL_Pos) /**< 44.1kHz */
#define ES9038_DEEMPH_48KHZ (0x2UL << ES9038_DEEMPH_SEL_Pos) /**< 48kHz */
#define ES9038_DOP_ENABLE_Msk (0x08U)                        /**< 1: 使能 DoP 解码逻辑 */
#define ES9038_VOLUME_RATE_Pos (0U)
#define ES9038_VOLUME_RATE_Msk (0x7UL)
#define ES9038_VOLUME_RATE_DEFAULT (2U) /**< 斜坡速率 2^rate*FSR/512 dB/s (默认) */

/* ---- Reg7 0x07: Filter Bandwidth & Mute ---------------------------------- */
#define ES9038_FILTER_SHAPE_Pos (5U)
#define ES9038_FILTER_SHAPE_Msk (0x7UL << ES9038_FILTER_SHAPE_Pos)
#define ES9038_FILTER_LINEAR_FAST (0x0UL << ES9038_FILTER_SHAPE_Pos)       /**< 线性相位 快速衰减 */
#define ES9038_FILTER_LINEAR_SLOW (0x1UL << ES9038_FILTER_SHAPE_Pos)       /**< 线性相位 慢速衰减 */
#define ES9038_FILTER_MINPHASE_FAST (0x2UL << ES9038_FILTER_SHAPE_Pos)     /**< 最小相位 快速衰减 */
#define ES9038_FILTER_MINPHASE_SLOW (0x3UL << ES9038_FILTER_SHAPE_Pos)     /**< 最小相位 慢速衰减 */
#define ES9038_FILTER_APODIZING_FAST (0x4UL << ES9038_FILTER_SHAPE_Pos)    /**< 窗函数补偿快速衰减 (默认) */
#define ES9038_FILTER_MINPHASE_FAST_FIX (0x6UL << ES9038_FILTER_SHAPE_Pos) /**< 修正最小相位快速衰减 */
#define ES9038_FILTER_BRICKWALL (0x7UL << ES9038_FILTER_SHAPE_Pos)         /**< 砖墙滤波 */
#define ES9038_FILTER_BYPASS_OSF_Msk (0x08U)                               /**< 1: 旁路内部8x过采样FIR (需外部8x过采样) */
#define ES9038_FILTER_MUTE_Msk (0x01U)                                     /**< 1: 双通道静音 */

/* ---- Reg8 0x08: GPIO1-2 Configuration ------------------------------------ */
#define ES9038_GPIO1_CFG_Pos (0U)
#define ES9038_GPIO1_CFG_Msk (0x0FUL)
#define ES9038_GPIO2_CFG_Pos (4U)
#define ES9038_GPIO2_CFG_Msk (0xFUL << ES9038_GPIO2_CFG_Pos)
/* GPIO 功能编号 (0~15, 配置时按位移入 GPIO1/GPIO2 字段) */
#define ES9038_GPIO_FUNC_AUTOMUTE_ST 0U     /**< 输出: automute 状态 */
#define ES9038_GPIO_FUNC_LOCK_ST 1U         /**< 输出: DPLL 锁定状态 */
#define ES9038_GPIO_FUNC_VOLUME_MIN 2U      /**< 输出: 音量到最小 */
#define ES9038_GPIO_FUNC_CLK_OUT 3U         /**< 输出: 缓冲 MCLK */
#define ES9038_GPIO_FUNC_AUTOMUTE_IRQ 4U    /**< 输出: automute/lock 中断 (读Reg64清除) */
#define ES9038_GPIO_FUNC_AMP_PDB 5U         /**< 输出: Reg39[6] 状态 */
#define ES9038_GPIO_FUNC_CP_CLK 6U          /**< 输出: 电荷泵时钟 */
#define ES9038_GPIO_FUNC_ADC_DATA 7U        /**< 输入: ADC 校准数据 */
#define ES9038_GPIO_FUNC_STANDARD_IN 8U     /**< 输入: 高阻, 回读见 Reg65 */
#define ES9038_GPIO_FUNC_INPUT_SEL 9U       /**< 输入: 输入源切换 (配合 Reg21) */
#define ES9038_GPIO_FUNC_MUTE_ALL 10U       /**< 输入: 高电平静音 */
#define ES9038_GPIO_FUNC_ANALOG_SD 13U      /**< 输入: 高电平关断芯片 (默认) */
#define ES9038_GPIO_FUNC_SOFTSTART_DONE 14U /**< 输出: 软启动完成 */
#define ES9038_GPIO_FUNC_OUTPUT_HIGH 15U    /**< 输出: 恒高 */

/* ---- Reg10 0x0A: Master Mode & Sync -------------------------------------- */
#define ES9038_MASTER_MODE_Msk (0x80U) /**< 1: 主模式 (驱动 DATA_CLK/DATA1) */
#define ES9038_MASTER_DIV_Pos (5U)
#define ES9038_MASTER_DIV_Msk (0x3UL << ES9038_MASTER_DIV_Pos)
#define ES9038_MASTER_BCLK_EQ_MCLK_DIV2 (0x0UL << ES9038_MASTER_DIV_Pos)  /**< BCLK=MCLK/2 (默认) */
#define ES9038_MASTER_BCLK_EQ_MCLK_DIV4 (0x1UL << ES9038_MASTER_DIV_Pos)  /**< BCLK=MCLK/4 */
#define ES9038_MASTER_BCLK_EQ_MCLK_DIV8 (0x2UL << ES9038_MASTER_DIV_Pos)  /**< BCLK=MCLK/8 */
#define ES9038_MASTER_BCLK_EQ_MCLK_DIV16 (0x3UL << ES9038_MASTER_DIV_Pos) /**< BCLK=MCLK/16 */
#define ES9038_MASTER_128FS_MODE_Msk (0x10U)                              /**< 1: MCLK=128*FSR 同步模式 (PCM/OSF bypass) */
#define ES9038_LOCK_SPEED_Pos (0U)
#define ES9038_LOCK_SPEED_Msk (0x0FUL)
#define ES9038_LOCK_SPEED_16384 (0U) /**< 16384 FSL 边沿 (最慢/最准) */
#define ES9038_LOCK_SPEED_8192 (1U)
#define ES9038_LOCK_SPEED_5461 (2U)  /**< 默认 */
#define ES9038_LOCK_SPEED_4096 (3U)
#define ES9038_LOCK_SPEED_3276 (4U)
#define ES9038_LOCK_SPEED_2730 (5U)
#define ES9038_LOCK_SPEED_2340 (6U)
#define ES9038_LOCK_SPEED_2048 (7U)
#define ES9038_LOCK_SPEED_1820 (8U)
#define ES9038_LOCK_SPEED_1638 (9U)
#define ES9038_LOCK_SPEED_1489 (10U)
#define ES9038_LOCK_SPEED_1365 (11U)
#define ES9038_LOCK_SPEED_1260 (12U)
#define ES9038_LOCK_SPEED_1170 (13U)
#define ES9038_LOCK_SPEED_1092 (14U)
#define ES9038_LOCK_SPEED_1024 (15U) /**< 1024 FSL 边沿 (最快/误差最大) */

/* ---- Reg11 0x0B: SPDIF Select -------------------------------------------- */
#define ES9038_SPDIF_SEL_Pos (4U)
#define ES9038_SPDIF_SEL_Msk (0xFUL << ES9038_SPDIF_SEL_Pos)
#define ES9038_SPDIF_IN_DATA_CLK (0x0UL << ES9038_SPDIF_SEL_Pos) /**< DATA_CLK (默认) */
#define ES9038_SPDIF_IN_DATA1 (0x1UL << ES9038_SPDIF_SEL_Pos)    /**< DATA1 */
#define ES9038_SPDIF_IN_DATA2 (0x2UL << ES9038_SPDIF_SEL_Pos)    /**< DATA2 */
#define ES9038_SPDIF_IN_GPIO1 (0x3UL << ES9038_SPDIF_SEL_Pos)    /**< GPIO1 */
#define ES9038_SPDIF_IN_GPIO2 (0x4UL << ES9038_SPDIF_SEL_Pos)    /**< GPIO2 */

/* ---- Reg12 0x0C: ASRC/DPLL Bandwidth -------------------------------------- */
#define ES9038_DPLL_BW_SERIAL_Pos (4U)
#define ES9038_DPLL_BW_SERIAL_Msk (0xFUL << ES9038_DPLL_BW_SERIAL_Pos)
#define ES9038_DPLL_BW_SERIAL_OFF (0x0UL << ES9038_DPLL_BW_SERIAL_Pos)
#define ES9038_DPLL_BW_SERIAL_LOWEST (0x1UL << ES9038_DPLL_BW_SERIAL_Pos)
#define ES9038_DPLL_BW_SERIAL_DEFAULT (0x5UL << ES9038_DPLL_BW_SERIAL_Pos) /**< 默认 */
#define ES9038_DPLL_BW_SERIAL_HIGHEST (0xFUL << ES9038_DPLL_BW_SERIAL_Pos)
#define ES9038_DPLL_BW_SERIAL_VAL(n) (((uint32_t)(n) & 0x0FU) << ES9038_DPLL_BW_SERIAL_Pos)

#define ES9038_DPLL_BW_DSD_Pos (0U)
#define ES9038_DPLL_BW_DSD_Msk (0x0FUL)
#define ES9038_DPLL_BW_DSD_OFF (0x0U)
#define ES9038_DPLL_BW_DSD_LOWEST (0x1U)
#define ES9038_DPLL_BW_DSD_DEFAULT (0x0AU) /**< 默认 */
#define ES9038_DPLL_BW_DSD_HIGHEST (0x0FU)
#define ES9038_DPLL_BW_DSD_VAL(n) ((uint32_t)(n) & 0x0FU)

/* ---- Reg13 0x0D: THD Bypass ---------------------------------------------- */
#define ES9038_THD_ENABLE (0x00U << 6)  /**< thd_enb=0: 使能THD补偿 (注意位反转) */
#define ES9038_THD_DISABLE (0x01U << 6) /**< thd_enb=1: 禁用THD补偿 (默认) */

/* ---- Reg14 0x0E: Soft Start ----------------------------------------------- */
#define ES9038_SOFT_START_Msk (0x80U)         /**< 1: ramp到AVCC/2正常输出; 0: ramp到地 */
#define ES9038_SOFT_START_ON_LOCK_Msk (0x40U) /**< 1: 锁定后才 ramp 到 AVCC/2 */
#define ES9038_SOFT_START_TIME_Pos (0U)
#define ES9038_SOFT_START_TIME_Msk (0x1FUL)
#define ES9038_SOFT_START_TIME_DEFAULT (10U) /**< 有效范围 0~20 */
/* ramp 时间(s) = 4096 * 2^(time+1) / MCLK */

/* ---- Reg15/16 0x0F/0x10: Volume -------------------------------------------- */
#define ES9038_VOLUME_REG_FULL_SCALE (0U) /**< 0x00 = 0 dB */
#define ES9038_VOLUME_REG_MAX (255U)      /**< 0xFF = -127.5 dB */
/* 步进 0.5dB: 衰减dB -> 寄存器值 = |dB| * 2, 例: -40dB -> 80 (上电默认) */
#define ES9038_VOLUME_ATTEN(db) ((uint8_t)((-(float)(db)) * 2.0f)) /**< 传入负dB值, 如 -6.0f */

/* ---- Reg17-20 0x11-0x14: Master Trim --------------------------------------- */
#define ES9038_MASTER_TRIM_FULL_SCALE (0x7FFFFFFFUL) /**< 默认 0dB 参考 */

/* ---- Reg21 0x15: GPIO Input Selection -------------------------------------- */
#define ES9038_GPIO1_IN_SEL_Pos (4U)
#define ES9038_GPIO1_IN_SEL_Msk (0x3UL << ES9038_GPIO1_IN_SEL_Pos)
#define ES9038_GPIO2_IN_SEL_Pos (6U)
#define ES9038_GPIO2_IN_SEL_Msk (0x3UL << ES9038_GPIO2_IN_SEL_Pos)
#define ES9038_GPIO_INPUT_SERIAL (0U) /**< GPIO=Input Select 时选择串行 (默认) */
#define ES9038_GPIO_INPUT_SPDIF (1U)
#define ES9038_GPIO_INPUT_DSD (3U)

/* ---- Reg22-25 0x16-0x19: THD Compensation Coefficients ---------------------- */
/* 16位有符号: C2 次谐波, C3 三次谐波 */

/* ---- Reg27 0x1B: General Configuration --------------------------------------- */
#define ES9038_GEN_ASRC_EN_Msk (0x80U)      /**< 1: 使能 ASRC (默认) */
#define ES9038_GEN_CH_VOLUME_Msk (0x08U)    /**< 1: 通道2共用通道1音量 (仅PCM) */
#define ES9038_GEN_LATCH_VOLUME_Msk (0x04U) /**< 1: 内部音量系数跟随寄存器 (默认) */
#define ES9038_GEN_18DB_GAIN_Pos (0U)
#define ES9038_GEN_18DB_GAIN_Msk (0x03UL)
#define ES9038_GAIN18_NONE (0U) /**< 无增益 (默认) */
#define ES9038_GAIN18_CH1 (1U)  /**< 通道1 +18dB */
#define ES9038_GAIN18_CH2 (2U)  /**< 通道2 +18dB */
#define ES9038_GAIN18_BOTH (3U) /**< 双通道 +18dB */

/* ---- Reg29 0x1D: GPIO Configuration ------------------------------------------- */
#define ES9038_GPIO_INVERT_Pos (6U)
#define ES9038_GPIO_INVERT_Msk (0x3UL << ES9038_GPIO_INVERT_Pos)
#define ES9038_GPIO_INVERT_NONE (0U) /**< 不反相 (默认) */
#define ES9038_GPIO_INVERT_GPIO1 (1U)
#define ES9038_GPIO_INVERT_GPIO2 (2U)
#define ES9038_GPIO_INVERT_BOTH (3U)

/* ---- Reg30/31 0x1E/0x1F: Charge Pump Clock (16位) ------------------------------ */
#define ES9038_CP_CLK_SEL_Pos (14U) /**< 00: fCLK = XI (仅默认有效) */
#define ES9038_CP_CLK_SEL_Msk (0x3UL << ES9038_CP_CLK_SEL_Pos)
#define ES9038_CP_CLK_SRC_XI (0x0UL << ES9038_CP_CLK_SEL_Pos)
#define ES9038_CP_CLK_EN_Pos (12U)
#define ES9038_CP_CLK_EN_Msk (0x3UL << ES9038_CP_CLK_EN_Pos)
#define ES9038_CP_CLK_TRISTATE (0x0UL << ES9038_CP_CLK_EN_Pos) /**< 三态 (默认) */
#define ES9038_CP_CLK_GND (0x1UL << ES9038_CP_CLK_EN_Pos)      /**< 接地 */
#define ES9038_CP_CLK_DVDD (0x2UL << ES9038_CP_CLK_EN_Pos)     /**< 接DVDD */
#define ES9038_CP_CLK_ACTIVE (0x3UL << ES9038_CP_CLK_EN_Pos)   /**< 输出时钟 */
#define ES9038_CP_CLK_DIV_Pos (0U)
#define ES9038_CP_CLK_DIV_Msk (0xFFFUL)                        /**< fcp = fCLK / (div*2) */

/* ---- Reg33 0x21: Interrupt Mask ----------------------------------------------- */
#define ES9038_IRQ_AUTOMUTE_MASK_Msk (0x02U) /**< 1: 屏蔽 automute 中断 */
#define ES9038_IRQ_LOCK_MASK_Msk (0x01U)     /**< 1: 屏蔽 lock 中断 */
#define ES9038_IRQ_RESERVED_Msk (0x3CU)      /**< 保留位 (写入保持默认1) */

/* ---- Reg34-37 0x22-0x25: Programmable NCO --------------------------------------- */
/* FSR = nco_num * MCLK / 2^32 ; 0 = 禁用 NCO 模式 */

/* ---- Reg39 0x27: General Configuration 2 ------------------------------------------ */
#define ES9038_AMP_PDB_SS_Msk (0x80U) /**< 1: DAC ramp 到地时自动关断功放 */
#define ES9038_AMP_PDB_Msk (0x40U)    /**< 1: 使能耳机功放 (HPA) */
#define ES9038_SW_CTRL_EN_Pos (0U)
#define ES9038_SW_CTRL_EN_Msk (0x03UL)
#define ES9038_SW_CTRL_EXTERNAL (0U)   /**< 开关由外部控制 (默认) */
#define ES9038_SW_CTRL_OVERRIDE_0 (1U) /**< 强制 = 0 (AUX关闭) */
#define ES9038_SW_CTRL_OVERRIDE_1 (3U) /**< 强制 = 1 (AUX开启) */

/* ---- Reg40-44 0x28-0x2C: Programmable FIR ------------------------------------------- */
#define ES9038_FIR_STAGE_Pos (7U)
#define ES9038_FIR_STAGE_Msk (0x80U)
#define ES9038_FIR_STAGE_1 (0x00U)             /**< 过采样滤波器 stage1 (128系数) */
#define ES9038_FIR_STAGE_2 (0x80U)             /**< 过采样滤波器 stage2 (16系数) */
#define ES9038_FIR_ADDR_Msk (0x7FU)
#define ES9038_FIR_CFG_STAGE2_EVEN_Msk (0x04U) /**< 1: stage2 余弦对称(28系数) */
#define ES9038_FIR_CFG_PROG_WE_Msk (0x02U)     /**< 1: 允许写系数RAM */
#define ES9038_FIR_CFG_PROG_EN_Msk (0x01U)     /**< 1: 启用自定义滤波器系数 */

/* ---- Reg45 0x2D: Low Power & Auto Calibration ----------------------------------------- */
#define ES9038_CALIB_EN_Msk (0x20U)    /**< 1: 使能 ADC 自动增益校准 */
#define ES9038_CALIB_LATCH_Msk (0x10U) /**< 1: 持续采用校准更新值 */
#define ES9038_BIAS_CTRL_Msk (0x01U)   /**< BIAS 引脚输出电平 */

/* ---- Reg46 0x2E: ADC Configuration ------------------------------------------------------ */
#define ES9038_ADC_ORDER_Msk (0x40U) /**< 1: 二阶调制器 (推荐) */
#define ES9038_ADC_CLK_Pos (4U)
#define ES9038_ADC_CLK_Msk (0x3UL << ES9038_ADC_CLK_Pos)
#define ES9038_ADC_CLK_EQ_CLK (0x0UL << ES9038_ADC_CLK_Pos) /**< ADC_CLK = CLK */
#define ES9038_ADC_CLK_EQ_CLK_DIV2 (0x1UL << ES9038_ADC_CLK_Pos)
#define ES9038_ADC_CLK_EQ_CLK_DIV4 (0x2UL << ES9038_ADC_CLK_Pos)
#define ES9038_ADC_CLK_EQ_CLK_DIV8 (0x3UL << ES9038_ADC_CLK_Pos)
#define ES9038_ADC_DITHERB_Msk (0x04U) /**< 0: TPDF 抖动开启 (默认); 1: 关闭 */
#define ES9038_ADC_PDB_Msk (0x01U)     /**< 1: 使能 ADC 模拟级 */

/* ---- Reg47-52 0x2F-0x34: ADC Filter (16位 x3) -------------------------------------------- */
#define ES9038_ADC_FTR_SCALE_DEFAULT (992U) /**< 低通默认 */
#define ES9038_ADC_FBQ_SCALE1_DEFAULT (1024U)
#define ES9038_ADC_FBQ_SCALE2_DEFAULT (1024U)

/* ---- Reg64 0x40: Chip ID & Status (只读) ------------------------------------------------------ */
#define ES9038_CHIP_ID_Pos (2U)
#define ES9038_CHIP_ID_Msk (0xFCU)
#define ES9038_STATUS_AUTOMUTE_Msk (0x02U) /**< 1: automute 已触发 */
#define ES9038_STATUS_LOCK_Msk (0x01U)     /**< 1: DPLL 已锁定 / 主模式 */

/* ---- Reg65 0x41: GPIO Readback (只读) ---------------------------------------------------------- */
#define ES9038_GPIO_READ_GPIO1_Msk (0x01U)
#define ES9038_GPIO_READ_GPIO2_Msk (0x02U)

/* ---- Reg96 0x60: Input Status (只读) ------------------------------------------------------------ */
#define ES9038_IN_ST_DOP_VALID_Msk (0x08U)   /**< 检测到有效 DoP 信号 */
#define ES9038_IN_ST_SPDIF_VALID_Msk (0x04U) /**< 检测到有效 SPDIF 信号 */
#define ES9038_IN_ST_I2S_SELECT_Msk (0x02U)  /**< 检测到有效帧/位时钟 */
#define ES9038_IN_ST_DSD_SELECT_Msk (0x01U)  /**< DSD 解码器作为回退在用 */

/* ========================================================================= */
/* 功能块配置结构体                                                            */
/* 说明: 枚举/State 类字段传 ENABLE/DISABLE 原始值; 带宏选项的字段传上面已移位   */
/*       的 ES9038_xxx 配置宏。所有函数传 NULL 表示使用内置默认配置。            */
/* ========================================================================= */

/** @brief Reg0 时钟与振荡器配置 (ES9038_Clock_Config) */
typedef struct
{
    uint32_t OscDrive; /**< ES9038_OSC_DRIVE_xxx 振荡器偏置电流 */
    uint32_t ClkGear;  /**< ES9038_CLK_GEAR_xxx  内部时钟分频 (省电) */
} ES9038_Clock_InitTypeDef;

/** @brief Reg1 串行音频接口 (SAI) 配置 (ES9038_SAI_Config) */
typedef struct
{
    uint32_t Format;      /**< ES9038_SAI_FORMAT_xxx  I2S / 左对齐 / 右对齐 */
    uint32_t WordLength;  /**< ES9038_SAI_WORD_xxx    16 / 24 / 32 bit */
    uint32_t InputSource; /**< ES9038_INPUT_xxx  仅当 AutoSelect=DISABLE 时生效 */
    uint32_t AutoSelect;  /**< ES9038_AUTOSEL_xxx 输入自动检测 */
} ES9038_SAI_InitTypeDef;

/** @brief Reg10 主/从模式与同步配置 (ES9038_MasterMode_Config) */
typedef struct
{
    ES9038_StateTypeDef Mode;      /**< ENABLE: DAC 为时钟主; DISABLE: 从 (默认) */
    uint32_t ClockDivider;         /**< ES9038_MASTER_BCLK_EQ_xxx 主模式 BCLK 分频 */
    ES9038_StateTypeDef Sync128Fs; /**< ENABLE: MCLK=128*FSR 同步模式 (旁路DPLL) */
    uint32_t LockSpeed;            /**< ES9038_LOCK_SPEED_xxx DPLL/ASRC 锁定速度 */
} ES9038_MasterMode_InitTypeDef;

/** @brief Reg12/Reg27 ASRC 与 DPLL 带宽配置 (ES9038_ASRC_Config) */
typedef struct
{
    ES9038_StateTypeDef AsrcEnable; /**< ENABLE: 使能 ASRC (库内默认使能) */
    uint32_t SerialDpllBw;          /**< ES9038_DPLL_BW_SERIAL_xxx / _VAL(n) I2S模式带宽 */
    uint32_t DsdDpllBw;             /**< ES9038_DPLL_BW_DSD_xxx / _VAL(n)    DSD模式带宽 */
} ES9038_ASRC_InitTypeDef;

/** @brief Reg7 数字滤波器配置 (ES9038_Filter_Config) */
typedef struct
{
    uint32_t Shape;                /**< ES9038_FILTER_xxx  8x FIR 插值滤波器形状 */
    ES9038_StateTypeDef BypassOsf; /**< ENABLE: 旁路内部过采样滤波器 (需外部8x过采样) */
} ES9038_Filter_InitTypeDef;

/** @brief Reg6 去加重 (de-emphasis) 配置 (ES9038_DeEmphasis_Config) */
typedef struct
{
    ES9038_StateTypeDef AutoEnable; /**< ENABLE: 依 SPDIF channel status 自动施加 */
    ES9038_StateTypeDef Bypass;     /**< ENABLE: 旁路(禁用)去加重滤波器; DISABLE: 启用 */
    uint32_t Select;                /**< ES9038_DEEMPH_xxx KHz 滤波器选择 */
} ES9038_DeEmphasis_InitTypeDef;

/** @brief Reg6/Reg27 音量相关配置 (ES9038_Volume_Config) — 音量值本身用 ES9038_Volume_Set */
typedef struct
{
    uint32_t RampRate;                /**< ES9038_VOLUME_RATE_DEFAULT / 0~7, 斜坡速率 = 2^rate*FSR/512 dB/s */
    ES9038_StateTypeDef Latch;        /**< ENABLE: 内部音量系数实时跟随寄存器 (默认) */
    ES9038_StateTypeDef ChannelShare; /**< ENABLE: 通道2共用通道1音量 (仅PCM) */
    uint32_t Gain18;                  /**< ES9038_GAIN18_xxx 数据通路 +18dB 增益 */
} ES9038_Volume_InitTypeDef;

/** @brief Reg2/4/5 Automute 配置 (ES9038_Automute_Config) */
typedef struct
{
    uint32_t Action; /**< ES9038_AUTOMUTE_ACTION_xxx 触发后的动作 */
    uint8_t Time;    /**< 0~255; 0 = 关闭 automute。时间(s)=2096896/(Time*FSR) */
    uint8_t Level;   /**< 0~127: 触发门限, 数值 = |dB|, 104 即 -104dB */
} ES9038_Automute_InitTypeDef;

/** @brief Reg14 软启动配置 (ES9038_SoftStart_Config) */
typedef struct
{
    ES9038_StateTypeDef OutputEnable; /**< ENABLE: 输出 ramp 至 AVCC/2 (正常输出);
                                            DISABLE: 输出 ramp 至地 (无模拟输出) */
    ES9038_StateTypeDef StartOnLock;  /**< ENABLE: DPLL 锁定后才 ramp 至 AVCC/2 */
    uint8_t Time;                     /**< 0~20 (默认10): 时间=4096*2^(Time+1)/MCLK */
} ES9038_SoftStart_InitTypeDef;

/** @brief Reg30/31/39 电源与功放配置 (ES9038_Power_Config) */
typedef struct
{
    ES9038_StateTypeDef AmpEnable;   /**< ENABLE: 打开耳机功放 (HPA, Reg39[6]) */
    ES9038_StateTypeDef AmpAutoDown; /**< ENABLE: 输出 ramp 到地时自动关断功放 (省电) */
    uint32_t SwitchCtrl;             /**< ES9038_SW_CTRL_xxx 外部开关(SABRE9602 AUX)控制 */
    uint32_t CpClkSource;            /**< ES9038_CP_CLK_SRC_xxx 电荷泵参考时钟 */
    uint32_t CpClkEnable;            /**< ES9038_CP_CLK_xxx (TRISTATE/GND/DVDD/ACTIVE) */
    uint16_t CpClkDiv;               /**< 0~4095: fcp = XI / (div*2), 0为默认 */
} ES9038_Power_InitTypeDef;

/** @brief Reg8/21/29 GPIO 配置 (ES9038_GPIO_Config) */
typedef struct
{
    uint8_t Gpio1Func;     /**< ES9038_GPIO_FUNC_xxx GPIO1 功能 */
    uint8_t Gpio2Func;     /**< ES9038_GPIO_FUNC_xxx GPIO2 功能 */
    uint8_t Gpio1InputSel; /**< ES9038_GPIO_INPUT_xxx GPIO1=Input Select 时的选择 */
    uint8_t Gpio2InputSel; /**< ES9038_GPIO_INPUT_xxx GPIO2=Input Select 时的选择 */
    uint8_t Invert;        /**< ES9038_GPIO_INVERT_xxx 输出反相 */
} ES9038_GPIO_InitTypeDef;

/** @brief Reg3/Reg11 SPDIF 配置 (ES9038_SPDIF_Config) */
typedef struct
{
    uint32_t Source;                 /**< ES9038_SPDIF_IN_xxx SPDIF 解码输入引脚 */
    ES9038_StateTypeDef UserBits;    /**< ENABLE: 回读接口呈现 user bits 而非 channel status */
    ES9038_StateTypeDef IgnoreData;  /**< ENABLE: 忽略 data 标志 (压缩流建议开) */
    ES9038_StateTypeDef IgnoreValid; /**< ENABLE: 忽略 valid 标志 */
} ES9038_SPDIF_InitTypeDef;

/** @brief Reg2 通道映射/混音配置 (ES9038_Mix_Config) */
typedef struct
{
    uint32_t Dac1Source; /**< ES9038_MIX_DAC1_FROM_xxx */
    uint32_t Dac2Source; /**< ES9038_MIX_DAC2_FROM_xxx */
} ES9038_Mix_InitTypeDef;

/** @brief Reg13/22-25 THD 补偿配置 (ES9038_THD_Config) */
typedef struct
{
    ES9038_StateTypeDef Enable; /**< ENABLE: 使能 THD 补偿 (默认关闭) */
    int16_t CoefC2;             /**< 16位有符号: 二次谐波补偿系数 */
    int16_t CoefC3;             /**< 16位有符号: 三次谐波补偿系数 */
} ES9038_THD_InitTypeDef;

/** @brief Reg45 自动增益校准配置 (ES9038_Calibration_Config) */
typedef struct
{
    ES9038_StateTypeDef Enable;  /**< ENABLE: 使能基于 ADC 的 master trim 自动校准 */
    ES9038_StateTypeDef Latch;   /**< ENABLE: 持续跟随校准更新值; DISABLE: 锁定当前值 */
    ES9038_StateTypeDef BiasOut; /**< ENABLE: BIAS 引脚输出高; DISABLE: 输出低 */
} ES9038_Calibration_InitTypeDef;

/** @brief Reg46 校准 ADC 配置 (ES9038_ADC_Config) */
typedef struct
{
    ES9038_StateTypeDef Enable;       /**< ENABLE: 打开 ADC 模拟级 (校准需要) */
    ES9038_StateTypeDef SecondOrder;  /**< ENABLE: 二阶调制器 (手册推荐) */
    uint32_t ClockDiv;                /**< ES9038_ADC_CLK_EQ_xxx ADC 时钟分频 */
    ES9038_StateTypeDef DitherEnable; /**< DISABLE: 关闭 TPDF 抖动; ENABLE: 开启(默认) */
} ES9038_ADC_InitTypeDef;

/** @brief Reg47-52 校准 ADC 抽取滤波器 (ES9038_ADCFilter_Config) */
typedef struct
{
    uint16_t FtrScale;  /**< 默认 992  (ES9038_ADC_FTR_SCALE_DEFAULT) */
    uint16_t FbqScale1; /**< 默认 1024 */
    uint16_t FbqScale2; /**< 默认 1024 */
} ES9038_ADCFilter_InitTypeDef;

/* ========================================================================= */
/* 默认配置宏 (结构体初始化器)                                                  */
/* ========================================================================= */
#define ES9038_CLOCK_DEFAULT_CONFIG {ES9038_OSC_DRIVE_FULL_BIAS, ES9038_CLK_GEAR_MCLK_EQ_XI}

#define ES9038_SAI_DEFAULT_CONFIG                  \
    {ES9038_SAI_FORMAT_I2S, ES9038_SAI_WORD_32BIT, \
     ES9038_INPUT_SERIAL, ES9038_AUTOSEL_DISABLE}

#define ES9038_MASTER_DEFAULT_CONFIG                  \
    {ES9038_DISABLE, ES9038_MASTER_BCLK_EQ_MCLK_DIV2, \
     ES9038_DISABLE, ES9038_LOCK_SPEED_5461}

#define ES9038_ASRC_DEFAULT_CONFIG \
    {ES9038_ENABLE, ES9038_DPLL_BW_SERIAL_HIGHEST, ES9038_DPLL_BW_DSD_DEFAULT}

#define ES9038_FILTER_DEFAULT_CONFIG \
    {ES9038_FILTER_APODIZING_FAST, ES9038_DISABLE}

#define ES9038_DEEMPH_DEFAULT_CONFIG \
    {ES9038_DISABLE, ES9038_ENABLE, ES9038_DEEMPH_44KHZ}

#define ES9038_VOLUME_DEFAULT_CONFIG \
    {ES9038_VOLUME_RATE_DEFAULT, ES9038_ENABLE, ES9038_DISABLE, ES9038_GAIN18_NONE}

#define ES9038_AUTOMUTE_DEFAULT_CONFIG                          \
    {ES9038_AUTOMUTE_ACTION_NONE, ES9038_AUTOMUTE_TIME_DISABLE, \
     ES9038_AUTOMUTE_LEVEL_DEFAULT}

#define ES9038_SOFTSTART_DEFAULT_CONFIG \
    {ES9038_ENABLE, ES9038_DISABLE, ES9038_SOFT_START_TIME_DEFAULT}

#define ES9038_POWER_DEFAULT_CONFIG                          \
    {ES9038_ENABLE, ES9038_DISABLE, ES9038_SW_CTRL_EXTERNAL, \
     ES9038_CP_CLK_SRC_XI, ES9038_CP_CLK_TRISTATE, 0U}

#define ES9038_GPIO_DEFAULT_CONFIG                           \
    {ES9038_GPIO_FUNC_ANALOG_SD, ES9038_GPIO_FUNC_ANALOG_SD, \
     ES9038_GPIO_INPUT_SERIAL, ES9038_GPIO_INPUT_SERIAL, ES9038_GPIO_INVERT_NONE}

#define ES9038_SPDIF_DEFAULT_CONFIG \
    {ES9038_SPDIF_IN_DATA_CLK, ES9038_DISABLE, ES9038_DISABLE, ES9038_DISABLE}

#define ES9038_MIX_DEFAULT_CONFIG \
    {ES9038_MIX_DAC1_FROM_CH2, ES9038_MIX_DAC2_FROM_CH1}

#define ES9038_THD_DEFAULT_CONFIG \
    {ES9038_DISABLE, 0, 0}

#define ES9038_CALIB_DEFAULT_CONFIG \
    {ES9038_DISABLE, ES9038_DISABLE, ES9038_DISABLE}

#define ES9038_ADC_DEFAULT_CONFIG \
    {ES9038_DISABLE, ES9038_ENABLE, ES9038_ADC_CLK_EQ_CLK, ES9038_ENABLE}

#define ES9038_ADCFILTER_DEFAULT_CONFIG                           \
    {ES9038_ADC_FTR_SCALE_DEFAULT, ES9038_ADC_FBQ_SCALE1_DEFAULT, \
     ES9038_ADC_FBQ_SCALE2_DEFAULT}
#define ES9038_SAMPLERATE_DEFAULT 48000

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/* ---- 整机初始化 / 复位 --------------------------------------------------- */
/**
 * @brief  整机默认参数初始化: 软复位 -> 芯片ID校验 -> 各功能块默认配置
 * @note   前提: RESETB 硬件复位已释放、外部电源与 XI 已稳定 (见手册上电时序)。
 *         ASRC 默认使能, 本地时钟按 ES9038_MCLK_HZ (24.576MHz) 处理。
 * @retval ES9038_OK / ES9038_ERROR (I2C失败或芯片ID不符)
 */
/* 初始化失败时的步骤号（0 = 成功）。见 .c 里的 ES9038_STEP */
extern volatile uint8_t es9038_init_step;
ES9038_StatusTypeDef ES9038_Init (void);

/**
 * @brief  软复位: 恢复全部寄存器为上电默认值
 * @note   Reg0[0] 恒读0, 复位完成通过其他寄存器回读验证; 内部延时1ms
 */
ES9038_StatusTypeDef ES9038_SoftReset (void);

/* ---- 低层寄存器访问 (供高级用户直接使用) ---------------------------------- */
ES9038_StatusTypeDef ES9038_WriteReg (uint8_t RegAddr, uint8_t Value);
ES9038_StatusTypeDef ES9038_ReadReg (uint8_t RegAddr, uint8_t *Value);
ES9038_StatusTypeDef ES9038_ModifyReg (uint8_t RegAddr, uint8_t Mask, uint8_t Value);

/* ---- 时钟块: Reg0 --------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_Clock_Config (const ES9038_Clock_InitTypeDef *Cfg);

/* ---- SAI 串行音频接口: Reg1 ---------------------------------------------- */
/**
 * @brief  串行音频接口初始化 (格式/字长/输入源/自动检测)
 * @note   InputSource 仅当 AutoSelect = ES9038_AUTOSEL_DISABLE 时生效;
 *         主模式下手册要求必须显式选择输入源 (不能用自动检测)。
 */
ES9038_StatusTypeDef ES9038_SAI_Config (const ES9038_SAI_InitTypeDef *Cfg);

/* ---- 主从模式: Reg10 + Reg34-37 NCO --------------------------------------- */
ES9038_StatusTypeDef ES9038_MasterMode_Config (const ES9038_MasterMode_InitTypeDef *Cfg);

/**
 * @brief  主模式下用 NCO 设定目标采样率 (FSR = nco * MCLK / 2^32)
 * @param  SampleRateHz 目标采样率 (Hz), 须小于 ES9038_MCLK_HZ
 * @note   仅主模式有意义; NCO 模式下 master_div 被忽略
 */
ES9038_StatusTypeDef ES9038_SampleRate_Config (uint32_t SampleRateHz);

/* ---- ASRC / DPLL: Reg12 + Reg27[7] ---------------------------------------- */
ES9038_StatusTypeDef ES9038_ASRC_Config (const ES9038_ASRC_InitTypeDef *Cfg);

/* ---- 数字滤波器: Reg7 ------------------------------------------------------ */
ES9038_StatusTypeDef ES9038_Filter_Config (const ES9038_Filter_InitTypeDef *Cfg);

/* ---- 去加重: Reg6[7:4] ----------------------------------------------------- */
ES9038_StatusTypeDef ES9038_DeEmphasis_Config (const ES9038_DeEmphasis_InitTypeDef *Cfg);

/* ---- DoP: Reg6[3] ---------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_DoP_Cmd (ES9038_StateTypeDef State);

/* ---- 音量: Reg6/15/16/17-20/27 -------------------------------------------- */
ES9038_StatusTypeDef ES9038_Volume_Config (const ES9038_Volume_InitTypeDef *Cfg);
/**
 * @brief  设置单通道数字衰减
 * @param  Channel        ES9038_CHANNEL_1 / ES9038_CHANNEL_2
 * @param  Attenuation05  0~255, 单位0.5dB (0=0dB, 80=-40dB);
 *                        可用 ES9038_VOLUME_ATTEN(-6.0f) 由dB值换算
 */
ES9038_StatusTypeDef ES9038_Volume_Set (uint8_t Channel, uint8_t Attenuation05);
/** @brief  设置 0dB 参考电平 (Master Trim, 32位有符号, 默认 0x7FFFFFFF) */
ES9038_StatusTypeDef ES9038_MasterTrim_Set (uint32_t Trim);

/* ---- 静音: Reg7[0] ---------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_MuteCmd (ES9038_StateTypeDef State);

/* ---- Automute: Reg2/4/5 ------------------------------------------------------ */
ES9038_StatusTypeDef ES9038_Automute_Config (const ES9038_Automute_InitTypeDef *Cfg);

/* ---- 软启动: Reg14 ------------------------------------------------------------ */
ES9038_StatusTypeDef ES9038_SoftStart_Config (const ES9038_SoftStart_InitTypeDef *Cfg);

/* ---- 电源/功放: Reg30/31/39 ---------------------------------------------------- */
ES9038_StatusTypeDef ES9038_Power_Config (const ES9038_Power_InitTypeDef *Cfg);

/**
 * @brief  软件睡眠: 静音 -> 输出 ramp 到地 -> 关断耳机功放
 * @note   按 soft_start_time 等待 ramp 完成后返回;
 *         最低功耗待机 (1.3mW) 需硬件拉低 RESETB, 本函数为软件低功耗态,
 *         唤醒后寄存器配置保持不变。
 */
ES9038_StatusTypeDef ES9038_Sleep (void);

/**
 * @brief  从 ES9038_Sleep() 唤醒: 打开功放 -> 输出 ramp 到 AVCC/2 -> 解除静音
 * @note   按 soft_start_time 等待 ramp 完成后返回
 */
ES9038_StatusTypeDef ES9038_Wake (void);

/* ---- GPIO: Reg8/21/29 ---------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_GPIO_Config (const ES9038_GPIO_InitTypeDef *Cfg);

/* ---- SPDIF: Reg3/11 -------------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_SPDIF_Config (const ES9038_SPDIF_InitTypeDef *Cfg);

/* ---- 通道映射: Reg2[3:0] ---------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_Mix_Config (const ES9038_Mix_InitTypeDef *Cfg);

/* ---- THD 补偿: Reg13/22-25 --------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_THD_Config (const ES9038_THD_InitTypeDef *Cfg);

/* ---- 自动增益校准: Reg45 ------------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_Calibration_Config (const ES9038_Calibration_InitTypeDef *Cfg);

/* ---- 校准 ADC: Reg46/47-52 -------------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_ADC_Config (const ES9038_ADC_InitTypeDef *Cfg);
ES9038_StatusTypeDef ES9038_ADCFilter_Config (const ES9038_ADCFilter_InitTypeDef *Cfg);

/* ---- 中断屏蔽: Reg33 --------------------------------------------------------------------- */
ES9038_StatusTypeDef ES9038_InterruptMask_Config (ES9038_StateTypeDef AutomuteMask,
                                                  ES9038_StateTypeDef LockMask);

/* ---- 可编程 FIR 滤波器: Reg40-44 ------------------------------------------------------------ */
/**
 * @brief  下载自定义过采样滤波器系数
 * @param  Coef  系数数组 (24位有符号量化值)
 * @param  Count 128 (stage1) 或 16 (stage2)
 * @note   与手册 "Example Source Code for Loading a Filter" 一致;
 *         下载完成后调用 ES9038_FIR_Cmd(ENABLE) 才会启用
 */
ES9038_StatusTypeDef ES9038_FIR_Load (const int32_t *Coef, uint16_t Count);
/** @brief  启用/关闭自定义滤波器系数 (Reg44[0]) */
ES9038_StatusTypeDef ES9038_FIR_Cmd (ES9038_StateTypeDef State);

/* ---- 状态回读 --------------------------------------------------------------------------------- */
/** @brief  读 Reg64 原始值 (含芯片ID与状态位) */
ES9038_StatusTypeDef ES9038_GetChipId (uint8_t *ChipId);
/** @brief  校验芯片ID (Reg64[7:2] == 0x1C) */
ES9038_StatusTypeDef ES9038_CheckDevice (ES9038_StateTypeDef *Result);
/** @brief  DPLL 锁定状态 (Reg64[0]) */
ES9038_StatusTypeDef ES9038_GetLockStatus (ES9038_StateTypeDef *State);
/** @brief  Automute 触发状态 (Reg64[1]) */
ES9038_StatusTypeDef ES9038_GetAutomuteStatus (ES9038_StateTypeDef *State);
/** @brief  轮询等待 DPLL 锁定 (主模式下本位恒1) */
ES9038_StatusTypeDef ES9038_WaitForLock (uint32_t TimeoutMs);
/** @brief  读 DPLL NUM (Reg66-69, 先读 Reg66 触发锁存); FSR = num*MCLK/2^32 */
ES9038_StatusTypeDef ES9038_GetDpllNumber (uint32_t *Number);
/** @brief  读输入状态 Reg96 (ES9038_IN_ST_xxx 位) */
ES9038_StatusTypeDef ES9038_GetInputStatus (uint8_t *Status);
/** @brief  读 GPIO 电平 Reg65 (低2位有效) */
ES9038_StatusTypeDef ES9038_GetGpioLevel (uint8_t *Level);
/** @brief  读校准 ADC 回读值 Reg100-102 (24位有符号, 读Reg100触发锁存) */
ES9038_StatusTypeDef ES9038_GetAdcValue (uint32_t *Value);

#ifdef __cplusplus
}
#endif

#endif /* ES9038Q2M_HAL_H */
