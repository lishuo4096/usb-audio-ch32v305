

#ifndef ES9028Q2M_HAL_H
#define ES9028Q2M_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* 版本                                                                      */
/* ========================================================================= */
#define ES9028_HAL_VERSION_MAJOR    1U
#define ES9028_HAL_VERSION_MINOR    0U

/* ========================================================================= */
/* 通用类型                                                                   */
/* ========================================================================= */
/** 函数返回状态 */
typedef enum
{
    ES9028_OK    = 0,      /**< 操作成功 */
    ES9028_ERROR = 1       /**< I2C 失败或参数错误 */
} ES9028_StatusTypeDef;

/** 通用使能状态 */
typedef enum
{
    ES9028_DISABLE = 0,
    ES9028_ENABLE  = 1
} ES9028_StateTypeDef;

/* ========================================================================= */
/* 库内部固定参数 (可在包含本头文件之前用编译器宏覆盖)                           */
/* ========================================================================= */
#ifndef ES9028_MCLK_HZ
#define ES9028_MCLK_HZ              24576000U   /**< 本地时钟: 24.576 MHz 晶振 (XI/MCLK) */
#endif

/* 手册时钟要求: PCM普通模式 MCLK>192*FSR (同步模式 MCLK=128*FSR);
 * OSF旁路 MCLK>24*FSR; SPDIF MCLK>386*FSR; DSD MCLK>3*FSR            */
#define ES9028_CHIP_ID_VALUE        1U          /**< Reg64[4:2] 期望芯片 ID (3'd1) */
#define ES9028_I2C_ADDR_7BIT_A0     0x48U       /**< ADDR=0, 8位写地址 0x90 */
#define ES9028_I2C_ADDR_7BIT_A1     0x49U       /**< ADDR=1, 8位写地址 0x92 */

/* 通道选择 (ES9028_Volume_Set / ES9028_MuteCmd) */
#define ES9028_CHANNEL_1            1U          /**< 通道1 (默认左声道) */
#define ES9028_CHANNEL_2            2U          /**< 通道2 (默认右声道) */

/* ========================================================================= */
/* 寄存器地址 (手册 REGISTER SETTINGS 编号 -> 8位地址)                         */
/* 说明: #2/#3、#31-33、#39-42、#44-63 手册未定义, 见注释                      */
/* ========================================================================= */
/* --- 可写寄存器 --- */
#define ES9028_REG_SYSTEM           0x00U   /**< Reg0  : 振荡器偏置/软复位 */
#define ES9028_REG_INPUT_SEL        0x01U   /**< Reg1  : 输入配置 (格式/字长/自动检测) */
/* 0x02, 0x03 手册未定义 */
#define ES9028_REG_AUTOMUTE_TIME    0x04U   /**< Reg4  : Automute 时间 */
#define ES9028_REG_AUTOMUTE_LEVEL   0x05U   /**< Reg5  : Automute 门限/回环动作 */
#define ES9028_REG_DEEMPH           0x06U   /**< Reg6  : 去加重/音量斜坡速率 */
#define ES9028_REG_GENERAL_CFG      0x07U   /**< Reg7  : 滤波器/IIR带宽/双通道静音 */
#define ES9028_REG_GPIO_CFG         0x08U   /**< Reg8  : GPIO1/2 功能配置 */
#define ES9028_REG_RESERVED_09      0x09U   /**< Reg9  : 保留 */
#define ES9028_REG_MASTER_MODE      0x0AU   /**< Reg10 : 主模式/分频/同步/锁定速度 */
#define ES9028_REG_CHANNEL_MAP      0x0BU   /**< Reg11 : 通道映射/SPDIF源选择 */
#define ES9028_REG_DPLL_BW          0x0CU   /**< Reg12 : DPLL 带宽 (I2S/SPDIF 与 DSD) */
#define ES9028_REG_THD_BYPASS       0x0DU   /**< Reg13 : THD 补偿使能 */
#define ES9028_REG_SOFT_START       0x0EU   /**< Reg14 : 软启动配置 */
#define ES9028_REG_VOLUME1          0x0FU   /**< Reg15 : 通道1 音量 */
#define ES9028_REG_VOLUME2          0x10U   /**< Reg16 : 通道2 音量 */
#define ES9028_REG_MASTER_TRIM1     0x11U   /**< Reg17 : Master Trim 字节0 (低字节/LSB) */
#define ES9028_REG_MASTER_TRIM2     0x12U   /**< Reg18 : Master Trim 字节1 */
#define ES9028_REG_MASTER_TRIM3     0x13U   /**< Reg19 : Master Trim 字节2 */
#define ES9028_REG_MASTER_TRIM4     0x14U   /**< Reg20 : Master Trim 字节3 (高字节/MSB) */
#define ES9028_REG_GPIO_INPUT_SEL   0x15U   /**< Reg21 : GPIO输入源选择/IIR与OSF旁路 */
#define ES9028_REG_THD_COMP_C2_1    0x16U   /**< Reg22 : 左/共享 THD C2 字节0 (LSB) */
#define ES9028_REG_THD_COMP_C2_2    0x17U   /**< Reg23 : 左/共享 THD C2 字节1 (MSB) */
#define ES9028_REG_THD_COMP_C3_1    0x18U   /**< Reg24 : 左/共享 THD C3 字节0 (LSB) */
#define ES9028_REG_THD_COMP_C3_2    0x19U   /**< Reg25 : 左/共享 THD C3 字节1 (MSB) */
#define ES9028_REG_PROG_FIR_ADDR    0x1AU   /**< Reg26 : 可编程FIR地址/级选择 */
#define ES9028_REG_PROG_FIR_DATA_1  0x1BU   /**< Reg27 : FIR系数 字节0 (低字节) */
#define ES9028_REG_PROG_FIR_DATA_2  0x1CU   /**< Reg28 : FIR系数 字节1 */
#define ES9028_REG_PROG_FIR_DATA_3  0x1DU   /**< Reg29 : FIR系数 字节2 (高字节) */
#define ES9028_REG_PROG_FIR_CFG     0x1EU   /**< Reg30 : FIR 控制 */
/* 0x1F-0x21 (Reg31-33) 手册未定义 */
#define ES9028_REG_THD_COMP_C2_R1   0x22U   /**< Reg34 : 右声道 THD C2 字节0 (LSB) */
#define ES9028_REG_THD_COMP_C2_R2   0x23U   /**< Reg35 : 右声道 THD C2 字节1 (MSB) */
#define ES9028_REG_THD_COMP_C3_R1   0x24U   /**< Reg36 : 右声道 THD C3 字节0 (LSB) */
#define ES9028_REG_THD_COMP_C3_R2   0x25U   /**< Reg37 : 右声道 THD C3 字节1 (MSB) */
#define ES9028_REG_THD_SEPARATE     0x26U   /**< Reg38 : 左右独立 THD 补偿使能 */
/* 0x27-0x2A (Reg39-42) 手册未定义 */
#define ES9028_REG_SLIMBUS          0x2BU   /**< Reg43 : SLIMbus 控制 */
/* 0x2C-0x3F (Reg44-63) 手册未定义 */

