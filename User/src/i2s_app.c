#include "i2s_app.h"
#include "dac_adc_app.h"
#include "ch32v30x_tim.h"
#include "settings.h"
#include "main.h"
#include "es9038q2m_hal.h"
#include <string.h>

/*********************** capture mode user configuration ********************/
/* Define CAPTURE_MODE_SLAVE to turn I2S2 into a slave receiver: BCLK/WS
 * come from the external ADC and the current rate is exported on M0/M1
 * (48k=00, 96k=01, 192k=10, 384k=11, M0 = bit0, M1 = bit1). */
// #define CAPTURE_MODE_SLAVE

#ifdef CAPTURE_MODE_SLAVE
#ifndef CAPTURE_M0_PORT
#define CAPTURE_M0_PORT GPIOC
#define CAPTURE_M0_PIN GPIO_Pin_6
#define CAPTURE_M1_PORT GPIOC
#define CAPTURE_M1_PIN GPIO_Pin_7
#endif
#endif
/****************************************************************************/


#define CAPTURE_BUF_BYTES 4096u /* power of two, DMA1_CH4 ring */
/* Optional stereo channel swap. This does not change halfword order. */
#define CAPTURE_RAW_SWAP 0

/* Playback ring: 8 KiB = 2.7ms at 384k/32bit, 21.3ms at 48k.  Must be a
 * power of two; the DMA counts half-words so CNTR*2 gives the byte
 * position. */
#define PLAY_RING_BYTES 8192u
#define PLAY_RING_HALF (PLAY_RING_BYTES / 2u)
#define PLAY_RING_MASK (PLAY_RING_BYTES - 1u)
#ifndef PLAY_RING_GUARD
#define PLAY_RING_GUARD 512u /* never write this close to the DMA read ptr */
#endif

/* PLC: remember the last N 8-byte frames of real audio and replay them,
 * attenuating each repeat, whenever a USB packet goes missing. */
#ifndef PLC_PATTERN_FRAMES
#define PLC_PATTERN_FRAMES 32u
#endif
#ifndef PLC_RAMP_FRAMES
#define PLC_RAMP_FRAMES 8u /* fade-in length when real audio returns */
#endif
#ifndef PLC_ENABLE
#define PLC_ENABLE 1
#endif
#ifndef PLC_DECAY_Q15
#define PLC_DECAY_Q15 20316u
#endif
#ifndef PLC_SILENCE_Q15
#define PLC_SILENCE_Q15 328u
#endif

#ifndef ZERO_DETECT_ENABLE
#define ZERO_DETECT_ENABLE 1
#endif
#ifndef ZERO_DETECT_THRESH
#define ZERO_DETECT_THRESH 644245u
#endif
#ifndef ZERO_DETECT_HOLD_PKTS
#define ZERO_DETECT_HOLD_PKTS 10u
#endif
/* 功放静音的门槛改成「连续数字静音多久」而不是「多少个包」 */
#ifndef ZERO_MUTE_HOLD_TICKS
#define ZERO_MUTE_HOLD_TICKS 25u
#endif
/* 换频后延迟解静音：DAC 重新锁定 DPLL 需要时间，25 x 20ms = 500ms。 */
#ifndef ZERO_UNMUTE_DELAY_TICKS
#define ZERO_UNMUTE_DELAY_TICKS 5u
#endif
/* 换频/换位深后 SDATA 保持低电平的窗口，单位是 TIM8 的 tick（960kHz）。
 * 9600 x (1/960000) = 10 ms。 */
#ifndef SDATA_HOLD_TICKS
#define SDATA_HOLD_TICKS 9600u
#endif
#ifndef ZERO_MUTE_ACTIVE_LEVEL
#define ZERO_MUTE_ACTIVE_LEVEL 1
#endif

#define AMP_MUTE_ASSERT() \
    GPIO_WritePin (HAMP_MUTE_PIN, (GPIO_PinState)ZERO_MUTE_ACTIVE_LEVEL)
#define AMP_MUTE_RELEASE()        \
    GPIO_WritePin (HAMP_MUTE_PIN, \
                   (GPIO_PinState)(ZERO_MUTE_ACTIVE_LEVEL ? 0 : 1))

/* ---- 诊断计数器：串口 'd' 命令读取并清零，用来定位速率相关的数据通路问题 ---- */
volatile uint32_t i2s_dbg_pkts;    /* 接受的 USB 包数 */
volatile uint32_t i2s_dbg_frames;  /* 写入环的帧数 */
volatile uint32_t i2s_dbg_lastlen; /* 最后一个 USB 包的字节数 */
volatile uint32_t i2s_dbg_lvl_max; /* 窗口内环占用最大值（字节） */
volatile uint32_t i2s_dbg_lvl_min; /* 窗口内环占用最小值（字节） */
volatile uint32_t i2s_dbg_zlp;     /* 零长包/丢包次数 */
volatile uint32_t i2s_dbg_dma_tc;  /* DMA2_Ch2 每转完一整环（1024 帧）加一 */
volatile uint32_t i2s_dbg_dma_ht;
/* 期望包长 vs 实际包长（串口 'd' 的 [pkt] 行） */
volatile uint32_t i2s_dbg_exp_len;
volatile uint32_t i2s_dbg_len_ok;
volatile uint32_t i2s_dbg_len_bad;

static uint32_t play_bits = 16;
static uint32_t cap_bits = 16;

__attribute__ ((aligned (4))) static uint8_t audio_buffer[PLAY_RING_BYTES];
__attribute__ ((aligned (4))) static uint8_t capture_buffer[CAPTURE_BUF_BYTES];
static uint32_t capture_read_ptr;
static volatile uint32_t capture_wr_base;
static volatile uint8_t capture_history_ready;
static uint8_t capture_priming;
static volatile uint8_t play_restarting;
static volatile uint8_t play_format_pending;
static uint32_t CaptureWrAbs (void);
static uint32_t PlayRdAbs (void);
static void CaptureRingRestart (void);

volatile uint32_t i2s_play_stop_timeout;
volatile uint32_t i2s_play_prime_fail;
volatile uint32_t i2s_cap_guard_count;
volatile uint32_t i2s_cap_guard_reason;
volatile uint32_t i2s_cap_read_abs;
volatile uint32_t i2s_cap_write_abs;
volatile uint32_t i2s_cap_dma_tc;
volatile uint32_t i2s_cap_prime_pkts;
volatile uint32_t i2s_play_progress_stall;

/* ---- 播放环的指针记账（重写过，见 README 里的说明）-------------------
 * play_wr_abs  : 单调递增的写指针，单位字节，只有生产者改。
 * play_rd_base : DMA 每转完一整环（TC 中断）加 PLAY_RING_BYTES。
 */
static uint32_t play_wr_abs;
static volatile uint32_t play_rd_base;
volatile uint32_t i2s_play_lap; /* DMA 追上写指针（欠载）并自愈的次数 */
volatile uint32_t i2s_pkt_in;   /* 交给播放环的所有包（含被丢弃的） */
volatile uint32_t i2s_play_drop;
volatile uint32_t i2s_plc_count;
volatile uint32_t i2s_dbg_peak;

volatile uint32_t i2s_dbg_nz;
volatile uint8_t i2s_dbg_head[4];
volatile uint8_t amp_muted;
volatile uint32_t i2s_rate;     /* 播放采样率（I2S3 + MCLK） */
volatile uint32_t i2s_cap_rate; /* 录音采样率（I2S2），与播放各自独立 */
/* USB 帧频：HS = 8000 微帧/秒，FS = 1000 帧/秒。由 usbbs 侧在枚举时设置，
 * 用来算「一个 USB 包应该有多少字节」（诊断用）。 */
volatile uint32_t i2s_usb_frame_hz = 8000u;

/* 播放流已关（SET_INTERFACE alt=0）：I2S3 与 DMA2 都已停，
 * 等下一次开流时由 I2S_PlayRingRestart() 整体重启。 */
static volatile uint8_t play_stopped;
static uint32_t play_stop_tick;
static void AmpApply (uint8_t mute);
void I2S_PlayStop (void);

void I2S_SetUsbFrameHz (uint32_t hz) {
    i2s_usb_frame_hz = (hz == 1000u) ? 1000u : 8000u;
}

/* Feedback: Q16.16 samples per USB micro-frame (HS) or frame (FS).
 * 每个 SOF 从缓存里读、原样发；真正的调节在 I2S_FeedbackUpdate()（20ms 主循环）
 * 里做，所以主机看到的是"每 20ms 变一次的分段常数"，不会逐帧抖。 */
static uint32_t feedback_nominal;
static uint32_t cap_nominal; /* capture packet size source, Q16.16 pairs/service */

/* 录音换频诊断（串口 'c' 打印）：区分「请求没到」「到了被拒」「到了并已重建」 */
volatile uint32_t i2s_dbg_cap_calls;
volatile uint32_t i2s_dbg_cap_req;
volatile uint32_t i2s_dbg_cap_ret;


static int32_t feedback_trim;

