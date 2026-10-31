#include "volume_app.h"
#include "main.h"
#include "board.h"
#include "dac_adc_app.h"
#include "fu_store.h"
#include "es9038q2m_hal.h"
#include "tlv320adc5120.h"
#include "i2s_app.h"
#include "usbhs_device.h"
#include <string.h>

/*=====================================================================
 *  Volume Handler 实体
 *=====================================================================*/

/* ---- 目标值（主机/串口设定，1/256 dB）---- */
static int16_t  s_dac_l;
static int16_t  s_dac_r;
static int16_t  s_adc;
static uint8_t  s_dac_mute; /* FU 静音，与零点检测的 amp_muted 是两个概念 */
static uint8_t  s_adc_mute;

/* ---- 已经落到硬件上的值（用来做「目标 vs 实际」比较）---- */
static uint8_t  hw_dac_att_l = 0xFFu;
static uint8_t  hw_dac_att_r = 0xFFu;
static uint8_t  hw_dac_mute = 0xFFu;
static uint8_t  hw_adc_pga = 0xFFu;
static uint8_t  hw_adc_dig = 0xFFu;

/* ---- 存盘防抖 ---- */
#define VOL_SAVE_DEBOUNCE_TICKS 75u   /* 最后一次改动后 1.5s 落盘 */
#define VOL_RETRY_TICKS 20u           /* I2C 写失败后 400ms 再试，不要每轮都打总线 */

static uint8_t  save_pending;
static uint32_t save_due;
static uint32_t retry_tick;
static uint8_t  dac_bus_bad;
static uint8_t  adc_bus_bad;

/*--------------------------------------------------------------------
 *  钳位
 *--------------------------------------------------------------------*/
static int16_t clamp16 (int32_t v, int16_t lo, int16_t hi) {
    if (v < (int32_t)lo) {
        return lo;
    }
    if (v > (int32_t)hi) {
        return hi;
    }
    return (int16_t)v;
}

/*--------------------------------------------------------------------
 *  DAC：1/256 dB -> ES9038 0.5dB 衰减步进（0 = 0dB，255 = -127.5dB）
 *--------------------------------------------------------------------*/
static uint8_t dac_atten_of (int16_t m256) {
    int32_t steps;

    if (m256 == VOL_M256_NEG_INF) {
        return ES9038_VOLUME_REG_MAX; /* -∞：给最大衰减 */
    }
    if (m256 >= 0) {
        return 0u; /* 不支持正增益：ES9038 数字音量只有衰减 */
    }
    steps = (int32_t)((-(int32_t)m256 * DAC_VOLUME_CURVE_NUM) /
                     (128u * DAC_VOLUME_CURVE_DEN));
    if (steps > (int32_t)ES9038_VOLUME_REG_MAX) {
        steps = (int32_t)ES9038_VOLUME_REG_MAX;
    }
    return (uint8_t)steps;
}

static void adc_codes_of (int16_t m256, uint8_t *pga, uint8_t *dig) {
    int32_t steps;      /* 总增益，单位 0.5 dB */
    int32_t pga_steps;  /* 交给模拟 PGA 的部分 */
    int32_t dig_steps;  /* 剩下的交给数字音量 */

    if ((m256 == VOL_M256_NEG_INF) || (m256 < VOL_ADC_MIN_M256)) {
        /* 只有 -∞ 与"低于 -100dB"才映射成硬静音；正好 -100dB 仍然给
         * 数字音量的最小码 0x01（0x00 是 mute，两者不是一回事）。 */
        *pga = 0u;                  /* PGA 0 dB */
        *dig = ADC5120_VOLUME_MUTE; /* 0x00 = 静音 */
        return;
    }

    steps = (int32_t)m256 / 128; /* 1 步 = 0.5 dB */

    pga_steps = (steps > 0) ? steps : 0;
    if (pga_steps > (int32_t)(VOL_ADC_PGA_MAX_DB * 2)) {
        pga_steps = (int32_t)(VOL_ADC_PGA_MAX_DB * 2); /* 42 dB 顶 */
    }
    dig_steps = steps - pga_steps;
    if (dig_steps < -200) {
        dig_steps = -200; /* -100 dB 是数字音量下限 */
    }
    if (dig_steps > 54) {
        dig_steps = 54; /* +27 dB，正常走不到（正增益已被 PGA 吃掉） */
    }

    *pga = (uint8_t)((uint32_t)pga_steps << 1); /* 正增益编码：bit0 = 0 */
    *dig = (uint8_t)(201 + dig_steps);
}