/* --- 只读寄存器 --- */
#define ES9028_REG_CHIP_STATUS      0x40U   /**< Reg64 : 修订/芯片ID/Automute/锁定状态 */
#define ES9028_REG_GPIO_STATUS      0x41U   /**< Reg65 : GPIO 电平回读 */
#define ES9028_REG_DPLL_NUM_1       0x42U   /**< Reg66 : DPLL NUM 字节0 (LSB, 读序: 先读此寄存器触发锁存) */
#define ES9028_REG_DPLL_NUM_2       0x43U   /**< Reg67 : DPLL NUM 字节1 */
#define ES9028_REG_DPLL_NUM_3       0x44U   /**< Reg68 : DPLL NUM 字节2 */
#define ES9028_REG_DPLL_NUM_4       0x45U   /**< Reg69 : DPLL NUM 字节3 (MSB) */
#define ES9028_REG_SPDIF_CHSTATUS_1 0x46U   /**< Reg70 : SPDIF 通道状态 字节0 (LSB) */
#define ES9028_REG_SPDIF_CHSTATUS_5 0x4AU   /**< Reg74 : SPDIF 通道状态 字节4 (MSB) */

/* ========================================================================= */
/* 寄存器位域宏                                                               */
/* 每个配置宏均已按寄存器位位置移位, 结构体字段可直接按位或写入                    */
/* ========================================================================= */

/* ---- Reg0 0x00: System -------------------------------------------------- */
#define ES9028_SYS_OSC_DRV_Pos          (4U)
#define ES9028_SYS_OSC_DRV_Msk          (0xFUL << ES9028_SYS_OSC_DRV_Pos)
#define ES9028_OSC_DRIVE_FULL_BIAS      (0x0UL << ES9028_SYS_OSC_DRV_Pos) /**< 全偏置 (默认) */
#define ES9028_OSC_DRIVE_3_4_BIAS       (0x8UL << ES9028_SYS_OSC_DRV_Pos) /**< 3/4 偏置 */
#define ES9028_OSC_DRIVE_1_2_BIAS       (0xCUL << ES9028_SYS_OSC_DRV_Pos) /**< 1/2 偏置 (省电) */
#define ES9028_OSC_DRIVE_1_4_BIAS       (0xEUL << ES9028_SYS_OSC_DRV_Pos) /**< 1/4 偏置 (省电) */
#define ES9028_OSC_DRIVE_SHUTDOWN       (0xFUL << ES9028_SYS_OSC_DRV_Pos) /**< 关断振荡器 */
/* Reg0[3:1] 为保留位, 必须保持 0; 本芯片无 clk_gear 时钟分频 */
#define ES9028_SYS_SOFT_RESET_Msk       (0x01U)  /**< 写1触发软复位, 自清零且恒读0 */

/* ---- Reg1 0x01: Input Configuration ------------------------------------- */
#define ES9028_IN_I2S_LEN_Pos           (6U)
#define ES9028_IN_I2S_LEN_Msk           (0x3UL << ES9028_IN_I2S_LEN_Pos)
#define ES9028_SAI_WORD_16BIT           (0x0UL << ES9028_IN_I2S_LEN_Pos) /**< 16-bit */
#define ES9028_SAI_WORD_24BIT           (0x1UL << ES9028_IN_I2S_LEN_Pos) /**< 24-bit */
#define ES9028_SAI_WORD_32BIT           (0x2UL << ES9028_IN_I2S_LEN_Pos) /**< 32-bit (默认) */

#define ES9028_IN_I2S_MODE_Pos          (4U)
#define ES9028_IN_I2S_MODE_Msk          (0x3UL << ES9028_IN_I2S_MODE_Pos)
#define ES9028_SAI_FORMAT_I2S           (0x0UL << ES9028_IN_I2S_MODE_Pos) /**< I2S (默认) */
#define ES9028_SAI_FORMAT_LEFT_JUST     (0x1UL << ES9028_IN_I2S_MODE_Pos) /**< 左对齐 */
/* 注: 本芯片不支持右对齐; 2'd2/2'd3 与 2'd0/2'd1 含义相同 */

#define ES9028_IN_AUTO_SEL_Pos          (2U)
#define ES9028_IN_AUTO_SEL_Msk          (0x3UL << ES9028_IN_AUTO_SEL_Pos)
#define ES9028_AUTOSEL_DISABLE          (0x0UL << ES9028_IN_AUTO_SEL_Pos) /**< 关闭自动, 用 InputSource */
#define ES9028_AUTOSEL_I2S_DSD          (0x1UL << ES9028_IN_AUTO_SEL_Pos) /**< 自动: I2S / DSD */
#define ES9028_AUTOSEL_I2S_SPDIF        (0x2UL << ES9028_IN_AUTO_SEL_Pos) /**< 自动: I2S / SPDIF */
#define ES9028_AUTOSEL_ALL              (0x3UL << ES9028_IN_AUTO_SEL_Pos) /**< 自动: I2S/SPDIF/DSD (默认) */

#define ES9028_IN_SEL_Pos               (0U)
#define ES9028_IN_SEL_Msk               (0x3UL << ES9028_IN_SEL_Pos)
#define ES9028_INPUT_I2S                (0x0UL << ES9028_IN_SEL_Pos)  /**< 串行 PCM (默认) */
#define ES9028_INPUT_SPDIF              (0x1UL << ES9028_IN_SEL_Pos)  /**< SPDIF */
#define ES9028_INPUT_DSD                (0x3UL << ES9028_IN_SEL_Pos)  /**< DSD */

