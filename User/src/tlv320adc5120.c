


#include "tlv320adc5120.h"

#include <stddef.h>

/* ============================================================================
 * 内部总线访问
 * ==========================================================================*/

/** 选回 Page 0（PAGE_CFG 存在于每一页，写 0 即回到配置页） */
static ADC5120_StatusTypeDef ADC5120_SelectPage0 (void) {
    uint8_t page = ADC5120_PAGE_0;

    if (adc5120_bus_write (ADC5120_I2C_ADDR, ADC5120_REG_PAGE_CFG, &page, 1U) != 0) {
        return ADC5120_ERROR;
    }
    return ADC5120_OK;
}

/** 假定已选页的单字节写 */
static ADC5120_StatusTypeDef ADC5120_WriteRaw (uint8_t RegAddr, uint8_t Value) {
    if (adc5120_bus_write (ADC5120_I2C_ADDR, RegAddr, &Value, 1U) != 0) {
        return ADC5120_ERROR;
    }
    return ADC5120_OK;
}

/** 假定已选页的单字节读 */
static ADC5120_StatusTypeDef ADC5120_ReadRaw (uint8_t RegAddr, uint8_t *Value) {
    if (adc5120_bus_read (ADC5120_I2C_ADDR, RegAddr, Value, 1U) != 0) {
        return ADC5120_ERROR;
    }
    return ADC5120_OK;
}

/* ============================================================================
 * 公开接口
 * ==========================================================================*/

ADC5120_StatusTypeDef ADC5120_Reg_Write (ADC5120_HandleTypeDef *hadc,
                                         uint8_t RegAddr, uint8_t Value) {
    ADC5120_StatusTypeDef ret;

    (void)hadc;
    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }
    return ADC5120_WriteRaw (RegAddr, Value);
}

ADC5120_StatusTypeDef ADC5120_Reg_Read (ADC5120_HandleTypeDef *hadc,
                                        uint8_t RegAddr, uint8_t *Value) {
    ADC5120_StatusTypeDef ret;

    (void)hadc;
    if (Value == NULL) {
        return ADC5120_ERROR;
    }
    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }
    return ADC5120_ReadRaw (RegAddr, Value);
}

ADC5120_StatusTypeDef ADC5120_Probe (ADC5120_HandleTypeDef *hadc) {
    uint8_t value;

    (void)hadc;
    /* 该芯片没有 ID 寄存器：读 PAGE_CFG（所有页都存在）能否得到应答，
     * 即可判断器件是否在线。 */
    if (adc5120_bus_read (ADC5120_I2C_ADDR, ADC5120_REG_PAGE_CFG, &value, 1U) != 0) {
        return ADC5120_ERROR_NOT_PRESENT;
    }
    return ADC5120_OK;
}

ADC5120_StatusTypeDef ADC5120_Reset (ADC5120_HandleTypeDef *hadc, uint8_t ResetMode) {
    ADC5120_StatusTypeDef ret;

    if (hadc == NULL) {
        return ADC5120_ERROR;
    }

    switch (ResetMode) {
    case ADC5120_RESET_NONE:
        /* 不复位：仅保证后续寄存器访问位于 Page 0 */
        return ADC5120_SelectPage0();

    case ADC5120_RESET_HARDWARE:
#ifdef adc5120_hw_reset
        adc5120_hw_reset();
        adc5120_delay_ms (ADC5120_DELAY_RESET_MS);
        return ADC5120_OK;
#else
        /* 未在 tlv320adc5120_port.h 中定义硬件复位宏 */
        return ADC5120_ERROR;
#endif

    case ADC5120_RESET_SOFTWARE:
        ret = ADC5120_SelectPage0();
        if (ret != ADC5120_OK) {
            return ret;
        }
        /* SW_RESET (0x01) D0 自清零；复位后所有寄存器回到 POR 默认值（含睡眠态） */
        ret = ADC5120_WriteRaw (ADC5120_REG_SW_RESET, 0x01U);
        if (ret != ADC5120_OK) {
            return ret;
        }
        adc5120_delay_ms (ADC5120_DELAY_RESET_MS);
        return ADC5120_OK;

    default:
        return ADC5120_ERROR;
    }
}