/*--------------------------------------------------------------------
 *  落硬件
 *--------------------------------------------------------------------*/
static uint8_t AmpGpioGate(uint8_t mute) {
#if AMP_MUTE_GPIO_EN
    static uint8_t previous=1u;
    static uint8_t pending;
    static uint16_t started;
    if(mute!=previous) {
        previous=mute;started=(uint16_t)TIM8->CNT;pending=1u;
        if(!mute) HAMP_MUTE_OFF();
    }
    if(pending) {
        uint32_t wait=mute ? AMP_MUTE_WAIT_US : AMP_GPIO_SETTLE_US;
        if((uint16_t)((uint16_t)TIM8->CNT-started)<wait*960u/1000u) return 1u;
        pending=0u;if(mute) HAMP_MUTE_ON();
    }
#else
    (void)mute;
#endif
    return mute;
}

static void apply_dac (void) {
#if !USB_PLAY_ENABLE
    return;
#endif
    uint8_t want_att_l = dac_atten_of (s_dac_l);
    uint8_t want_att_r = dac_atten_of (s_dac_r);
    uint8_t want_mute = (uint8_t)((amp_muted != 0u) || (s_dac_mute != 0u) ||
        (s_dac_l == VOL_M256_NEG_INF && s_dac_r == VOL_M256_NEG_INF));
    uint8_t changed = 0u;
    uint8_t bad = 0u;
    want_mute=AmpGpioGate(want_mute);
#if !I2C_ENABLE
    return;
#endif
#if !DAC_SOFT_MUTE_EN
    want_mute=0u;
#endif

    if ((want_att_l == hw_dac_att_l) && (want_att_r == hw_dac_att_r) &&
        (want_mute == hw_dac_mute)) {
        dac_bus_bad = 0u;
        return;
    }
    /* 上次写失败就退避 400ms 再试：不然一颗不在位的 DAC 会让主循环每轮都
     * 打一次阻塞 I2C（每次都走到 200000 次超时自旋），把整个系统拖死。 */
    if ((dac_bus_bad != 0u) && ((int32_t)(tim6_tick - retry_tick) < 0)) {
        return;
    }

    if (want_att_l != hw_dac_att_l) {
        changed = 1u;
        if (ES9038_Volume_Set (ES9038_CHANNEL_1, want_att_l) == ES9038_OK) {
            hw_dac_att_l = want_att_l;
        } else {
            bad = 1u;
        }
    }
    USB_StreamService(); /* Refill transport between blocking I2C transactions. */
    if (want_att_r != hw_dac_att_r) {
        changed = 1u;
        if (ES9038_Volume_Set (ES9038_CHANNEL_2, want_att_r) == ES9038_OK) {
            hw_dac_att_r = want_att_r;
        } else {
            bad = 1u;
        }
    }
    USB_StreamService();
    if (want_mute != hw_dac_mute) {
        changed = 1u;
        /* Reg7[0]：ES9038 的软静音（数字域），不碰 OPA1622 的 EN，
         * 所以没有硬件 mute 那个 pop。 */
        if (ES9038_MuteCmd ((want_mute != 0u) ? ES9038_ENABLE : ES9038_DISABLE) == ES9038_OK) {
            hw_dac_mute = want_mute;
        } else {
            bad = 1u;
        }
    }

    USB_StreamService();
    dac_bus_bad = bad;
    if ((changed != 0u) && (bad != 0u)) {
        retry_tick = tim6_tick + VOL_RETRY_TICKS;
    }
}