/* ---- Reg4 0x04: Automute Time ------------------------------------------- */
#define ES9028_AUTOMUTE_TIME_DISABLE    (0x00U)  /**< 0 = 关闭 automute (默认) */
/* 触发时间(s) = 2096896 / (automute_time * DATA_CLK), 非0有效 */

/* ---- Reg5 0x05: Automute Level & Loopback --------------------------------- */
#define ES9028_AUTOMUTE_LOOPBACK_Msk    (0x80U)  /**< 1: automute 时 ramp 到 - */
#define ES9028_AUTOMUTE_LEVEL_Msk       (0x7FU)
#define ES9028_AUTOMUTE_LEVEL_DEFAULT   (104U)   /**< 默认 -104dB (数值 = |dB|) */
#define ES9028_AUTOMUTE_ACTION_NONE     (0x00U)  /**< 触发后无动作 (默认) */
#define ES9028_AUTOMUTE_ACTION_RAMP_INF (0x80U)  /**< 触发后 ramp 到 - */

/* ---- Reg6 0x06: De-emphasis / Volume Ramp ---------------------------------- */
#define ES9028_DEEMPH_AUTO_Msk          (0x80U)  /**< 1: SPDIF 模式自动去加重 */
#define ES9028_DEEMPH_BYPASS_Msk        (0x40U)  /**< 1: 旁路(禁用)去加重滤波器 (默认) */
#define ES9028_DEEMPH_SEL_Pos           (4U)
#define ES9028_DEEMPH_SEL_Msk           (0x3UL << ES9028_DEEMPH_SEL_Pos)
#define ES9028_DEEMPH_32KHZ             (0x0UL << ES9028_DEEMPH_SEL_Pos) /**< 32kHz (默认) */
#define ES9028_DEEMPH_44KHZ             (0x1UL << ES9028_DEEMPH_SEL_Pos) /**< 44.1kHz */
#define ES9028_DEEMPH_48KHZ             (0x2UL << ES9028_DEEMPH_SEL_Pos) /**< 48kHz */
/* Reg6[3] 保留位, 必须保持 1 才能正常工作 (读改写时自然保持) */
#define ES9028_VOLUME_RATE_Pos          (0U)
#define ES9028_VOLUME_RATE_Msk          (0x7UL)
#define ES9028_VOLUME_RATE_DEFAULT      (2U)     /**< 默认 2; 斜坡 = 0.0078125*fs/2^(rate-5) dB/s */

/* ---- Reg7 0x07: General Settings ------------------------------------------- */
/* Reg7[7]、Reg7[4] 为保留位, 必须保持默认电平 (读改写时自然保持) */
#define ES9028_FILTER_SHAPE_Pos         (5U)
#define ES9028_FILTER_SHAPE_Msk         (0x3UL << ES9028_FILTER_SHAPE_Pos)
#define ES9028_FILTER_FAST_ROLL_OFF     (0x0UL << ES9028_FILTER_SHAPE_Pos) /**< 快速衰减 (默认) */
#define ES9028_FILTER_SLOW_ROLL_OFF     (0x1UL << ES9028_FILTER_SHAPE_Pos) /**< 慢速衰减 */
#define ES9028_FILTER_MIN_PHASE         (0x2UL << ES9028_FILTER_SHAPE_Pos) /**< 最小相位 */

#define ES9028_IIR_BW_Pos               (2U)
#define ES9028_IIR_BW_Msk               (0x3UL << ES9028_IIR_BW_Pos)
#define ES9028_IIR_BW_NORMAL            (0x0UL << ES9028_IIR_BW_Pos) /**< 1.0757*fs (默认) */
#define ES9028_IIR_BW_50KHZ             (0x1UL << ES9028_IIR_BW_Pos) /**< 1.1338*fs */
#define ES9028_IIR_BW_60KHZ             (0x2UL << ES9028_IIR_BW_Pos) /**< 1.3605*fs */
#define ES9028_IIR_BW_70KHZ             (0x3UL << ES9028_IIR_BW_Pos) /**< 1.5873*fs */

#define ES9028_MUTE_CH1_Msk             (0x01U)  /**< bit0: 通道1 (左) 静音 */
#define ES9028_MUTE_CH2_Msk             (0x02U)  /**< bit1: 通道2 (右) 静音 */
#define ES9028_MUTE_BOTH_Msk            (0x03U)  /**< 双通道静音 */

/* ---- Reg8 0x08: GPIO Configuration ------------------------------------------- */
#define ES9028_GPIO1_CFG_Pos            (0U)
#define ES9028_GPIO1_CFG_Msk            (0x0FUL)
#define ES9028_GPIO2_CFG_Pos            (4U)
#define ES9028_GPIO2_CFG_Msk            (0xFUL << ES9028_GPIO2_CFG_Pos)
/* GPIO 功能编号 (0~15, 配置时按位移入 GPIO1/GPIO2 字段) */
#define ES9028_GPIO_FUNC_AUTOMUTE_ST    0U    /**< 输出: automute 状态 */
#define ES9028_GPIO_FUNC_DPLL_LOCK_ST   1U    /**< 输出: DPLL 锁定状态 */
#define ES9028_GPIO_FUNC_VOLUME_MIN     2U    /**< 输出: 双通道音量到最小 (-127.5dB) */
#define ES9028_GPIO_FUNC_MCLK_OUT       3U    /**< 输出: MCLK */
#define ES9028_GPIO_FUNC_LOCK_IRQ       4U    /**< 输出: 锁定变化中断 (读Reg64清除) */
#define ES9028_GPIO_FUNC_AUTOMUTE_IRQ   5U    /**< 输出: automute 变化中断 (读Reg64清除) */
#define ES9028_GPIO_FUNC_IRQ_BOTH       6U    /**< 输出: 锁定或automute中断 (读Reg64清除) */
#define ES9028_GPIO_FUNC_OUTPUT_LOW     7U    /**< 输出: 恒低 */
#define ES9028_GPIO_FUNC_INPUT          8U    /**< 输入: 高阻, 电平见 Reg65 (默认) */
#define ES9028_GPIO_FUNC_INPUT_SEL      9U    /**< 输入: 输入源切换 (配合 Reg21) */
#define ES9028_GPIO_FUNC_OUTPUT_HIGH    15U   /**< 输出: 恒高 */

