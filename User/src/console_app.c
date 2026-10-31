#include "console_app.h"
#include "main.h"
#include "i2s_app.h"
#include "hid_app.h"
#include "ch32_bootloader.h"
#include "usbhs_device.h"
#include "usb_desc.h"
#include "dac_adc_app.h"
#include "es9038q2m_hal.h"
#include "tlv320adc5120.h"
#include "board.h"
#include "key_driver.h"
#include "volume_app.h"
#include "fu_store.h"
#include <string.h>

#define CON_LINE_MAX 64

static char    s_line[CON_LINE_MAX];
static uint8_t s_len;
static uint8_t s_overflow;
static uint8_t s_banner_done;
/* "等回车"自诊断：最后一次收到字节的 tim6 tick + 本行是否已经提示过 */
static uint32_t s_last_rx_tick;
static uint8_t  s_hint_done;

/*--------------------------------------------------------------------
 *  小的格式化辅助
 *--------------------------------------------------------------------*/
/* 左对齐打印一个名字，列宽 8 —— 让整屏读数能对齐着看 */
static const char *pad8 (const char *s) {
    static char buf[9];
    uint8_t i = 0u;

    while ((s[i] != '\0') && (i < 8u)) {
        buf[i] = s[i];
        i++;
    }
    while (i < 8u) {
        buf[i++] = ' ';
    }
    buf[8] = '\0';
    return buf;
}

static uint8_t con_match (const char *a, const char *b) {
    while ((*a != '\0') && (*b != '\0')) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0u;
        }
    }
    return (uint8_t)((*a == '\0') && (*b == '\0'));
}

/* "on/off/1/0" -> 1/0；不认识的返回 0xFF */
static uint8_t con_parse_onoff (const char *s) {
    if ((con_match (s, "on") != 0u) || (con_match (s, "1") != 0u) ||
        (con_match (s, "true") != 0u) || (con_match (s, "mute") != 0u)) {
        return 1u;
    }
    if ((con_match (s, "off") != 0u) || (con_match (s, "0") != 0u) ||
        (con_match (s, "false") != 0u) || (con_match (s, "unmute") != 0u)) {
        return 0u;
    }
    return 0xFFu;
}

/* 十进制整数（支持 0x 前缀）*/


