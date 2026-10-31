#include "main.h"
#include "dac_adc_app.h"
#include "board.h"
#include "es9038q2m_hal.h"
#include "tlv320adc5120.h"

/* ADC5120_Init() 的实际返回码（0=OK 1=总线/参数 2=探测无应答 3=状态未达预期）。
 * 以前 ADC 失败只返回一个 1，看不出是哪一类；v/w 会打印这个数。 */
volatile uint8_t adc_init_status;

/* 初始化后回读的 ASI_CFG0（Page0 R7）。期望 0x70 = I2S + 32bit + 无填充。
 * 本板曾出现"ADC 只在 32bit 槽的高 16 位驱动数据、低 16 位悬空"，
 * 逻辑分析仪抓到低 16 位是均匀随机 —— 这个回读就是用来判那一类问题的。 */
volatile uint8_t adc_asi_cfg0;

/* 保留下来的 ADC 句柄：运行期改录音音量/PGA 要用（ADC5120_Reg_Write 只认
 * 这个句柄；库内部只有 I2C 地址是编译期常量，句柄本身不带状态，但不留一份
 * 就得在每个调用点重新构造，容易漏字段）。 */
static ADC5120_HandleTypeDef s_adc;
static uint8_t s_adc_ready;
static uint8_t s_dre_on, s_dre_pending, s_adc_power;

uint8_t ADC_PrepareRate (uint32_t rate) {
#if I2C_ENABLE && USB_CAPTURE_ENABLE
    /* TI SBAA491A Table 7-1: stereo DRE supports 48/96k, not 192/384k. */
    uint8_t want = ADC_DRE_ENABLE && (rate == 48000u || rate == 96000u), a, b;
    if (!s_adc_ready)
        return 1u; /* I2S clocks must start before ADC initialization. */
    if (s_dre_pending && !ADC_FinishRate())
        return 0u;
    if (want == s_dre_on)
        return 1u;
    if (!DAC_ADC_AdcRead (0x75, &s_adc_power) || !DAC_ADC_AdcWrite (0x75, s_adc_power & ~0x40u))
        return 0u;
    uint32_t begun = tim6_tick;
    for (uint32_t tries = 0; tries < 20000u; tries++) {
        if (!DAC_ADC_AdcRead (0x76, &a))
            goto fail;
        if (!(a & 0xC0u))
            break;
        if (tim6_tick - begun >= 2u || tries == 19999u)
            goto fail;
        USB_StreamService();
    }
    if (!DAC_ADC_AdcRead (0x3C, &a) || !DAC_ADC_AdcRead (0x41, &b))
        goto fail;
    /* Change DRE only with the ADC powered down. */
    if (!DAC_ADC_AdcWrite (0x6D, ADC_DRE_CONFIG) ||
        !DAC_ADC_AdcWrite (0x3C, (a & ~1u) | want) || !DAC_ADC_AdcWrite (0x41, (b & ~1u) | want))
        goto fail;
    s_dre_pending = 1u;
    s_dre_on = want;
    return 1u;
fail:
    (void)DAC_ADC_AdcWrite (0x75, s_adc_power);
    return 0u;
#else
    (void)rate;
    return 1u;
#endif
}

uint8_t ADC_FinishRate (void) {
    if (!s_dre_pending)
        return 1u;
    if (!DAC_ADC_AdcWrite (0x75, s_adc_power))
        return 0u;
    uint16_t started = (uint16_t)TIM8->CNT;
    uint32_t budget = 100000u;
    while ((uint16_t)((uint16_t)TIM8->CNT - started) < 960u && --budget) USB_StreamService();
    s_dre_pending = 0u;
    return 1u;
}

uint8_t DAC_ADC_AdcWrite (uint8_t reg, uint8_t value) {
    if (s_adc_ready == 0u) {
        return 0u;
    }
    return (ADC5120_Reg_Write (&s_adc, reg, value) == ADC5120_OK) ? 1u : 0u;
}

uint8_t DAC_ADC_AdcRead (uint8_t reg, uint8_t *value) {
    if ((s_adc_ready == 0u) || (value == NULL)) {
        return 0u;
    }
    return (ADC5120_Reg_Read (&s_adc, reg, value) == ADC5120_OK) ? 1u : 0u;
}

uint8_t DAC_ADC_Probe (void) {
    uint8_t ret = 0;
    uint8_t id = 0;

    if (ES9038_GetChipId (&id) != ES9038_OK) {
        ret |= 0x01u;
    } else {
    }

    if (I2C_Probe (ADC_I2C_ADDR7) != 0) {
        ret |= 0x02u;
    } else {
    }

    return ret;
}

void DAC_SetSampleRate (uint32_t sample_rate) {
    (void)ES9038_SampleRate_Config (sample_rate);
}