/* ---- Reg10 0x0A: Master Mode Control ------------------------------------------ */
#define ES9028_MASTER_MODE_Msk          (0x80U)  /**< 1: 主模式 (驱动 BCLK/帧时钟) */
#define ES9028_CLOCK_DIV_Pos            (5U)
#define ES9028_CLOCK_DIV_Msk            (0x3UL << ES9028_CLOCK_DIV_Pos)
#define ES9028_MASTER_BCLK_MCLK_DIV4    (0x0UL << ES9028_CLOCK_DIV_Pos) /**< BCLK=MCLK/4 (默认) */
#define ES9028_MASTER_BCLK_MCLK_DIV8    (0x1UL << ES9028_CLOCK_DIV_Pos) /**< BCLK=MCLK/8 */
#define ES9028_MASTER_BCLK_MCLK_DIV16   (0x2UL << ES9028_CLOCK_DIV_Pos) /**< BCLK=MCLK/16 */
/* 帧时钟 = BCLK/64; 2'b11 与 2'b10 相同 (MCLK/16) */
#define ES9028_SYNC_MODE_Msk            (0x10U)  /**< 1: 快速锁定 (fs与MCLK同步且MCLK=128*FSR, 仅PCM普通模式) */
#define ES9028_STOP_DIV_Pos             (0U)
#define ES9028_STOP_DIV_Msk             (0x0FUL)
#define ES9028_STOP_DIV_16384           (0U)   /**< 16384 FSR 边沿 (FSR>96kHz 时手册要求用此值) */
#define ES9028_STOP_DIV_8192            (1U)
#define ES9028_STOP_DIV_5461            (2U)   /**< 默认 */
#define ES9028_STOP_DIV_4096            (3U)
#define ES9028_STOP_DIV_3276            (4U)
#define ES9028_STOP_DIV_2730            (5U)
#define ES9028_STOP_DIV_2340            (6U)
#define ES9028_STOP_DIV_2048            (7U)
#define ES9028_STOP_DIV_1820            (8U)
#define ES9028_STOP_DIV_1638            (9U)
#define ES9028_STOP_DIV_1489            (10U)
#define ES9028_STOP_DIV_1365            (11U)
#define ES9028_STOP_DIV_1260            (12U)
#define ES9028_STOP_DIV_1170            (13U)
#define ES9028_STOP_DIV_1092            (14U)
#define ES9028_STOP_DIV_1024            (15U)  /**< 1024 FSR 边沿 (最快) */

/* ---- Reg11 0x0B: Channel Mapping ----------------------------------------------- */
#define ES9028_MAP_SPDIF_SEL_Pos        (4U)
#define ES9028_MAP_SPDIF_SEL_Msk        (0x7UL << ES9028_MAP_SPDIF_SEL_Pos)
#define ES9028_SPDIF_IN_DATA_CLK        (0x0UL << ES9028_MAP_SPDIF_SEL_Pos) /**< DATA_CLK (默认) */
#define ES9028_SPDIF_IN_DATA2           (0x1UL << ES9028_MAP_SPDIF_SEL_Pos) /**< DATA2 */
#define ES9028_SPDIF_IN_DATA1           (0x2UL << ES9028_MAP_SPDIF_SEL_Pos) /**< DATA1 */
#define ES9028_SPDIF_IN_GPIO1           (0x3UL << ES9028_MAP_SPDIF_SEL_Pos) /**< GPIO1 (需配置为输入) */
#define ES9028_SPDIF_IN_GPIO2           (0x4UL << ES9028_MAP_SPDIF_SEL_Pos) /**< GPIO2 (需配置为输入) */
/* Reg11[7] 保留位, 保持 0 */
#define ES9028_CH2_ANALOG_SWAP_Msk      (0x08U)  /**< 1: 通道2 交换 DAC/DACB */
#define ES9028_CH1_ANALOG_SWAP_Msk      (0x04U)  /**< 1: 通道1 交换 DAC/DACB */
#define ES9028_CH2_SEL_Msk              (0x02U)  /**< 通道2 音频源: 0=左, 1=右 (默认) */
#define ES9028_CH1_SEL_Msk              (0x01U)  /**< 通道1 音频源: 0=左 (默认), 1=右 */
#define ES9028_MIX_CH_FROM_LEFT         (0x00U)  /**< 选择左声道数据 */
#define ES9028_MIX_CH_FROM_RIGHT        (0x01U)  /**< 选择右声道数据 */

/* ---- Reg12 0x0C: DPLL/ASRC Settings -------------------------------------------- */
#define ES9028_DPLL_BW_I2S_Pos          (4U)
#define ES9028_DPLL_BW_I2S_Msk          (0xFUL << ES9028_DPLL_BW_I2S_Pos)
#define ES9028_DPLL_BW_I2S_OFF          (0x0UL << ES9028_DPLL_BW_I2S_Pos)
#define ES9028_DPLL_BW_I2S_LOWEST       (0x1UL << ES9028_DPLL_BW_I2S_Pos)
#define ES9028_DPLL_BW_I2S_DEFAULT      (0x5UL << ES9028_DPLL_BW_I2S_Pos) /**< 默认 (I2S/SPDIF) */
#define ES9028_DPLL_BW_I2S_HIGHEST      (0xFUL << ES9028_DPLL_BW_I2S_Pos)
#define ES9028_DPLL_BW_I2S_VAL(n)       (((uint32_t)(n) & 0x0FU) << ES9028_DPLL_BW_I2S_Pos)

#define ES9028_DPLL_BW_DSD_Pos          (0U)
#define ES9028_DPLL_BW_DSD_Msk          (0x0FUL)
#define ES9028_DPLL_BW_DSD_OFF          (0x0U)
#define ES9028_DPLL_BW_DSD_LOWEST       (0x1U)
#define ES9028_DPLL_BW_DSD_DEFAULT      (0x0AU)  /**< 默认 (DSD) */
#define ES9028_DPLL_BW_DSD_HIGHEST      (0x0FU)
#define ES9028_DPLL_BW_DSD_VAL(n)       ((uint32_t)(n) & 0x0FU)