/* 串口 [fb ] 行用的现场（只在主循环写，读无竞争） */
volatile int32_t  i2s_fb_trim;
volatile uint32_t i2s_fb_level;
volatile uint32_t i2s_fb_target;

/* 最近一次收到 USB 播放包的时刻（tim6_tick）：I2S_PlayFill() 里更新，
 * I2S_FeedbackUpdate() 用它判「当前到底在不在放音」。初值故意给一个很早的时刻，
 * 免得开机后 tim6_tick 还很小时被误判成「正在放音」。 */
static uint32_t i2s_last_pkt_tick = 0xFFFFF000u;

/* Capture: fractional sample-pair accumulator (16.16 pairs per service). */
/* Capture: 读指针伺服用不到小数累加器了（48/96/192/384k 都是整数对/微帧）*/

/* PLC state */
static uint8_t plc_pattern[PLC_PATTERN_FRAMES * 8u];
static uint32_t plc_frames;      /* valid frames in plc_pattern */
static uint32_t plc_gain_q15;    /* 32768 = unity */
static uint32_t zero_silent_cnt; /* consecutive silent packets */
static uint32_t zdet_t0;         /* tim6_tick 时刻，数字静音起点；0 = 未开始 */
static uint32_t zdet_hold_until; /* 换频后压住解静音的时刻（DAC 重新锁定用） */

static uint32_t FeedbackNominalQ16 (uint32_t freq) {
    switch (freq) {
    case 384000U:
        return 48u << 16;
    case 192000U:
        return 24u << 16;
    case 96000U:
        return 12u << 16;
    case 48000U:
        return 6u << 16;
    default:
        return 6u << 16;
    }
}

static uint32_t PlaySyncLevel (void);
static uint32_t PlayTargetLevel (void);
static int32_t PlayLevel (void);
static int32_t SlotLoad (const uint8_t *p);
static void SlotStore (uint8_t *p, uint32_t value);
/* 定义在后面的小工具（欠载补音用它做衰减）*/
static void RingScale (uint32_t off, uint32_t frames, uint32_t gain_q15);

void I2S_FeedbackReset (void) {
    /* Only drop the trim. Do not write feedback_nominal here — callers
     * change i2s_rate around this call and a stale i2s_rate would report the
     * old rate back to the host. */
    feedback_trim = 0;
}

uint32_t I2S_FeedbackUpdate (uint16_t tick_now, uint32_t ticks_per_service) {
    int32_t nominal;
    int32_t lvl;
    int32_t target;
    int32_t err;
    int32_t trim;
    int32_t lim;

    (void)tick_now;
    (void)ticks_per_service;

    nominal = (int32_t)FeedbackNominalQ16 (i2s_rate);

    /* 500ms 没收到包（主机暂停/停流）：回名义值，别拿"水位被抽干"去调速率 */
    if ((uint32_t)(tim6_tick - i2s_last_pkt_tick) >= 25u) {
        feedback_trim = 0;
        feedback_nominal = (uint32_t)nominal;
        i2s_fb_trim = 0;
        i2s_fb_level = 0u;
        i2s_fb_target = 0u;
        return feedback_nominal;
    }

    lvl = PlayLevel ();
    target = (int32_t)PlayTargetLevel ();

    /* 水位读数不可信（欠载为负 / 少进了一次 TC 中断而超大）：本轮不调，
     * 交给 PlaySyncLevel() 的便宜自愈去修，下一轮再看。 */
    if ((lvl < 0) || (lvl >= (int32_t)PLAY_RING_BYTES)) {
        i2s_fb_level = (uint32_t)((lvl < 0) ? 0 : lvl);
        i2s_fb_target = (uint32_t)target;
        return feedback_nominal;
    }

    err = lvl - target;
    /* 【符号】err > 0 = 环里积压太多 → 要**少**送 → trim 取负。
     * 反馈值 = nominal + trim 是"请主机每微帧送多少样本"，报大 = 送更多 = 积压更多，
     * 所以这里必须是负号；写成正号就是正反馈，一上电就冲顶再欠载。 */
    trim = -(err * 1024) / (int32_t)FEEDBACK_TAU_MS;

    /* 限幅：±N% 名义值。别设太小 —— 那是"把水位从 0 拉回来"的唯一权限；
     * 也别设太大 —— 主机侧会跟着做明显的速率/重采样调整。 */
    lim = (nominal * FEEDBACK_TRIM_MAX_PCT) / 100;
    if (lim < 16) {
        lim = 16;
    }
    if (trim > lim) {
        trim = lim;
    } else if (trim < -lim) {
        trim = -lim;
    }

    feedback_trim = trim;
    feedback_nominal = (uint32_t)(nominal + trim);

    i2s_fb_trim = trim;
    i2s_fb_level = (uint32_t)lvl;
    i2s_fb_target = (uint32_t)target;

    return feedback_nominal;
}

/* Cached report for the SOF path (every 125 us). Must stay trivial. */
uint32_t I2S_FeedbackGet (void) {
    return feedback_nominal;
}

static void WZero (void *dst, uint32_t len) {
    uint32_t *d = (uint32_t *)dst;
    uint32_t n = len >> 2;

    while (n-- != 0u) {
        *d++ = 0u;
    }
}

/* ---- 播放环指针 ------------------------------------------------------- */

/* Protect DMA position snapshots and format handoff from interrupt races. */
static uint32_t IrqLock (void) {
    uint32_t m;

    __asm volatile ("csrr %0, 0x800" : "=r"(m) : : "memory");
    __asm volatile ("csrc 0x800, %0" : : "r"(0x88u) : "memory");
    __asm volatile ("fence.i");
    return m;
}

static void IrqUnlock (uint32_t m) {
    m &= 0x88u;
    if (m != 0u) {
        __asm volatile ("csrs 0x800, %0" : : "r"(m) : "memory");
        __asm volatile ("fence.i");
    }
}

 
volatile uint32_t i2s_heal_cnt;
volatile uint32_t i2s_heal_reason;
volatile uint32_t i2s_gap_miss;

volatile uint32_t i2s_gap_max;
volatile uint32_t i2s_cap_under;
volatile uint32_t i2s_cap_over;

volatile uint32_t i2s_cap_fill_pairs;
volatile uint32_t i2s_cap_short_pkts;
volatile uint32_t i2s_cap_busy;
/* 运行期左右交换：0 = L 在第一个槽（正常），1 = 交换 */
static volatile uint8_t lr_swap_play;
static volatile uint8_t lr_swap_cap;
static uint8_t auto_mute_enabled=ZERO_DETECT_ENABLE;
static uint8_t unmute_holding;
static uint16_t unmute_started;

static void PlcInject (void); /* 定义在后面（PLC_ENABLE 两种形态都兼容）*/

void I2S_SetLrSwap (uint8_t play_swap, uint8_t cap_swap) {
    lr_swap_play = (uint8_t)((play_swap != 0u) ? 1u : 0u);
    lr_swap_cap = (uint8_t)((cap_swap != 0u) ? 1u : 0u);
}

void I2S_GetLrSwap (uint8_t *play_swap, uint8_t *cap_swap) {
    if (play_swap != NULL) {
        *play_swap = lr_swap_play;
    }
    if (cap_swap != NULL) {
        *cap_swap = lr_swap_cap;
    }
}

/* 环里 [off, off+frames*8) 的每一帧，左右两个 4 字节槽对调 */
static void PlaySwapSlots (uint32_t off, uint32_t frames) {
    uint32_t i;
    for (i = 0u; i < frames; i++) {
        uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
        uint32_t t;
        memcpy (&t, &audio_buffer[d], 4u);
        memcpy (&audio_buffer[d], &audio_buffer[d + 4u], 4u);
        memcpy (&audio_buffer[d + 4u], &t, 4u);
    }
}

/* 累计"被接受的包"总数：不被play 命令清零，专供健康检查做"有没有在落环"的判据 */
static volatile uint32_t i2s_pkt_acc;