static void apply_adc (void) {
#if !I2C_ENABLE || !USB_CAPTURE_ENABLE
    return;
#endif
    uint8_t pga, dig;
    uint8_t want_dig;

    adc_codes_of (s_adc, &pga, &dig);
    want_dig = (s_adc_mute != 0u) ? ADC5120_VOLUME_MUTE : dig;

    if ((pga == hw_adc_pga) && (want_dig == hw_adc_dig)) {
        adc_bus_bad = 0u;
        return;
    }
    if ((adc_bus_bad != 0u) && ((int32_t)(tim6_tick - retry_tick) < 0)) {
        return;
    }

    /* 两声道同值。CH1 = 0x3D/0x3E，CH2 = 0x42/0x43（Page 0）。
     * 四个都写成功才算落地；任何一个失败就整组下次重来（影子保持无效）。 */
    {
        static const uint8_t regs[4] = {ADC5120_REG_CH1_CFG1, ADC5120_REG_CH2_CFG1,
                                      ADC5120_REG_CH1_CFG2, ADC5120_REG_CH2_CFG2};
        uint8_t i;
        for (i = 0u; i < 4u; i++) {
            if ((i < 2u && pga == hw_adc_pga) ||
                (i >= 2u && want_dig == hw_adc_dig)) continue;
            uint8_t ok = DAC_ADC_AdcWrite (regs[i], i < 2u ? pga : want_dig);
            USB_StreamService();
            if (ok == 0u) {
                adc_bus_bad = 1u;
                retry_tick = tim6_tick + VOL_RETRY_TICKS;
                return;
            }
        }
    }
    adc_bus_bad = 0u;
    hw_adc_pga = pga;
    hw_adc_dig = want_dig;
}

/*--------------------------------------------------------------------
 *  生命周期
 *--------------------------------------------------------------------*/
void Volume_Init (void) {
    FuRecord rec;

    s_dac_l = 0;
    s_dac_r = 0;
    /* 录音默认 PGA 增益：没有 Flash 记录（也没收到 SET_CUR）时就用它。
     * 单位 1/256 dB，见 settings.h 的 ADC_DEFAULT_GAIN_DB 注释。 */
    s_adc = (int16_t)(ADC_DEFAULT_GAIN_DB * 256);
    s_dac_mute = 0u;
    s_adc_mute = 0u;
    I2S_SetLrSwap(PLAY_SWAP_DEFAULT,CAPTURE_SWAP_DEFAULT);

    if (FuStore_Load (&rec) != 0u) {
        s_dac_l = (rec.dac_l_m256 == VOL_M256_NEG_INF) ? VOL_M256_NEG_INF : clamp16 (rec.dac_l_m256, VOL_DAC_MIN_M256, VOL_DAC_MAX_M256);
        s_dac_r = (rec.dac_r_m256 == VOL_M256_NEG_INF) ? VOL_M256_NEG_INF : clamp16 (rec.dac_r_m256, VOL_DAC_MIN_M256, VOL_DAC_MAX_M256);
        s_adc = (rec.adc_m256 == VOL_M256_NEG_INF) ? VOL_M256_NEG_INF : clamp16 (rec.adc_m256, VOL_ADC_MIN_M256, VOL_ADC_MAX_M256);
        s_dac_mute = rec.flags & 1u;
        if(rec.magic==FU_REC_MAGIC) I2S_SetLrSwap((rec.flags>>1)&1u,(rec.flags>>2)&1u);
        s_adc_mute = (uint8_t)((rec.adc_mute != 0u) ? 1u : 0u);
    }
#if ADC_LIMIT_SAVED_GAIN_ON_BOOT
    if (s_adc > (int16_t)(ADC_DEFAULT_GAIN_DB * 256)) {
        s_adc = (int16_t)(ADC_DEFAULT_GAIN_DB * 256);
    }
#endif

    /* 立刻落一次硬件：让上电/枚举之前的 DAC 就处在正确音量上 */
    Volume_Handler ();
}

void Volume_Handler (void) {
    /* 目标 vs 实际：只有变了才碰 I2C */
    apply_dac ();
    apply_adc ();

#if FU_SAVE_ON_CHANGE
    if (save_pending != 0u) {
        if ((int32_t)(tim6_tick - save_due) >= 0) {
            uint8_t streaming = (uint8_t)((USBHS_DevAltSetting[1] != 0u) ||
                                          (USBHS_DevAltSetting[2] != 0u));

            if (streaming == 0u) {
                Volume_SaveNow (1u);
            }
        }
    }
#endif
}

/*--------------------------------------------------------------------
 *  设置
 *--------------------------------------------------------------------*/
static void mark_changed (void) {
    save_pending = 1u;
    save_due = tim6_tick + VOL_SAVE_DEBOUNCE_TICKS;
}

void Volume_SettingsChanged(void) {mark_changed();}