/* ---- Reg13 0x0D: THD Compensation ---------------------------------------------- */
/* Reg13[5:0]、[7] 保留, 保持默认 0 */
#define ES9028_THD_BYPASS_Msk           (0x40U)
#define ES9028_THD_ENABLE               (0x00U << 6) /**< bypass_thd=0: 使能THD补偿 (注意位反转) */
#define ES9028_THD_DISABLE              (0x01U << 6) /**< bypass_thd=1: 禁用THD补偿 (默认) */

/* ---- Reg14 0x0E: Soft Start ------------------------------------------------------ */
#define ES9028_SOFT_START_Msk           (0x80U)  /**< 1: ramp到AVCC/2正常输出 (默认); 0: ramp到地 */
#define ES9028_SOFT_START_ON_LOCK_Msk   (0x40U)  /**< 1: 失锁时强制输出拉低 */
#define ES9028_MUTE_ON_LOCK_Msk         (0x20U)  /**< 1: 失锁时强制静音 */
#define ES9028_SOFT_START_TIME_Pos      (0U)
#define ES9028_SOFT_START_TIME_Msk      (0x1FUL)
#define ES9028_SOFT_START_TIME_DEFAULT  (10U)    /**< 有效范围 0~20 */
/* ramp 时间(s) = 4096 * 2^(time+1) / MCLK */

/* ---- Reg15/16 0x0F/0x10: Volume ---------------------------------------------------- */
#define ES9028_VOLUME_REG_FULL_SCALE    (0U)      /**< 0x00 = 0 dB */
#define ES9028_VOLUME_REG_MAX           (255U)    /**< 0xFF = -127.5 dB */
/* 步进 0.5dB: 衰减dB -> 寄存器值 = |dB| * 2, 例: -40dB -> 80 (上电默认) */
#define ES9028_VOLUME_ATTEN(db)         ((uint8_t)((-(float)(db)) * 2.0f)) /**< 传入负dB值, 如 -6.0f */

/* ---- Reg17-20 0x11-0x14: Master Trim (手册: Reg17=LSB, Reg20=MSB, 小端) -------------- */
#define ES9028_MASTER_TRIM_FULL_SCALE   (0x7FFFFFFFUL) /**< 默认 0dB 参考 */

/* ---- Reg21 0x15: GPIO Input Sel / OSF & IIR Bypass ----------------------------------- */
#define ES9028_GPIO_IN_SEL2_Pos         (6U)
#define ES9028_GPIO_IN_SEL2_Msk         (0x3UL << ES9028_GPIO_IN_SEL2_Pos)
#define ES9028_GPIO_IN_SEL1_Pos         (4U)
#define ES9028_GPIO_IN_SEL1_Msk         (0x3UL << ES9028_GPIO_IN_SEL1_Pos)
#define ES9028_GPIO_INPUT_I2S           (0U)   /**< GPIO=InputSel 时选择 I2S (默认) */
#define ES9028_GPIO_INPUT_SPDIF         (1U)
#define ES9028_GPIO_INPUT_DSD           (3U)
/* Reg21[3]、[1] 保留位, 保持 0 */
#define ES9028_IIR_BYPASS_Msk           (0x04U)  /**< 1: 旁路 IIR 滤波器 */
#define ES9028_OSF_BYPASS_Msk           (0x01U)  /**< 1: 旁路 8x FIR (需外部8x过采样) */

/* ---- Reg22-25 / Reg34-37: THD Compensation Coefficients --------------------------------- */
/* 16位有符号, 小端 (低字节在偶数寄存器): C2/C3 左(共享)与右声道系数 */

/* ---- Reg26-30: Programmable FIR ---------------------------------------------------------- */
#define ES9028_FIR_STAGE_Msk            (0x80U)
#define ES9028_FIR_STAGE_1              (0x00U)  /**< stage1 (128系数) */
#define ES9028_FIR_STAGE_2              (0x80U)  /**< stage2 (16系数) */
#define ES9028_FIR_ADDR_Msk             (0x7FU)
#define ES9028_FIR_CFG_EVEN_STAGE2_Msk  (0x04U)  /**< 1: stage2 余弦对称(28系数) */
#define ES9028_FIR_CFG_PROG_WE_Msk      (0x02U)  /**< 1: 允许写系数RAM (写期间自定义滤波器被旁路) */
#define ES9028_FIR_CFG_PROG_EN_Msk      (0x01U)  /**< 1: 启用自定义滤波器系数 */

/* ---- Reg38 0x26: Separate THD Compensation ------------------------------------------------- */
/* Reg38[7:1] 保留, 保持 0 */
#define ES9028_THD_SEPARATE_Msk         (0x01U)  /**< 1: 右声道使用 Reg34-37 系数 */

/* ---- Reg43 0x2B: SLIMbus Control ---------------------------------------------------------------- */
/* Reg43[7:4] 保留, 保持 0 */
#define ES9028_SB_AUX_WK_EN_Msk         (0x08U)
#define ES9028_SB_AUX_WK_OVERRIDE_Msk   (0x04U)
#define ES9028_SB_DRV_STRENGTH_EN_Msk   (0x02U)
#define ES9028_SB_DRV_STRENGTH_OVR_Msk  (0x01U)

/* ---- Reg64 0x40: Chip Status (只读) -------------------------------------------------------------- */
#define ES9028_CHIP_REVISION_Msk        (0x20U)  /**< bit5: 修订号 */
#define ES9028_CHIP_ID_Pos              (2U)
#define ES9028_CHIP_ID_Msk              (0x1CU)  /**< bit[4:2]: 3'd1 = ES9028Q2M */
#define ES9028_STATUS_AUTOMUTE_Msk      (0x02U)  /**< 1: automute 已触发 */
#define ES9028_STATUS_LOCK_Msk          (0x01U)  /**< 1: Jitter Eliminator 已锁定 */

/* ---- Reg65 0x41: GPIO Status (只读) ----------------------------------------------------------------- */
#define ES9028_GPIO_READ_GPIO1_Msk      (0x01U)
#define ES9028_GPIO_READ_GPIO2_Msk      (0x02U)

/* ========================================================================= */
/* 功能块配置结构体                                                            */
/* 说明: 枚举/State 类字段传 ENABLE/DISABLE 原始值; 带宏选项的字段传上面已移位   */
/*       的 ES9028_xxx 配置宏。所有配置函数传 NULL 表示使用内置默认配置。        */
/* ========================================================================= */