uint8_t I2S_PlayHealthCheck (void) {
    static uint32_t last_tick;
    static uint32_t hc_pkt_in;
    static uint32_t hc_pkt_acc;
    static uint8_t bad_ticks;
    static uint8_t bad_align;
    static uint8_t bad_lvl;
    uint32_t reason = 0u;
    int32_t lvl;

    if (tim6_tick == last_tick) {
        return 0u; /* 20ms 限速：这条路径全部是寄存器读，但没必要每轮都做 */
    }
    last_tick = tim6_tick;

    if (play_stopped != 0u || play_restarting != 0u || play_format_pending != 0u ||
        USBHS_DevAltSetting[1] == 0u) {
        bad_ticks = 0u;
        bad_align = 0u;
        bad_lvl = 0u;
        hc_pkt_in = i2s_pkt_in;
        hc_pkt_acc = i2s_pkt_acc;
        return 0u;
    }

    /* a) 帧对齐（2 拍去抖：这条理论上不会瞬变，但宁可多等 20ms 也不误重启）*/
    if (((play_wr_abs & 7u) != 0u) || ((play_rd_base & 7u) != 0u)) {
        if (++bad_align >= 2u) {
            reason |= 0x01u;
        }
    } else {
        bad_align = 0u;
    }

    /* b) 水位（同样是 2 拍）。注意 PlayFill 里已经有 PlaySyncLevel() 在做
     *    便宜的自愈（只补静音+移指针），所以这里能连续两拍看到异常，
     *    说明"便宜的自愈也没兜住"，才值得整段重启。 */
    lvl = PlayLevel ();
    if ((lvl < 0) || ((uint32_t)lvl >= PLAY_RING_BYTES)) {
        if (++bad_lvl >= 2u) {
            reason |= 0x02u;
        }
    } else {
        bad_lvl = 0u;
    }

    /* Configuration checks cannot infer PCM values or internal WS/halfword phase. */
    if (I2S_PlayHwOk() == 0u) {
        reason |= 0x08u;
    }
    {
        static uint32_t previous_rd;
        uint32_t rd = PlayRdAbs();
        if (rd == previous_rd && (uint32_t)(tim6_tick - i2s_last_pkt_tick) < 5u) {
            if (++i2s_play_progress_stall >= 2u) {
                reason |= 0x10u;
            }
        } else {
            i2s_play_progress_stall = 0u;
        }
        previous_rd = rd;
    }

    /* c) 收包不落环：连续 5 拍（100ms）都这样才判死 */
    if ((i2s_pkt_in != hc_pkt_in) && (i2s_pkt_acc == hc_pkt_acc)) {
        if (++bad_ticks >= 5u) {
            reason |= 0x04u;
            bad_ticks = 0u;
            hc_pkt_in = i2s_pkt_in;
            hc_pkt_acc = i2s_pkt_acc;
        }
    } else {
        bad_ticks = 0u;
        hc_pkt_in = i2s_pkt_in;
        hc_pkt_acc = i2s_pkt_acc;
    }

    if (reason == 0u) {
        return 0u;
    }

    bad_align = 0u;
    bad_lvl = 0u;
    i2s_heal_reason = reason;
    i2s_heal_cnt++;
    USB_RequestPlayRecovery();
    return (uint8_t)reason;
}

/* 丢包补偿：SOF 里发现"距上一包超过 1 个微帧"就补上缺的那几包。
 * 为什么必须在这里做：等时包在总线上丢了 = 这个 OUT 完成事件根本不来，
 * I2S_PlayFill() 不会被调用，PLC（只在 num==0 时触发）永远看不到它 ——
 * 这就是"PLC 计数为 0 却听到 pop"的全部原因。 */
void I2S_PlayLostPackets (uint32_t n) {
    uint32_t i;

    if (play_stopped != 0u || play_restarting != 0u || play_format_pending != 0u ||
        USBHS_DevAltSetting[1] == 0u) {
        return;
    }
    if (n > 8u) {
        n = 8u; /* 一次性断流不要补太多，交给 StreamWatch 判死 */
    }
    for (i = 0u; i < n; i++) {
        PlcInject ();
    }
    i2s_gap_miss += n;
}

/* DMA 当前读指针，绝对字节。
 * CNTR 从 PLAY_RING_HALF 递减到 1，所以环内偏移 = (HALF - CNTR)*2。
 * 读 CNTR 与 play_rd_base 之间有可能正好跨过一次 TC（中断刚加了一环），
 * 那样两者不同源，算出来会差一整环；用「双读校验」把这种撕裂读挡掉。 */
static uint32_t PlayRdAbs (void) {
    uint32_t a, b, result, irq = NVIC_GetStatusIRQ(DMA2_Channel2_IRQn);
    NVIC_DisableIRQ(DMA2_Channel2_IRQn);
    /* Both readers and ISR claim TC under this lock. CNTR reload is hardware. */
    do {
        a = DMA2_Channel2->CNTR;
        if (DMA_GetITStatus (DMA2_IT_TC2) != RESET) {
            DMA_ClearITPendingBit (DMA2_IT_TC2);
            play_rd_base += PLAY_RING_BYTES;
            i2s_dbg_dma_tc++;
            continue;
        }
        b = DMA2_Channel2->CNTR;
        if (b <= a && DMA_GetITStatus (DMA2_IT_TC2) == RESET) {
            result = play_rd_base + (PLAY_RING_HALF - b) * 2u;
            break;
        }
    } while (1);
    if(irq) NVIC_EnableIRQ(DMA2_Channel2_IRQn);
    return result;
}

/* 环里待播的字节数。负数 = DMA 已经跑到写指针前面（欠载）。 */
static int32_t PlayLevel (void) {
    return (int32_t)(play_wr_abs - PlayRdAbs());
}

/* 把 [off, off+len) 这段环内区域填成数字静音（可能跨越环尾）。 */
static void PlayZeroSpan (uint32_t off, uint32_t len) {
    uint32_t o = off & PLAY_RING_MASK;
    uint32_t first = PLAY_RING_BYTES - o;

    if (first > len) {
        first = len;
    }
    WZero (&audio_buffer[o], first);
    if (len > first) {
        WZero (&audio_buffer[0], len - first);
    }
}

static uint32_t PlayTargetLevel (void) {
#if PLAY_TARGET_US % 1000 != 0
#error "PLAY_TARGET_US must be a multiple of 1000 (integer math, no float)"
#endif
    /* samples = rate * T_us / 1e6，改写成 rate/1000 * (T_us/1000) 避免 64 位除法 */
    uint32_t samples = (i2s_rate / 1000u) * (PLAY_TARGET_US / 1000u);

    if (samples < 16u) {
        samples = 16u;
    }
    return samples * 8u;
}


static void PlayFillConceal (uint32_t off, uint32_t len) {
    uint32_t frames = len / 8u;
    uint32_t i;

    if ((plc_frames == 0u) || (frames == 0u)) {
        PlayZeroSpan (off, len); /* 还没播过任何东西：只能补零 */
        return;
    }
    for (i = 0u; i < frames; i++) {
        uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
        uint32_t s = ((i % plc_frames) * 8u);

        memcpy (&audio_buffer[d], &plc_pattern[s], 8u);
    }
    if (plc_gain_q15 < 32768u) {
        RingScale (off, frames, plc_gain_q15);
    }
    plc_gain_q15 = (plc_gain_q15 * PLC_DECAY_Q15) >> 15;
    if (plc_gain_q15 < PLC_SILENCE_Q15) {
        plc_gain_q15 = 0u;
    }
    if (len > frames * 8u) { /* 尾部不足一帧（理论上不会）*/
        PlayZeroSpan ((off + frames * 8u) & PLAY_RING_MASK, len - frames * 8u);
    }
}

/* 欠载自愈：DMA 已经越过写指针时，重新在它前面留出目标余量。
 * 那段必须是静音，否则 DMA 会接着把环里的旧数据重播一遍
 * —— 听感就是「突然变成重复的嗡声 / 白噪声」。
 * 返回当前待播字节数（已保证 >= 0 且 <= PLAY_RING_BYTES）。 */

static uint32_t PlaySyncLevel (void) {
    int32_t lvl = PlayLevel();

    if (lvl < 0 || (uint32_t)lvl >= PLAY_RING_BYTES) {
        
        uint32_t m = IrqLock();

        lvl = PlayLevel();
        if (lvl < 0 || (uint32_t)lvl >= PLAY_RING_BYTES) {
            uint32_t base = PlayRdAbs() & ~7u;
            uint32_t target = PlayTargetLevel();

            i2s_play_lap++;
            play_wr_abs = base + target;
            /* 补的是"上一段音频的衰减重放"，不是纯零：低延迟档下这能把
             * "咔"变成几乎听不出来的"糊一下"（见 PlayFillConceal 的注释）。 */
            PlayFillConceal (base, target);
            lvl = (int32_t)target;
        }
        IrqUnlock (m);
    }
    return (uint32_t)lvl;
}

/* Free bytes between the write pointer and the DMA read pointer. */
static uint32_t PlayRingFree (void) {
    uint32_t lvl = PlaySyncLevel();

    return PLAY_RING_BYTES - lvl;
}

/* ---- SDATA 静音门 ----------------------------------------------------
 * 换频窗口里把 I2S3 的 SD(PC12) 从复用输出切成通用推挽输出并写 0，
 * 硬保证线上是低电平；BCLK(PC10)/LRCK(PA4) 仍由 SPI3 继续输出，
 * 所以 DAC 那边时钟不断、可以安心锁相，只是收到的全是 0（数字静音）。 */
static volatile uint8_t sdata_gated; /* 1 = 门关着 */

static void SdataParkLow (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* 先写输出锁存器，再切成输出模式 —— 这样引脚一接管就是低，
     * 不会出现「接管瞬间输出上一位残留电平」的毛刺。 */
    GPIO_ResetBits (GPIOC, GPIO_Pin_12);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);

    sdata_gated = 1u;
}

static void SdataReleaseAf (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);
}

/* 把 SD 交回 SPI3（复用推挽）。*/
static void SdataReleaseNow (void) {
    if (sdata_gated) {
        sdata_gated = 0u;
        SdataReleaseAf();
    }
}