/** 写 SLEEP_CFG 唤醒器件（AREG 选择 + VREF 快充时长 + SLEEP_ENZ=1） */
static ADC5120_StatusTypeDef ADC5120_WakeUp (ADC5120_HandleTypeDef *hadc) {
    const ADC5120_InitTypeDef *cfg = &hadc->Init;
    ADC5120_StatusTypeDef ret;
    uint8_t value;

    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }

    value = (uint8_t)(((cfg->AregSupply & 0x01U) << 7) |
                      ((cfg->VrefQuickCharge & 0x03U) << 3) |
                      0x01U); /* bit0 SLEEP_ENZ = 1：退出睡眠 */
    ret = ADC5120_WriteRaw (ADC5120_REG_SLEEP_CFG, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    adc5120_delay_ms (ADC5120_DELAY_WAKE_MS);
    return ADC5120_OK;
}

ADC5120_StatusTypeDef ADC5120_SAI_Config (ADC5120_HandleTypeDef *hadc) {
    const ADC5120_InitTypeDef *cfg;
    ADC5120_StatusTypeDef ret;
    uint8_t value;

    if (hadc == NULL) {
        return ADC5120_ERROR;
    }
    cfg = &hadc->Init;

    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* ASI_CFG0 (0x07)：格式/字长/FSYNC 极性/BCLK 极性/发送边沿/空闲填充 */
    value = (uint8_t)(((cfg->AsiFormat & 0x03U) << 6) |
                      ((cfg->AsiWordLength & 0x03U) << 4) |
                      ((cfg->FsyncPolarity & 0x01U) << 3) |
                      ((cfg->BclkPolarity & 0x01U) << 2) |
                      ((cfg->TxEdge & 0x01U) << 1) |
                      (cfg->TxFill & 0x01U));
    ret = ADC5120_WriteRaw (ADC5120_REG_ASI_CFG0, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 槽位分配（保留位 bit7:6 写 0） */
    ret = ADC5120_WriteRaw (ADC5120_REG_ASI_CH1, (uint8_t)(cfg->Ch1.Slot & 0x3FU));
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_ASI_CH2, (uint8_t)(cfg->Ch2.Slot & 0x3FU));
    if (ret != ADC5120_OK) {
        return ret;
    }
    /* CH3/CH4 只可能是 PDM 通道：未启用 PDM 时不写，保持芯片复位值（slot 2 / 3） */
    if (cfg->Pdm.Enable == ADC5120_PDM_ENABLE) {
        ret = ADC5120_WriteRaw (ADC5120_REG_ASI_CH3, (uint8_t)(cfg->Ch3.Slot & 0x3FU));
        if (ret != ADC5120_OK) {
            return ret;
        }
        ret = ADC5120_WriteRaw (ADC5120_REG_ASI_CH4, (uint8_t)(cfg->Ch4.Slot & 0x3FU));
        if (ret != ADC5120_OK) {
            return ret;
        }
    }

    /* MST_CFG0 (0x13)：主从 | 自动时钟保持使能(bit6=0) | PLL 开关 |
     * BCLK/FSYNC 门控保持关闭(bit4=0) | fs 家族 | MCLK 频率档位 */
    value = (uint8_t)(((cfg->MstSlvCfg & 0x01U) << 7) |
                      ((cfg->PllEnable & 0x01U) << 5) |
                      ((cfg->FsFamily & 0x01U) << 3) |
                      (cfg->MclkFreq & 0x07U));
    ret = ADC5120_WriteRaw (ADC5120_REG_MST_CFG0, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* MST_CFG1 (0x14)：采样率档位 | BCLK/FSYNC 比率（仅主机模式生效） */
    value = (uint8_t)(((cfg->FsRate & 0x0FU) << 4) | (cfg->BclkRatio & 0x0FU));
    ret = ADC5120_WriteRaw (ADC5120_REG_MST_CFG1, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* CLK_SRC (0x16)：从模式根时钟 | MCLK 频率指定方式 | MCLK/FSYNC 比率；
     * bit2/bit1/bit0 保持复位值 0 */
    value = (uint8_t)(((cfg->SlaveRootClock & 0x01U) << 7) |
                      ((cfg->MclkFreqMode & 0x01U) << 6) |
                      ((cfg->MclkFsyncRatio & 0x07U) << 3));
    ret = ADC5120_WriteRaw (ADC5120_REG_CLK_SRC, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    return ADC5120_OK;
}

/** PDM 引脚/时钟组合的合法性检查（手册表 8-51 与 8.3.11 的约束） */
static ADC5120_StatusTypeDef ADC5120_PDM_CheckPinCfg (const ADC5120_InitTypeDef *cfg) {
    uint8_t ch12_pdm;
    uint8_t ch34_en;

    if (cfg->Pdm.ClockDiv > ADC5120_PDMCLK_5P6448M) {
        return ADC5120_ERROR;
    }
    if ((cfg->Pdm.ClockPin != ADC5120_PDMCLK_PIN_GPO1) &&
        (cfg->Pdm.ClockPin != ADC5120_PDMCLK_PIN_GPIO1)) {
        return ADC5120_ERROR;
    }
    if ((cfg->Pdm.DataPin != ADC5120_PDMDIN_PIN_GPI1_GPI2) &&
        (cfg->Pdm.DataPin != ADC5120_PDMDIN_PIN_GPIO1)) {
        return ADC5120_ERROR;
    }
    /* GPIO1 只有一根：不能既输出 PDMCLK 又接收 PDMDIN */
    if ((cfg->Pdm.ClockPin == ADC5120_PDMCLK_PIN_GPIO1) &&
        (cfg->Pdm.DataPin == ADC5120_PDMDIN_PIN_GPIO1)) {
        return ADC5120_ERROR;
    }

    ch12_pdm = (uint8_t)(((cfg->Ch1.InputSrc == ADC5120_INPUT_PDM) ||
                          (cfg->Ch2.InputSrc == ADC5120_INPUT_PDM))
                             ? 1U
                             : 0U);
    ch34_en = (uint8_t)(((cfg->Ch3.Enable == ADC5120_CH_ENABLE) ||
                         (cfg->Ch4.Enable == ADC5120_CH_ENABLE))
                            ? 1U
                            : 0U);

    /* IN2M/IN2P 要先从模拟前端释放才能当 GPO1/GPI1 用，办法是把 CH2 切到 PDM 输入；
     * 只有真的用到这两根脚时才需要（用 GPIO1 出时钟 + MICBIAS 收数据时不必牺牲 CH2） */
    if (((cfg->Pdm.ClockPin == ADC5120_PDMCLK_PIN_GPO1) ||
         ((cfg->Pdm.DataPin == ADC5120_PDMDIN_PIN_GPI1_GPI2) && (ch12_pdm != 0U))) &&
        (cfg->Ch2.InputSrc != ADC5120_INPUT_PDM)) {
        return ADC5120_ERROR;
    }
    /* CH3/CH4 走 PDMDIN2：GPI 通路下 PDMDIN2 复用在 MICBIAS 脚上，必须把
     * MBIAS_VAL 设成 7d（MICBIAS 作为 GPI2），否则数据脚不存在 */
    if ((ch34_en != 0U) &&
        (cfg->Pdm.DataPin == ADC5120_PDMDIN_PIN_GPI1_GPI2) &&
        (cfg->MicbiasVal != ADC5120_MBIAS_GPI2)) {
        return ADC5120_ERROR;
    }
    /* 手册限制：5.6448/6.144 MHz 档只能用于 CH1/CH2 */
    if ((ch34_en != 0U) && (cfg->Pdm.ClockDiv == ADC5120_PDMCLK_5P6448M)) {
        return ADC5120_ERROR;
    }
    return ADC5120_OK;
}

ADC5120_StatusTypeDef ADC5120_PDM_Config (ADC5120_HandleTypeDef *hadc) {
    const ADC5120_InitTypeDef *cfg;
    ADC5120_StatusTypeDef ret;
    uint8_t value;

    if (hadc == NULL) {
        return ADC5120_ERROR;
    }
    cfg = &hadc->Init;

    /* PDM 未启用：一个寄存器都不碰，GPIO1 等复用脚保持复位默认（IRQ 输出） */
    if (cfg->Pdm.Enable != ADC5120_PDM_ENABLE) {
        return ADC5120_OK;
    }

    ret = ADC5120_PDM_CheckPinCfg (cfg);
    if (ret != ADC5120_OK) {
        return ret;
    }

    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* PDMCLK_CFG (0x1F)：保留位 bit6:2 固定为 10000b（复位值），只写 bit1:0 分频 */
    value = (uint8_t)(0x40U | (cfg->Pdm.ClockDiv & 0x03U));
    ret = ADC5120_WriteRaw (ADC5120_REG_PDMCLK_CFG, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* PDMIN_CFG (0x20)：PDMDIN1 的采样沿，决定 CH1/CH2 各用正沿还是负沿 */
    value = (uint8_t)((cfg->Pdm.Din1Edge & 0x01U) << 7);
    ret = ADC5120_WriteRaw (ADC5120_REG_PDMIN_CFG, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* GPIO_CFG0 (0x21)：仅当 GPIO1 参与 PDM 时改写，否则保留复位值 */
    if ((cfg->Pdm.ClockPin == ADC5120_PDMCLK_PIN_GPIO1) ||
        (cfg->Pdm.DataPin == ADC5120_PDMDIN_PIN_GPIO1)) {
        if (cfg->Pdm.ClockPin == ADC5120_PDMCLK_PIN_GPIO1) {
            /* GPIO1 输出 PDMCLK：必须是推挽，否则时钟边沿带不动容性负载 */
            value = (uint8_t)((ADC5120_GPO_FUNC_PDMCLK << 4) | ADC5120_PIN_DRV_PUSH_PULL);
        } else {
            /* GPIO1 收 PDMDIN1：作输入，驱动档位无意义，保持复位默认弱上拉 */
            value = (uint8_t)((ADC5120_GPO_FUNC_PDMDIN1 << 4) | ADC5120_PIN_DRV_OD_WEAK_HIGH);
        }
        ret = ADC5120_WriteRaw (ADC5120_REG_GPIO_CFG0, value);
        if (ret != ADC5120_OK) {
            return ret;
        }
    }

    /* GPO_CFG0 (0x22)：IN2M/GPO1 输出 PDMCLK */
    if (cfg->Pdm.ClockPin == ADC5120_PDMCLK_PIN_GPO1) {
        value = (uint8_t)((ADC5120_GPO_FUNC_PDMCLK << 4) | ADC5120_PIN_DRV_PUSH_PULL);
        ret = ADC5120_WriteRaw (ADC5120_REG_GPO_CFG0, value);
        if (ret != ADC5120_OK) {
            return ret;
        }
    }

    /* GPI_CFG0 (0x2B)：IN2P/GPI1 收 PDMDIN1（仅 CH1/CH2 走 PDM 时需要）；
     * MICBIAS/GPI2 收 PDMDIN2（仅 CH3/CH4 启用时需要） */
    if (cfg->Pdm.DataPin == ADC5120_PDMDIN_PIN_GPI1_GPI2) {
        value = 0x00U;
        if ((cfg->Ch1.InputSrc == ADC5120_INPUT_PDM) ||
            (cfg->Ch2.InputSrc == ADC5120_INPUT_PDM)) {
            value = (uint8_t)(value | (ADC5120_GPI_FUNC_PDMDIN1 << 4));
        }
        if ((cfg->Ch3.Enable == ADC5120_CH_ENABLE) || (cfg->Ch4.Enable == ADC5120_CH_ENABLE)) {
            value = (uint8_t)(value | ADC5120_GPI_FUNC_PDMDIN2);
        }
        if (value != 0x00U) {
            ret = ADC5120_WriteRaw (ADC5120_REG_GPI_CFG0, value);
            if (ret != ADC5120_OK) {
                return ret;
            }
        }
    }

    /* DSP_CFG0 (0x6B) 的 DECI_FILT[1:0]：读-改-写，避免破坏 HPF_SEL / CH_SUM */
    if (cfg->Pdm.DecimationFilter != ADC5120_DECI_FILT_KEEP) {
        ret = ADC5120_ReadRaw (ADC5120_REG_DSP_CFG0, &value);
        if (ret != ADC5120_OK) {
            return ret;
        }
        value = (uint8_t)((value & 0xCFU) | ((cfg->Pdm.DecimationFilter & 0x03U) << 4));
        ret = ADC5120_WriteRaw (ADC5120_REG_DSP_CFG0, value);
        if (ret != ADC5120_OK) {
            return ret;
        }
    }

    return ADC5120_OK;
}

ADC5120_StatusTypeDef ADC5120_Input_Config (ADC5120_HandleTypeDef *hadc) {
    const ADC5120_InitTypeDef *cfg;
    ADC5120_StatusTypeDef ret;
    uint8_t value;

    if (hadc == NULL) {
        return ADC5120_ERROR;
    }
    cfg = &hadc->Init;

    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* CM_TOL_CFG (0x3A)：两通道输入共模容差 */
    value = (uint8_t)(((cfg->Ch1.CmTolerance & 0x03U) << 6) |
                      ((cfg->Ch2.CmTolerance & 0x03U) << 4));
    ret = ADC5120_WriteRaw (ADC5120_REG_CM_TOL_CFG, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* BIAS_CFG (0x3B)：MICBIAS 电压档 | ADC 满量程(VREF) */
    value = (uint8_t)(((cfg->MicbiasVal & 0x07U) << 4) | (cfg->AdcFullScale & 0x03U));
    ret = ADC5120_WriteRaw (ADC5120_REG_BIAS_CFG, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* P0_R108 DSP_CFG1：关闭 biquad[6:5]、AGC/DRC/avoid-clip，保留 soft-step。
     * 复位值 0x40 会启用每通道两个 biquad；192k/384k 不支持该处理块
     * （TI SBAA381B §3.4/3.5）。直通录音无需 biquad，所有速率统一关闭，
     * 避免高采样率 DSP 处理超限。CHx_DREEN 单独选择是否启用 DRE。 */
    ret = ADC5120_ReadRaw (0x6CU, &value);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (0x6CU, (uint8_t)(value & ~0x6BU));
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 通道 1：DREEN=1 且 DSP_CFG1 选择 DRE；PDM 输入不启用模拟 DRE。 */
    value = (uint8_t)(((cfg->Ch1.InputType & 0x01U) << 7) |
                      ((cfg->Ch1.InputSrc & 0x03U) << 5) |
                      ((cfg->Ch1.Coupling & 0x01U) << 4) |
                      ((cfg->Ch1.InputZ & 0x03U) << 2) |
                      ((cfg->DreEnable != 0U && cfg->Ch1.InputSrc != ADC5120_INPUT_PDM) ? 1U : 0U));
    ret = ADC5120_WriteRaw (ADC5120_REG_CH1_CFG0, value);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH1_CFG1, cfg->Ch1.Gain); /* 模拟增益 */
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH1_CFG2, cfg->Ch1.Volume); /* 数字音量 */
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH1_CFG3, 0x80U); /* 增益校准 0 dB（复位值） */
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH1_CFG4, 0x00U); /* 相位校准关闭（复位值） */
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 通道 2 */
    value = (uint8_t)(((cfg->Ch2.InputType & 0x01U) << 7) |
                      ((cfg->Ch2.InputSrc & 0x03U) << 5) |
                      ((cfg->Ch2.Coupling & 0x01U) << 4) |
                      ((cfg->Ch2.InputZ & 0x03U) << 2) |
                      ((cfg->DreEnable != 0U && cfg->Ch2.InputSrc != ADC5120_INPUT_PDM) ? 1U : 0U));
    ret = ADC5120_WriteRaw (ADC5120_REG_CH2_CFG0, value);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH2_CFG1, cfg->Ch2.Gain);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH2_CFG2, cfg->Ch2.Volume);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH2_CFG3, 0x80U);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_CH2_CFG4, 0x00U);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 通道 3/4：PDM 专用，没有 CFG0/CFG1（无输入选择与模拟增益），
     * 只写数字音量、增益校准、相位校准 */
    if (cfg->Pdm.Enable == ADC5120_PDM_ENABLE) {
        if (cfg->Ch3.Enable == ADC5120_CH_ENABLE) {
            ret = ADC5120_WriteRaw (ADC5120_REG_CH3_CFG2, cfg->Ch3.Volume);
            if (ret != ADC5120_OK) {
                return ret;
            }
            ret = ADC5120_WriteRaw (ADC5120_REG_CH3_CFG3, 0x80U);
            if (ret != ADC5120_OK) {
                return ret;
            }
            ret = ADC5120_WriteRaw (ADC5120_REG_CH3_CFG4, 0x00U);
            if (ret != ADC5120_OK) {
                return ret;
            }
        }
        if (cfg->Ch4.Enable == ADC5120_CH_ENABLE) {
            ret = ADC5120_WriteRaw (ADC5120_REG_CH4_CFG2, cfg->Ch4.Volume);
            if (ret != ADC5120_OK) {
                return ret;
            }
            ret = ADC5120_WriteRaw (ADC5120_REG_CH4_CFG3, 0x80U);
            if (ret != ADC5120_OK) {
                return ret;
            }
            ret = ADC5120_WriteRaw (ADC5120_REG_CH4_CFG4, 0x00U);
            if (ret != ADC5120_OK) {
                return ret;
            }
        }
    }

    return ADC5120_OK;
}