void Volume_SetDac (uint8_t ch, int16_t m256) {
    if (m256 > VOL_DAC_MAX_M256) {
        m256 = VOL_DAC_MAX_M256;
    }
    if (m256 != VOL_M256_NEG_INF && m256 < VOL_DAC_MIN_M256) {
        m256 = VOL_DAC_MIN_M256;
    }

    switch (ch) {
    case 1: /* 左 */
        s_dac_l = m256;
        break;
    case 2: /* 右 */
        s_dac_r = m256;
        break;
    default: /* master / 0xFF 全声道 */
        s_dac_l = m256;
        s_dac_r = m256;
        break;
    }
    mark_changed ();
}

void Volume_SetAdc (uint8_t ch, int16_t m256) {
    (void)ch; /* 录音只有 master 一条 */

    if (m256 > VOL_ADC_MAX_M256) {
        m256 = VOL_ADC_MAX_M256;
    }
    if (m256 != VOL_M256_NEG_INF && m256 < VOL_ADC_MIN_M256) {
        m256 = VOL_ADC_MIN_M256;
    }
    s_adc = m256;
    mark_changed ();
}

void Volume_SetDacMute (uint8_t on) {
    s_dac_mute = (uint8_t)((on != 0u) ? 1u : 0u);
    mark_changed ();
}

void Volume_SetAdcMute (uint8_t on) {
    s_adc_mute = (uint8_t)((on != 0u) ? 1u : 0u);
    mark_changed ();
}

int16_t Volume_GetDac (uint8_t ch) {
    if (ch == 2) {
        return s_dac_r;
    }
    return s_dac_l;
}

int16_t Volume_GetAdc (uint8_t ch) {
    (void)ch;
    return s_adc;
}

uint8_t Volume_GetDacMute (void) {
    return s_dac_mute;
}

uint8_t Volume_GetAdcMute (void) {
    return s_adc_mute;
}

uint8_t Volume_IsDirty (void) {
    return save_pending;
}

void Volume_Reapply (void) {
    hw_dac_att_l = 0xFFu;
    hw_dac_att_r = 0xFFu;
    hw_dac_mute = 0xFFu;
    hw_adc_pga = 0xFFu;
    hw_adc_dig = 0xFFu;
    dac_bus_bad = 0u;
    adc_bus_bad = 0u;
    apply_dac ();
    apply_adc ();
}

void Volume_SaveNow (uint8_t allow_erase) {
    FuRecord rec;
    if(USBHS_DevAltSetting[1] || USBHS_DevAltSetting[2]) {Volume_SettingsChanged();return;}

    rec.magic = FU_REC_MAGIC;
    rec.seq = 0u;
    rec.dac_l_m256 = s_dac_l;
    rec.dac_r_m256 = s_dac_r;
    rec.adc_m256 = s_adc;
    {uint8_t p,c;I2S_GetLrSwap(&p,&c);rec.flags=(s_dac_mute&1u)|(p<<1)|(c<<2);}
    rec.adc_mute = s_adc_mute;
    rec.crc = 0u;

    if (FuStore_Save (&rec, allow_erase) != 0u) {
        save_pending = 0u;
    }
    /* Failed or interrupted records are skipped by the append-only store. */
}

/*--------------------------------------------------------------------
 *  格式化成 "x.xx"（1/256 dB 单位，两位小数）
 *--------------------------------------------------------------------*/
void Volume_FormatDb (char *buf, int16_t m256) {
    int32_t v;
    int32_t ip;
    int32_t fp;
    char sign = '+';
    uint8_t n = 0u;

    if (buf == NULL) {
        return;
    }
    if (m256 == VOL_M256_NEG_INF) {
        buf[0] = '-';
        buf[1] = 'i';
        buf[2] = 'n';
        buf[3] = 'f';
        buf[4] = '\0';
        return;
    }

    v = (int32_t)m256;
    if (v < 0) {
        sign = '-';
        v = -v;
    }
    ip = v / 256;
    fp = ((v % 256) * 100) / 256;

    /* 手写格式化：不用 %f（ilp32 无 F 扩展），也不用 snprintf（省 1KB 代码） */
    buf[n++] = sign;
    if (ip >= 100) {
        buf[n++] = (char)('0' + (int)(ip / 100));
    }
    if (ip >= 10) {
        buf[n++] = (char)('0' + (int)((ip / 10) % 10));
    }
    buf[n++] = (char)('0' + (int)(ip % 10));
    buf[n++] = '.';
    buf[n++] = (char)('0' + (int)(fp / 10));
    buf[n++] = (char)('0' + (int)(fp % 10));
    buf[n] = '\0';
}