/** @brief Reg0 振荡器配置 (ES9028_Clock_Config) — 本芯片无 clk_gear 分频 */
typedef struct
{
    uint32_t OscDrive;   /**< ES9028_OSC_DRIVE_xxx 振荡器偏置电流 */
} ES9028_Clock_InitTypeDef;

/** @brief Reg1 串行音频接口 (SAI) 配置 (ES9028_SAI_Config) */
typedef struct
{
    uint32_t Format;       /**< ES9028_SAI_FORMAT_xxx  I2S / 左对齐 (无右对齐) */
    uint32_t WordLength;   /**< ES9028_SAI_WORD_xxx    16 / 24 / 32 bit */
    uint32_t InputSource;  /**< ES9028_INPUT_xxx  仅当 AutoSelect=DISABLE 时生效 */
    uint32_t AutoSelect;   /**< ES9028_AUTOSEL_xxx 输入自动检测 */
} ES9028_SAI_InitTypeDef;

/**
 * @brief Reg10 主模式配置 (ES9028_MasterMode_Config)
 * @note   主模式仅支持 I2S + 32bit (手册要求 Reg1 先配置为 I2S/32bit);
 *         帧时钟 = BCLK/64; 本芯片无 NCO, 采样率由 BCLK 分频决定
 */
typedef struct
{
    ES9028_StateTypeDef Mode;         /**< ENABLE: DAC 输出 BCLK/帧时钟; DISABLE: 从 (默认) */
    uint32_t ClockDivider;            /**< ES9028_MASTER_BCLK_MCLK_xxx BCLK 分频 */
    ES9028_StateTypeDef Sync128Fs;    /**< ENABLE: 快速锁定同步模式 (MCLK=128*FSR, 仅PCM普通模式) */
    uint32_t StopDiv;                 /**< ES9028_STOP_DIV_xxx 锁定所需 FSR 边沿数 */
} ES9028_MasterMode_InitTypeDef;

/** @brief Reg12 DPLL 带宽配置 (ES9028_DPLL_Config) — 手册称 DPLL/ASRC Settings */
typedef struct
{
    uint32_t I2sDpllBw;   /**< ES9028_DPLL_BW_I2S_xxx / _VAL(n) I2S/SPDIF 模式带宽 */
    uint32_t DsdDpllBw;   /**< ES9028_DPLL_BW_DSD_xxx / _VAL(n) DSD 模式带宽 */
} ES9028_DPLL_InitTypeDef;

/** @brief Reg7 + Reg21 数字滤波器配置 (ES9028_Filter_Config) */
typedef struct
{
    uint32_t Shape;                /**< ES9028_FILTER_xxx  8x FIR 滤波器形状 */
    uint32_t IirBandwidth;         /**< ES9028_IIR_BW_xxx  IIR 带宽 */
    ES9028_StateTypeDef BypassOsf; /**< ENABLE: 旁路 8x FIR (需外部8x过采样) */
    ES9028_StateTypeDef BypassIir; /**< ENABLE: 旁路 IIR 滤波器 */
} ES9028_Filter_InitTypeDef;

/** @brief Reg6 去加重 (de-emphasis) 配置 (ES9028_DeEmphasis_Config) */
typedef struct
{
    ES9028_StateTypeDef AutoEnable;  /**< ENABLE: SPDIF 模式自动施加去加重 */
    ES9028_StateTypeDef Bypass;      /**< ENABLE: 旁路(禁用)去加重滤波器; DISABLE: 启用 */
    uint32_t Select;                 /**< ES9028_DEEMPH_xxx KHz 滤波器选择 */
} ES9028_DeEmphasis_InitTypeDef;

/** @brief Reg4/Reg5 Automute 配置 (ES9028_Automute_Config) */
typedef struct
{
    uint32_t Action;    /**< ES9028_AUTOMUTE_ACTION_xxx 触发后的动作 */
    uint8_t  Time;      /**< 0~255; 0 = 关闭 automute */
    uint8_t  Level;     /**< 0~127: 触发门限, 数值 = |dB|, 104 即 -104dB */
} ES9028_Automute_InitTypeDef;

/** @brief Reg14 软启动配置 (ES9028_SoftStart_Config) */
typedef struct
{
    ES9028_StateTypeDef OutputEnable;    /**< ENABLE: 输出 ramp 至 AVCC/2 (默认);
                                              DISABLE: 输出 ramp 至地 (无模拟输出) */
    ES9028_StateTypeDef ForceLowOnLock;  /**< ENABLE: 失锁时强制输出拉低 */
    ES9028_StateTypeDef MuteOnLock;      /**< ENABLE: 失锁时强制静音 */
    uint8_t  Time;                       /**< 0~20 (默认10): 时间=4096*2^(Time+1)/MCLK */
} ES9028_SoftStart_InitTypeDef;

/** @brief Reg8 + Reg21 GPIO 配置 (ES9028_GPIO_Config) */
typedef struct
{
    uint8_t Gpio1Func;      /**< ES9028_GPIO_FUNC_xxx GPIO1 功能 */
    uint8_t Gpio2Func;      /**< ES9028_GPIO_FUNC_xxx GPIO2 功能 */
    uint8_t GpioLowLevelSel;  /**< ES9028_GPIO_INPUT_xxx GPIO=InputSel 且电平为低时选择的输入 */
    uint8_t GpioHighLevelSel; /**< ES9028_GPIO_INPUT_xxx GPIO=InputSel 且电平为高时选择的输入 */
} ES9028_GPIO_InitTypeDef;

/** @brief Reg11 通道映射配置 (ES9028_Mix_Config) */
typedef struct
{
    uint32_t Dac1Source;        /**< ES9028_MIX_CH_FROM_LEFT / _RIGHT (通道1 音频源) */
    uint32_t Dac2Source;        /**< ES9028_MIX_CH_FROM_LEFT / _RIGHT (通道2 音频源) */
    ES9028_StateTypeDef Dac1AnalogSwap;  /**< ENABLE: 通道1 交换 DAC/DACB */
    ES9028_StateTypeDef Dac2AnalogSwap;  /**< ENABLE: 通道2 交换 DAC/DACB */
} ES9028_Mix_InitTypeDef;