ADC5120_StatusTypeDef ADC5120_Power_Config (ADC5120_HandleTypeDef *hadc) {
    const ADC5120_InitTypeDef *cfg;
    ADC5120_StatusTypeDef ret;
    uint8_t ch1_en;
    uint8_t ch2_en;
    uint8_t ch3_en;
    uint8_t ch4_en;
    uint8_t value;

    if (hadc == NULL) {
        return ADC5120_ERROR;
    }
    cfg = &hadc->Init;

    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }

    ch1_en = (cfg->Ch1.Enable == ADC5120_CH_ENABLE) ? 0x80U : 0x00U;
    ch2_en = (cfg->Ch2.Enable == ADC5120_CH_ENABLE) ? 0x40U : 0x00U;
    /* CH3/CH4 只可能是 PDM 通道：未启用 PDM 时无论通道结构体怎么写都不上电 */
    ch3_en = ((cfg->Pdm.Enable == ADC5120_PDM_ENABLE) &&
              (cfg->Ch3.Enable == ADC5120_CH_ENABLE))
                 ? 0x20U
                 : 0x00U;
    ch4_en = ((cfg->Pdm.Enable == ADC5120_PDM_ENABLE) &&
              (cfg->Ch4.Enable == ADC5120_CH_ENABLE))
                 ? 0x10U
                 : 0x00U;
    value = (uint8_t)(ch1_en | ch2_en | ch3_en | ch4_en);

    /* 顺序按手册 8.4.2：输入通道使能 -> ASI 输出槽使能 -> 最后上电 */
    ret = ADC5120_WriteRaw (ADC5120_REG_IN_CH_EN, value);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_WriteRaw (ADC5120_REG_ASI_OUT_CH_EN, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* PWR_CFG (0x75)：MICBIAS 按配置 | ADC 核心上电 | PLL 按配置 |
     * 动态通道上下电(bit4)=0、VAD(bit0)=0 保持复位值 */
    value = (uint8_t)(((cfg->MicbiasPower & 0x01U) << 7) |
                      0x40U |
                      ((cfg->PllEnable == ADC5120_PLL_AUTO_ON) ? 0x20U : 0x00U));
    ret = ADC5120_WriteRaw (ADC5120_REG_PWR_CFG, value);
    if (ret != ADC5120_OK) {
        return ret;
    }

    return ADC5120_OK;
}


