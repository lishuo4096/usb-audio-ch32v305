#ifndef __CH32V30X_I2S_DEVICE_H__
#define __CH32V30X_I2S_DEVICE_H__

#include "debug.h"
#include "settings.h"   

void I2S_Setup(void);

/* Reconfigure playback (I2S3) and capture (I2S2) for a new sample rate,
 * reset DMA/feedback/capture accumulators. Returns 1 on success. */
uint8_t I2S_SetSampleRate(uint32_t freq);
uint32_t I2S_GetRate (void);
uint32_t I2S_GetCapRate (void);
uint8_t  I2S_SetPlayRate (uint32_t freq);
uint8_t  I2S_SetCapRate  (uint32_t freq);
void     I2S_SetUsbFrameHz (uint32_t hz);
uint32_t I2S_PlayFree (void);
void     I2S_StreamWatch (void);
void     I2S_PlayGuard (void);
extern volatile uint32_t i2s_guard_flush;
extern volatile uint32_t i2s_play_lap;   /* DMA 追上写指针并自愈的累计次数 */
uint32_t I2S_GetPlayBits (void);
uint32_t I2S_GetCapBits (void);
uint32_t I2S_GetCapNominal (void);
/* 录音换频诊断：calls=调用次数，req=最后一次请求的速率，
 * ret=结果（0 不在白名单 / 1 已重建 / 2 同速率早退） */
extern volatile uint32_t i2s_dbg_cap_calls;
extern volatile uint32_t i2s_dbg_cap_req;
extern volatile uint32_t i2s_dbg_cap_ret;


/* 播放侧健康检查（主循环每次调用，内部按 20ms 限速）。
 * 三查：帧对齐（写指针与环基址必须 8 的倍数）、水位、收包但不落环。
 * 任一异常 → 整段重启（停 I2S3 + 清环 + I2S_Init + 重装 DMA）。
 * 返回 0 = 正常；非 0 = 本次触发重启，位图：bit0 对齐 / bit1 水位 / bit2 收包不落环 */
uint8_t I2S_PlayHealthCheck (void);
extern volatile uint32_t i2s_heal_cnt;
extern volatile uint32_t i2s_heal_reason;

/* 等时 OUT 丢包补偿：SOF 里发现"距上一包超过 1 个微帧"就调它。
 * 参数 n = 丢了几包（内部夹到 8）。补的是 PLC 衰减重放，不是静音。 */
void I2S_PlayLostPackets (uint32_t n);
extern volatile uint32_t i2s_gap_miss;   /* 检出的缺包数；无历史/环满时不一定实际注入 PLC */
extern volatile uint32_t i2s_gap_max;    /* 最长缺口：HS 服务周期 125us，FS 1ms */


/* 运行期左右声道交换（判/修 ES9038 与 ADC 两侧的 L/R 反）。
 * 0 = 正常（L 在第一个槽），1 = 交换。 */
void I2S_SetLrSwap (uint8_t play_swap, uint8_t cap_swap);
void I2S_GetLrSwap (uint8_t *play_swap, uint8_t *cap_swap);

/* 录音读指针伺服计数（判"录音白噪声/斩波"用）
 *   cap_under = 本包可用样本不够、被补零的次数
 *   cap_over  = 可用量远超目标水位（读指针落后太多）的次数 */
extern volatile uint32_t i2s_cap_under;
extern volatile uint32_t i2s_cap_over;

/* 录音短包统计（判"125us 毛刺"用）：
 *   fill_pairs = 用"保持上一对"补出来的对数
 *   short_pkts = 发生补对的包数
 *   busy       = SOF 时发现上一包还没被 SIE 取走、于是跳过重填的次数 */
extern volatile uint32_t i2s_cap_fill_pairs;
extern volatile uint32_t i2s_cap_short_pkts;
extern volatile uint32_t i2s_cap_busy;

/* 反馈时基 TIM8 的分频系数（i2s_app.c 里 PSC = 这个值 - 1）。
 * usbhs_device.c 算 ticks_per_service 时要用，保持两边一致。 */
#define I2S_FB_TIM_DIV 128u

/* 当前生效的采样率 / 播放位深 */
extern volatile uint32_t i2s_rate;      /* 播放 */
extern volatile uint32_t i2s_cap_rate;  /* 录音 */

/* USB 帧频：HS 8000 微帧/秒，FS 1000 帧/秒。usbhs_device.c 在 SOF 里
 * 按当前速度更新，PLC 补包量按 8000/i2s_usb_frame_hz 折算。 */
extern volatile uint32_t i2s_usb_frame_hz;