/** @brief Reg13/22-25/34-38 THD 补偿配置 (ES9028_THD_Config) */
typedef struct
{
    ES9028_StateTypeDef Enable;         /**< ENABLE: 使能 THD 补偿 (默认关闭) */
    int16_t CoefC2;                     /**< 左/共享 二次谐波系数 (16位有符号) */
    int16_t CoefC3;                     /**< 左/共享 三次谐波系数 */
    ES9028_StateTypeDef SeparateRight;  /**< ENABLE: 右声道使用独立系数 (Reg34-37) */
    int16_t CoefC2Right;                /**< 右声道 二次谐波系数 */
    int16_t CoefC3Right;                /**< 右声道 三次谐波系数 */
} ES9028_THD_InitTypeDef;

/** @brief Reg43 SLIMbus 控制 (ES9028_SLIMbus_Config) */
typedef struct
{
    ES9028_StateTypeDef AuxWeakEnable;      /**< ENABLE: aux weak 使能 (需 Override 同时开) */
    ES9028_StateTypeDef AuxWeakOverride;    /**< ENABLE: aux weak override */
    ES9028_StateTypeDef DrvStrengthEnable;  /**< ENABLE: 驱动强度使能 (需 Override 同时开) */
    ES9028_StateTypeDef DrvStrengthOverride;/**< ENABLE: 驱动强度 override */
} ES9028_SLIMbus_InitTypeDef;

/* ========================================================================= */
/* 默认配置宏 (结构体初始化器)                                                  */
/* ========================================================================= */
#define ES9028_CLOCK_DEFAULT_CONFIG   { ES9028_OSC_DRIVE_FULL_BIAS }

#define ES9028_SAI_DEFAULT_CONFIG                                                   \
    { ES9028_SAI_FORMAT_I2S, ES9028_SAI_WORD_32BIT,                                \
      ES9028_INPUT_I2S, ES9028_AUTOSEL_DISABLE }

#define ES9028_MASTER_DEFAULT_CONFIG                                                \
    { ES9028_DISABLE, ES9028_MASTER_BCLK_MCLK_DIV4,                                \
      ES9028_DISABLE, ES9028_STOP_DIV_5461 }

#define ES9028_DPLL_DEFAULT_CONFIG                                                  \
    { ES9028_DPLL_BW_I2S_DEFAULT, ES9028_DPLL_BW_DSD_DEFAULT }

#define ES9028_FILTER_DEFAULT_CONFIG                                                \
    { ES9028_FILTER_FAST_ROLL_OFF, ES9028_IIR_BW_NORMAL,                           \
      ES9028_DISABLE, ES9028_DISABLE }

#define ES9028_DEEMPH_DEFAULT_CONFIG                                                \
    { ES9028_DISABLE, ES9028_ENABLE, ES9028_DEEMPH_32KHZ }

#define ES9028_AUTOMUTE_DEFAULT_CONFIG                                              \
    { ES9028_AUTOMUTE_ACTION_NONE, ES9028_AUTOMUTE_TIME_DISABLE,                   \
      ES9028_AUTOMUTE_LEVEL_DEFAULT }

#define ES9028_SOFTSTART_DEFAULT_CONFIG                                             \
    { ES9028_ENABLE, ES9028_DISABLE, ES9028_DISABLE,                               \
      ES9028_SOFT_START_TIME_DEFAULT }

#define ES9028_GPIO_DEFAULT_CONFIG                                                  \
    { ES9028_GPIO_FUNC_INPUT, ES9028_GPIO_FUNC_INPUT,                              \
      ES9028_GPIO_INPUT_I2S, ES9028_GPIO_INPUT_I2S }

#define ES9028_MIX_DEFAULT_CONFIG                                                   \
    { ES9028_MIX_CH_FROM_LEFT, ES9028_MIX_CH_FROM_RIGHT,                           \
      ES9028_DISABLE, ES9028_DISABLE }

#define ES9028_THD_DEFAULT_CONFIG                                                   \
    { ES9028_DISABLE, 0, 0, ES9028_DISABLE, 0, 0 }

#define ES9028_SLIMBUS_DEFAULT_CONFIG                                               \
    { ES9028_DISABLE, ES9028_DISABLE, ES9028_DISABLE, ES9028_DISABLE }

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/* ---- 整机初始化 / 复位 --------------------------------------------------- */
/**
 * @brief  整机默认参数初始化: 软复位 -> 芯片ID校验 -> 各功能块默认配置
 * @note   前提: RESETB 硬件复位已释放、外部电源与 XI 已稳定。
 *         本地时钟按 ES9028_MCLK_HZ (24.576MHz) 处理。
 * @retval ES9028_OK / ES9028_ERROR (I2C失败或芯片ID不符)
 */
ES9028_StatusTypeDef ES9028_Init(void);

/**
 * @brief  软复位: 恢复全部寄存器为上电默认值
 * @note   Reg0[0] 恒读0, 复位完成通过其他寄存器回读验证; 内部延时1ms
 */
ES9028_StatusTypeDef ES9028_SoftReset(void);

/* ---- 低层寄存器访问 (供高级用户直接使用) ---------------------------------- */
ES9028_StatusTypeDef ES9028_WriteReg(uint8_t RegAddr, uint8_t Value);
ES9028_StatusTypeDef ES9028_ReadReg(uint8_t RegAddr, uint8_t *Value);
ES9028_StatusTypeDef ES9028_ModifyReg(uint8_t RegAddr, uint8_t Mask, uint8_t Value);

/* ---- 时钟块: Reg0 (振荡器偏置) -------------------------------------------- */
ES9028_StatusTypeDef ES9028_Clock_Config(const ES9028_Clock_InitTypeDef *Cfg);

/* ---- SAI 串行音频接口: Reg1 ---------------------------------------------- */
/**
 * @brief  串行音频接口初始化 (格式/字长/输入源/自动检测)
 * @note   InputSource 仅当 AutoSelect = ES9028_AUTOSEL_DISABLE 时生效;
 *         本芯片仅支持 I2S 与左对齐, 无右对齐。
 */
ES9028_StatusTypeDef ES9028_SAI_Config(const ES9028_SAI_InitTypeDef *Cfg);