volatile uint8_t adc5120_dbg_sts0;
volatile uint8_t adc5120_dbg_sts1;

ADC5120_StatusTypeDef ADC5120_State_Get (ADC5120_HandleTypeDef *hadc,
                                         ADC5120_StateTypeDef *state) {
    ADC5120_StatusTypeDef ret;
    uint8_t dev_sts0;
    uint8_t dev_sts1;
    uint8_t asi_sts;
    uint8_t mode;
    uint8_t fs_rate_sts;
    uint8_t bclk_ratio_sts;
    uint32_t flags = 0U;

    if ((hadc == NULL) || (state == NULL)) {
        return ADC5120_ERROR;
    }

    ret = ADC5120_SelectPage0();
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_ReadRaw (ADC5120_REG_DEV_STS0, &dev_sts0);
    if (ret != ADC5120_OK) {
        return ret;
    }
    ret = ADC5120_ReadRaw (ADC5120_REG_DEV_STS1, &dev_sts1);
    if (ret != ADC5120_OK) {
        return ret;
    }
    adc5120_dbg_sts0 = dev_sts0;
    adc5120_dbg_sts1 = dev_sts1;
    ret = ADC5120_ReadRaw (ADC5120_REG_ASI_STS, &asi_sts);
    if (ret != ADC5120_OK) {
        return ret;
    }

    if ((dev_sts0 & 0x80U) != 0U) {
        flags |= ADC5120_FLAG_CH1_POWERED;
    }
    if ((dev_sts0 & 0x40U) != 0U) {
        flags |= ADC5120_FLAG_CH2_POWERED;
    }

    mode = (uint8_t)((dev_sts1 >> 5) & 0x07U);
    if (mode == 0x04U) {
        flags |= ADC5120_FLAG_MODE_SLEEP;
    } else if ((mode == 0x06U) || (mode == 0x07U)) {
        flags |= ADC5120_FLAG_MODE_ACTIVE;
    }

    fs_rate_sts = (uint8_t)((asi_sts >> 4) & 0x0FU);
    bclk_ratio_sts = (uint8_t)(asi_sts & 0x0FU);
    /* 0~8 为有效采样率档（8 = 768 kHz 档），9~15 为保留或无效 */
    if (fs_rate_sts <= (uint8_t)ADC5120_FS_RATE_768K) {
        flags |= ADC5120_FLAG_ASI_CLK_VALID;
    }

    state->Flags = flags;
    state->FsRateSts = fs_rate_sts;
    state->BclkRatioSts = bclk_ratio_sts;
    return ADC5120_OK;
}