static void SdataStopI2s (void) {
    uint16_t started;
    uint16_t budget = (uint16_t)(3840000u / (i2s_rate ? i2s_rate : 48000u) + 8u);
    SdataParkLow();
    SPI_I2S_DMACmd (SPI3, SPI_I2S_DMAReq_Tx, DISABLE);
    DMA_Cmd (DMA2_Channel2, DISABLE);
    started = (uint16_t)TIM8->CNT;
    while ((SPI3->I2SCFGR & 0x0400u) != 0u &&
           (((SPI3->STATR & SPI_I2S_FLAG_TXE) == 0u) ||
            ((SPI3->STATR & SPI_I2S_FLAG_BSY) != 0u))) {
        if ((uint16_t)((uint16_t)TIM8->CNT - started) >= budget) {
            i2s_play_stop_timeout++;
            break; /* Continuous master TX may keep BSY high: reset is the fallback. */
        }
    }
    I2S_Cmd (SPI3, DISABLE);
}

void I2S_PlayRingRestart (void) {
    I2S_InitTypeDef cfg = {0};
    uint32_t irq = IrqLock();
    uint16_t started;
    uint32_t i;

    play_restarting = 1u;
    SdataParkLow();
    IrqUnlock (irq);
    SdataStopI2s();
    SPI_I2S_DeInit (SPI3); /* RCC reset, not merely I2S_Init register writes. */
    WZero (audio_buffer, PLAY_RING_BYTES);

    cfg.I2S_Mode = I2S_Mode_MasterTx;
    cfg.I2S_Standard = I2S_Standard_Phillips;
    cfg.I2S_DataFormat = I2S_DataFormat_32b;
    cfg.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    cfg.I2S_AudioFreq = i2s_rate;
    cfg.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init (SPI3, &cfg);

    irq = IrqLock();
    play_wr_abs = PlayTargetLevel();
    play_rd_base = 0u;
    plc_frames = 0u;
    plc_gain_q15 = 32768u;
    zero_silent_cnt = 0u;
    zdet_t0 = 0u;
    DMA_ClearITPendingBit (DMA2_IT_HT2 | DMA2_IT_TC2);
    DMA2_Channel2->CNTR = PLAY_RING_HALF;
    DMA2_Channel2->MADDR = (uint32_t)audio_buffer;
    DMA2_Channel2->PADDR = (uint32_t)&SPI3->DATAR;

    /* Prime one silent stereo frame after reset. DMA then starts at offset 0
     * on the next stereo frame. Only this short handoff is atomic. */
    I2S_Cmd (SPI3, ENABLE);
    started = (uint16_t)TIM8->CNT;
    /* RM 20.3.6.2: TXE is zero while I2SE=0. Count all four writes
     * after enabling, rather than treating a disabled write as half a frame. */
    for (i = 0u; i < 4u; i++) {
        while ((SPI3->STATR & SPI_I2S_FLAG_TXE) == 0u) {
            if ((uint16_t)((uint16_t)TIM8->CNT - started) >= 80u) {
                i2s_play_prime_fail++;
                I2S_Cmd (SPI3, DISABLE);
                play_stopped = 1u;
                play_restarting = 0u;
                IrqUnlock (irq);
                return;
            }
        }
        SPI3->DATAR = 0u;
    }
    DMA_Cmd (DMA2_Channel2, ENABLE);
    SPI_I2S_DMACmd (SPI3, SPI_I2S_DMAReq_Tx, ENABLE);
    SdataReleaseNow();
    play_stopped = 0u;
    play_restarting = 0u;
    IrqUnlock (irq);
}

/* 播放流关闭（SET_INTERFACE alt=0）：把 I2S3 与 DMA2 真的停掉。
 * 只静音不停流的话，环被抽干后 I2S_PlayGuard/PlaySyncLevel() 会每 20ms
 * 补一次静音，耳朵听到的就是每 20ms 一次斩波（锯齿声）。
 * 下次开流走 I2S_PlayRingRestart()，那里有完整的 停/清环/init/重启 序列。 */
void I2S_PlayStop (void) {
    uint32_t irq = IrqLock();
    play_stopped = 1u;
    play_stop_tick = tim6_tick;
    AmpApply (1u);
    SdataParkLow();
    SPI_I2S_DMACmd (SPI3, SPI_I2S_DMAReq_Tx, DISABLE);
    DMA_Cmd (DMA2_Channel2, DISABLE);
    I2S_Cmd (SPI3, DISABLE);
    /* alt0 can arrive in EP0 ISR. Next start uses RCC reset; do not wait here. */
    IrqUnlock (irq);
}

uint8_t I2S_PlayIsStopped (void) {
    return play_stopped;
}

uint32_t I2S_ZdetHold (void) {
    return zdet_hold_until;
}

/* ---- helpers operating on one 4-byte I2S slot ------------------------- */
/* DATAR is 16 bits wide. Each 32-bit slot is stored as high halfword,
 * then low halfword; bytes within each halfword remain little-endian.
 * RM 20.3.2.1: 0x8EAA3300 is written as 0x8EAA, then 0x3300. */
static int32_t SlotLoad (const uint8_t *p) {
    return (int32_t)(((uint32_t)p[1] << 24) | ((uint32_t)p[0] << 16) |
                     ((uint32_t)p[3] << 8) | (uint32_t)p[2]);
}

static void SlotStore (uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 16);
    p[1] = (uint8_t)(value >> 24);
    p[2] = (uint8_t)value;
    p[3] = (uint8_t)(value >> 8);
}

static void SlotScale (uint8_t *p, uint32_t gain_q15) {
    int32_t v = SlotLoad (p);
    int32_t o = (int32_t)(((int64_t)v * (int32_t)gain_q15) >> 15);
    SlotStore (p, (uint32_t)o);
}

static uint32_t SlotAbs (uint8_t *p) {
    int32_t v = SlotLoad (p);
    int64_t a = (v < 0) ? -(int64_t)v : (int64_t)v;
    return (uint32_t)((a > 0x7FFFFFFF) ? 0x7FFFFFFFu : a);
}

/* Apply gain to `frames` 8-byte frames starting at ring offset `off`. */
static void RingScale (uint32_t off, uint32_t frames, uint32_t gain_q15) {
    uint32_t i;
    for (i = 0; i < frames; i++) {
        uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
        SlotScale (&audio_buffer[d], gain_q15);
        SlotScale (&audio_buffer[d + 4u], gain_q15);
    }
}

/* Peak magnitude over `frames` frames (max of both channels). */
static uint32_t RingPeak (uint32_t off, uint32_t frames) {
    uint32_t i, peak = 0u;
    for (i = 0; i < frames; i++) {
        uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
        uint32_t a = SlotAbs (&audio_buffer[d]);
        uint32_t b = SlotAbs (&audio_buffer[d + 4u]);
        if (a > peak) {
            peak = a;
        }
        if (b > peak) {
            peak = b;
        }
    }
    return peak;
}

/* ---- zero detection / amplifier mute --------------------------------- */
static void AmpApply (uint8_t mute) {
    amp_muted = mute ? 1u : 0u;
}

static void ZeroDetectUpdate (uint32_t peak) {
    if(unmute_holding) {
        if((uint16_t)((uint16_t)TIM8->CNT-unmute_started)<PLAY_UNMUTE_DELAY_US*960u/1000u) return;
        unmute_holding=0u;
    }
    if (!auto_mute_enabled || peak > ZERO_DETECT_THRESH) {
        zero_silent_cnt = 0u;
        zdet_t0 = 0u;
        /* 换频后先压住一小会儿再放开功放 */
        if (amp_muted) {
            AmpApply (0u);
        }
        return;
    }
    /* 数字静音：从第一包静音起算满 ZERO_MUTE_HOLD_TICKS 拍才关功放，
     * 这样乐曲里的弱音乐句不会被当成停播。 */
    if (zdet_t0 == 0u) {
        zdet_t0 = tim6_tick ? tim6_tick : 1u;
        return;
    }
    if ((uint32_t)(tim6_tick - zdet_t0) >= ZERO_MUTE_HOLD_TICKS && !amp_muted) {
        AmpApply (1u);
    }
}

void I2S_SetAutoMute(uint8_t enabled) {auto_mute_enabled=enabled ? 1u : 0u;}
uint8_t I2S_GetAutoMute(void) {return auto_mute_enabled;}

void AMP_SetMute (BitAction unmute) {
    if (unmute == Bit_SET) {
        zero_silent_cnt = 0u;
        zdet_t0 = 0u;
        AmpApply (0u);
    } else {
        zero_silent_cnt = 0u;
        zdet_t0 = 0u;
        AmpApply (1u);
    }
}

/* 换频 / 换位深时用：先静音，并把解静音推迟 ZERO_UNMUTE_DELAY_TICKS 拍。 */
void I2S_PlayMuteSettle (void) {
    zero_silent_cnt = 0u;
    zdet_t0 = 0u;
    AmpApply (1u);
    unmute_started=(uint16_t)TIM8->CNT;unmute_holding=1u;
    zdet_hold_until=PLAY_UNMUTE_DELAY_US;
}