/* "-6.5" / "+12" / "0" / "-inf" -> 1/256 dB（int16） */
static uint8_t con_parse_db (const char *s, int16_t *out) {
    int32_t sign = 1;
    int32_t ip = 0;
    int32_t fp = 0;
    int32_t scale = 1;
    uint8_t any = 0u;

    if ((con_match (s, "-inf") != 0u) || (con_match (s, "inf") != 0u)) {
        *out = VOL_M256_NEG_INF;
        return 1u;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while ((*s >= '0') && (*s <= '9')) {
        ip = ip * 10 + (*s - '0');
        if (ip > 200) {
            return 0u;
        }
        s++;
        any = 1u;
    }
    if (*s == '.') {
        s++;
        while ((*s >= '0') && (*s <= '9')) {
            if (scale < 1000) {
                fp = fp * 10 + (*s - '0');
                scale *= 10;
            }
            s++;
            any = 1u;
        }
    }
    if (any == 0u) {
        return 0u;
    }
    *out = (int16_t)(sign * (ip * 256 + (fp * 256) / scale));
    return 1u;
}

/*--------------------------------------------------------------------
 *  各命令的实现
 *--------------------------------------------------------------------*/
static void print_volume (void) {
    char bl[10], br[10], ba[10];

    Volume_FormatDb (bl, Volume_GetDac (1));
    Volume_FormatDb (br, Volume_GetDac (2));
    Volume_FormatDb (ba, Volume_GetAdc (0));

    printf ("  dac    L=%s dB  R=%s dB  mute=%u   (FU2  -> ES9038 Reg15/16 + Reg7[0])\r\n",
            bl, br, (unsigned)Volume_GetDacMute ());
    printf ("  adc    %s dB  mute=%u          (FU8  -> ADC5120 PGA + digital vol)\r\n",
            ba, (unsigned)Volume_GetAdcMute ());
}

static void print_flash (void) {
    /* base/seq/writes/slot/dirty：
     *   slot >= 256 表示 4KB 日志已写满，下一次"允许擦除"的保存会整片擦掉重来 */
    printf ("  flash  base=0x%08X seq=%u writes=%u slot=%u dirty=%u\r\n",
            (unsigned)FU_FLASH_BASE, (unsigned)FuStore_Seq (),
            (unsigned)FuStore_WriteCount (), (unsigned)FuStore_Slot (),
            (unsigned)Volume_IsDirty ());
}

static void cmd_help (int argc, char **argv);

/* Exercise the normal HID press/release path without stopping live audio. */

static void cmd_info (int argc, char **argv) {
    char bl[10], br[10], ba[10];

    (void)argc;
    (void)argv;

    Volume_FormatDb (bl, Volume_GetDac (1));
    Volume_FormatDb (br, Volume_GetDac (2));
    Volume_FormatDb (ba, Volume_GetAdc (0));

    printf ("CH32V305GBU6  USB Audio Bridge   uptime=%u.%us\r\n",
            (unsigned)(tim6_tick / 50u), (unsigned)((tim6_tick % 50u) * 2u));
    printf ("  DAC control curve=%u/%u (host dB logical; DAC attenuation scaled)\r\n",
            (unsigned)DAC_VOLUME_CURVE_NUM, (unsigned)DAC_VOLUME_CURVE_DEN);
    printf ("  reset=0x%08X iwdg=%u i2c_timeouts=%u recover=%u\r\n",(unsigned)system_reset_flags,(unsigned)((system_reset_flags>>29)&1u),(unsigned)i2c_timeout_count,(unsigned)i2c_recovery_count);
    printf ("  %splay=%u Hz/%u bit   ring=%u B  level=%u B  stopped=%u\r\n",
            pad8 ("i2s"), (unsigned)i2s_rate, (unsigned)I2S_GetPlayBits (),
            (unsigned)8192u, (unsigned)I2S_PlayFree (), (unsigned)I2S_PlayIsStopped ());
    printf ("       cap =%u Hz/%u bit   nom=%u (Q16.16 pairs per uframe)\r\n",
            (unsigned)i2s_cap_rate, (unsigned)I2S_GetCapBits (),
            (unsigned)I2S_GetCapNominal ());
    printf ("  %sdac L=%s dB R=%s dB mute=%u   adc=%s dB mute=%u\r\n",
            pad8 ("vol"), bl, br, (unsigned)Volume_GetDacMute (),
            ba, (unsigned)Volume_GetAdcMute ());
    printf ("  %sspeed=%s cfg=%u alt[play=%u cap=%u] intbusy=%u hid=0x%04X\r\n",
            pad8 ("usb"), (USBHS_DevSpeed == USBHS_SPEED_HIGH) ? "HIGH" : "FULL",
            (unsigned)USBHS_DevConfig, (unsigned)USBHS_DevAltSetting[1],
            (unsigned)USBHS_DevAltSetting[2], (unsigned)usb_int_busy,
            (unsigned)HID_GetBits ());
    printf ("  %sinit_st=%u (0=ok) dac_step=%u  fu_set=%u\r\n",
            pad8 ("chip"), (unsigned)adc_init_status, (unsigned)es9038_init_step,
            (unsigned)usb_fu_set_cnt);
    /* 自愈/丢包计数：原来只在 usb / stat 里，info 一眼看不到，放一份在这里 */
    printf ("  %sheal=%u reason=0x%X gap_miss=%u cap_under=%u cap_over=%u\r\n",
            pad8 ("fix"), (unsigned)i2s_heal_cnt, (unsigned)i2s_heal_reason,
            (unsigned)i2s_gap_miss, (unsigned)i2s_cap_under, (unsigned)i2s_cap_over);
    /* DAC 实际音量寄存器（FU 的目标值在 vol 行；这里读的是芯片里真正的值）*/
    if (I2C_Probe (DAC_I2C_ADDR7) == 0) {
        uint8_t r0f = 0u, r10 = 0u;

        (void)ES9038_ReadReg (ES9038_REG_VOLUME1, &r0f);
        (void)ES9038_ReadReg (ES9038_REG_VOLUME2, &r10);
        printf ("  %s0F=%02X 10=%02X  (0x00=0dB, 0x50=-40dB; chip vs FU target)\r\n",
                pad8 ("reg"), (unsigned)r0f, (unsigned)r10);
    }
    print_flash ();
}

static void cmd_stat (int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf ("[play] pkts_in=%u accepted=%u frames=%u drop_ring=%u zlp=%u plc=%u lap=%u\r\n",
            (unsigned)i2s_pkt_in, (unsigned)i2s_dbg_pkts, (unsigned)i2s_dbg_frames,
            (unsigned)i2s_play_drop, (unsigned)i2s_dbg_zlp,
            (unsigned)i2s_plc_count, (unsigned)i2s_play_lap);
    printf ("[play] len_ok=%u len_bad=%u exp=%u last=%u peak=%u nz=%u\r\n",
            (unsigned)i2s_dbg_len_ok, (unsigned)i2s_dbg_len_bad,
            (unsigned)i2s_dbg_exp_len, (unsigned)i2s_dbg_lastlen,
            (unsigned)i2s_dbg_peak, (unsigned)i2s_dbg_nz);
    printf ("[cap ] calls=%u req=%u ret=%u (0=reject 1=rebuild 2=same-rate skip)\r\n",
            (unsigned)i2s_dbg_cap_calls, (unsigned)i2s_dbg_cap_req,
            (unsigned)i2s_dbg_cap_ret);
    printf ("[mute] amp_muted=%u dac_fu=%u adc_fu=%u  uart_tx_drop=%u\r\n",
            (unsigned)amp_muted, (unsigned)Volume_GetDacMute (),
            (unsigned)Volume_GetAdcMute (), (unsigned)uart7_tx_drop);
    printf ("[heal] fmt_cnt=%u fmt_ret=0x%03X heal=%u guard_flush=%u\r\n",
            (unsigned)usb_fmt_cnt, (unsigned)usb_fmt_ret,
            (unsigned)usb_fmt_heal, (unsigned)i2s_guard_flush);
}

static void cmd_play (int argc, char **argv) {
    uint32_t lvl_min = i2s_dbg_lvl_min;
    uint32_t lvl_max = i2s_dbg_lvl_max;

    (void)argc;
    (void)argv;

    printf ("[i2s ] rate=%u bits=%u pkts=%u frames=%u zlp=%u drop=%u plc=%u\r\n",
            (unsigned)i2s_rate, (unsigned)I2S_GetPlayBits (),
            (unsigned)i2s_dbg_pkts, (unsigned)i2s_dbg_frames,
            (unsigned)i2s_dbg_zlp, (unsigned)i2s_play_drop,
            (unsigned)i2s_plc_count);
    printf ("[ring] last=%u lvl_min=%u lvl_max=%u free=%u cntr=%u lap=%u\r\n",
            (unsigned)i2s_dbg_lastlen, (unsigned)lvl_min, (unsigned)lvl_max,
            (unsigned)I2S_PlayFree (), (unsigned)DMA2_Channel2->CNTR,
            (unsigned)i2s_play_lap);
    printf ("[pkt ] bits=%u rate=%u exp=%u ok=%u BAD=%u last=%u\r\n",
            (unsigned)I2S_GetPlayBits (), (unsigned)I2S_GetRate (),
            (unsigned)i2s_dbg_exp_len, (unsigned)i2s_dbg_len_ok,
            (unsigned)i2s_dbg_len_bad, (unsigned)i2s_dbg_lastlen);
    printf ("[out ] evt=%u notog=%u  (EP1 OUT completions; notog should stay 0)\r\n",
            (unsigned)usb_ep1_out_evt, (unsigned)usb_ep1_out_notog);
    printf ("[dma ] tc=%u ht=%u tick=%u (1 tc = 1024 frames)\r\n",
            (unsigned)i2s_dbg_dma_tc, (unsigned)i2s_dbg_dma_ht, (unsigned)tim6_tick);
    printf ("[st  ] stop=%u amp=%u hold=%u alt1=%u sd_gate=%u\r\n",
            (unsigned)I2S_PlayIsStopped (), (unsigned)amp_muted,
            (unsigned)I2S_ZdetHold (), (unsigned)USBHS_DevAltSetting[1],
            (unsigned)GPIO_ReadOutputDataBit (GPIOC, GPIO_Pin_12));
    printf ("[st  ] pk=%u nz=%u head=%02X%02X%02X%02X\r\n",
            (unsigned)i2s_dbg_peak, (unsigned)i2s_dbg_nz,
            (unsigned)i2s_dbg_head[0], (unsigned)i2s_dbg_head[1],
            (unsigned)i2s_dbg_head[2], (unsigned)i2s_dbg_head[3]);
    /* 自愈与丢包补偿：heal 涨 = 三查自愈触发过；gap=丢包补偿；lap=欠载自愈；
     * gap_max = 主机最长断供间隔（微帧 x 0.125ms）—— 目标水位必须大于它。 */
    printf ("[heal] cnt=%u reason=0x%X gap_miss=%u gap_max=%u(x0.125ms) lap=%u guard=%u\r\n",
            (unsigned)i2s_heal_cnt, (unsigned)i2s_heal_reason,
            (unsigned)i2s_gap_miss, (unsigned)i2s_gap_max, (unsigned)i2s_play_lap,
            (unsigned)i2s_guard_flush);
    printf ("[sync] pending=%u stop_timeout=%u prime_fail=%u hw_ok=%u\r\n",
            (unsigned)I2S_PlayFmtPending(), (unsigned)i2s_play_stop_timeout,
            (unsigned)i2s_play_prime_fail, (unsigned)I2S_PlayHwOk());
    /* 水位伺服：trim 是发给主机的反馈偏差（Q16.16 样本/微帧）。
     * 调 FEEDBACK_TAU_MS 时看这里：trim 应该在 0 附近小幅抖动，
     * 长期顶在 ±限幅上说明限幅不够或主机侧根本不跟。 */
    printf ("[fb  ] trim=%d level=%u target=%u (Q16.16/uframe; + means ask more)\r\n",
            (int)i2s_fb_trim, (unsigned)i2s_fb_level, (unsigned)i2s_fb_target);
    

    I2S_DbgReset (); /* 保留旧行为：play/d 读一次就清零，方便算增量 */
}

static void cmd_cap (int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf ("[cap] req=%u hw=%u alt=%u act=%u cnt=%u last=%u bits=%u nom=%u\r\n",
            (unsigned)USB_GetCapRate (), (unsigned)I2S_GetCapRate (),
            (unsigned)USBHS_DevAltSetting[2], (unsigned)USBHS_CaptureActive,
            (unsigned)USBHS_CapSetCurCnt, (unsigned)USBHS_CapSetCurLast,
            (unsigned)I2S_GetCapBits (), (unsigned)I2S_GetCapNominal ());
    printf ("[cap] ep3 ctrl=0x%02X len=%u max=%u  sc=%u\r\n",
            (unsigned)USBHSD->UEP3_TX_CTRL, (unsigned)USBHSD->UEP3_TX_LEN,
            (unsigned)USBHSD->UEP3_MAX_LEN, (unsigned)USBHS_ScCnt);
    printf ("[cap] setup n=%u w0=0x%08X w1=0x%08X\r\n",
            (unsigned)usb_setup_cnt, (unsigned)usb_setup_w0, (unsigned)usb_setup_w1);
    printf ("[cap] rates n=%u %u %u %u %u\r\n", (unsigned)USB_RateCount,
            (unsigned)USB_RateList[0], (unsigned)USB_RateList[1],
            (unsigned)USB_RateList[2], (unsigned)USB_RateList[3]);
    printf ("[cap] setcap calls=%u req=%u ret=%u\r\n",
            (unsigned)i2s_dbg_cap_calls, (unsigned)i2s_dbg_cap_req,
            (unsigned)i2s_dbg_cap_ret);
    /* 读指针伺服：under 涨 = 每包可用样本不够（现在改用"保持上一对"填充，不再是补零）；
     * over 涨 = 读指针落后太多、被追平。两者长期为 0 才算录音链路健康。 */
    printf ("[cap] servo under=%u over=%u  (both should stay near 0)\r\n",
            (unsigned)i2s_cap_under, (unsigned)i2s_cap_over);
    printf ("[cap] guard=%u reason=0x%X rd=%u wr=%u tc=%u prime=%u hw_ok=%u\r\n",
            (unsigned)i2s_cap_guard_count, (unsigned)i2s_cap_guard_reason,
            (unsigned)i2s_cap_read_abs, (unsigned)i2s_cap_write_abs,
            (unsigned)i2s_cap_dma_tc, (unsigned)i2s_cap_prime_pkts,
            (unsigned)I2S_CapHwOk());
    /* busy counts endpoint-not-ready skips; it does not prove SIE ownership.
     * short/fill count capture sample shortages after initial priming. */
    printf ("[cap] not_ready=%u short=%u fill=%u\r\n",
            (unsigned)i2s_cap_busy, (unsigned)i2s_cap_short_pkts,
            (unsigned)i2s_cap_fill_pairs);
}

static void cmd_usb (int argc, char **argv) {
    extern volatile uint32_t usb_out_delta[4];
    extern volatile uint32_t usb_iso_wait_timeout,usb_cap_in_empty;
    uint64_t avail = (uint64_t)usb_ep4_sof * (USBHS_DevSpeed==USBHS_SPEED_FULL ? 960u : 120u);
    uint32_t pct = 0u;
    uint32_t avg = 0u;

    (void)argc;
    (void)argv;

    printf("[outdelta] 0=%u 1=%u 2=%u other=%u\r\n",
        (unsigned)usb_out_delta[0],(unsigned)usb_out_delta[1],(unsigned)usb_out_delta[2],(unsigned)usb_out_delta[3]);
    printf("[iso] wait_timeout=%u cap_no_slot=%u\r\n",
        (unsigned)usb_iso_wait_timeout,(unsigned)usb_cap_in_empty);

    if (avail != 0u) {
        pct = (uint32_t)(((uint64_t)usb_isr_ticks * 100u) / avail);
    }
    if (usb_isr_cnt != 0u) {
        avg = usb_isr_ticks / usb_isr_cnt;
    }

    printf ("[ep4 ] ctrl=0x%02X len=%u max=%u sof=%u poll=%u pend=%u ok=%u to=%u hid=0x%04X\r\n",
            (unsigned)USBHSD->UEP4_TX_CTRL, (unsigned)USBHSD->UEP4_TX_LEN,
            (unsigned)USBHSD->UEP4_MAX_LEN, (unsigned)usb_ep4_sof,
            (unsigned)usb_ep4_poll, (unsigned)usb_ep4_pend,
            (unsigned)usb_ep4_ok, (unsigned)usb_ep4_to,
            (unsigned)HID_GetBits ());
    printf ("[isr ] cnt=%u ticks=%u load=%u%% avg=%utick busy=%u\r\n",
            (unsigned)usb_isr_cnt, (unsigned)usb_isr_ticks, (unsigned)pct,
            (unsigned)avg, (unsigned)usb_int_busy);
    printf ("[dr  ] d1=%u d2=%u d3=%u ovf=%u\r\n",
            (unsigned)usb_isr_drain[0], (unsigned)usb_isr_drain[1],
            (unsigned)usb_isr_drain[2], (unsigned)usb_isr_drain_ovf);
    printf ("[ep0 ] out=%u last=%08X data=%08X skip=%u\r\n",(unsigned)usb_ep0_out_count,(unsigned)usb_ep0_out_last,(unsigned)usb_ep0_out_data,(unsigned)usb_ep0_out_skip);
    printf ("[ctrl] type=%02X req=%02X value=%04X index=%04X len=%u tx=%02X rx=%02X\r\n",USBHS_SetupReqType,USBHS_SetupReqCode,USBHS_SetupReqValue,USBHS_SetupReqIndex,USBHS_SetupReqLen,USBHSD->UEP0_TX_CTRL,USBHSD->UEP0_RX_CTRL);
    printf ("[fmt ] cnt=%u ret=0x%03X heal=%u  (ret: 1=ok 2=verify fail, bit8=play)\r\n",
            (unsigned)usb_fmt_cnt, (unsigned)usb_fmt_ret, (unsigned)usb_fmt_heal);
    printf ("[queue] rx_drop=%u stale=%u peak=%u cap_under=%u cap_sent=%u fifo=%u isr_max=%utick xfer_max=%utick\r\n",
            (unsigned)usb_rx_queue_drop, (unsigned)usb_rx_queue_stale, (unsigned)usb_rx_queue_peak,
            (unsigned)usb_cap_queue_under, (unsigned)usb_cap_packets,
            (unsigned)usb_fifo_overflow, (unsigned)usb_isr_max_ticks, (unsigned)usb_xfer_max_ticks);
    printf ("[fu  ] set_cnt=%u last: ent=%u sel=%u ch=%u val=%d\r\n",
            (unsigned)usb_fu_set_cnt, (unsigned)usb_fu_set_ent,
            (unsigned)usb_fu_set_sel, (unsigned)usb_fu_set_ch,
            (int)usb_fu_set_val);
}

static void cmd_rate (int argc, char **argv) {
    uint8_t i;

    (void)argc;
    (void)argv;

    printf ("[rate] count=%u  play=%u  cap=%u\r\n", (unsigned)USB_RateCount,
            (unsigned)USB_GetCurrentRate (), (unsigned)USB_GetCapRate ());
    for (i = 0u; i < USB_RateCount && i < 4u; i++) {
        printf ("       [%u] %u Hz%s\r\n", (unsigned)i, (unsigned)USB_RateList[i],
                (USB_RateList[i] == USB_GetCurrentRate ()) ? "  <- play" : "");
    }
    printf ("[alt ] play alts=%u  cap alts=%u  (alt0 = zero bandwidth)\r\n",
            (unsigned)USB_PlayAltCount, (unsigned)USB_CapAltCount);
    for (i = 1u; i < USB_PlayAltCount && i < USB_ALT_MAX; i++) {
        printf ("       play alt%u = %u bit   cap alt%u = %u bit\r\n",
                (unsigned)i, (unsigned)USB_PlayAltDepth[i],
                (unsigned)i, (unsigned)((i < USB_CapAltCount) ? USB_CapAltDepth[i] : 0u));
    }
}

static void cmd_vol (int argc, char **argv) {
    int16_t db;

    if (argc == 1) {
        print_volume ();
        return;
    }
    if (con_match (argv[1], "dac") != 0u) {
        if (argc < 3) {
            char b[10];
            Volume_FormatDb (b, Volume_GetDac (0));
            printf ("dac volume = %s dB (master when L/R equal)\r\n", b);
            return;
        }
        if (con_parse_db (argv[2], &db) == 0u) {
            printf ("usage: vol dac <dB>   e.g. vol dac -6.5\r\n");
            return;
        }
        Volume_SetDac (0u, db);
        printf ("dac volume -> %s dB (hardware applied by main loop)\r\n", argv[2]);
        return;
    }
    if (con_match (argv[1], "adc") != 0u) {
        if (argc < 3) {
            char b[10];
            Volume_FormatDb (b, Volume_GetAdc (0));
            printf ("adc volume = %s dB (PGA + digital stage)\r\n", b);
            return;
        }
        if (con_parse_db (argv[2], &db) == 0u) {
            printf ("usage: vol adc <dB>   e.g. vol adc 12  range %d..%d\r\n",
                    (int)(VOL_ADC_MIN_M256 / 256), (int)(VOL_ADC_MAX_M256 / 256));
            return;
        }
        Volume_SetAdc (0u, db);
        printf ("adc volume -> %s dB\r\n", argv[2]);
        return;
    }
    if (con_parse_db (argv[1], &db) != 0u) { /* vol -6.5 = dac 简写 */
        Volume_SetDac (0u, db);
        printf ("dac volume -> %s dB\r\n", argv[1]);
        return;
    }
    printf ("usage: vol dac|adc [dB]\r\n");
}

static void cmd_automute(int argc,char **argv) {
    if(argc>1) {uint8_t v=con_parse_onoff(argv[1]);if(v>1u){printf("usage: automute on|off\r\n");return;}I2S_SetAutoMute(v);}
    printf("automute=%u (diagnostic override is not saved)\r\n",(unsigned)I2S_GetAutoMute());
}

static void cmd_mute (int argc, char **argv) {
    uint8_t on;

    if (argc == 1) {
        printf ("dac mute=%u  adc mute=%u  (amp_muted=%u)\r\n",
                (unsigned)Volume_GetDacMute (), (unsigned)Volume_GetAdcMute (),
                (unsigned)amp_muted);
        return;
    }
    if (con_match (argv[1], "dac") != 0u) {
        if (argc < 3) {
            on = (uint8_t)((Volume_GetDacMute () != 0u) ? 0u : 1u);
        } else {
            on = con_parse_onoff (argv[2]);
            if (on == 0xFFu) {
                printf ("usage: mute dac on|off\r\n");
                return;
            }
        }
        Volume_SetDacMute (on);
        printf ("dac mute -> %u\r\n", (unsigned)on);
        return;
    }
    if (con_match (argv[1], "adc") != 0u) {
        if (argc < 3) {
            on = (uint8_t)((Volume_GetAdcMute () != 0u) ? 0u : 1u);
        } else {
            on = con_parse_onoff (argv[2]);
            if (on == 0xFFu) {
                printf ("usage: mute adc on|off\r\n");
                return;
            }
        }
        Volume_SetAdcMute (on);
        printf ("adc mute -> %u\r\n", (unsigned)on);
        return;
    }
    on = con_parse_onoff (argv[1]);
    if (on == 0xFFu) {
        printf ("usage: mute dac|adc [on|off]\r\n");
        return;
    }
    Volume_SetDacMute (on);
    printf ("dac mute -> %u\r\n", (unsigned)on);
}

static void cmd_fu (int argc, char **argv) {
    char bl[10], br[10], ba[10];

    (void)argc;
    (void)argv;

    Volume_FormatDb (bl, Volume_GetDac (1));
    Volume_FormatDb (br, Volume_GetDac (2));
    Volume_FormatDb (ba, Volume_GetAdc (0));

    printf ("[FU2] play : L=%s dB R=%s dB mute=%u  (raw 1/256dB: 0x%04X 0x%04X)\r\n",
            bl, br, (unsigned)Volume_GetDacMute (),
            (unsigned)(uint16_t)Volume_GetDac (1),
            (unsigned)(uint16_t)Volume_GetDac (2));
    printf ("       range %d..%d dB, step 0.5dB -> ES9038 Reg15/16 (0x00=0dB,0xFF=-127.5dB)\r\n",
            (int)(VOL_DAC_MIN_M256 / 256), (int)(VOL_DAC_MAX_M256 / 256));
    printf ("[FU8] cap  : %s dB mute=%u  (raw 1/256dB: 0x%04X)\r\n",
            ba, (unsigned)Volume_GetAdcMute (),
            (unsigned)(uint16_t)Volume_GetAdc (0));
    printf ("       range %d..%d dB, step 0.5dB -> ADC5120 PGA + digital vol\r\n",
            (int)(VOL_ADC_MIN_M256 / 256), (int)(VOL_ADC_MAX_M256 / 256));
    printf ("[host] SET_CUR count=%u last ent=%u sel=%u ch=%u val=%d\r\n",
            (unsigned)usb_fu_set_cnt, (unsigned)usb_fu_set_ent,
            (unsigned)usb_fu_set_sel, (unsigned)usb_fu_set_ch,
            (int)usb_fu_set_val);
    print_flash ();
}

static void cmd_save (int argc, char **argv) {
    (void)argc;
    (void)argv;
    if(USBHS_DevAltSetting[1] || USBHS_DevAltSetting[2]) {
        Volume_SettingsChanged();printf("[save] queued until audio is idle\r\n");return;
    }

    /* 手动保存：允许擦除（电源肯定是好的）*/
    Volume_SaveNow (1u);
    if (Volume_IsDirty () == 0u) {
        printf ("[fu] saved, seq=%u slot=%u writes=%u\r\n",
                (unsigned)FuStore_Seq (), (unsigned)FuStore_Slot (),
                (unsigned)FuStore_WriteCount ());
    } else {
        printf ("[fu] save FAILED (program/verify failed; slot not blank and no erase?)\r\n");
    }
}

static void cmd_dac (int argc, char **argv) {
    uint8_t id = 0u;
    uint8_t r00 = 0u, r01 = 0u, r07 = 0u, r0a = 0u, r0c = 0u, r0e = 0u;
    uint8_t r0f = 0u, r10 = 0u, r1b = 0u;
    int ok = (I2C_Probe (DAC_I2C_ADDR7) == 0) ? 1 : 0;

    (void)argc;
    (void)argv;

    if (ok == 0) {
        printf ("[dac] I2C 0x%02X no ACK\r\n", (unsigned)DAC_I2C_ADDR7);
        return;
    }
    (void)ES9038_GetChipId (&id);
    (void)ES9038_ReadReg (ES9038_REG_SYSTEM, &r00);
    (void)ES9038_ReadReg (ES9038_REG_INPUT_SEL, &r01);
    (void)ES9038_ReadReg (0x07u, &r07);
    (void)ES9038_ReadReg (ES9038_REG_MASTER_MODE, &r0a);
    (void)ES9038_ReadReg (ES9038_REG_ASRC_DPLL_BW, &r0c);
    (void)ES9038_ReadReg (0x0Eu, &r0e);
    (void)ES9038_ReadReg (ES9038_REG_VOLUME1, &r0f);
    (void)ES9038_ReadReg (ES9038_REG_VOLUME2, &r10);
    (void)ES9038_ReadReg (ES9038_REG_GENERAL_CFG, &r1b);

    printf ("[dac] i2c=ok  id=0x%02X (want 0x1C)  init_step=%u\r\n",
            (unsigned)((id >> 2) & 0x3Fu), (unsigned)es9038_init_step);
    printf ("      00 System=0x%02X  (clk_gear / soft reset bit0)\r\n", (unsigned)r00);
    printf ("      01 Input =0x%02X  (want 0xC0: 32bit + I2S + serial + autosel off)\r\n",
            (unsigned)r01);
    printf ("      07 Mute  =0x%02X  (bit0=1 means digital mute)\r\n", (unsigned)r07);
    printf ("      0A Master=0x%02X  (bit7 must be 0 = slave mode)\r\n", (unsigned)r0a);
    printf ("      0C DPLL  =0x%02X  serialBW=%u (bit7:4, default 5) | DSD=%u\r\n",
            (unsigned)r0c, (unsigned)((r0c >> 4) & 0x0Fu), (unsigned)(r0c & 0x0Fu));
    printf ("      0E SoftStart=0x%02X (Reg14[7]=0 ramps output to ground = silence)\r\n",
            (unsigned)r0e);
    printf ("      0F/10 vol =0x%02X/0x%02X  (0x00=0dB, 0x50=-40dB)\r\n",
            (unsigned)r0f, (unsigned)r10);
    printf ("      1B GenCfg =0x%02X  ASRC_EN=%u CH_SHARE=%u LATCH=%u\r\n",
            (unsigned)r1b, (unsigned)((r1b >> 7) & 1u), (unsigned)((r1b >> 3) & 1u),
            (unsigned)((r1b >> 2) & 1u));
    {
        uint8_t r2d = 0u;
        ES9038_StateTypeDef lk = ES9038_DISABLE;
        uint32_t dpll = 0u;

        /* Reg45：内部 ADC 自动增益校准。ES9038_Init() 没配它，停在上电默认值；
         * 若 CALIB_EN/LATCH 为 1，校准环路逐声道动作 —— 正是"单侧、不同时"
         * 的 pop 的可疑来源，测 pop 时先确认这两位。 */
        (void)ES9038_ReadReg (ES9038_REG_LOW_POWER, &r2d);
        printf ("      2D Calib =0x%02X  CALIB_EN=%u LATCH=%u BIAS=%u\r\n",
                (unsigned)r2d, (unsigned)((r2d >> 5) & 1u), (unsigned)((r2d >> 4) & 1u),
                (unsigned)(r2d & 1u));

        (void)ES9038_GetLockStatus (&lk);
        (void)ES9038_GetDpllNumber (&dpll);
        printf ("      lock=%u dpll=0x%08X  (FSR = dpll*MCLK/2^32, MCLK=98.304MHz)\r\n",
                (unsigned)((lk != ES9038_DISABLE) ? 1u : 0u), (unsigned)dpll);
    }
}


static void cmd_adc (int argc, char **argv) {
    uint8_t c1cfg = 0u, c2cfg = 0u, dsp = 0u, dre = 0u;
    uint8_t c1g = 0u, c1v = 0u, c2g = 0u, c2v = 0u, r07 = 0u, r13 = 0u, r76 = 0u, r77 = 0u;
    int ok = (I2C_Probe (ADC_I2C_ADDR7) == 0) ? 1 : 0;
    uint8_t read_ok = 1u;

    (void)argc;
    (void)argv;

    printf ("[adc] i2c@0x%02X=%s  init_st=%u (0=ok 1=bus 2=no-ack 3=not-ready)\r\n",
            (unsigned)ADC_I2C_ADDR7, (ok != 0) ? "ok" : "NA",
            (unsigned)adc_init_status);
    if (ok == 0) {
        return;
    }
    printf ("      ASI_CFG0(after init)=0x%02X expected=0x%02X (TX_EDGE=%u)\r\n",
            (unsigned)adc_asi_cfg0, (unsigned)(0x70u | (0 ? 2u : 0u)),
            (unsigned)0);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_ASI_CFG0, &r07);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_MST_CFG0, &r13);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_CH1_CFG1, &c1g);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_CH1_CFG2, &c1v);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_CH2_CFG1, &c2g);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_CH2_CFG2, &c2v);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_DEV_STS0, &r76);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_DEV_STS1, &r77);

    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_CH1_CFG0, &c1cfg);
    read_ok &= DAC_ADC_AdcRead (ADC5120_REG_CH2_CFG0, &c2cfg);
    read_ok &= DAC_ADC_AdcRead (0x6Cu, &dsp);
    read_ok &= DAC_ADC_AdcRead (0x6Du, &dre);
    if (!read_ok) {
        printf ("      register read failed; incomplete values omitted, retry adc\r\n");
        return;
    }
    printf ("      DRE_CFG0=0x%02X; CH_CFG0=0x%02X/0x%02X\r\n",
            (unsigned)dre, (unsigned)c1cfg, (unsigned)c2cfg);
    printf ("      07 ASI_CFG0=0x%02X TX_EDGE=%u; CH DREEN=%u/%u DSP_CFG1=0x%02X\r\n",
            (unsigned)r07, (unsigned)((r07 >> 1) & 1u),
            (unsigned)(c1cfg & 1u), (unsigned)(c2cfg & 1u), (unsigned)dsp);
    printf ("      13 MST_CFG0=0x%02X (bit7=0 slave mode)\r\n", (unsigned)r13);
    printf ("      CH1 PGA=0x%02X = %d.%ddB   DIG=0x%02X = %d.%ddB\r\n",
            (unsigned)c1g, (int)((c1g >> 1) * 5) / 10, (int)((c1g >> 1) * 5) % 10,
            (unsigned)c1v, (int)(((int)c1v - 201) * 5) / 10,
            (int)(((int)c1v - 201) * 5) % 10);
    printf ("      CH2 PGA=0x%02X = %d.%ddB   DIG=0x%02X = %d.%ddB  [DIG=0x00 is mute]\r\n",
            (unsigned)c2g, (int)((c2g >> 1) * 5) / 10, (int)((c2g >> 1) * 5) % 10,
            (unsigned)c2v, (int)(((int)c2v - 201) * 5) / 10,
            (int)(((int)c2v - 201) * 5) % 10);
    printf ("      STS0=0x%02X STS1=0x%02X  (STS0 bit7/6 = CH1/CH2 powered, STS1 MODE=6/7 active)\r\n",
            (unsigned)r76, (unsigned)r77);
}