#if USB_CAPTURE_ENABLE
static uint8_t ADC5120_Setup (void) {
    ADC5120_HandleTypeDef *hadc = &s_adc;
    uint8_t ret = 0;

    s_adc_ready = 0u;
    s_dre_on = 0u;
    s_dre_pending = 0u;
    memset (hadc, 0, sizeof (*hadc));
    hadc->Init.ResetMode = ADC5120_RESET_SOFTWARE;
    hadc->Init.AregSupply = ADC5120_AREG_INTERNAL; /* AVDD = 3.3V */
    hadc->Init.VrefQuickCharge = ADC5120_VREF_QCHG_10MS;
    hadc->Init.AsiFormat = ADC5120_ASI_FORMAT_I2S;
    hadc->Init.AsiWordLength = ADC5120_ASI_WLEN_32BIT;
    hadc->Init.FsyncPolarity = ADC5120_POL_NORMAL;
    hadc->Init.BclkPolarity = ADC5120_POL_NORMAL;
    hadc->Init.TxEdge = ADC5120_TX_EDGE_DEFAULT;
    hadc->Init.TxFill = ADC5120_TX_FILL_ZERO;
    hadc->Init.MstSlvCfg = ADC5120_ASI_SLAVE;
    hadc->Init.FsFamily = ADC5120_FS_FAMILY_48K;
    hadc->Init.FsRate = ADC5120_FS_RATE_48K;
    hadc->Init.PllEnable = ADC5120_PLL_AUTO_ON;
    hadc->Init.SlaveRootClock = ADC5120_ROOT_CLK_BCLK;
    hadc->Init.AdcFullScale = ADC5120_VREF_2P75V;
    hadc->Init.MicbiasVal = ADC5120_MBIAS_VREF;
    hadc->Init.MicbiasPower = ADC5120_MICBIAS_POWER_DOWN; /* 线路输入，不需要偏置 */
    hadc->Init.DreEnable = 0; /* Rate service enables DRE after codec initialization. */
    hadc->Init.Pdm.Enable = ADC5120_PDM_DISABLE;          /* 本板走模拟线路输入，不开 PDM */

    /* CH1 -> 左声道 slot0，CH2 -> 右声道 slot0 */
    hadc->Init.Ch1.Enable = ADC5120_CH_ENABLE;
    hadc->Init.Ch1.Slot = ADC5120_ASI_SLOT_0;
    hadc->Init.Ch1.InputType = ADC5120_INPUT_LINE;
    hadc->Init.Ch1.InputSrc = ADC5120_INPUT_DIFFERENTIAL;
    hadc->Init.Ch1.Coupling = ADC5120_COUPLING_AC;
    hadc->Init.Ch1.InputZ = ADC_INPUT_IMPEDANCE_OHMS == 2500u ? ADC5120_INPUT_Z_2K5 :
                          ADC_INPUT_IMPEDANCE_OHMS == 10000u ? ADC5120_INPUT_Z_10K : ADC5120_INPUT_Z_20K;
    hadc->Init.Ch1.CmTolerance = ADC5120_CM_TOL_DEFAULT;
    hadc->Init.Ch1.Gain = ADC5120_GAIN_DB (0);
    hadc->Init.Ch1.Volume = ADC5120_VOLUME_DB (0);

    hadc->Init.Ch2.Enable = ADC5120_CH_ENABLE;
    hadc->Init.Ch2.Slot = ADC5120_ASI_SLOT_RIGHT_0;
    hadc->Init.Ch2.InputType = ADC5120_INPUT_LINE;
    hadc->Init.Ch2.InputSrc = ADC5120_INPUT_DIFFERENTIAL;
    hadc->Init.Ch2.Coupling = ADC5120_COUPLING_AC;
    hadc->Init.Ch2.InputZ = hadc->Init.Ch1.InputZ;
    hadc->Init.Ch2.CmTolerance = ADC5120_CM_TOL_DEFAULT;
    hadc->Init.Ch2.Gain = ADC5120_GAIN_DB (0);
    hadc->Init.Ch2.Volume = ADC5120_VOLUME_DB (0);

    {
        ADC5120_StatusTypeDef st = ADC5120_Init (hadc);

        adc_init_status = (uint8_t)st;
        if (st != ADC5120_OK) {
            ret = ADC5120_ERROR;
        } else {
            s_adc_ready = 1u;
        }
    }
    return ret;
}
#endif

uint8_t DAC_ADC_Setup (uint32_t sample_rate) {
    uint8_t ret = 0u;
    const uint8_t required = (USB_PLAY_ENABLE ? 1u : 0u) | (USB_CAPTURE_ENABLE ? 2u : 0u);
    (void)sample_rate;
#if USB_PLAY_ENABLE
    DAC_Reset();
#endif
    for (uint8_t attempt = 0u; attempt < 5u; attempt++) {
#if USB_PLAY_ENABLE
        if (!(ret & 1u) && ES9038_Init() == ES9038_OK)
            ret |= 1u;
#endif
#if USB_CAPTURE_ENABLE
        if (!(ret & 2u) && ADC5120_Setup() == ADC5120_OK) {
            /* Apply the initial rate too: first opening 48k does not change I2S clocks. */
            if (ADC_PrepareRate (sample_rate) && ADC_FinishRate()) {
                ret |= 2u;
                (void)DAC_ADC_AdcRead (ADC5120_REG_ASI_CFG0, (uint8_t *)&adc_asi_cfg0);
            } else {
                adc_init_status = ADC5120_ERROR;
            }
        }
#endif
        if ((ret & required) == required)
            break;
        Delay_Ms (5);
    }
    return ret;
}