/* 诊断计数器（串口 'd' 读一次就清零） */
extern volatile uint32_t i2s_dbg_pkts;
extern volatile uint32_t i2s_dbg_frames;
extern volatile uint32_t i2s_dbg_lastlen;
extern volatile uint32_t i2s_dbg_lvl_max;
extern volatile uint32_t i2s_dbg_lvl_min;
extern volatile uint32_t i2s_dbg_zlp;
extern volatile uint32_t i2s_dbg_dma_tc;
extern volatile uint32_t i2s_dbg_dma_ht;
extern volatile uint32_t i2s_dbg_exp_len;
extern volatile uint32_t i2s_dbg_len_ok;
extern volatile uint32_t i2s_dbg_len_bad;
void I2S_DbgReset (void);

/* Playback: repack USB PCM into I2S slot layout and push it into the
 * DMA2_CH2 circular ring.  Never stops the DMA, so BCLK never gaps. */
void I2S_PlayFill(uint8_t *src, uint32_t num);

/* Clear the playback ring and re-arm DMA2_CH2 in circular mode.  Only
 * called on a sample-rate change. */
void I2S_PlayRingRestart(void);
void I2S_PlayStop(void);
uint8_t I2S_PlayIsStopped(void);
uint32_t I2S_ZdetHold(void);
extern volatile uint32_t i2s_dbg_peak;


extern volatile uint32_t i2s_dbg_nz;
extern volatile uint8_t  i2s_dbg_head[4];

/* Packets dropped because the ring was full (diagnostic counter). */
extern volatile uint32_t i2s_play_drop;

/* 交给播放环的所有包（含零长包与因环满被丢的）—— 用来判"主机还在不在发包"，
 * 也就是 I2S_StreamWatch() 判掉线的依据。 */
extern volatile uint32_t i2s_pkt_in;

/* Packet-loss-concealment events (one per concealed USB packet). */
extern volatile uint32_t i2s_plc_count;

/* Current amplifier mute state (1 = muted). */
extern volatile uint8_t amp_muted;

uint32_t I2S_CapRead(uint8_t *dst, uint32_t scale, uint32_t max_bytes);

void I2S_SetPlayBits(uint32_t bits);
void I2S_SetCapBits(uint32_t bits);
/* 采样率/位深变更登记时立刻调用：从现在起丢弃旧格式残包（防切换锯齿声）。 */
void I2S_PlayFmtSuspend(void);
void I2S_PlayFmtCommit(void);
uint8_t I2S_PlayFmtPending(void);
void I2S_CapHealthCheck(void);
extern volatile uint32_t i2s_play_stop_timeout, i2s_play_prime_fail;
extern volatile uint32_t i2s_cap_guard_count, i2s_cap_guard_reason;
extern volatile uint32_t i2s_cap_read_abs, i2s_cap_write_abs;
extern volatile uint32_t i2s_cap_dma_tc, i2s_cap_prime_pkts;

/* ---- 格式落地（主循环，差值驱动）------------------------------------
 * 0 = 已经一致（没动）1 = 本次重建且校验通过 2 = 没做成 / 校验不过 */
uint8_t I2S_ApplyPlayFormat(uint32_t rate, uint32_t bits);
uint8_t I2S_ApplyCapFormat (uint32_t rate, uint32_t bits);
/* 硬件实况自检（寄存器回读，不看软件标志）：1 = 流确实在跑 */
uint8_t I2S_PlayHwOk(void);
uint8_t I2S_CapHwOk(void);
/* 录音侧兜底重启：只把 SPI2 / DMA1_CH4 开回来，不动时钟 */
void    I2S_CapHardStart(void);

/* Host driven mute (SET_INTERFACE alt 0).  unmute = Bit_SET releases the
 * amplifier, Bit_RESET asserts mute and disables zero detection until the
 * next unmute. */
void AMP_SetMute(BitAction unmute);
/* 换频/换位深：静音并推迟解静音，等 DAC 重新锁住 DPLL。 */
void I2S_PlayMuteSettle(void);

/* Align the capture read pointer to the current DMA write position. */
void I2S_CapStart(void);

/* 反馈测量：TIM8 自由计数器读取值 + 每拍 tick 数（tick 参数当前不用）。 */
void I2S_FeedbackReset(void);
uint32_t I2S_FeedbackUpdate(uint16_t tick_now, uint32_t ticks_per_service);
uint32_t I2S_FeedbackGet(void);

/* 水位伺服现场（串口 play 命令的 [fb ] 行）：
 *   trim   = 当前反馈偏差（Q16.16 样本/微帧，正 = 让主机多送）
 *   level  = 上一次采样到的环占用（字节）
 *   target = 目标占用（字节，= PLAY_TARGET_US 对应的样本数 x 8） */
extern volatile int32_t  i2s_fb_trim;
extern volatile uint32_t i2s_fb_level;
extern volatile uint32_t i2s_fb_target;


void I2S_SetAutoMute(uint8_t enabled);
uint8_t I2S_GetAutoMute(void);
#endif