static void cmd_fmt (int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf ("[fmt] cnt=%u ret=0x%03X heal=%u | guard_flush=%u lap=%u\r\n",
            (unsigned)usb_fmt_cnt, (unsigned)usb_fmt_ret, (unsigned)usb_fmt_heal,
            (unsigned)i2s_guard_flush, (unsigned)i2s_play_lap);
    printf ("[fmt] play: want=%u/%u bit, hw=%u/%u bit  stopped=%u\r\n",
            (unsigned)USB_GetCurrentRate (),
            (unsigned)((USBHS_DevAltSetting[1] < USB_PlayAltCount)
                           ? USB_PlayAltDepth[USBHS_DevAltSetting[1]]
                           : 0u),
            (unsigned)I2S_GetRate (), (unsigned)I2S_GetPlayBits (),
            (unsigned)I2S_PlayIsStopped ());
    printf ("[fmt] cap : want=%u/%u bit, hw=%u/%u bit  active=%u\r\n",
            (unsigned)USB_GetCapRate (),
            (unsigned)((USBHS_DevAltSetting[2] < USB_CapAltCount)
                           ? USB_CapAltDepth[USBHS_DevAltSetting[2]]
                           : 0u),
            (unsigned)I2S_GetCapRate (), (unsigned)I2S_GetCapBits (),
            (unsigned)USBHS_CaptureActive);
}