/* ---- packet loss concealment ----------------------------------------- */
#if PLC_ENABLE
/* Append a decayed repeat of the last real audio to the ring.  Frames are
 * taken from plc_pattern cyclically so a single lost 20ms burst still
 * sounds like audio instead of silence. */
static void PlcInject (void) {
    /* feedback_nominal 是 Q16.16「每微帧样本数」。一个 USB 包在 HS 只
     * 覆盖 1 个微帧（125us），在 FS 覆盖 8 个（1ms），所以要乘
     * 8000/i2s_usb_frame_hz */
    uint32_t frames = i2s_rate / i2s_usb_frame_hz;
    uint32_t out_bytes, off, i;

    if (plc_frames == 0u) {
        return; /* nothing played yet, nothing to conceal */
    }
    if (frames == 0u) {
        frames = 6u;
    }
    /* Pattern length limits history, not the number of frames to replace.
     * 384k HS must replace 48 frames, cyclically reusing the stored pattern. */
    out_bytes = frames * 8u;
    if (out_bytes + PLAY_RING_GUARD > PlayRingFree()) {
        return; /* ring is full, the DMA still has audio to play */
    }

    off = play_wr_abs & PLAY_RING_MASK;
    for (i = 0; i < frames; i++) {
        uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
        uint32_t s = ((i % plc_frames) * 8u);
        memcpy (&audio_buffer[d], &plc_pattern[s], 8u);
    }
    if (plc_gain_q15 < 32768u) {
        RingScale (off, frames, plc_gain_q15);
    }
    play_wr_abs += out_bytes;

    /* attenuate the next repeat; below ~-40dB just go to digital zero */
    plc_gain_q15 = (plc_gain_q15 * PLC_DECAY_Q15) >> 15;
    if (plc_gain_q15 < PLC_SILENCE_Q15) {
        plc_gain_q15 = 0u;
    }
    i2s_plc_count++;
}
#else
static void PlcInject (void) { }
#endif

/* Save the tail of the packet we just wrote as the concealment pattern. */
static void PlcRemember (uint32_t off, uint32_t frames) {
#if PLC_ENABLE
    uint32_t keep = (frames < PLC_PATTERN_FRAMES) ? frames : PLC_PATTERN_FRAMES;
    uint32_t i;
    uint32_t first = frames - keep;
    for (i = 0; i < keep; i++) {
        uint32_t d = (off + (first + i) * 8u) & PLAY_RING_MASK;
        memcpy (&plc_pattern[i * 8u], &audio_buffer[d], 8u);
    }
    plc_frames = keep;
#else
    (void)off;
    (void)frames;
#endif
}

/* 换档（采样率或位深变了）之后，主机会继续按旧格式发 2~3 包。按新位深解释
 * 这些包，样本数会差 1.5~2 倍，写进环就是一段锯齿。所以换档时置
 * play_fmt_settle，只放行长度落在 [标称, 标称+1 slot] 的包，第一包对上才清。
 * I2S_SetPlayRate()/I2S_SetPlayBits() 负责置位并顺带清环 + 静音。 */
static volatile uint8_t play_fmt_settle;
static uint8_t play_fmt_tries;

static void PlayPairFromPcm (const uint8_t *src, uint8_t *dst) {
    if (play_bits == 32u) {
        dst[0]=src[2]; dst[1]=src[3]; dst[2]=src[0]; dst[3]=src[1];
        dst[4]=src[6]; dst[5]=src[7]; dst[6]=src[4]; dst[7]=src[5];
    } else if (play_bits == 24u) {
        dst[0]=src[1]; dst[1]=src[2]; dst[2]=0u; dst[3]=src[0];
        dst[4]=src[4]; dst[5]=src[5]; dst[6]=0u; dst[7]=src[3];
    } else {
        dst[0]=src[0]; dst[1]=src[1]; dst[2]=0u; dst[3]=0u;
        dst[4]=src[2]; dst[5]=src[3]; dst[6]=0u; dst[7]=0u;
    }
}

void I2S_PlayFill (uint8_t *src, uint32_t num) {
    uint32_t samples, out_bytes, i, off, peak;

    /* 主机还在发包就算流活着，不管后面是否因为环满被丢*/
    i2s_pkt_in++;
    i2s_last_pkt_tick = tim6_tick; /* 同上：反馈控制用它判「在不在放音」 */

    if (play_stopped != 0u || play_restarting != 0u || play_format_pending != 0u) {
        return; /* 流已关，I2S3 已停，写进环也没人放 */
    }

    if (num == 0u || src == 0) {
        /* Zero-length packet or a dropped USB frame.  The DMA must keep
         * running - the old firmware disabled the channel here and never
         * re-armed it, which is exactly why BCLK died on a lost packet.
         * Inject concealment audio instead so playback stays continuous. */
        i2s_dbg_zlp++;
        i2s_pkt_acc++; /* 零长包也算"处理过了"，别让健康检查把它当成"收包不落环" */
        PlcInject();
        return;
    }

    if (play_bits == 32) {
        samples = num / 8u;
    } else if (play_bits == 24) {
        samples = num / 6u;
    } else {
        samples = num / 4u;
    }
    if (samples == 0u) {
        i2s_pkt_acc++; /* 短到解不出样本：也算处理过，交给 plc 兜 */
        PlcInject();
        return;
    }
    out_bytes = samples * 8u;

    /* --- 实测：主机每包应发多少字节 --- */
    {
        uint32_t bpf = (play_bits == 32u) ? 8u : ((play_bits == 24u) ? 6u : 4u);
        uint32_t exp = (i2s_rate / i2s_usb_frame_hz) * bpf;
        i2s_dbg_exp_len = exp;
        if (num % bpf == 0u && num + bpf >= exp && num <= exp + bpf) {
            i2s_dbg_len_ok++;
        } else {
            i2s_dbg_len_bad++;
        }
        /* 换档残包剔除：允许标称 ±1 个 slot（主机按 UAC2 反馈每帧可以少/多
         * 一个 slot，抓包里 1920 字节的 URB 中就有 1916/1732 的变体），超出这个
         * 带宽说明这几包还是旧格式，写进环就是锯齿，直接丢。
         * 超时也不能接受已知格式错误的包。长度相同的两种格式仍无法靠包长区分。 */
        if (num % bpf != 0u) return;
        if (play_fmt_settle != 0u) {
            if ((num + bpf >= exp) && (num <= exp + bpf)) {
                play_fmt_settle = 0u;
                play_fmt_tries = 0u;
            } else {
                play_fmt_tries++;
                return; /* Never accept a known wrong-length packet after a timeout. */
            }
        }
    }

    {
        uint32_t freeb, lvl;

        /* 先做欠载自愈（如有），再拿真正的余量做判断。 */
        lvl = PlaySyncLevel();
        freeb = PLAY_RING_BYTES - lvl;

        if (lvl > i2s_dbg_lvl_max) {
            i2s_dbg_lvl_max = lvl;
        }
        if (lvl < i2s_dbg_lvl_min) {
            i2s_dbg_lvl_min = lvl;
        }
        /* 环满时整包丢弃：丢的是内容，不会让 DMA 读到半新半旧的数据 */
        if (out_bytes + PLAY_RING_GUARD > freeb) {
            i2s_play_drop++;
            return;
        }
    }
    i2s_dbg_pkts++;
    i2s_dbg_frames += samples;
    i2s_dbg_lastlen = num;
    i2s_pkt_acc++;

    off = play_wr_abs & PLAY_RING_MASK;
    for (i = 0u; i < samples; i++) {
        uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
        PlayPairFromPcm (src + i * (play_bits / 8u) * 2u, &audio_buffer[d]);
    }

    /* 运行期左右交换（串口 swap play 控制）：只在开关打开时才做，
     * 代价是每帧两次 4 字节复制 —— 384k 下 48 帧 = 96 次，可接受。
     * 用来判定/修正"ES9038 侧 L/R 反"：开关一拨就能听出来，不用重烧。 */
    if (lr_swap_play != 0u) {
        PlaySwapSlots (off, samples);
    }

    /* Recovery ramp: if we were concealing, fade the real audio back in
     * over PLC_RAMP_FRAMES frames instead of jumping to full amplitude. */
    if (plc_gain_q15 < 32768u) {
        uint32_t n = (samples < PLC_RAMP_FRAMES) ? samples : PLC_RAMP_FRAMES;
        uint32_t g0 = plc_gain_q15;
        for (i = 0; i < n; i++) {
            uint32_t g = g0 + (uint32_t)(((int64_t)(32768 - g0) * i) / n);
            uint32_t d = (off + i * 8u) & PLAY_RING_MASK;
            SlotScale (&audio_buffer[d], g);
            SlotScale (&audio_buffer[d + 4u], g);
        }
        plc_gain_q15 = 32768u;
    }

    peak = RingPeak (off, samples);
    if (peak > i2s_dbg_peak) {
        i2s_dbg_peak = peak;
    }
    if (peak != 0u) {
        i2s_dbg_nz++;
    }
    if (num >= 4u) {
        /* 取原始载荷，不经 PLC/增益，才能反映"主机到底发了什么" */
        i2s_dbg_head[0] = src[0];
        i2s_dbg_head[1] = src[1];
        i2s_dbg_head[2] = src[2];
        i2s_dbg_head[3] = src[3];
    }
    zero_silent_cnt = 0u; /* real packet arrived */
    ZeroDetectUpdate (peak);
    /* Silence is valid history too: a later gap must not replay old music. */
    PlcRemember (off, samples);

    play_wr_abs += out_bytes;
}