ADC5120_StatusTypeDef ADC5120_Init (ADC5120_HandleTypeDef *hadc) {
    ADC5120_StatusTypeDef ret;
    ADC5120_StateTypeDef state;
    uint32_t expected = 0U;

    if (hadc == NULL) {
        return ADC5120_ERROR;
    }

    /* 1. 等待电源稳定（上电即睡眠，同时满足唤醒前 >=10 ms 的要求） */
    adc5120_delay_ms (ADC5120_DELAY_POWER_UP_MS);

    /* 2. 在线探测：读寄存器看 I2C 是否有应答 */
    ret = ADC5120_Probe (hadc);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 3. 复位（软/硬件由 Init.ResetMode 决定） */
    ret = ADC5120_Reset (hadc, hadc->Init.ResetMode);
    if (ret != ADC5120_OK) {
        return ret;
    }
    Delay_ms (1);  // 等待1ms ADC复位成功

    /* 4. 唤醒并等待内部唤醒序列完成 */
    ret = ADC5120_WakeUp (hadc);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 5. ASI 串行接口与时钟/主从配置 */
    ret = ADC5120_SAI_Config (hadc);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 6. PDM 时钟与引脚复用（未启用 PDM 时该函数直接返回，不写任何寄存器）。
     *    必须排在 Input_Config 之前：手册与 TI EVM 序列都是先定引脚复用再写
     *    BIAS_CFG/CHx_CFG0，否则 MICBIAS→GPI2 的切换会踩掉刚配好的通道输入。 */
    ret = ADC5120_PDM_Config (hadc);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 7. 模拟前端与增益/音量（唤醒后 >=10 ms 已由第 4 步保证） */
    ret = ADC5120_Input_Config (hadc);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 8. 通道使能与上电（PWR_CFG 最后写入） */
    ret = ADC5120_Power_Config (hadc);
    if (ret != ADC5120_OK) {
        return ret;
    }

    /* 9. 读状态标志校验：使能的通道须已上电且器件处于活动模式。
     *    ASI 时钟由主机提供，可能尚未启动，仅作为标志返回不作为失败条件。
     *    注意 DEV_STS0 只有 CH1/CH2 状态位，PDM 的 CH3/CH4 无法在此校验。 */
    if (hadc->Init.Ch1.Enable == ADC5120_CH_ENABLE) {
        expected |= ADC5120_FLAG_CH1_POWERED;
    }
    if (hadc->Init.Ch2.Enable == ADC5120_CH_ENABLE) {
        expected |= ADC5120_FLAG_CH2_POWERED;
    }


    {
        uint8_t t;

        for (t = 0U; t < 20U; t++) {
            ret = ADC5120_State_Get (hadc, &state);
            if (ret != ADC5120_OK) {
                return ret;
            }
            if (((state.Flags & expected) == expected) &&
                ((state.Flags & ADC5120_FLAG_MODE_ACTIVE) != 0U)) {
                break;
            }
            adc5120_delay_ms (3U);
        }
        if (t >= 20U) {
            return ADC5120_ERROR_NOT_READY;
        }
    }

    return ADC5120_OK;
}