static void cmd_reset (int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf ("[sys] soft reset ...\r\n");
    UART7_Flush ();
    NVIC_SystemReset ();
}

/* ADC5120 的 Page0 寄存器直接读写：ASI_CFG0 / 槽位 / PGA 这些现场试验用它，
 * 不用每次改代码重烧。例：`adcreg 7 0x70`（I2S + 32bit）*/

/* ES9038 数据通路 +18dB（Reg27[1:0]）：音源电平很低时用。
 * ⚠ 数字增益，任何高于 -18dBFS 的内容都会削顶，正常音乐请关。 */

static void cmd_swap (int argc, char **argv) {
    uint8_t p = 0u, c = 0u;

    I2S_GetLrSwap (&p, &c);

    if (argc == 1) {
        printf ("[swap] play=%u cap=%u  (0 = L in first slot, 1 = swapped)\r\n",
                (unsigned)p, (unsigned)c);
        printf ("       board mapping DAC_OUTPUT_CROSS=%u; swap is applied separately and saved.\r\n",(unsigned)DAC_OUTPUT_CROSS);
        printf ("       usage: swap play on|off   or   swap cap on|off\r\n");
        return;
    }
    if ((con_match (argv[1], "play") != 0u) || (con_match (argv[1], "dac") != 0u)) {
        uint8_t on = (argc >= 3) ? con_parse_onoff (argv[2])
                                 : (uint8_t)((p != 0u) ? 0u : 1u);

        if (on == 0xFFu) {
            printf ("usage: swap play on|off\r\n");
            return;
        }
        I2S_SetLrSwap (on, c);
        Volume_SettingsChanged();
        printf ("[swap] play -> %u (play a left-only source and check which ear)\r\n",
                (unsigned)on);
        return;
    }
    if ((con_match (argv[1], "cap") != 0u) || (con_match (argv[1], "adc") != 0u)) {
        uint8_t on = (argc >= 3) ? con_parse_onoff (argv[2])
                                 : (uint8_t)((c != 0u) ? 0u : 1u);

        if (on == 0xFFu) {
            printf ("usage: swap cap on|off\r\n");
            return;
        }
        I2S_SetLrSwap (p, on);
        Volume_SettingsChanged();
        printf ("[swap] cap -> %u (feed only the tip and see which channel it lands on)\r\n",
                (unsigned)on);
        return;
    }
    printf ("usage: swap play|cap [on|off]\r\n");
}