static void CapturePairToPcm (const uint8_t raw[8], uint8_t *dst, uint8_t swap) {
    uint8_t sw = (uint8_t)(swap | (uint8_t)CAPTURE_RAW_SWAP);
    const uint8_t *a = raw;     /* 槽 0（左）*/
    const uint8_t *b = raw + 4; /* 槽 1（右）*/

    if (sw != 0u) {
        a = raw + 4;
        b = raw;
    }

    if (cap_bits == 32u) {
        dst[0] = a[2];
        dst[1] = a[3];
        dst[2] = a[0];
        dst[3] = a[1];
        dst[4] = b[2];
        dst[5] = b[3];
        dst[6] = b[0];
        dst[7] = b[1];
    } else if (cap_bits == 24u) {
        dst[0] = a[3];
        dst[1] = a[0];
        dst[2] = a[1];
        dst[3] = b[3];
        dst[4] = b[0];
        dst[5] = b[1];
    } else /* 16-bit */
    {
        dst[0] = a[0];
        dst[1] = a[1];
        dst[2] = b[0];
        dst[3] = b[1];
    }
}

static uint32_t CaptureWrAbs (void) {
    uint32_t a, b, result, irq = NVIC_GetStatusIRQ(DMA1_Channel4_IRQn);
    NVIC_DisableIRQ(DMA1_Channel4_IRQn);
    do {
        a = DMA1_Channel4->CNTR;
        if (DMA_GetITStatus (DMA1_IT_TC4) != RESET) {
            DMA_ClearITPendingBit (DMA1_IT_TC4);
            capture_wr_base += CAPTURE_BUF_BYTES;
            capture_history_ready = 1u;
            i2s_cap_dma_tc++;
            continue;
        }
        b = DMA1_Channel4->CNTR;
        if (b <= a && DMA_GetITStatus (DMA1_IT_TC4) == RESET) {
            result = capture_wr_base + (CAPTURE_BUF_BYTES / 2u - b) * 2u;
            break;
        }
    } while (1);
    if(irq) NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    return result & ~7u;
}

static uint32_t CaptureLag (void) {
    uint32_t lag = (i2s_cap_rate / i2s_usb_frame_hz) * 16u;
    return lag > CAPTURE_BUF_BYTES / 2u ? CAPTURE_BUF_BYTES / 2u : lag;
}

static void CaptureRebase (uint32_t wr, uint32_t reason) {
    uint32_t lag = CaptureLag();
    capture_read_ptr = (capture_history_ready || wr >= lag) ? wr - lag : 0u;
    capture_priming = 1u;
    i2s_cap_guard_count++;
    i2s_cap_guard_reason = reason;
}

uint32_t I2S_CapRead (uint8_t *dst, uint32_t scale, uint32_t max_bytes) {
    uint32_t pair_bytes = cap_bits == 32u ? 8u : cap_bits == 24u ? 6u : 4u;
    uint32_t max_pairs = max_bytes / pair_bytes;
    uint32_t base = (i2s_cap_rate / 8000u) * scale;
    uint32_t wr = CaptureWrAbs();
    int32_t available = (int32_t)(wr - capture_read_ptr);
    uint32_t pairs_avail, npairs, pairs_do, i;
    if (!max_pairs) return 0u;
    if (!base) base = 6u * scale;
    if ((capture_read_ptr & 7u) || available < 0 ||
        (uint32_t)available >= CAPTURE_BUF_BYTES - 8u) {
        CaptureRebase (wr, (capture_read_ptr & 7u) ? 1u : available < 0 ? 2u : 4u);
        available = (int32_t)(wr - capture_read_ptr);
    }
    i2s_cap_write_abs = wr;
    i2s_cap_read_abs = capture_read_ptr;
    pairs_avail = (uint32_t)available / 8u;
    /* Newly cleared RAM is not valid captured history. Prime without reading it. */
    if (capture_priming && pairs_avail < CaptureLag() / 8u) {
        npairs = base < max_pairs ? base : max_pairs;
        memset (dst, 0, npairs * pair_bytes);
        i2s_cap_prime_pkts++;
        return npairs * pair_bytes;
    }
    capture_priming = 0u;
    if (pairs_avail > base * 3u) {
        npairs = base + 1u;
        i2s_cap_over++;
    } else if (pairs_avail < base) {
        npairs = base > 1u ? base - 1u : base;
        i2s_cap_under++;
        i2s_cap_short_pkts++;
    } else npairs = base;
    if (npairs > max_pairs) npairs = max_pairs;
    pairs_do = npairs < pairs_avail ? npairs : pairs_avail;
    for (i = 0u; i < pairs_do; i++) {
        uint8_t raw[8];
        memcpy (raw, &capture_buffer[capture_read_ptr & (CAPTURE_BUF_BYTES - 1u)], 8u);
        capture_read_ptr += 8u;
        CapturePairToPcm (raw, dst + i * pair_bytes, lr_swap_cap);
    }
    if (pairs_do < npairs) {
        uint8_t last[8] = {0};
        if (pairs_do) memcpy (last, dst + (pairs_do - 1u) * pair_bytes, pair_bytes);
        for (i = pairs_do; i < npairs; i++) memcpy (dst + i * pair_bytes, last, pair_bytes);
        i2s_cap_fill_pairs += npairs - pairs_do;
    }
    i2s_cap_read_abs = capture_read_ptr;
    return npairs * pair_bytes;
}

void I2S_SetPlayBits (uint32_t bits) {
    i2s_dbg_len_ok = 0u;
    i2s_dbg_len_bad = 0u;
    play_bits = bits;
    /* 位深换了：主机还在按旧位深发包，先立标志让 I2S_PlayFill() 丢弃残包 */
    play_fmt_settle = 1u;
}

/* 采样率/位深的变更登记时立刻调用（不等主循环落地）。
 * 速率是在主循环里才真正改的，这中间主机会按新格式发包，而 i2s_rate 还是旧的，
 * 按旧参数解析这些包写进环就是切换时那段锯齿声。提前立标志把它们全丢掉。 */
void I2S_PlayFmtSuspend (void) {
    play_format_pending = 1u;
    play_fmt_settle = 1u;
    play_fmt_tries = 0u;
}

void I2S_PlayFmtCommit (void) {
    play_format_pending = 0u;
}

uint8_t I2S_PlayFmtPending (void) {
    return play_format_pending;
}

void I2S_SetCapBits (uint32_t bits) { cap_bits = bits; }

uint32_t I2S_GetCapBits (void) { return cap_bits; }

uint32_t I2S_GetCapNominal (void) { return cap_nominal; }

void I2S_CapStart (void) {
    uint32_t wr = CaptureWrAbs(), lag = CaptureLag();
    capture_read_ptr = (capture_history_ready || wr >= lag) ? wr - lag : 0u;
    capture_priming = 1u;
}

/* Switch the PA8 MCLK PWM band with the sample rate.  ARR preload is
 * disabled so the new period applies immediately (called while the stream
 * is stopped, in SET_CUR). */
/* 当前 MCLK 档位：0 = 未设置，1 = 12.288MHz，2 = 24.576MHz */
static uint32_t mclk_band;

static void MclkPwm_SetRate (uint32_t play, uint32_t cap) {
    uint32_t band = (play > 192000U) ? 2u : 1u;

    /* MCLK 只跟播放走 */
    uint32_t freq = play ? play : cap;
    (void)cap;

    if (band == mclk_band) {
        return; /* 档位没变：TIM1 一动都不动 */
    }
    mclk_band = band;

    if (freq > 192000U) {
        /* 384k: 122.88/5 = 24.576MHz, 3/5 = 60% duty */
        TIM_SetCompare1 (TIM1, 3);
        TIM_SetAutoreload (TIM1, 4);
    } else {
        /* <=192k: 122.88/10 = 12.288MHz, 50% duty */
        TIM_SetCompare1 (TIM1, 5);
        TIM_SetAutoreload (TIM1, 9);
    }

    /* 必须补一次软件更新事件，把 CNT 归零 */
    TIM_GenerateEvent (TIM1, TIM_EventSource_Update);
}