/* ---- 主模式: Reg10 --------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_MasterMode_Config(const ES9028_MasterMode_InitTypeDef *Cfg);

/* ---- DPLL 带宽: Reg12 -------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_DPLL_Config(const ES9028_DPLL_InitTypeDef *Cfg);

/* ---- 数字滤波器: Reg7[6:2] + Reg21[2],[0] -------------------------------------- */
ES9028_StatusTypeDef ES9028_Filter_Config(const ES9028_Filter_InitTypeDef *Cfg);

/* ---- 去加重: Reg6[7:4] --------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_DeEmphasis_Config(const ES9028_DeEmphasis_InitTypeDef *Cfg);

/* ---- 音量斜坡速率: Reg6[2:0] ------------------------------------------------------ */
/** @brief  设置音量斜坡速率 RampRate (0~7, 默认 ES9028_VOLUME_RATE_DEFAULT) */
ES9028_StatusTypeDef ES9028_VolumeRamp_Config(uint32_t RampRate);

/* ---- 音量 / Master Trim: Reg15-20 -------------------------------------------------- */
/**
 * @brief  设置单通道数字衰减
 * @param  Channel        ES9028_CHANNEL_1 / ES9028_CHANNEL_2
 * @param  Attenuation05  0~255, 单位0.5dB (0=0dB, 80=-40dB);
 *                        可用 ES9028_VOLUME_ATTEN(-6.0f) 由dB值换算
 */
ES9028_StatusTypeDef ES9028_Volume_Set(uint8_t Channel, uint8_t Attenuation05);
/** @brief  设置 0dB 参考电平 (Master Trim, 32位有符号, 默认 0x7FFFFFFF) */
ES9028_StatusTypeDef ES9028_MasterTrim_Set(uint32_t Trim);

/* ---- 静音: Reg7[1:0] (按通道独立) ---------------------------------------------------- */
/**
 * @brief  软静音/解除静音单通道
 * @param  Channel ES9028_CHANNEL_1 / ES9028_CHANNEL_2; 两个通道需分别调用
 */
ES9028_StatusTypeDef ES9028_MuteCmd(uint8_t Channel, ES9028_StateTypeDef State);

/* ---- Automute: Reg4/Reg5 ------------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_Automute_Config(const ES9028_Automute_InitTypeDef *Cfg);

/* ---- 软启动: Reg14 --------------------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_SoftStart_Config(const ES9028_SoftStart_InitTypeDef *Cfg);

/* ---- GPIO: Reg8 + Reg21 ------------------------------------------------------------------ */
ES9028_StatusTypeDef ES9028_GPIO_Config(const ES9028_GPIO_InitTypeDef *Cfg);

/* ---- SPDIF 输入源: Reg11[6:4] -------------------------------------------------------------- */
/** @brief  选择 SPDIF 解码输入引脚 (ES9028_SPDIF_IN_xxx) */
ES9028_StatusTypeDef ES9028_SPDIF_Select(uint32_t Source);

/* ---- 通道映射: Reg11[3:0] -------------------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_Mix_Config(const ES9028_Mix_InitTypeDef *Cfg);

/* ---- THD 补偿: Reg13/22-25/34-38 ----------------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_THD_Config(const ES9028_THD_InitTypeDef *Cfg);

/* ---- SLIMbus: Reg43 ------------------------------------------------------------------------------- */
ES9028_StatusTypeDef ES9028_SLIMbus_Config(const ES9028_SLIMbus_InitTypeDef *Cfg);

/* ---- 可编程 FIR: Reg26-30 ---------------------------------------------------------------------------- */
/**
 * @brief  下载自定义过采样滤波器系数
 * @param  Coef  系数数组 (24位有符号量化值)
 * @param  Count 128 (stage1) 或 16 (stage2)
 * @note   与手册 "Example Source Code for Loading a Filter" 一致;
 *         下载完成后调用 ES9028_FIR_Cmd(ENABLE) 才会启用
 */
ES9028_StatusTypeDef ES9028_FIR_Load(const int32_t *Coef, uint16_t Count);
/** @brief  启用/关闭自定义滤波器系数 (Reg30[0]) */
ES9028_StatusTypeDef ES9028_FIR_Cmd(ES9028_StateTypeDef State);

/* ---- 睡眠 / 唤醒 --------------------------------------------------------------------------------------- */
/**
 * @brief  软件睡眠: 双通道静音 -> 输出 ramp 到地 (soft_start=0)
 * @note   按 soft_start_time 等待 ramp 完成后返回;
 *         最低功耗待机 (<1mW) 需先 ramp 到地再硬件拉低 RESETB (会复位全部
 *         寄存器), 本函数为软件低功耗态, 寄存器配置保持不变。
 */
ES9028_StatusTypeDef ES9028_Sleep(void);

/**
 * @brief  从 ES9028_Sleep() 唤醒: 输出 ramp 到 AVCC/2 -> 解除双通道静音
 * @note   按 soft_start_time 等待 ramp 完成后返回
 */
ES9028_StatusTypeDef ES9028_Wake(void);

/* ---- 状态回读 ------------------------------------------------------------------------------------------------- */
/** @brief  读 Reg64 原始值 (修订/芯片ID/状态) */
ES9028_StatusTypeDef ES9028_GetChipId(uint8_t *ChipStatus);
/** @brief  校验芯片ID (Reg64[4:2] == 3'd1) */
ES9028_StatusTypeDef ES9028_CheckDevice(ES9028_StateTypeDef *Result);
/** @brief  DPLL 锁定状态 (Reg64[0]) */
ES9028_StatusTypeDef ES9028_GetLockStatus(ES9028_StateTypeDef *State);
/** @brief  Automute 触发状态 (Reg64[1]) */
ES9028_StatusTypeDef ES9028_GetAutomuteStatus(ES9028_StateTypeDef *State);
/** @brief  轮询等待 DPLL 锁定 (主模式下本位恒1) */
ES9028_StatusTypeDef ES9028_WaitForLock(uint32_t TimeoutMs);
/** @brief  读 DPLL NUM (Reg66-69, 先读 Reg66 触发锁存); FSR = num*MCLK/2^32 */
ES9028_StatusTypeDef ES9028_GetDpllNumber(uint32_t *Number);
/** @brief  读 GPIO 电平 Reg65 (低2位有效) */
ES9028_StatusTypeDef ES9028_GetGpioLevel(uint8_t *Level);

#ifdef __cplusplus
}
#endif

#endif /* ES9028Q2M_HAL_H */