/*--------------------------------------------------------------------
 *  命令表
 *--------------------------------------------------------------------*/
typedef void (*ConCmdFn) (int argc, char **argv);

typedef struct {
    const char *name;
    ConCmdFn    fn;
    const char *args;
    const char *desc;
} ConCmd;

static void cmd_hid(int argc,char **argv) {
    uint16_t bits=0u;
    if(argc==3 && con_match(argv[1],"vol")) {
        if(con_match(argv[2],"up")) bits=HID_KEY_VOL_UP;
        else if(con_match(argv[2],"down")) bits=HID_KEY_VOL_DOWN;
    } else if(argc==2) {
        if(con_match(argv[1],"play") || con_match(argv[1],"pause")) bits=HID_KEY_PLAY_PAUSE;
        else if(con_match(argv[1],"next")) bits=HID_KEY_NEXT;
        else if(con_match(argv[1],"prev")) bits=HID_KEY_PREV;
        else if(con_match(argv[1],"stop")) bits=HID_KEY_STOP;
        else if(con_match(argv[1],"mute")) bits=HID_KEY_MUTE;
    }
    if(!bits) {printf("hid vol up|down / hid play|pause|next|prev|stop|mute\r\n");return;}
    printf("hid %s\r\n",HID_SendKey(bits) ? "queued" : "unavailable (offline, disabled or queue full)");
}
static void cmd_boot(int argc,char **argv) {
    if(argc==2 && con_match(argv[1],"status")) {
        printf("ISP base=0x%08X present=%u\r\n",(unsigned)CH32_BL_BASE,(unsigned)ch32_bl_present());
    } else if(argc==1) Board_EnterBootloader();
    else printf("boot [status]\r\n");
}
static const ConCmd s_cmds[] = {
    {"hid",cmd_hid,"vol up|down / play|pause|next|prev|stop|mute","send media key; automatic release"},
    {"boot",cmd_boot,"[status]","enter WCH USB ISP or query installed image"},
    {"help", cmd_help, "[cmd]", "command list / per-command usage"},
    {"info", cmd_info, "", "overview: rate / volume / USB / flash / uptime"},
    {"stat", cmd_stat, "", "cumulative counters (not cleared)"},
    {"play", cmd_play, "", "playback path snapshot (cleared on read)"},
    {"cap", cmd_cap, "", "capture path snapshot"},
    {"usb", cmd_usb, "", "USB endpoints / ISR load / drain / FU diag"},
    {"rate", cmd_rate, "", "rate table and alt<->bitdepth map"},
    {"vol", cmd_vol, "dac|adc [dB]", "get/set volume (1/256dB, e.g. -6.5)"},
    {"automute",cmd_automute,"[on|off]","zero detection policy"},
    {"mute", cmd_mute, "dac|adc [on|off]", "get/set FU mute (no arg = toggle)"},
    {"fu", cmd_fu, "", "FU2/FU8 raw values and flash record"},
    {"save", cmd_save, "", "write FU params to flash now"},
    {"dac", cmd_dac, "", "ES9038Q2M register snapshot"},
    {"adc", cmd_adc, "", "TLV320ADC5120 status and gain/volume regs"},
    {"swap", cmd_swap, "play|cap [on|off]", "L-R swap (detect/fix reversed channels)"},
    {"fmt", cmd_fmt, "", "format landing ledger (target vs hw)"},
    {"reset", cmd_reset, "", "soft reset"},
};