#ifdef CAPTURE_MODE_SLAVE
static void CaptureFmtPins_Init (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = CAPTURE_M0_PIN;
    GPIO_Init (CAPTURE_M0_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = CAPTURE_M1_PIN;
    GPIO_Init (CAPTURE_M1_PORT, &GPIO_InitStructure);
}

/* M1:M0 rate code for the external ADC: 48k=00, 96k=01, 192k=10, 384k=11 */
static void CaptureFmtPins_Update (uint32_t freq) {
    uint32_t code;

    switch (freq) {
    case 96000U:
        code = 1;
        break;
    case 192000U:
        code = 2;
        break;
    case 384000U:
        code = 3;
        break;
    default:
        code = 0; /* 48kHz */
        break;
    }
    GPIO_WriteBit (CAPTURE_M0_PORT, CAPTURE_M0_PIN,
                   (code & 1u) ? Bit_SET : Bit_RESET);
    GPIO_WriteBit (CAPTURE_M1_PORT, CAPTURE_M1_PIN,
                   (code & 2u) ? Bit_SET : Bit_RESET);
}
#else
#define CaptureFmtPins_Init() ((void)0)
#define CaptureFmtPins_Update(freq) ((void)0)
#endif

uint8_t I2S_SetSampleRate (uint32_t freq) {
    if (I2S_SetPlayRate (freq) == 0u) {
        return 0u;
    }
    return I2S_SetCapRate (freq);
}

uint8_t I2S_SetPlayRate (uint32_t freq) {
    switch (freq) {
    case 48000u: case 96000u: case 192000u: case 384000u:
        break;
    default:
        return 0u;
    }
    i2s_rate = freq;
    AmpApply (1u);
    SdataParkLow();
    MclkPwm_SetRate (freq, i2s_cap_rate);
    I2S_PlayRingRestart();
    feedback_nominal = FeedbackNominalQ16 (freq);
    I2S_FeedbackReset();
    I2S_PlayMuteSettle();
    play_fmt_settle = 1u;
    return 1u;
}

static void CaptureRingRestart (void) {
    I2S_InitTypeDef cfg = {0};
    uint32_t irq;
    SPI_I2S_DMACmd (SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    DMA_Cmd (DMA1_Channel4, DISABLE);
    I2S_Cmd (SPI2, DISABLE);
    SPI_I2S_DeInit (SPI2);
    WZero (capture_buffer, CAPTURE_BUF_BYTES);
#ifdef CAPTURE_MODE_SLAVE
    cfg.I2S_Mode = I2S_Mode_SlaveRx;
#else
    cfg.I2S_Mode = I2S_Mode_MasterRx;
#endif
    cfg.I2S_Standard = I2S_Standard_Phillips;
    cfg.I2S_DataFormat = I2S_DataFormat_32b;
    cfg.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    cfg.I2S_AudioFreq = i2s_cap_rate;
    cfg.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init (SPI2, &cfg);
    irq = IrqLock();
    capture_wr_base = 0u;
    capture_history_ready = 0u;
    capture_read_ptr = 0u;
    capture_priming = 1u;
    DMA_ClearITPendingBit (DMA1_IT_GL4);
    DMA1_Channel4->CNTR = CAPTURE_BUF_BYTES / 2u;
    DMA1_Channel4->MADDR = (uint32_t)capture_buffer;
    DMA1_Channel4->PADDR = (uint32_t)&SPI2->DATAR;
    DMA_Cmd (DMA1_Channel4, ENABLE);
    SPI_I2S_DMACmd (SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
    I2S_Cmd (SPI2, ENABLE);
    IrqUnlock (irq);
}

uint8_t I2S_SetCapRate (uint32_t freq) {
    i2s_dbg_cap_calls++;
    i2s_dbg_cap_req = freq;
    switch (freq) {
    case 48000u: case 96000u: case 192000u: case 384000u: break;
    default: i2s_dbg_cap_ret = 0u; return 0u;
    }
    if(!ADC_PrepareRate(freq)) {i2s_dbg_cap_ret=0u;return 0u;}
    i2s_cap_rate = freq;
    cap_nominal = FeedbackNominalQ16 (freq);
    CaptureFmtPins_Update (freq);
    CaptureRingRestart();
    if(!ADC_FinishRate()) {i2s_dbg_cap_ret=0u;return 0u;}
    i2s_dbg_cap_ret = 1u;
    return 1u;
}

#define I2S_I2SE_BIT 0x0400u   /* SPIx->I2SCFGR bit10，见 ch32v30x_spi.c 的 I2SCFGR_I2SE_Set */
#define I2S_DMA_EN_BIT 0x0001u /* DMAy_Channelx->CFGR bit0 = EN */

/* 播放侧硬件实况：流应当开着时，这三条任一不成立就是"停了没开回来"。
 * 判据全部来自寄存器回读，不看软件标志 —— 软件标志正是会丢的那一层。 */
static uint8_t StreamHwOk (SPI_TypeDef *spi, DMA_Channel_TypeDef *dma,
                              uint32_t rate, uint32_t mode, uint32_t dir,
                              uint32_t buffer) {
    uint32_t div = SystemCoreClock / (rate * 64u);
    uint32_t prescaler = (div / 2u) | ((div & 1u) << 8);
    uint32_t expected = 0x0800u | 0x0400u | mode | I2S_DataFormat_32b;
    uint32_t dma_expected = DMA_Mode_Circular | DMA_MemoryInc_Enable |
        DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord | dir | 1u;
    if ((spi->I2SCFGR & 0x0FBFu) != expected) return 0u;
#ifndef CAPTURE_MODE_SLAVE
    if ((spi->I2SPR & 0x03FFu) != prescaler) return 0u;
#else
    if (spi == SPI3 && (spi->I2SPR & 0x03FFu) != prescaler) return 0u;
#endif
    if ((dma->CFGR & 0x4FF1u) != dma_expected) return 0u;
    if (dma->MADDR != buffer || dma->PADDR != (uint32_t)&spi->DATAR) return 0u;
    return 1u;
}

uint8_t I2S_PlayHwOk (void) {
    if (play_stopped || play_restarting || sdata_gated) return 0u;
    if ((SPI3->CTLR2 & SPI_I2S_DMAReq_Tx) == 0u) return 0u;
    return StreamHwOk (SPI3, DMA2_Channel2, i2s_rate, I2S_Mode_MasterTx,
                       DMA_DIR_PeripheralDST, (uint32_t)audio_buffer);
}

uint8_t I2S_CapHwOk (void) {
    if ((SPI2->STATR & SPI_I2S_FLAG_OVR) != 0u) return 0u;
    if ((SPI2->CTLR2 & SPI_I2S_DMAReq_Rx) == 0u) return 0u;
#ifdef CAPTURE_MODE_SLAVE
    uint32_t mode = I2S_Mode_SlaveRx;
#else
    uint32_t mode = I2S_Mode_MasterRx;
#endif
    return StreamHwOk (SPI2, DMA1_Channel4, i2s_cap_rate, mode,
                       DMA_DIR_PeripheralSRC, (uint32_t)capture_buffer);
}

void I2S_CapHardStart (void) { CaptureRingRestart(); }

void I2S_CapHealthCheck (void) {
    static uint32_t tick, previous_wr;
    static uint8_t stalled;
    uint32_t wr, irq;
    int32_t available;
    if (tim6_tick == tick) return;
    tick = tim6_tick;
    if (USBHS_DevAltSetting[2] == 0u || USBHS_CaptureActive == 0u) {
        stalled = 0u;
        return;
    }
    wr = CaptureWrAbs();
    if (wr == previous_wr) stalled++; else stalled = 0u;
    previous_wr = wr;
    if (!I2S_CapHwOk() || stalled >= 2u) {
        i2s_cap_guard_count++;
        i2s_cap_guard_reason = stalled >= 2u ? 0x10u : 0x08u;
        USB_RequestCapRecovery();
        stalled = 0u;
        return;
    }
    irq = IrqLock();
    wr = CaptureWrAbs();
    available = (int32_t)(wr - capture_read_ptr);
    if ((capture_read_ptr & 7u) || available < 0 ||
        (uint32_t)available >= CAPTURE_BUF_BYTES - 8u) CaptureRebase (wr, 0x20u);
    IrqUnlock (irq);
}

uint8_t I2S_ApplyPlayFormat (uint32_t rate, uint32_t bits) {
    uint8_t did = 0u;

    if (bits != play_bits) {
        I2S_SetPlayBits (bits); /* 只改"怎么解释字"，不动时钟 */
        did = 1u;
    }
    if (rate != i2s_rate) {
        if (I2S_SetPlayRate (rate) == 0u) {
            return 2u; /* 不在白名单：保持原状，下一轮还会看见差 */
        }
        did = 1u;
    } else if (did != 0u) {
        /* 只换位深：字节槽布局不变，但环里是旧位深解析出来的样本，必须清环
         * 重开。I2S_SetPlayRate() 自己会重启，所以只在没走那条路时补一次。 */
        I2S_PlayRingRestart();
        I2S_PlayMuteSettle();
    }

    if (did == 0u) {
        return 0u;
    }
    return (I2S_PlayHwOk() != 0u) ? 1u : 2u;
}

uint8_t I2S_ApplyCapFormat (uint32_t rate, uint32_t bits) {
    uint8_t did = 0u;
    if(!ADC_FinishRate()) return 2u;

    if (bits != cap_bits) {
        I2S_SetCapBits (bits);
        did = 1u;
    }
    if (rate != i2s_cap_rate) {
        if (I2S_SetCapRate (rate) == 0u) {
            return 2u;
        }
        did = 1u;
    }
    if (did == 0u) {
        return 0u;
    }
    I2S_CapStart(); /* 重建后把读指针对齐到 DMA 当前写位置 */
    return (I2S_CapHwOk() != 0u) ? 1u : 2u;
}

static void MclkPwm_Init (void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    /* TIM1CLK = PCLK2 = 122.88MHz; PWM = 122.88M / (1 * 10) = 12.288MHz
     * with 50% duty (ARR=9, CCR1=5) */
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_Period = 9;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit (TIM1, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 5;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init (TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs (TIM1, ENABLE); /* advanced timer MOE */
    TIM_OC1PreloadConfig (TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig (TIM1, DISABLE);
    TIM_Cmd (TIM1, ENABLE);
}

void I2S_Setup (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};

    RCC_I2S2CLKConfig (RCC_I2S2CLKSource_SYSCLK); /* 122.88MHz */
    RCC_I2S3CLKConfig (RCC_I2S3CLKSource_SYSCLK);
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_SPI2 | RCC_APB1Periph_SPI3, ENABLE);
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                                RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO |
                                RCC_APB2Periph_TIM1 | RCC_APB2Periph_TIM8,
                            ENABLE);
    RCC_AHBPeriphClockCmd (RCC_AHBPeriph_DMA1 | RCC_AHBPeriph_DMA2, ENABLE);

    /* SPI3/I2S3 -> PA4 (WS), PC10 (CK), PC12 (SD) */
    GPIO_PinRemapConfig (GPIO_Remap_SPI3, ENABLE);

    SPI_I2S_DeInit (SPI3);
    SPI_I2S_DeInit (SPI2);

    /* I2S3 playback: PC10 CK, PC12 SD, PA4 WS */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init (GPIOA, &GPIO_InitStructure);

    /* I2S2 capture: PB15 SD input in both modes.  WS/CK are outputs in
     * master mode and inputs when CAPTURE_MODE_SLAVE is defined. */
#ifdef CAPTURE_MODE_SLAVE
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init (GPIOB, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init (GPIOB, &GPIO_InitStructure);
#endif
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init (GPIOB, &GPIO_InitStructure);

    /* PA8 = TIM1_CH1 -> MCLK (12.288 / 24.576MHz band) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init (GPIOA, &GPIO_InitStructure);

    /* HAMP_MUTE (PA7) is initialised by GPIO_Setup(); mute it now so
     * the amplifier stays quiet until audio really starts. */
    AmpApply (1u);

    CaptureFmtPins_Init();
    MclkPwm_Init();

    /* TIM8: 自由运行的反馈时基。 */
    TIM_TimeBaseInitStructure.TIM_Prescaler = (I2S_FB_TIM_DIV - 1u);
    TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit (TIM8, &TIM_TimeBaseInitStructure);
    TIM_Cmd (TIM8, ENABLE);

    /* Playback DMA: DMA2 ch2 memory -> SPI3 TX, circular forever */
    DMA_DeInit (DMA2_Channel2);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&SPI3->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)audio_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = PLAY_RING_HALF;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_ITConfig (DMA2_Channel2, DMA_IT_TC | DMA_IT_HT, ENABLE);
    {
        NVIC_InitTypeDef nvic = {0};
        nvic.NVIC_IRQChannel = DMA2_Channel2_IRQn;
        nvic.NVIC_IRQChannelPreemptionPriority = 1;
        nvic.NVIC_IRQChannelSubPriority = 0;
        nvic.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init (&nvic);
    }
    DMA_Init (DMA2_Channel2, &DMA_InitStructure);
    SPI_I2S_DMACmd (SPI3, SPI_I2S_DMAReq_Tx, ENABLE);

    /* Capture DMA: DMA1 ch4 SPI2 RX -> ring buffer, circular */
    DMA_DeInit (DMA1_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&SPI2->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)capture_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = CAPTURE_BUF_BYTES / 2;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_Init (DMA1_Channel4, &DMA_InitStructure);
    DMA_ITConfig (DMA1_Channel4, DMA_IT_TC, ENABLE);
    {
        NVIC_InitTypeDef nvic = {0};
        nvic.NVIC_IRQChannel = DMA1_Channel4_IRQn;
        nvic.NVIC_IRQChannelPreemptionPriority = 1;
        nvic.NVIC_IRQChannelSubPriority = 1;
        nvic.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init (&nvic);
    }
    DMA_Cmd (DMA1_Channel4, ENABLE);
    SPI_I2S_DMACmd (SPI2, SPI_I2S_DMAReq_Rx, ENABLE);

    /* Default: 48kHz on both interfaces */
    I2S_SetSampleRate (48000);
#if !USB_PLAY_ENABLE
    I2S_PlayStop();
#endif
#if !USB_CAPTURE_ENABLE
    SPI_I2S_DMACmd(SPI2,SPI_I2S_DMAReq_Rx,DISABLE);
    DMA_Cmd(DMA1_Channel4,DISABLE);I2S_Cmd(SPI2,DISABLE);
#endif

    /* Stream is armed - release the amplifier. */
    AMP_SetMute (USB_PLAY_ENABLE ? Bit_SET : Bit_RESET);
}

void DMA2_Channel2_IRQHandler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));

void DMA2_Channel2_IRQHandler (void) {
    uint32_t irq = IrqLock();
    if (DMA_GetITStatus (DMA2_IT_HT2) != RESET) {
        DMA_ClearITPendingBit (DMA2_IT_HT2);
        i2s_dbg_dma_ht++;
    }
    if (DMA_GetITStatus (DMA2_IT_TC2) != RESET) {
        DMA_ClearITPendingBit (DMA2_IT_TC2);
        play_rd_base += PLAY_RING_BYTES;
        i2s_dbg_dma_tc++;
    }
    IrqUnlock (irq);
}

void DMA1_Channel4_IRQHandler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));
void DMA1_Channel4_IRQHandler (void) {
    uint32_t irq = IrqLock();
    if (DMA_GetITStatus (DMA1_IT_TC4) != RESET) {
        DMA_ClearITPendingBit (DMA1_IT_TC4);
        capture_wr_base += CAPTURE_BUF_BYTES;
        capture_history_ready = 1u;
        i2s_cap_dma_tc++;
    }
    IrqUnlock (irq);
}