#define CON_CMD_COUNT (sizeof (s_cmds) / sizeof (s_cmds[0]))

static void cmd_help (int argc, char **argv) {
    uint8_t i;

    if (argc >= 2) {
        for (i = 0u; i < CON_CMD_COUNT; i++) {
            if (con_match (argv[1], s_cmds[i].name) != 0u) {
                printf ("%s %s\r\n    %s\r\n", s_cmds[i].name, s_cmds[i].args,
                        s_cmds[i].desc);
                return;
            }
        }
        printf ("no such command: %s (type help)\r\n", argv[1]);
        return;
    }

    printf ("command list (case-insensitive, args space separated, Enter to run)\r\n");
    for (i = 0u; i < CON_CMD_COUNT; i++) {
        printf ("  %-8s %-18s %s\r\n", s_cmds[i].name, s_cmds[i].args, s_cmds[i].desc);
    }
}

/*--------------------------------------------------------------------
 *  一行命令的执行
 *--------------------------------------------------------------------*/
static void con_exec (void) {
    char *argv[6];
    int argc = 0;
    char *p = s_line;
    int i;

    s_line[s_len] = '\0';

    /* 切词：空格分隔，最多 6 个参数（够用了，再多也没命令要） */
    while ((*p != '\0') && (argc < 6)) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while ((*p != '\0') && (*p != ' ')) {
            p++;
        }
        if (*p == ' ') {
            *p = '\0';
            p++;
        }
    }
    if (argc == 0) {
        return;
    }

    for (i = 0; i < (int)CON_CMD_COUNT; i++) {
        if (con_match (argv[0], s_cmds[i].name) != 0u) {
            s_cmds[i].fn (argc, argv);
            return;
        }
    }
    printf ("unknown command: %s   (type help for the list)\r\n", argv[0]);
}

void Console_Service (void) {
    uint8_t ch;

    if (s_banner_done == 0u) {
        s_banner_done = 1u;
        printf ("\r\nterminal ready (no echo; Enter terminates a line)\r\n");
        printf ("  type help for all commands; e.g. info / vol dac -6.5 / swap play on\r\n> ");
    }

    while (UART7_GetByte (&ch)) {
        s_last_rx_tick = tim6_tick;
        s_hint_done = 0u;

        if ((ch == 8u) || (ch == 127u)) { /* 退格：只改缓冲，不回显 */
            if (s_len > 0u) {
                s_len--;
            }
            continue;
        }
        if (ch == 21u) { /* Ctrl-U 清行 */
            s_len = 0u;
            continue;
        }
        if (ch == 3u) { /* Ctrl-C 取消 */
            s_len = 0u;
            printf ("^C\r\n> ");
            continue;
        }
        if (ch < 0x20u) {
            /* 其余控制字节一律当行结束符（见函数头的第 2 条）*/
            printf ("\r\n");
            con_exec ();
            s_len = 0u;
            s_overflow = 0u;
            printf ("> ");
            continue;
        }
        if (ch > 0x7Eu) {
            continue; /* 高位字节：上电毛刺、噪声、中文输入，直接丢 */
        }
        if (s_len < (CON_LINE_MAX - 1u)) {
            s_line[s_len++] = (char)ch;
        } else if (s_overflow == 0u) {
            s_overflow = 1u;
            printf ("\r\n! line too long, truncated (press Enter to run)\r\n");
        }
    }

    /* "等回车"自诊断：非空且 600ms 没动静就提示一次 */
    if ((s_len > 0u) && (s_hint_done == 0u) &&
        ((uint32_t)(tim6_tick - s_last_rx_tick) >= 30u)) {
        s_hint_done = 1u;
        s_line[s_len] = '\0';
        printf ("\r\n! got [%s] but no CR/LF -- command not executed.\r\n"
                "  check your serial tool's 'send newline / append CRLF' option;\r\n"
                "  or set the line terminator to CR(0x0D) or LF(0x0A).\r\n> ",
                s_line);
    }
}