uint32_t I2S_GetPlayBits (void) { return play_bits; }

uint32_t I2S_PlayFree (void) { return PlayRingFree(); }

volatile uint32_t i2s_guard_flush;

void I2S_PlayGuard (void) {
    static uint32_t last_tick;
    static uint32_t hi_cnt;
    uint32_t lvl;

    if (tim6_tick == last_tick) {
        return;
    }
    last_tick = tim6_tick;

    if (play_stopped || play_restarting || play_format_pending ||
        USBHS_DevAltSetting[1] == 0u) return;

    lvl = PlaySyncLevel();

    if (lvl > PLAY_RING_BYTES - PLAY_RING_GUARD * 2u) {
        if (++hi_cnt >= 3u) {
            hi_cnt = 0u;
            i2s_guard_flush++;
        }
    } else {
        hi_cnt = 0u;
    }
}

void I2S_StreamWatch (void) {
    static uint32_t last_pkts;
    static uint32_t last_tick;
    static uint8_t dead;
    uint32_t now = tim6_tick;

    if (play_stopped != 0u) {
        last_pkts = i2s_pkt_in;
        last_tick = now;
        dead = 0u;
        return; /* 流是主机主动关的，不是掉线 */
    }

    /* 用 i2s_pkt_in（收到的包）而不是 i2s_dbg_pkts（被接受的包）：
     * 环满丢包时主机其实还在发，不能当成掉线。 */
    if (i2s_pkt_in != last_pkts) {
        last_pkts = i2s_pkt_in;
        last_tick = now;
        dead = 0u;
        return;
    }
    if (!dead && (uint32_t)(now - last_tick) >= 5u) { /* 5 x 20ms = 100ms */
        dead = 1u;
        AmpApply (1u);                                /* 功放静音 */
        USB_RequestPlayRecovery();                   /* Serialized with format changes. */
    }
}

void I2S_DbgReset (void) {
    i2s_dbg_pkts = 0u;
    i2s_dbg_frames = 0u;
    i2s_dbg_lastlen = 0u;
    

    i2s_dbg_peak = 0u;
    i2s_dbg_nz = 0u;
    i2s_dbg_lvl_max = 0u;
    i2s_dbg_lvl_min = PLAY_RING_BYTES;
    i2s_dbg_zlp = 0u;
    i2s_dbg_dma_tc = 0u;
    i2s_dbg_dma_ht = 0u;
    i2s_dbg_len_ok = 0u;
    i2s_dbg_len_bad = 0u;
    i2s_play_drop = 0u;
    i2s_plc_count = 0u;
    i2s_play_lap = 0u;
    i2s_pkt_in = 0u;
    i2s_gap_miss = 0u;
    i2s_cap_under = 0u;
    i2s_cap_over = 0u;
    i2s_heal_cnt = 0u;
    i2s_heal_reason = 0u;
}

uint32_t I2S_GetRate (void) {
    return i2s_rate;
}

uint32_t I2S_GetCapRate (void) {
    return i2s_cap_rate;
}
