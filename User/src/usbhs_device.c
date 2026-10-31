/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v30x_usbhs_device.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/11/20
 * Description        : This file provides all the USBHS firmware functions.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "usbhs_device.h"
#include "settings.h"
#include "usb_desc.h"
#include "i2s_app.h"
#include "hid_app.h"
#include "volume_app.h"
#include "ch32v30x_tim.h"
/* TIM6 的 20ms 心跳计数（定义在 tim6_app.c）。不 include tim6_app.h：
 * 那个头里用到了 bool，而本文件在 ch32v30x.h 之前的 include 顺序里没有它。 */
extern volatile uint32_t tim6_tick;
#include <stdio.h>          /* 只用于主循环里的诊断打印（不在中断里调用） */

/******************************************************************************/
/* Variable Definition */
/* test mode */
volatile uint8_t USBHS_Test_Flag;
__attribute__ ((aligned (4))) uint8_t IFTest_Buf[53] = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xFE,        // 26
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 37
    0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD,                          // 44
    0xFC, 0x7E, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0x7E               // 53
};

/* Six 1KiB DMA banks: one/two hardware-owned, the rest for main-loop PCM.
 * Completed banks are queued by index; the ISR never copies audio payload. */
__attribute__((aligned(4))) static uint8_t usb_rx_extra[4][1024];
static uint8_t usb_rx_bank_state[6], usb_rx_hw_bank[2];
static uint8_t usb_rx_bank_queue[8];
static uint16_t usb_rx_len[8];
static uint32_t usb_rx_seq[8];
static volatile uint32_t usb_rx_write, usb_rx_read, usb_rx_epoch;
static void USB_RxInit(void);
static volatile uint8_t usb_cap_state[2]; /* 0 free, 1 filling, 2 ready, 3 SIE-owned */
static uint16_t usb_cap_len[2];
static volatile int8_t usb_cap_active_slot = -1;
static volatile uint8_t usb_sof_pending;
volatile uint32_t usb_rx_queue_drop, usb_rx_queue_stale, usb_rx_queue_peak;
volatile uint32_t usb_cap_queue_under, usb_cap_packets, usb_fifo_overflow;
volatile uint32_t usb_isr_max_ticks;
volatile uint32_t usb_xfer_max_ticks;
volatile uint32_t usb_iso_wait_timeout;
volatile uint32_t usb_cap_in_empty;
volatile uint32_t usb_ep0_out_count, usb_ep0_out_last, usb_ep0_out_data, usb_ep0_out_skip;
volatile uint32_t usb_out_delta[4];
static uint32_t UsbFormatLock(void);
static void UsbFormatUnlock(uint32_t m);
static void USB_UpdateFeedback(void);
static void USB_CapSubmit(void);

/* Global */
const uint8_t *pUSBHS_Descr;

/* Setup Request */
volatile uint8_t USBHS_SetupReqCode;
volatile uint8_t USBHS_SetupReqType;
volatile uint16_t USBHS_SetupReqValue;
volatile uint16_t USBHS_SetupReqIndex;
volatile uint16_t USBHS_SetupReqLen;

/* USB Device Status */
volatile uint8_t USBHS_DevConfig;
volatile uint8_t USBHS_DevAddr;
volatile uint16_t USBHS_DevMaxPackLen;
volatile uint8_t USBHS_DevSpeed;
volatile uint8_t USBHS_DevSleepStatus;
volatile uint8_t USBHS_DevEnumStatus;
/* EP4：toggle 由硬件翻（TOG_AUTO=1），软件只管 armed。
 *   armed=1 → ACK，主机可取走；armed=0 → NAK。
 * 两者都不写 MASK_UEP_T_TOG。ep4_chk 是 SOF 里的复核计数。 */
static volatile uint8_t ep4_armed;
static volatile uint16_t ep4_chk;
/* TOG 影子：SOF 里比对 bit3 就知道主机取走没取走（不依赖完成中断）。 */
static volatile uint8_t ep4_tog_exp;
volatile uint32_t usb_ep4_ok;   /* 主机确实取走一份报告的次数 */
volatile uint32_t usb_ep4_poll; /* EP4 被主机轮询的总次数（含 NAK），判断主机到底有没有在问 */
volatile uint32_t usb_ep4_pend; /* 其中"已 armed、正等主机取走"期间的轮询次数 */
volatile uint32_t usb_ep4_to;   /* 握手被复核补回 NAK 的次数（报告被兜底丢弃） */
volatile uint32_t usb_ep4_sof;  /* SOF 计数，用来核对 poll 的量级（HS 每 125us 一个） */

/* SETUP 诊断：进来多少次、原始八字节（w0=bRequestType|bRequest<<8|wValue<<16,
 * w1=wIndex|wLength<<16）。用来判定"播放中改录音采样率"时 SET_CUR 到底怎么丢的。 */
volatile uint32_t usb_setup_cnt;
volatile uint32_t usb_setup_w0;
volatile uint32_t usb_setup_w1;

/* EP1 OUT 收包统计：事件数、其中 TOG_OK 为 0 的次数。
 * 等时端点的 TOG 匹配按手册 §22.2.2.10 是"无效"的，INT_BUSY=0 之后它会间歇清零，
 * 旧代码用它当"数据有效"用，会把包丢在门口。这两个数是用来证实/否证这件事的。 */
volatile uint32_t usb_ep1_out_evt;
volatile uint32_t usb_ep1_out_notog;

/* TIM8 runs at 960kHz; one HS microframe is 120 ticks. */
volatile uint32_t usb_isr_cnt;   /* USB 中断进入次数 */
volatile uint32_t usb_isr_ticks; /* USB 中断内累计消耗的 TIM8 tick 数 */

/* 分分支耗时：SOF 与 EP1 OUT 各自的 tick 累计与次数 */
volatile uint32_t usb_isr_sof_ticks;
volatile uint32_t usb_isr_sof_cnt;
volatile uint32_t usb_isr_out_ticks;
volatile uint32_t usb_isr_out_cnt;
/* 上一次 EP1 OUT 事件发生时的 SOF 序号。SOF 里拿它算"缺了几个微帧"，
 * 也就是"丢了几包"（等时包丢了就不会有完成事件，只能这样数）。 */
static volatile uint16_t s_last_out_frame;
static volatile uint8_t s_out_gap_accounted, s_out_gap_active;
static volatile uint32_t s_play_format_seq, s_cap_format_seq;
volatile uint8_t usb_int_busy = 1u;

volatile uint32_t usb_isr_drain[4];
volatile uint32_t usb_isr_drain_ovf;
/* 格式重建账：cnt = 重建次数，ret = 最后一次结果（bit0..1：1=成功 2=校验
 * 不过；bit8 置位 = 播放侧，清零 = 录音侧），heal = 硬件自愈触发次数。 */
volatile uint32_t usb_fmt_cnt;
volatile uint32_t usb_fmt_ret;
volatile uint32_t usb_fmt_heal;

/* 每个接口记住自己的 alt。描述符实测 4 个接口（不是 5 个，旧注释写错过）：
 *   0 = AudioControl（播放+录音共用）  1 = AS 播放
 *   2 = AS 录音                        3 = HID */
volatile uint8_t USBHS_DevAltSetting[5];

volatile uint8_t USBHS_PlayRatePending;
volatile uint8_t USBHS_CapRatePending;
/* 录音开流请求：SET_INTERFACE(录音AS, alt>0) 只登记，等 CapRatePending
 * 真正落进 I2S2 之后再把 EP2 置 ACK。顺序反过来的话，主机拿到的头几包
 * 会按旧速率/旧位深打包，usbaudio2 认为格式不匹配，起录要等好几秒
 * （现象正是「录音软件卡 5~10 秒才开始」）。 */
volatile uint8_t USBHS_CapArmPending;
volatile uint32_t USBHS_PlayRateReq = 48000u;
volatile uint32_t USBHS_CapRateReq = 48000u;
/* 诊断：收到过多少次录音时钟 SET_CUR、最后一次请求的频率 */
volatile uint32_t USBHS_CapSetCurCnt;
volatile uint32_t USBHS_CapSetCurLast;
/* 诊断：任意一次采样率 SET_CUR 的现场（主循环里打印，见 USB_ApplyPending） */
volatile uint32_t USBHS_ScCnt;
volatile uint32_t USBHS_ScFreq;
volatile uint8_t  USBHS_ScEnt;
volatile uint8_t  USBHS_ScClk;

volatile uint32_t usb_fu_set_cnt;   /* FU 的 SET_CUR 次数 */
volatile uint8_t  usb_fu_set_ent;   /* 最后一次：实体 ID（2=播放 8=录音） */
volatile uint8_t  usb_fu_set_sel;   /* 最后一次：控制选择器（1=Mute 2=Volume） */
volatile uint8_t  usb_fu_set_ch;    /* 最后一次：声道号（0=master） */
volatile int16_t  usb_fu_set_val;   /* 最后一次：原始值（mute=0/1，volume=1/256dB） */

/* Endpoint Buffer */
__attribute__ ((aligned (4))) uint8_t USBHS_EP0_Buf[DEF_USBD_UEP0_SIZE];
__attribute__ ((aligned (4))) uint8_t USBHS_EP1_Rx_Buf[2][DEF_USB_EP1_HS_SIZE];
volatile uint8_t USBHS_EP1_Rx_Buf_PTR;
__attribute__ ((aligned (4))) uint8_t USBHS_EP1_Tx_Buf[DEF_USB_EP1_FD_HS_SIZE];
__attribute__ ((aligned (4))) uint8_t USBHS_EP2_Tx_Buf[DEF_USB_EP1_HS_SIZE];
__attribute__ ((aligned (4))) uint8_t USBHS_EP3_Tx_Buf[DEF_USB_EP1_FD_HS_SIZE];
/* HID 中断 IN 端点（EP4）：8 字节够放 1 字节报告位图 */
__attribute__ ((aligned (4))) uint8_t USBHS_EP4_Tx_Buf[DEF_USB_EP4_SIZE];

/* Capture streaming active (AudioStreaming interface 2 alt > 0) */
volatile uint8_t USBHS_CaptureActive;

volatile uint8_t USBHS_PlayRelandPending;
/* 1 = 强制 USB PHY 工作在全速（FS）。FS 没有微帧、没有 chirp，枚举最稳。
 * 代价：FS 每毫秒 1 包，32bit 下最高只能到 96kHz。 */
volatile uint8_t USBHS_ForceFS = USB_FORCE_FS;

#define SampleFreqPlay USB_SampleFreqPlay /* clock 4：播放 */
#define SampleFreqCap USB_SampleFreqCap   /* clock 5：录音 */

/* Rate currently programmed into I2S3/I2S2/MCLK.  I2S_SetSampleRate() is
 * expensive (it stops both I2S blocks and restarts the playback ring), so it
 * is only called when the value really changes. */
static uint32_t USBHS_AppliedRate;    /* 播放，上电后由 bus reset / main 同步 */
static uint32_t USBHS_AppliedCapRate; /* 录音，同上 */

/* UAC2：高字节是实体 ID，低字节是 AC 接口号。 */
static uint8_t ClockIdOf(uint16_t index) {return (uint8_t)(index>>8);}

/* All UAC2 CUR writes use bRequest=1. Route by entity, not request code.
 * This helper only changes RAM; I2C and format rebuilds run in the main loop. */
static uint8_t USB_EntityPresent(uint8_t ent) {
    return (USB_PLAY_ENABLE && (ent==AUDIO_CLOCK_ID || (USB_FU_ACTIVE && ent==AUDIO_FU_ID))) ||
           (USB_CAPTURE_ENABLE && (ent==AUDIO_CLOCK_ID_CAP || (USB_FU_ACTIVE && ent==AUDIO_FU_CAP_ID)));
}

static uint16_t USB_CurWriteLength (void) {
    uint8_t ent = USBHS_SetupReqIndex >> 8;
    uint8_t sel = USBHS_SetupReqValue >> 8;
    if (USBHS_SetupReqType != 0x21u || USBHS_SetupReqCode != AUDIO_REQ_CUR ||
        (USBHS_SetupReqIndex & 0xFFu) || (USBHS_SetupReqValue & 0xFFu)) return 0u;
    if(!USB_EntityPresent(ent)) return 0u;
    if (ent == AUDIO_FU_ID || ent == AUDIO_FU_CAP_ID)
        return sel == FU_MUTE_CONTROL ? 1u : sel == FU_VOLUME_CONTROL ? 2u : 0u;
    if ((ent == AUDIO_CLOCK_ID || ent == AUDIO_CLOCK_ID_CAP) && sel == CS_SAM_FREQ_CONTROL) return 4u;
    return 0u;
}

static uint8_t USB_SetCurData (const uint8_t *data, uint16_t len) {
    uint8_t ent = (uint8_t)(USBHS_SetupReqIndex >> 8);
    uint8_t sel = (uint8_t)(USBHS_SetupReqValue >> 8);
    uint8_t ch = (uint8_t)USBHS_SetupReqValue;
    uint8_t clk = ClockIdOf (USBHS_SetupReqIndex);
    uint32_t rate;
    uint8_t k;

    if (!len || len != USB_CurWriteLength()) {
        return 0u;
    }
    if (ent == AUDIO_FU_ID || ent == AUDIO_FU_CAP_ID) {
        /* Only master controls are advertised in the descriptor. */
        if (ch != 0u) {
            return 0u;
        }
        if (sel == FU_MUTE_CONTROL && len == 1u) {
            usb_fu_set_val = (data[0] != 0u) ? 1 : 0;
            if (ent == AUDIO_FU_ID) {
                Volume_SetDacMute ((uint8_t)usb_fu_set_val);
            } else {
                Volume_SetAdcMute ((uint8_t)usb_fu_set_val);
            }
        } else if (sel == FU_VOLUME_CONTROL && len == 2u) {
            usb_fu_set_val = (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
            if (ent == AUDIO_FU_ID) {
                Volume_SetDac (ch, usb_fu_set_val);
            } else {
                Volume_SetAdc (ch, usb_fu_set_val);
            }
        } else {
            return 0u;
        }
        usb_fu_set_ent = ent;
        usb_fu_set_sel = sel;
        usb_fu_set_ch = ch;
        usb_fu_set_cnt++;
        return 1u;
    }
    if ((ent != AUDIO_CLOCK_ID && ent != AUDIO_CLOCK_ID_CAP) ||
        (clk != AUDIO_CLOCK_ID && clk != AUDIO_CLOCK_ID_CAP) ||
        sel != CS_SAM_FREQ_CONTROL || ch != 0u || len != 4u) {
        return 0u;
    }
    rate = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    for (k = 0u; k < USB_RateCount; k++) {
        if (USB_RateList[k] == rate) {
            break;
        }
    }
    if (k == USB_RateCount) {
        return 0u;
    }
    USBHS_ScEnt = ent;
    USBHS_ScClk = clk;
    USBHS_ScFreq = rate;
    USBHS_ScCnt++;
    if (clk == AUDIO_CLOCK_ID) {
        if (USB_GetCurrentRate() != rate) {
            memcpy (SampleFreqPlay, data, 4u);
            USB_RequestPlayRecovery();
        }
    } else if (USB_GetCapRate() != rate) {
        memcpy (SampleFreqCap, data, 4u);
        USB_RequestCapRecovery();
    }
    if (clk == AUDIO_CLOCK_ID_CAP) {
        USBHS_CapSetCurCnt++;
        USBHS_CapSetCurLast = rate;
    }
    return 1u;
}

/* 发一份 HID 报告；完成事件或 SOF toggle 复核将端点收回 NAK。 */
/* ---- UAC2 Feature Unit（音量/静音）读回，供串口诊断用 ----
 * 真正的存储在 volume_app.c（那里才落硬件、才存 Flash），这里只是转发，
 * 保证串口打印的和 GET_CUR 返回的是同一份数据。 */
uint8_t USB_GetFuMute (void) { return Volume_GetDacMute (); }

int16_t USB_GetFuVolL (void) { return Volume_GetDac (1); }

int16_t USB_GetFuVolR (void) { return Volume_GetDac (2); }

void USB_HID_Send (const uint8_t *data, uint8_t len) {
#if !USB_HID_ENABLE
    (void)data;
    (void)len;
    return; /* HID 暂关闭（USB_HID_ENABLE=0） */
#endif
    if (USBHS_DevConfig == 0) {
        return;
    }
    if (len > DEF_USB_EP4_SIZE) {
        len = DEF_USB_EP4_SIZE;
    }
    /* 写入缓冲区 / 长度。toggle 由硬件管（UEP4_TX_CTRL 的 TOG_AUTO=1）。 */
    USBHSD->UEP4_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP4_Tx_Buf;
    memcpy (USBHS_EP4_Tx_Buf, data, len);
    USBHSD->UEP4_TX_LEN = len;

    ep4_tog_exp = USBHSD->UEP4_TX_CTRL & USBHS_UEP_T_TOG_DATA1;
    ep4_armed = 1u;
    USBHSD->UEP4_TX_CTRL = (uint8_t)((USBHSD->UEP4_TX_CTRL & ~USBHS_UEP_T_RES_MASK) |
                                     USBHS_UEP_T_RES_ACK);
}

/******************************************************************************/
/* Interrupt Service Routine Declaration*/
void USBHS_IRQHandler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));

void USB_SetIntBusy (uint8_t en) {
    uint8_t ctrl = USBHSD->CONTROL;
    if (en != 0u) {
        ctrl |= USBHS_UC_INT_BUSY;
    } else {
        ctrl = (uint8_t)(ctrl & ~USBHS_UC_INT_BUSY);
    }
    USBHSD->CONTROL = ctrl;
    usb_int_busy = (en != 0u) ? 1u : 0u;
}

static volatile uint8_t ep0_out_pending,ep0_out_poll_ready;
static void USB_IntBusyAuto (void) {
    uint8_t want = !(USBHS_DevAltSetting[1] || USBHS_DevAltSetting[2]);
    if (want != usb_int_busy) USB_SetIntBusy (want);
}

void USB_TestMode_Deal (void) {
    /* start test */
    USBHS_Test_Flag &= ~0x80;
    if (USBHS_SetupReqIndex == 0x0100) {
        /* Test_J */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_J;
    } else if (USBHS_SetupReqIndex == 0x0200) {
        /* Test_K */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_K;
    } else if (USBHS_SetupReqIndex == 0x0300) {
        /* Test_SE0_NAK */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_SE0;
    } else if (USBHS_SetupReqIndex == 0x0400) {
        /* Test_Packet */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_PACKET;

        USBHSD->CONTROL |= USBHS_UC_HOST_MODE;
        USBHSH->HOST_EP_CONFIG = USBHS_UH_EP_TX_EN | USBHS_UH_EP_RX_EN;
        USBHSH->HOST_EP_TYPE |= 0xff;

        USBHSH->HOST_TX_DMA = (uint32_t)(&IFTest_Buf[0]);
        USBHSH->HOST_TX_LEN = 53;
        USBHSH->HOST_EP_PID = (USB_PID_SETUP << 4);
        USBHSH->INT_FG = USBHS_UIF_TRANSFER;
    }
}

void USBHS_RCC_Init (void) {
    /* USBHS PLL reference = HSE / USBHSDIV = 16MHz / 4 = 4MHz (exact).
     * USBHSCLK is declared as 4MHz to match the actual reference. */
    RCC_USBCLK48MConfig (RCC_USBCLK48MCLKSource_USBPHY);
    RCC_USBHSPLLCLKConfig (RCC_HSBHSPLLCLKSource_HSE);
    RCC_USBHSConfig (RCC_USBPLL_Div4);
    RCC_USBHSPLLCKREFCLKConfig (RCC_USBHSPLLCKREFCLK_4M);
    RCC_USBHSPHYPLLALIVEcmd (ENABLE);
    RCC_AHBPeriphClockCmd (RCC_AHBPeriph_USBHS, ENABLE);
}

void USBHS_Device_Endp_Init (void) {

    USBHSD->ENDP_CONFIG = (USB_PLAY_ENABLE ? USBHS_UEP1_R_EN|USBHS_UEP2_T_EN : 0u) | (USB_CAPTURE_ENABLE ? USBHS_UEP3_T_EN : 0u)
#if USB_HID_ENABLE
                          | USBHS_UEP4_T_EN
#endif
        ;
    USBHSD->ENDP_TYPE = (USB_PLAY_ENABLE ? USBHS_UEP1_R_TYPE|USBHS_UEP2_T_TYPE : 0u) | (USB_CAPTURE_ENABLE ? USBHS_UEP3_T_TYPE : 0u);

#ifndef PLAY_EP1_BUF_MOD
#define PLAY_EP1_BUF_MOD 0
#endif
#if PLAY_EP1_BUF_MOD
    USBHSD->BUF_MODE = USBHS_UEP1_BUF_MOD;
#else
    USBHSD->BUF_MODE = 0x00000000;
#endif

    USBHSD->UEP0_MAX_LEN = DEF_USBD_UEP0_SIZE;

    /* 告诉 I2S 侧当前是 HS(8000 微帧/秒) 还是 FS(1000 帧/秒)：
     * 包长诊断与 wMaxPacketSize 都依赖它。 */
    I2S_SetUsbFrameHz (((USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK) == USBHS_SPEED_HIGH)
                           ? 8000u
                           : 1000u);
    /* EP1/EP2 max packet depends on the bus speed (1024 HS / 1023 FS) */
    if ((USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK) == USBHS_SPEED_HIGH) {
        USBHSD->UEP1_MAX_LEN = DEF_USB_EP1_HS_SIZE;   /* 播放数据 OUT */
        USBHSD->UEP3_MAX_LEN = DEF_USB_EP1_HS_SIZE;   /* 录音数据 IN  */
    } else {
        USBHSD->UEP1_MAX_LEN = DEF_USB_EP1_FS_SIZE;
        USBHSD->UEP3_MAX_LEN = DEF_USB_EP1_FS_SIZE;
    }
    USBHSD->UEP2_MAX_LEN = DEF_USB_EP1_FD_HS_SIZE;    /* 播放反馈 IN，4 字节 */
#if USB_HID_ENABLE
    USBHSD->UEP4_MAX_LEN = DEF_USB_EP4_SIZE;
#endif

    USBHSD->UEP0_DMA = (uint32_t)(uint8_t *)USBHS_EP0_Buf;
    /* 播放数据的另一块缓冲。BUF_MODE=1 时它是硬件乒乓的第二块；
     * BUF_MODE=0 时软件把 UEP1_RX_DMA 在两块之间来回指。两种用法共用。 */
    USBHSD->UEP1_RX_DMA = (uint32_t)(uint8_t *)USBHS_EP1_Rx_Buf[0];
    USBHSD->UEP1_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP1_Rx_Buf[1];
    /* 【必须一起复位】软件侧的乒乓指针。上面刚把 UEP1_RX_DMA 指回 buf[0]，
     * 这里若不归零，复位后的第一包会被当作 buf[1] 读走（读的是上一轮的旧
     * 数据），第二包又覆盖 buf[0] —— 每次插拔/重连都有 1 个包（125us）的
     * 旧数据进环。单独一声"咔"，也可能被当成切换时的噪声。 */
    USBHS_EP1_Rx_Buf_PTR = 0u;
    USB_RxInit();
    /* EP2 = 播放反馈（4 字节），复用原 EP1 的反馈缓冲 */
    USBHSD->UEP2_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP1_Tx_Buf;
    USB_UpdateFeedback();
    /* EP3 = 录音数据，复用原 EP2 的 1KB 采集缓冲 */
    USBHSD->UEP3_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP2_Tx_Buf;
#if USB_HID_ENABLE
    USBHSD->UEP4_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP4_Tx_Buf;
#endif

    USBHSD->UEP0_TX_LEN = 0;
    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;

    USBHSD->UEP1_RX_CTRL = USBHS_UEP_T_TOG_DATA0; /* RES=ACK, 等时 OUT 常开 */
    USBHSD->UEP1_TX_LEN = 0;
    USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_TOG_DATA0; /* EP1 已单向，此位无效 */

    /* EP2 = 播放反馈：常开 ACK（4 字节，每微帧一份）。
     * LEN 先给 0（ZLP），第一个 SOF 会填成真值 —— 与旧版 EP1 反馈一致。 */
    USBHSD->UEP2_TX_LEN = 0;
    USBHSD->UEP2_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_ACK;

    /* EP3 = 录音数据：默认 NAK；SET_INTERFACE(录音, alt>0) 才由
     * USBHS_CapArm() 填好一包标称长度后置 ACK */
    USBHSD->UEP3_TX_LEN = 0;
    USBHSD->UEP3_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_NAK;
    
#if USB_HID_ENABLE
    memset (USBHS_EP4_Tx_Buf, 0, DEF_USB_EP4_SIZE);
    USBHSD->UEP4_TX_LEN = 0;
    USBHSD->UEP4_TX_CTRL = (uint8_t)(USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_TOG_AUTO |
                                     USBHS_UEP_T_RES_NAK);
    ep4_armed = 0u;
    ep4_chk = 0u;
    /* 总线复位后硬件 TOG 被重写成 DATA0（上面那行），影子跟着归零 */
    ep4_tog_exp = 0u;
#endif
}

void USBHS_Device_Init (FunctionalState sta) {
    if (sta) {
        /* Build the configuration / rate / alternate-setting descriptors
         * before D+ is pulled up: the host may ask for them within
         * microseconds of seeing the pull-up. */
        USB_BuildDescriptors();
        /* USB_BuildDescriptors() 把 USB_SampleFreq 复位成矩阵里最高那档(384k)，
         * 而 I2S 上电实际跑的是 I2S_Setup() 里那档(48k)。两者不一致时
         * GET_CUR 会对主机谎报一个 I2S 根本没输出的频率，Windows 声音面板
         * 就会一直显示 384k */
        USB_SetCurrentRate (I2S_GetRate(), I2S_GetCapRate());
        USBHS_AppliedRate = USB_GetCurrentRate();

        USBHSD->CONTROL = USBHS_UC_CLR_ALL | USBHS_UC_RESET_SIE;
        Delay_Us (10);
        USBHSD->CONTROL &= ~USBHS_UC_RESET_SIE;
        USBHSD->HOST_CTRL = USBHS_UH_PHY_SUSPENDM;
        USBHSD->CONTROL = USBHS_UC_DMA_EN | USBHS_UC_INT_BUSY;
        if (!USBHS_ForceFS) {
            USBHSD->CONTROL |= USBHS_UC_SPEED_HIGH;
        }
        USBHSD->INT_EN = USBHS_UIE_SETUP_ACT | USBHS_UIE_TRANSFER | USBHS_UIE_DETECT | USBHS_UIE_SUSPEND | USBHS_UIE_SOF_ACT | USBHS_UIE_ISO_ACT;
        USBHS_Device_Endp_Init();
        USBHSD->CONTROL |= USBHS_UC_DEV_PU_EN;
        NVIC_EnableIRQ (USBHS_IRQn);
    } else {
        USBHSD->CONTROL = USBHS_UC_CLR_ALL | USBHS_UC_RESET_SIE;
        Delay_Us (10);
        USBHSD->CONTROL &= ~USBHS_UC_RESET_SIE;
        NVIC_DisableIRQ (USBHS_IRQn);
    }
}

/* 位深切换要清环重启 DMA；和采样率请求一样在中断里登记、主循环里落地。 */
volatile uint8_t USBHS_PlayRestartPending;

/* 把中断里登记的采样率请求真正应用（主循环调用）。
 * 放在主循环的好处：停 DMA / 停 I2S / 清环这一串不再占用 USB 中断上下文，
 * 枚举期间主机连续探测 alt 也不会被拖着走。 */
/* Format owner calls this while EP3 is NAK and the producer is stopped.
 * Fill first; publish DMA address and length before marking the packet ready. */
static void USBHS_CapArm (void) {
    uint32_t scale = USBHS_DevSpeed == USBHS_SPEED_FULL ? 8u : 1u;
    uint32_t capacity = scale == 1u ? 512u : 1024u;
    usb_cap_state[0] = usb_cap_state[1] = 0u;
    usb_cap_active_slot = -1;
    usb_cap_len[0] = (uint16_t)I2S_CapRead(USBHS_EP2_Tx_Buf, scale, capacity);
    usb_cap_state[0] = 2u;
    USB_CapSubmit();
}

static uint32_t UsbFormatLock (void) {
    uint32_t m;
    __asm volatile ("csrr %0, 0x800" : "=r"(m) : : "memory");
    __asm volatile ("csrc 0x800, %0" : : "r"(0x88u) : "memory");
    __asm volatile ("fence.i");
    return m;
}

static void UsbFormatUnlock (uint32_t m) {
    m &= 0x88u;
    if (m) {
        __asm volatile ("csrs 0x800, %0" : : "r"(m) : "memory");
        __asm volatile ("fence.i");
    }
}

void USB_RequestPlayRecovery (void) {
    uint32_t irq = UsbFormatLock();
    I2S_PlayFmtSuspend();
    s_play_format_seq++;
    USBHS_PlayRelandPending = 1u;
    s_out_gap_active = 0u;
    UsbFormatUnlock (irq);
}

void USB_RequestCapRecovery (void) {
    uint32_t irq = UsbFormatLock();
    USBHS_CaptureActive = 0u;
    USBHSD->UEP3_TX_LEN = 0u;
    USBHSD->UEP3_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_NAK;
    s_cap_format_seq++;
    UsbFormatUnlock (irq);
}

void USB_ApplyPending (void) {
    static uint32_t play_applied, cap_applied;
    static uint32_t heal_tick;
    uint32_t irq, seq, f, b;
    uint8_t alt, r;

    irq = UsbFormatLock();
    seq = s_play_format_seq;
    alt = USBHS_DevAltSetting[1];
    f = USB_GetCurrentRate();
    b = alt < USB_PlayAltCount ? USB_PlayAltDepth[alt] : 0u;
    UsbFormatUnlock (irq);
    if (alt && b && f && (seq != play_applied || USBHS_PlayRelandPending ||
        I2S_PlayIsStopped() || f != I2S_GetRate() || b != I2S_GetPlayBits())) {
        I2S_PlayFmtSuspend();
        /* A rate change already rebuilds: do not first restart the old rate. */
        r = I2S_ApplyPlayFormat (f, b);
        if (r == 0u) {
            I2S_PlayRingRestart();
            I2S_PlayMuteSettle();
        }
        usb_fmt_cnt++;
        usb_fmt_ret = 0x100u | (I2S_PlayHwOk() ? 1u : 2u);
        irq = UsbFormatLock();
        if (seq == s_play_format_seq && alt == USBHS_DevAltSetting[1] &&
            f == USB_GetCurrentRate() && I2S_PlayHwOk()) {
            play_applied = seq;
            USBHS_PlayRelandPending = 0u;
            I2S_PlayFmtCommit();
        } else if (USBHS_DevAltSetting[1] == 0u) {
            I2S_PlayStop(); /* Close/reset may have interrupted the rebuild. */
        }
        UsbFormatUnlock (irq);
    } else if (!alt && !I2S_PlayIsStopped()) {
        I2S_PlayStop();
    }

    irq = UsbFormatLock();
    seq = s_cap_format_seq;
    alt = USBHS_DevAltSetting[2];
    f = USB_GetCapRate();
    b = alt < USB_CapAltCount ? USB_CapAltDepth[alt] : 0u;
    UsbFormatUnlock (irq);
    if (alt && b && f && (seq != cap_applied || !USBHS_CaptureActive ||
        f != I2S_GetCapRate() || b != I2S_GetCapBits())) {
        uint32_t old_rate = I2S_GetCapRate();
        USBHS_CaptureActive = 0u;
        r = I2S_ApplyCapFormat (f, b);
        if (r != 2u && f == old_rate) I2S_CapHardStart();
        usb_fmt_cnt++;
        usb_fmt_ret = I2S_CapHwOk() ? 1u : 2u;
        irq = UsbFormatLock();
        if (r != 2u && seq == s_cap_format_seq && alt == USBHS_DevAltSetting[2] &&
            f == USB_GetCapRate() && f == I2S_GetCapRate() && I2S_CapHwOk()) {
            I2S_CapStart();
            USBHS_CapArm();
            USBHS_CaptureActive = 1u;
            cap_applied = seq;
        }
        UsbFormatUnlock (irq);
    }
    /* Hardware failures request the same serialized path as a format change. */
    if (tim6_tick != heal_tick) {
        heal_tick = tim6_tick;
        if (USBHS_DevAltSetting[1] && !I2S_PlayFmtPending() && !I2S_PlayHwOk()) {
            USB_RequestPlayRecovery();
            usb_fmt_heal++;
        }
        if (USBHS_DevAltSetting[2] && USBHS_CaptureActive && !I2S_CapHwOk()) {
            USB_RequestCapRecovery();
            usb_fmt_heal++;
        }
    }
    USBHS_PlayRatePending = USBHS_PlayRestartPending = 0u;
    USBHS_CapRatePending = USBHS_CapArmPending = 0u;
    USB_IntBusyAuto();
}

/* Use completed SOF events for both OUT and main-loop accounting. FRAME_NO
 * changes in hardware independently of ISR dispatch; mixing those phases
 * can classify the same OUT in different service intervals. */
static volatile uint32_t usb_last_sof_tick;
uint8_t USB_HostReady(void) {
    return USBHS_DevConfig && !(USBHS_DevSleepStatus & 2u) &&
           (uint32_t)(tim6_tick-usb_last_sof_tick)<5u;
}
static uint16_t USB_ServiceFrame(void) {
    return (uint16_t)usb_ep4_sof;
}

static void USB_PlayGapPoll (void) {
    uint32_t lost = 0u;
    uint32_t irq = UsbFormatLock();
    uint16_t now = USB_ServiceFrame();
    if (s_out_gap_active && USBHS_DevAltSetting[1] && !I2S_PlayFmtPending()) {
        uint32_t elapsed=(uint16_t)(now-s_last_out_frame);
        /* The current interval can still receive OUT. Only earlier, completed
         * intervals are missing. Snapshot and accounting must be atomic with OUT. */
        uint32_t missing = elapsed > 1u ? elapsed-1u : 0u;
        uint32_t target = missing < 8u ? missing : 8u;
        if (missing > i2s_gap_max) i2s_gap_max = missing;
        if (target > s_out_gap_accounted) {
            lost = target - s_out_gap_accounted;
            s_out_gap_accounted = (uint8_t)target;
        }
        if (missing >= 8u) s_out_gap_active = 0u;
    }
    UsbFormatUnlock(irq);
    if (lost) I2S_PlayLostPackets(lost); /* PCM work always outside the lock. */
}

static uint32_t USB_RxStride(void) {
    return USBHS_DevSpeed == USBHS_SPEED_FULL ? 1024u : 512u;
}

static uint8_t *USB_RxBank(uint32_t bank) {
    return bank < 2u ? USBHS_EP1_Rx_Buf[bank] : usb_rx_extra[bank-2u];
}

static void USB_RxInit(void) {
    uint32_t i;
    usb_rx_epoch++;
    usb_rx_read=usb_rx_write=0u;
    for(i=0u;i<6u;i++) usb_rx_bank_state[i]=0u;
    usb_rx_hw_bank[0]=0u;usb_rx_hw_bank[1]=1u;
    usb_rx_bank_state[0]=2u;
#if PLAY_EP1_BUF_MOD
    usb_rx_bank_state[1]=2u;
#endif
}

static void USB_RxComplete(uint32_t which, uint16_t len) {
    uint32_t w=usb_rx_write, slot=w&7u, next;
    uint8_t bank=usb_rx_hw_bank[which];
    if(I2S_PlayFmtPending() || !USBHS_DevAltSetting[1]) return;
    if(len>1024u) {usb_rx_queue_drop++;return;}
    for(next=0u;next<6u;next++) if(usb_rx_bank_state[next]==0u) break;
    if(next==6u) {usb_rx_queue_drop++;return;} /* Keep this bank hardware-owned. */
    usb_rx_bank_state[next]=2u;usb_rx_hw_bank[which]=(uint8_t)next;
#if PLAY_EP1_BUF_MOD
    if(which==1u) USBHSD->UEP1_TX_DMA=(uint32_t)USB_RxBank(next);
    else
#endif
    USBHSD->UEP1_RX_DMA=(uint32_t)USB_RxBank(next);
    usb_rx_bank_state[bank]=1u;
    usb_rx_bank_queue[slot]=bank;usb_rx_len[slot]=len;
    usb_rx_seq[slot]=s_play_format_seq;
    __asm volatile("fence rw, rw" ::: "memory");
    usb_rx_write=w+1u;
    if(usb_rx_write-usb_rx_read>usb_rx_queue_peak) usb_rx_queue_peak=usb_rx_write-usb_rx_read;
}

static void USB_UpdateFeedback(void) {
    uint32_t f=I2S_FeedbackGet();
    if (USBHS_DevSpeed==USBHS_SPEED_FULL) f <<= 1;
    *(uint32_t *)USBHS_EP1_Tx_Buf=f;
    USBHSD->UEP2_TX_LEN=USBHS_DevSpeed==USBHS_SPEED_FULL ? 3u : 4u;
}

static void USB_CapSubmit(void) {
    uint32_t stride=USB_RxStride(), count=stride==512u ? 2u : 1u;
    uint32_t i;
    if (usb_cap_active_slot >= 0) return;
    for(i=0u;i<count;i++) if(usb_cap_state[i]==2u) {
        USBHSD->UEP3_TX_DMA=(uint32_t)(USBHS_EP2_Tx_Buf+i*stride);
        USBHSD->UEP3_TX_LEN=usb_cap_len[i];
        usb_cap_state[i]=3u;
        usb_cap_active_slot=(int8_t)i;
        __asm volatile("fence rw, rw" ::: "memory");
        USBHSD->UEP3_TX_CTRL=USBHS_UEP_T_TOG_DATA0|USBHS_UEP_T_RES_ACK;
        return;
    }
}

static void USB_CaptureService(void) {
    uint32_t i,stride=USB_RxStride();
    if(!USBHS_CaptureActive) return;
    uint32_t submit_irq=UsbFormatLock();
    USB_CapSubmit();
    UsbFormatUnlock(submit_irq);
    uint32_t slots=stride==512u ? 2u : 1u;
    for(i=0u;i<slots;i++) {
        uint32_t irq=UsbFormatLock(), seq=s_cap_format_seq;
        if(!USBHS_CaptureActive || usb_cap_state[i]!=0u) {UsbFormatUnlock(irq);continue;}
        usb_cap_state[i]=1u;
        UsbFormatUnlock(irq);
        uint16_t len=(uint16_t)I2S_CapRead(USBHS_EP2_Tx_Buf+i*stride,
            USBHS_DevSpeed==USBHS_SPEED_FULL ? 8u : 1u,stride);
        irq=UsbFormatLock();
        if(USBHS_CaptureActive && seq==s_cap_format_seq) {
            /* Publish an older READY bank before making this one visible. */
            USB_CapSubmit();
            usb_cap_len[i]=len;usb_cap_state[i]=2u;
            USB_CapSubmit();
        } else usb_cap_state[i]=0u;
        UsbFormatUnlock(irq);
    }
}

void USB_StreamService(void) {
    uint32_t i;
    USB_CaptureService();
    /* Drain playback backlog, servicing capture between every PCM packet. */
    for(i=0u;i<6u;i++) {
        uint32_t irq=UsbFormatLock(), r=usb_rx_read, slot=r&7u, epoch=usb_rx_epoch;
        uint8_t bank;
        if(r==usb_rx_write) {UsbFormatUnlock(irq);break;}
        bank=usb_rx_bank_queue[slot];usb_rx_bank_state[bank]=3u;
        UsbFormatUnlock(irq);
        if(usb_rx_seq[slot]==s_play_format_seq && !I2S_PlayFmtPending())
            I2S_PlayFill(USB_RxBank(bank),usb_rx_len[slot]);
        else usb_rx_queue_stale++;
        irq=UsbFormatLock();
        if(epoch!=usb_rx_epoch) {UsbFormatUnlock(irq);return;} /* Bus reset during conversion. */
        usb_rx_bank_state[bank]=0u;usb_rx_read=r+1u;
        UsbFormatUnlock(irq);
        USB_CaptureService();
    }
    if(usb_sof_pending) {
        uint32_t irq=UsbFormatLock();
        usb_sof_pending=0u;
        UsbFormatUnlock(irq);
        USB_PlayGapPoll();
        /* Completion itself is handled in the ISR. This is only a timeout. */
        if(ep4_armed && ++ep4_chk >= (USBHS_DevSpeed==USBHS_SPEED_FULL ? 64u : 512u)) {
            irq=UsbFormatLock();
            if(ep4_armed) {
                USBHSD->UEP4_TX_CTRL=(USBHSD->UEP4_TX_CTRL & ~USBHS_UEP_T_RES_MASK)|USBHS_UEP_T_RES_NAK;
                ep4_armed=0u; usb_ep4_to++;
            }
            UsbFormatUnlock(irq);
        }
    }
    USB_CaptureService();
    /* Prepare PCM banks first. The highest-priority USB ISR then receives a
     * short EP0 OUT in a bounded polling window, without IRQ-entry latency. */
    if(ep0_out_pending && !ep0_out_poll_ready) {
        ep0_out_poll_ready=1u;
        NVIC_SetPendingIRQ(USBHS_IRQn);
    }
}

static void __attribute__((noinline)) USB_Ep0In(void) {
    uint16_t len;
                    if (USBHS_SetupReqLen == 0) {
                        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
                    }
                    if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
                        /* Standard request endpoint 0 Data upload */
                        switch (USBHS_SetupReqCode) {
                        case USB_GET_DESCRIPTOR:
                            
                            if (USBHS_SetupReqLen) {
                                len = USBHS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                                memcpy (USBHS_EP0_Buf, pUSBHS_Descr, len);
                                USBHS_SetupReqLen -= len;
                                pUSBHS_Descr += len;
                                USBHSD->UEP0_TX_LEN = len;
                                USBHSD->UEP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                            } else if (USB_DescIsOtherSpeed != 0u) {
                                /* OTHER_SPEED_CONFIGURATION 的字节已经全部发给主机了：
                                 * 把共享描述符缓冲和随之重建的能力表恢复到当前总线速度那一份。 */
                                USB_DescIsOtherSpeed = 0u;
                                USB_RestoreSpeedDesc ((uint8_t)(USBHS_DevSpeed == USBHS_SPEED_HIGH));
                            }
                            break;

                        case USB_SET_ADDRESS:
                            USBHSD->DEV_AD = USBHS_DevAddr;
                            break;

                        default:
                            USBHSD->UEP0_TX_LEN = 0;
                            break;
                        }
                    } else if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
                        /* Non-standard request endpoint 0 Data upload */
                        if (USBHS_SetupReqType & AUDIO_REQ_GET_MSK) {  // GET
                            switch (USBHS_SetupReqCode) {
                            case AUDIO_REQ_RANGE:
                                
                                if (USBHS_SetupReqLen) {
                                    len = USBHS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                                    memcpy (USBHS_EP0_Buf, pUSBHS_Descr, len);
                                    USBHS_SetupReqLen -= len;
                                    pUSBHS_Descr += len;
                                    USBHSD->UEP0_TX_LEN = len;
                                    USBHSD->UEP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                                }
                                break;

                            default:
                                /* Only GET_CUR lands here (4 bytes, already in
                                 * EP0_Buf).  Terminate a longer transfer with a
                                 * ZLP, otherwise leave the endpoint alone so the
                                 * status stage can complete. */
                                if (USBHS_SetupReqLen) {
                                    USBHSD->UEP0_TX_LEN = 0;
                                    USBHSD->UEP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                                }
                                break;
                            }
                        } else {
                            USBHSD->UEP0_TX_LEN = 0;
                        }
                    } else {
                        // not supported
                    }

                    /* test mode */
                    if (USBHS_Test_Flag & 0x80) {
                        USB_TestMode_Deal();
                    }

}

static void __attribute__((noinline)) USB_Ep0Out(uint8_t intst, uint16_t rx_len) {
    uint16_t len;
    usb_ep0_out_count++;
    usb_ep0_out_last=((uint32_t)intst<<24)|((uint32_t)USBHS_SetupReqLen<<12)|rx_len;
    memcpy((void *)&usb_ep0_out_data,USBHS_EP0_Buf,4u);
    if(!(intst & USBHS_UIS_TOG_OK)) usb_ep0_out_skip++;
                    len = rx_len;
                    if (len > USBHS_SetupReqLen || len > DEF_USBD_UEP0_SIZE) {
                        ep0_out_pending=0u;
                        USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_STALL;
                        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_STALL;
                    } else if ((intst & USBHS_UIS_TOG_OK) != 0u) {
                        if (len != 0u && USB_SetCurData (USBHS_EP0_Buf, len) == 0u) {
                            ep0_out_pending=0u;
                            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_STALL;
                            USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_STALL;
                            return;
                        }
                        USBHS_SetupReqLen -= len;
                        if (USBHS_SetupReqLen == 0u) {
                            /* A zero-length OUT here can also be a control-IN status stage. */
                            USBHSD->UEP0_TX_LEN = 0u;
                            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                            ep0_out_pending=0u;
                        }
                    }

}

static void __attribute__((noinline)) USB_SetupEvent(void) {
    uint16_t len;
    uint8_t errflag;
        ep0_out_pending=0u;
        USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_NAK;
        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_NAK;

        /* 诊断：SETUP 到底有没有进到这里、原始八个字节是什么。
         * 「播放中改录音采样率」时固件没解析到 SET_CUR，靠这三个量分辨是
         * (a) SETUP 根本没来 / (b) 来了但内容被数据阶段覆盖 / (c) 来了也解析了。 */
        usb_setup_cnt++;
        usb_setup_w0 = (uint32_t)pUSBHS_SetupReqPak->bRequestType |
                       ((uint32_t)pUSBHS_SetupReqPak->bRequest << 8) |
                       ((uint32_t)pUSBHS_SetupReqPak->wValue << 16);
        usb_setup_w1 = (uint32_t)pUSBHS_SetupReqPak->wIndex |
                       ((uint32_t)pUSBHS_SetupReqPak->wLength << 16);

        /* Store All Setup Values */
        USBHS_SetupReqType = pUSBHS_SetupReqPak->bRequestType;
        USBHS_SetupReqCode = pUSBHS_SetupReqPak->bRequest;
        USBHS_SetupReqLen = pUSBHS_SetupReqPak->wLength;
        USBHS_SetupReqValue = pUSBHS_SetupReqPak->wValue;
        USBHS_SetupReqIndex = pUSBHS_SetupReqPak->wIndex;

        len = 0;
        errflag = 0;
        if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
            /* usb standard request processing */
            switch (USBHS_SetupReqCode) {
            /* get device/configuration/string/report/... descriptors */
            case USB_GET_DESCRIPTOR:
                switch ((uint8_t)(USBHS_SetupReqValue >> 8)) {
                /* get usb device descriptor */
                case USB_DESCR_TYP_DEVICE:
                    /* RAM 副本：idProduct 按芯片 UID 派生 */
                    pUSBHS_Descr = USB_GetDeviceDesc();
                    len = DEF_USBD_DEVICE_DESC_LEN;
                    break;

                    /* get usb configuration descriptor */
                case USB_DESCR_TYP_CONFIG:
                    /* Query current usb speed */
                    if ((USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK) == USBHS_SPEED_HIGH) {
                        /* High speed mode */
                        USBHS_DevSpeed = USBHS_SPEED_HIGH;
                        USBHS_DevMaxPackLen = DEF_USBD_HS_PACK_SIZE;
                    } else {
                        /* Full speed mode */
                        USBHS_DevSpeed = USBHS_SPEED_FULL;
                        USBHS_DevMaxPackLen = DEF_USBD_FS_PACK_SIZE;
                    }
                    I2S_SetUsbFrameHz(USBHS_DevSpeed == USBHS_SPEED_HIGH ? 8000u : 1000u);

                    /* The configuration descriptor is built at boot and
                     * patched here for the speed the host enumerated at. */
                    {
                        uint16_t cfg_len = 0;
                        pUSBHS_Descr = USB_GetConfigDesc (
                            (uint8_t)(USBHS_DevSpeed == USBHS_SPEED_HIGH), 0,
                            &cfg_len);
                        len = cfg_len;
                    }
                    break;

                    /* get usb string descriptor */
                case USB_DESCR_TYP_STRING:
                    switch ((uint8_t)(USBHS_SetupReqValue & 0xFF)) {
                    /* Descriptor 0, Language descriptor */
                    case DEF_STRING_DESC_LANG:
                        pUSBHS_Descr = MyLangDescr;
                        len = DEF_USBD_LANG_DESC_LEN;
                        break;

                        /* Descriptor 1, Manufacturers String descriptor */
                    case DEF_STRING_DESC_MANU:
                        pUSBHS_Descr = USB_GetManufacturerDesc();
                        len = pUSBHS_Descr[0];
                        break;

                        /* Descriptor 2, Product String descriptor */
                    case DEF_STRING_DESC_PROD:
                        pUSBHS_Descr = USB_GetProductDesc();
                        len = pUSBHS_Descr[0];
                        break;

                        /* Descriptor 3, Serial-number String descriptor */
                    case DEF_STRING_DESC_SERN:
                        /* "CH32V305-<UID>"，长度随前缀配置变，直接取 bLength */
                        pUSBHS_Descr = USB_GetSerialDesc();
                        len = pUSBHS_Descr[0];
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                    }
                    break;

                    /* get usb device qualify descriptor */
                case USB_DESCR_TYP_QUALIF:
                    pUSBHS_Descr = MyQuaDesc;
                    len = DEF_USBD_QUALFY_DESC_LEN;
                    break;

#if USB_HID_ENABLE
                    /* HID：主机用标准 GET_DESCRIPTOR 取 0x21(HID) / 0x22(Report)。
                     * Windows 的 hidclass 枚举时一定会来要这两份，缺了 HID 功能会报 Code 10*/
                case 0x21:
                case 0x22:
                    if ((USBHS_SetupReqIndex & 0xFF) == USB_HID_IF) {
                        if ((uint8_t)(USBHS_SetupReqValue >> 8) == 0x22) {
                            pUSBHS_Descr = USB_HID_ReportDesc;
                            len = USB_HID_REPORT_DESC_LEN;
                        } else {
                            /* HID 描述符本体就在配置描述符里，回一段等价的 */
                            static const uint8_t hid_desc[9] = {
                                0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
                                (uint8_t)(USB_HID_REPORT_DESC_LEN & 0xFF),
                                (uint8_t)(USB_HID_REPORT_DESC_LEN >> 8)};
                            pUSBHS_Descr = hid_desc;
                            len = sizeof (hid_desc);
                        }
                    } else {
                        errflag = 0xFF;
                    }
                    break;
#endif /* USB_HID_ENABLE */

                    /* get usb BOS descriptor */
                case USB_DESCR_TYP_BOS:
                    /* USB 2.00 DO NOT support BOS descriptor */
                    errflag = 0xFF;
                    break;

                    /* get usb other-speed descriptor */
                case USB_DESCR_TYP_SPEED:
                    /* Same generator, opposite speed, bDescriptorType forced
                     * to OTHER_SPEED_CONFIGURATION by USB_GetConfigDesc(). */
                    {
                        uint16_t osc_len = 0;
                        pUSBHS_Descr = USB_GetConfigDesc (
                            (uint8_t)(USBHS_DevSpeed == USBHS_SPEED_HIGH), 1,
                            &osc_len);
                        len = osc_len;
                    }
                    break;

                default:
                    errflag = 0xFF;
                    break;
                }

                /* Copy Descriptors to Endp0 DMA buffer */
                if (USBHS_SetupReqLen > len) {
                    USBHS_SetupReqLen = len;
                }
                len = (USBHS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                memcpy (USBHS_EP0_Buf, pUSBHS_Descr, len);
                pUSBHS_Descr += len;
                break;

                /* Set usb address */
            case USB_SET_ADDRESS:
                USBHS_DevAddr = (uint16_t)(USBHS_SetupReqValue & 0xFF);
                break;

                /* Get usb configuration now set */
            case USB_GET_CONFIGURATION:
                USBHS_EP0_Buf[0] = USBHS_DevConfig;
                if (USBHS_SetupReqLen > 1) {
                    USBHS_SetupReqLen = 1;
                }
                break;

                /* Set usb configuration to use */
            case USB_SET_CONFIGURATION:
                USBHS_DevConfig = (uint8_t)(USBHS_SetupReqValue & 0xFF);
                USBHS_DevEnumStatus = 0x01;
                break;

                /* Clear or disable one usb feature */
            case USB_CLEAR_FEATURE:
                if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
                    /* clear one device feature */
                    if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == 0x01) {
                        /* clear usb sleep status, device not prepare to sleep */
                        USBHS_DevSleepStatus &= ~0x01;
                    } else {
                        errflag = 0xFF;
                    }
                } else if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                    /* Set End-point Feature */
                    if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == USB_REQ_FEAT_ENDP_HALT) {
                        /* Clear End-point Feature */
                        switch ((uint8_t)(USBHS_SetupReqIndex & 0xFF)) {
                        case (DEF_UEP1 | DEF_UEP_OUT):
                            /* Set End-point 1 OUT ACK */
                            USBHSD->UEP1_RX_CTRL = USBHS_UEP_R_RES_ACK;
                            break;

                        case (DEF_UEP1 | DEF_UEP_IN):
                            /* 反馈端点（等时 IN）本来就是常开 ACK */
                            USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_TOG_DATA0;
                            break;

                        case (DEF_UEP2 | DEF_UEP_IN):
                            /* 0x82 = 播放反馈：常开 ACK */
                            USBHSD->UEP2_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_ACK;
                            break;

                        case (DEF_UEP3 | DEF_UEP_IN):
                            /* 0x83 = 录音数据：按流状态 ACK/NAK */
                            USBHSD->UEP3_TX_CTRL = (uint8_t)(USBHS_UEP_T_TOG_DATA0 |
                                                             (USBHS_CaptureActive ? USBHS_UEP_T_RES_ACK
                                                                                  : USBHS_UEP_T_RES_NAK));
                            break;

                        default:
                            errflag = 0xFF;
                            break;
                        }
                    } else {
                        errflag = 0xFF;
                    }
                } else {
                    errflag = 0xFF;
                }
                break;

                /* set or enable one usb feature */
            case USB_SET_FEATURE:
                if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
                    /* Set Device Feature */
                    if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == USB_REQ_FEAT_REMOTE_WAKEUP) {
                        if (MyCfgDescr_FS[7] & 0x20) {
                            /* Set Wake-up flag, device prepare to sleep */
                            USBHS_DevSleepStatus |= 0x01;
                        } else {
                            errflag = 0xFF;
                        }
                    } else if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == 0x02) {
                        /* test mode deal */
                        if ((USBHS_SetupReqIndex == 0x0100) || (USBHS_SetupReqIndex == 0x0200) || (USBHS_SetupReqIndex == 0x0300) || (USBHS_SetupReqIndex == 0x0400)) {
                            /* Set the flag and wait for the status to be uploaded before proceeding with the actual operation */
                            USBHS_Test_Flag |= 0x80;
                        }
                    } else {
                        errflag = 0xFF;
                    }
                } else if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                    /* Set End-point Feature */
                    if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == USB_REQ_FEAT_ENDP_HALT) {
                        /* Set end-points status stall */
                        switch ((uint8_t)(USBHS_SetupReqIndex & 0xFF)) {
                        case (DEF_UEP1 | DEF_UEP_OUT):
                            /* Set End-point 1 OUT STALL */
                            USBHSD->UEP1_RX_CTRL = (USBHSD->UEP1_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_STALL;
                            break;

                        case (DEF_UEP1 | DEF_UEP_IN):
                            /* Set End-point 1 IN STALL */
                            USBHSD->UEP1_TX_CTRL = (USBHSD->UEP1_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
                            break;

                        case (DEF_UEP2 | DEF_UEP_IN):
                            USBHSD->UEP2_TX_CTRL = (USBHSD->UEP2_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
                            break;

                        case (DEF_UEP3 | DEF_UEP_IN):
                            USBHSD->UEP3_TX_CTRL = (USBHSD->UEP3_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
                            break;

                        default:
                            errflag = 0xFF;
                            break;
                        }
                    }
                }
                break;

                /* This request allows the host to select another setting for the specified interface  */
            case USB_GET_INTERFACE: {
                uint8_t ifidx = (uint8_t)(USBHS_SetupReqIndex & 0xFF);
                /* 5 个接口：0=AC播放 1=AS播放 2=AC录音 3=AS录音 4=HID */
                if(ifidx>=USB_INTERFACE_COUNT) {errflag=0xFF;break;}
                USBHS_EP0_Buf[0] = ifidx==USB_IF_PLAY ? USBHS_DevAltSetting[1] : ifidx==USB_IF_CAP ? USBHS_DevAltSetting[2] : 0u;
                if (USBHS_SetupReqLen > 1) {
                    USBHS_SetupReqLen = 1;
                }
                break;
            }

            case USB_SET_INTERFACE: {
                uint8_t ifidx = (uint8_t)USBHS_SetupReqIndex;
                uint8_t alt = (uint8_t)USBHS_SetupReqValue;
                if (USBHS_SetupReqType != 0x01u || USBHS_SetupReqLen != 0u ||
                    USBHS_SetupReqIndex >= USB_INTERFACE_COUNT || USBHS_SetupReqValue > 255u ||
                    ((ifidx != USB_IF_PLAY && ifidx != USB_IF_CAP) && alt != 0u) ||
                    (ifidx == USB_IF_PLAY && alt >= USB_PlayAltCount) ||
                    (ifidx == USB_IF_CAP && alt >= USB_CapAltCount)) {
                    errflag = 0xFF;
                    break;
                }
                ifidx=ifidx==USB_IF_PLAY ? 1u : ifidx==USB_IF_CAP ? 2u : 0u;
                USBHS_DevAltSetting[ifidx] = alt;
                if (ifidx == 1u) {
                    USB_RequestPlayRecovery();
                    I2S_FeedbackReset();
                    USBHS_PlayRateReq = USB_GetCurrentRate();
                    if (!alt) {
                        I2S_PlayStop();
                        AMP_SetMute (Bit_RESET);
                    }
                } else if (ifidx == 2u) {
                    USB_RequestCapRecovery();
                    USBHS_CapRateReq = USB_GetCapRate();
                }
                break;
            }

                /* host get status of specified device/interface/end-points */
            case USB_GET_STATUS:
                USBHS_EP0_Buf[0] = 0x00;
                USBHS_EP0_Buf[1] = 0x00;
                if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                    switch ((uint8_t)(USBHS_SetupReqIndex & 0xFF)) {
                    case (DEF_UEP_OUT | DEF_UEP1):
                        if (((USBHSD->UEP1_RX_CTRL) & USBHS_UEP_R_RES_MASK) == USBHS_UEP_R_RES_STALL) {
                            USBHS_EP0_Buf[0] = 0x01;
                        }
                        break;

                    case (DEF_UEP_IN | DEF_UEP1):
                        if (((USBHSD->UEP1_TX_CTRL) & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL) {
                            USBHS_EP0_Buf[0] = 0x01;
                        }
                        break;

                    case (DEF_UEP_IN | DEF_UEP2):
                        if (((USBHSD->UEP2_TX_CTRL) & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL) {
                            USBHS_EP0_Buf[0] = 0x01;
                        }
                        break;

                    case (DEF_UEP_IN | DEF_UEP3):
                        if (((USBHSD->UEP3_TX_CTRL) & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL) {
                            USBHS_EP0_Buf[0] = 0x01;
                        }
                        break;

                    case (DEF_UEP_IN | DEF_UEP4):
                        if (((USBHSD->UEP4_TX_CTRL) & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL) {
                            USBHS_EP0_Buf[0] = 0x01;
                        }
                        break;

                    default:
                        /* EP0 的状态永远是「未 halt」，回 0 即可 */
                        if ((uint8_t)(USBHS_SetupReqIndex & 0x0F) == DEF_UEP0) {
                            break;
                        }
                        errflag = 0xFF;
                        break;
                    }
                } else if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
                    if (USBHS_DevSleepStatus & 0x01) {
                        USBHS_EP0_Buf[0] = 0x02;
                    }
                }

                if (USBHS_SetupReqLen > 2) {
                    USBHS_SetupReqLen = 2;
                }
                break;

            default:
                errflag = 0xFF;
                break;
            }
        } else if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
#if USB_HID_ENABLE
            /* HID 类的三个标准请求（recipient = interface）。
             * hidclass 枚举时会发 SET_IDLE / SET_PROTOCOL，不处理会 STALL，
             * 设备管理器里就是「该设备无法启动」。这里统一 ACK 掉。 */
            if ((uint8_t)(USBHS_SetupReqIndex & 0xFF) == USB_HID_IF &&
                (USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF) {
                switch (USBHS_SetupReqCode) {
                case 0x09:   /* SET_REPORT */
                case 0x0A:   /* SET_IDLE  */
                case 0x0B:   /* SET_PROTOCOL */
                    len = 0; /* 无数据阶段，交给统一收尾回 ACK */
                    break;
                default:
                    errflag = 0xFF;
                    break;
                }
            } else
#endif                                                      /* USB_HID_ENABLE */
                if (USBHS_SetupReqType == 0xA1u && (USBHS_SetupReqIndex & 0xFFu) == 0u && USB_EntityPresent(USBHS_SetupReqIndex>>8)) /* GET */
                {
                    if (USBHS_SetupReqCode == AUDIO_REQ_CUR) {
                        uint8_t clk = ClockIdOf (USBHS_SetupReqIndex);
                        uint8_t ent = (uint8_t)(USBHS_SetupReqIndex >> 8);
                        uint8_t sel = (uint8_t)(USBHS_SetupReqValue >> 8);

                        /* 先按**实体 ID**分流，再看控制选择器。
                         * 不能先看选择器：CS 是按实体类型各自编号的，时钟源的
                         * 1 是"采样频率"、Feature Unit 的 1 是"Mute"，数值撞车
                         * （Audio 2.0 附录 A.14 / A.17.7 的编号就是从 1 各起一套）。 */
                        if (((ent == AUDIO_FU_ID) || (ent == AUDIO_FU_CAP_ID)) &&
                            (USBHS_SetupReqValue & 0xFFu) == 0u) {
                            uint8_t ch = (uint8_t)(USBHS_SetupReqValue & 0xFF);
                            uint8_t is_cap = (uint8_t)((ent == AUDIO_FU_CAP_ID) ? 1u : 0u);
                            static uint8_t fu_tmp[2];

                            if (sel == FU_MUTE_CONTROL) {
                                fu_tmp[0] = (uint8_t)((is_cap != 0u) ? Volume_GetAdcMute ()
                                                                     : Volume_GetDacMute ());
                                len = 1;
                                pUSBHS_Descr = fu_tmp;
                            } else if (sel == FU_VOLUME_CONTROL) {
                                int16_t v = (is_cap != 0u) ? Volume_GetAdc (ch)
                                                           : Volume_GetDac (ch);

                                fu_tmp[0] = (uint8_t)((uint16_t)v & 0xFF);
                                fu_tmp[1] = (uint8_t)((uint16_t)v >> 8);
                                len = 2;
                                pUSBHS_Descr = fu_tmp;
                            } else {
                                errflag = 0xFF;
                            }
                        } else if (USBHS_SetupReqValue == (CS_SAM_FREQ_CONTROL << 8) &&
                                   (clk == AUDIO_CLOCK_ID || clk == AUDIO_CLOCK_ID_CAP)) {
                            if (clk == AUDIO_CLOCK_ID) {
                                len = sizeof (SampleFreqPlay);
                                pUSBHS_Descr = SampleFreqPlay;
                            } else {
                                len = sizeof (SampleFreqCap);
                                pUSBHS_Descr = SampleFreqCap;
                            }
                        } else if (USBHS_SetupReqValue == (CS_CLOCK_VALID_CONTROL << 8) &&
                                   (clk == AUDIO_CLOCK_ID || clk == AUDIO_CLOCK_ID_CAP)) {
                            /* Clock Validity：描述符里 bmControls=0x03 广告了它，
                             * 主机（audiosrv/usbaudio2）枚举时一定会来读。 */
                            static uint8_t clk_valid = 1;
                            len = 1;
                            pUSBHS_Descr = &clk_valid;
                        } else {
                            errflag = 0xFF;
                        }
                    } else if (USBHS_SetupReqCode == AUDIO_REQ_RANGE) {
                        uint8_t clk = ClockIdOf (USBHS_SetupReqIndex);
                        uint8_t ent = (uint8_t)(USBHS_SetupReqIndex >> 8);
                        uint8_t sel = (uint8_t)(USBHS_SetupReqValue >> 8);

                        if ((ent == AUDIO_FU_ID || ent == AUDIO_FU_CAP_ID) &&
                            (USBHS_SetupReqValue & 0xFFu) == 0u && sel == FU_VOLUME_CONTROL) {
                            
                            static uint8_t fu_range[8];
                            int16_t vmin = (ent == AUDIO_FU_CAP_ID) ? (int16_t)VOL_ADC_MIN_M256
                                                                    : (int16_t)VOL_DAC_MIN_M256;
                            int16_t vmax = (ent == AUDIO_FU_CAP_ID) ? (int16_t)VOL_ADC_MAX_M256
                                                                    : (int16_t)VOL_DAC_MAX_M256;
                            int16_t vres = (ent == AUDIO_FU_CAP_ID) ? (int16_t)VOL_ADC_RES_M256
                                                                    : (int16_t)VOL_DAC_RES_M256;

                            fu_range[0] = 1; /* wNumSubRanges = 1 */
                            fu_range[1] = 0;
                            fu_range[2] = (uint8_t)((uint16_t)vmin & 0xFF);
                            fu_range[3] = (uint8_t)((uint16_t)vmin >> 8);
                            fu_range[4] = (uint8_t)((uint16_t)vmax & 0xFF);
                            fu_range[5] = (uint8_t)((uint16_t)vmax >> 8);
                            fu_range[6] = (uint8_t)((uint16_t)vres & 0xFF);
                            fu_range[7] = (uint8_t)((uint16_t)vres >> 8);
                            len = sizeof (fu_range);
                            pUSBHS_Descr = fu_range;
                        } else if (USBHS_SetupReqValue == (CS_SAM_FREQ_CONTROL << 8) &&
                                   (clk == AUDIO_CLOCK_ID || clk == AUDIO_CLOCK_ID_CAP)) {
                            if (USBHS_DevSpeed == USBHS_SPEED_FULL) {
                                len = USB_RangeDesc_FS_Len;
                                pUSBHS_Descr = USB_RangeDesc_FS;
                            } else {
                                len = USB_RangeDesc_HS_Len;
                                pUSBHS_Descr = USB_RangeDesc_HS;
                            }
                        } else {
                            errflag = 0xFF;
                        }
                    } else {
                        errflag = 0xFF;
                    }

                    /* Copy Descriptors to Endp0 DMA buffer */
                    if (USBHS_SetupReqLen > len) {
                        USBHS_SetupReqLen = len;
                    }
                    len = (USBHS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                    memcpy (USBHS_EP0_Buf, pUSBHS_Descr, len);
                    pUSBHS_Descr += len;
                } else {
                    uint16_t expected = USB_CurWriteLength();
                    if (expected == 0u || USBHS_SetupReqLen != expected) errflag = 0xFF;
                }
        } else {
            /* usb non-standard request processing */
            errflag = 0xFF;
        }

        /* errflag = 0xFF means a request not support or some errors occurred, else correct */
        if (errflag == 0xFF) {
            /* if one request not support, return stall */
            USBHSD->UEP0_TX_CTRL =
                USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_STALL;
            USBHSD->UEP0_RX_CTRL =
                USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_STALL;
        } else {
            /* end-point 0 data Tx/Rx */
            if (USBHS_SetupReqType & DEF_UEP_IN) {
                /* tx */
                len = (USBHS_SetupReqLen > DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                USBHS_SetupReqLen -= len;
                USBHSD->UEP0_TX_LEN = len;
                /* A short GET_CUR can complete <1us after feedback IN and its
                 * completion latch may be replaced. Prepare the zero-length
                 * DATA1 status OUT now; it never overwrites the IN payload.
                 * Waiting for the final IN interrupt can NAK status forever. */
                USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
                USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
            } else {
                /* rx */
                if (USBHS_SetupReqLen == 0) {
                    USBHSD->UEP0_TX_LEN = 0;
                    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                } else {
                    ep0_out_pending=1u;
                    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_NAK;
                }
            }
        }
        USBHSD->INT_FG = USBHS_UIF_SETUP_ACT;

}

static void __attribute__((noinline)) USB_ResetEvent(void) {
        ep0_out_pending=ep0_out_poll_ready=0u;
        /* usb reset interrupt processing */
        USBHS_DevConfig = 0;
        USBHS_DevAddr = 0;
        USBHS_DevSleepStatus = 0;
        USBHS_DevEnumStatus = 0;

        USBHSD->DEV_AD = 0;
        /* Re-sample the bus speed on every reset: it used to be latched
         * only inside GET_DESCRIPTOR(CONFIG) and kept its power-on value 0
         * (= full speed) until that happened, which made GET_RANGE hand out
         * the 96k/48k table on a high-speed port. */
        USBHS_DevSpeed = (uint8_t)(USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK);
        USBHS_DevAltSetting[0] = 0;
        USBHS_DevAltSetting[1] = 0;
        USBHS_DevAltSetting[2] = 0;
        USBHS_DevAltSetting[3] = 0;
        USBHS_DevAltSetting[4] = 0; /* 接口 4 = HID */
        /* 换速后「当前采样率」可能超出新速度能承载的上限 */
        {
            uint32_t cap_max = (USBHS_DevSpeed == USBHS_SPEED_HIGH) ? 384000u : 96000u;
            uint32_t pr = USB_GetCurrentRate();
            uint32_t cr = USB_GetCapRate();
            USB_SetCurrentRate ((pr > cap_max) ? cap_max : pr,
                                (cr > cap_max) ? cap_max : cr);
        }
        USBHS_AppliedRate = USB_GetCurrentRate();
        USBHS_AppliedCapRate = USB_GetCapRate();
        /* 重新枚举时把上一轮的挂账清掉，避免用旧请求把刚复位的端点又打开 */
        USBHS_PlayRatePending = 0u;
        USBHS_CapRatePending = 0u;
        USBHS_CapArmPending = 0u;
        USBHS_PlayRateReq = USB_GetCurrentRate();
        USBHS_CapRateReq = USB_GetCapRate();
        if (USBHS_CapRateReq == 0u) {
            USBHS_CapRateReq = 48000u;
        }
        USBHS_Device_Endp_Init();
        USB_IntBusyAuto (); /* 复位后没有流 → 回到 INT_BUSY=1（保护 EP0） */
        USBHS_CaptureActive = 0;
        I2S_SetPlayBits (32);
        I2S_SetCapBits (32);
        I2S_FeedbackReset();
        /* 下次开流必须整段重开（见 I2S_PlayHealthCheck 的注释）*/
        USB_RequestPlayRecovery();
        USB_RequestCapRecovery();
        I2S_PlayStop();
        USBHSD->INT_FG = USBHS_UIF_BUS_RST;

    usb_cap_state[0]=usb_cap_state[1]=0u;
    usb_cap_active_slot=-1;
    usb_sof_pending=0u;
}

static void __attribute__((noinline,optimize("O2"))) USB_TransferEvent(uint8_t st,uint16_t len) {
    uint8_t token=st & USBHS_UIS_TOKEN_MASK, ep=st & USBHS_UIS_ENDP_MASK;
    uint16_t phase=(uint16_t)TIM8->CNT;
    /* NAK is a retry, not a completed data/status stage. */
    if(ep==DEF_UEP0 && (token==USBHS_UIS_TOKEN_IN || token==USBHS_UIS_TOKEN_OUT) &&
       (st & USBHS_UIS_IS_NAK)) return;
            if(token==USBHS_UIS_TOKEN_SOF) {
                /* Reset-time SPEED_TYPE can still be FS before HS negotiation.
                 * Keep packet sizing and feedback on the negotiated bus clock. */
                I2S_SetUsbFrameHz((USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK) ==
                                  USBHS_SPEED_HIGH ? 8000u : 1000u);
                usb_last_sof_tick=tim6_tick;
                USBHS_DevSleepStatus &= ~2u;
                usb_ep4_sof++;usb_sof_pending=1u;
                /* Non-isochronous EP4 has no packet-start interrupt. Hardware
                 * toggles only after a successful host ACK, so close it before
                 * the next HID poll even if UIF_TRANSFER was overwritten. */
                if(ep4_armed &&
                   (USBHSD->UEP4_TX_CTRL & USBHS_UEP_T_TOG_DATA1)!=ep4_tog_exp) {
                    USBHSD->UEP4_TX_CTRL=(USBHSD->UEP4_TX_CTRL & ~USBHS_UEP_T_RES_MASK)|USBHS_UEP_T_RES_NAK;
                    ep4_armed=0u;ep4_chk=0u;usb_ep4_ok++;
                }
            } else if(token==USBHS_UIS_TOKEN_OUT && ep==DEF_UEP0) {
                USB_Ep0Out(st,len);
            } else if(token==USBHS_UIS_TOKEN_IN && ep==DEF_UEP0) {
                uint16_t owed=USBHS_SetupReqLen;
                USB_Ep0In();
                if(owed && (USBHS_SetupReqType & 0x80u))
                    USBHSD->UEP0_TX_CTRL=(USBHSD->UEP0_TX_CTRL & ~USBHS_UEP_T_RES_MASK)|USBHS_UEP_T_RES_ACK;
            } else if(token==USBHS_UIS_TOKEN_OUT && ep==DEF_UEP1) {
                usb_ep1_out_evt++;
                if(!(st & USBHS_UIS_TOG_OK)) usb_ep1_out_notog++;
                {
                    uint16_t frame=USB_ServiceFrame();
                    uint16_t delta=(frame-s_last_out_frame)&0x3fffu;
                    usb_out_delta[delta<3u ? delta : 3u]++;
                    s_last_out_frame=frame;
                }
                s_out_gap_accounted=0u;
                s_out_gap_active=USBHS_DevAltSetting[1] && !I2S_PlayFmtPending();
#if PLAY_EP1_BUF_MOD
                USB_RxComplete((USBHSD->UEP1_RX_CTRL & USBHS_UEP_R_TOG_DATA1) ? 0u : 1u,len);
#else
                USB_RxComplete(0u,len);
#endif
                usb_isr_out_cnt++;usb_isr_out_ticks+=(uint16_t)((uint16_t)TIM8->CNT-phase);
            } else if(token==USBHS_UIS_TOKEN_IN && ep==DEF_UEP2) {
                USB_UpdateFeedback(); /* Previous DMA transfer is finished. */
            } else if(token==USBHS_UIS_TOKEN_IN && ep==DEF_UEP3) {
                /* Isochronous IN has no NAK handshake. Only software bank
                 * ownership determines whether this completion has payload. */
                if(usb_cap_active_slot<0) {
                    if(USBHS_CaptureActive) usb_cap_in_empty++;
                    return;
                }
                USBHSD->UEP3_TX_CTRL=USBHS_UEP_T_TOG_DATA0|USBHS_UEP_T_RES_NAK;
                if(usb_cap_active_slot>=0) usb_cap_state[(uint8_t)usb_cap_active_slot]=0u;
                usb_cap_active_slot=-1;usb_cap_packets++;
                if(USBHS_CaptureActive) {
                    if(usb_cap_state[0]!=2u && usb_cap_state[1]!=2u) usb_cap_queue_under++;
                    USB_CapSubmit(); /* Register-only handoff; no PCM in IRQ. */
                }
            } else if(token==USBHS_UIS_TOKEN_IN && ep==DEF_UEP4 && !(st & USBHS_UIS_IS_NAK)) {
                usb_ep4_poll++;
                if(ep4_armed) {ep4_armed=0u;ep4_chk=0u;usb_ep4_ok++;}
                USBHSD->UEP4_TX_CTRL=(USBHSD->UEP4_TX_CTRL & ~USBHS_UEP_T_RES_MASK)|USBHS_UEP_T_RES_NAK;
            }
            {
                uint16_t used=(uint16_t)((uint16_t)TIM8->CNT-phase);
                if(used>usb_xfer_max_ticks) usb_xfer_max_ticks=used;
            }
            if(token==USBHS_UIS_TOKEN_SOF) {
                usb_isr_sof_cnt++;usb_isr_sof_ticks+=(uint16_t)((uint16_t)TIM8->CNT-phase);
            }
}

void USBHS_IRQHandler(void) {
    static uint8_t control_attempt;
    uint16_t started=(uint16_t)TIM8->CNT;
    uint8_t drained;
    uint8_t poll_control=ep0_out_poll_ready;
    /* Walk the window phase against the host's 125us retry schedule. */
    uint16_t control_budget=32u+8u*(control_attempt&3u);
    if(poll_control) control_attempt++;
    uint32_t poll_spins=20000u;
    ep0_out_poll_ready=0u;
    /* Reset and SETUP have independent flags and cannot be starved by SOF/OUT. */
    for(drained=0u;drained<8u;) {
        uint8_t flags=USBHSD->INT_FG;
        if(poll_control && ep0_out_pending && !(flags & (USBHS_UIF_SETUP_ACT|USBHS_UIF_BUS_RST))) {
            USBHSD->UEP0_RX_CTRL=USBHS_UEP_R_TOG_DATA1|USBHS_UEP_R_RES_ACK;
            if(!flags && (uint16_t)((uint16_t)TIM8->CNT-started)<control_budget && --poll_spins) continue;
        }
        if(!flags) break;
        drained++;
        if(flags & USBHS_UIF_BUS_RST) {USB_ResetEvent();continue;}
        if(flags & USBHS_UIF_SETUP_ACT) {
            /* A new SETUP aborts the previous EP0 transfer. Its late IN/OUT
             * completion must not close or advance the newly armed stage. */
            if((flags & USBHS_UIF_TRANSFER) && !(USBHSD->INT_ST & USBHS_UIS_ENDP_MASK))
                USBHSD->INT_FG=USBHS_UIF_TRANSFER;
            USB_SetupEvent();continue;
        }
        if((flags & USBHS_UIF_ISO_ACT) && !(flags & USBHS_UIF_TRANSFER)) {
            /* Enter at packet START so interrupt-entry latency is paid while
             * bytes are on the wire, before adjacent completions share INT_ST.
             * DMA buffers remain untouched until UIF_TRANSFER confirms END.
             * CRC-error OUT has no completion: bound this wait explicitly. */
            uint16_t begin=(uint16_t)TIM8->CNT;
            uint16_t budget=30u;
            uint32_t guard=60000u;
            USBHSD->INT_FG=USBHS_UIF_ISO_ACT;
            /* FS token/data spacing already covers IRQ-entry latency. Do not
             * spin for hundreds of us and starve main-loop PCM preparation. */
            /* ISO_ACT can remain set after its completion was already drained.
             * An idle SIE has nothing left to wait for; leave CPU time for PCM. */
            if(USBHS_DevSpeed==USBHS_SPEED_FULL || (USBHSD->MIS_ST & USBHS_UMS_SIE_FREE)) continue;
            while(!(USBHSD->INT_FG & (USBHS_UIF_TRANSFER|USBHS_UIF_SETUP_ACT|USBHS_UIF_BUS_RST|USBHS_UIF_FIFO_OV))) {
                if((uint16_t)((uint16_t)TIM8->CNT-begin)>=budget || !--guard) {
                    usb_iso_wait_timeout++;break;
                }
            }
            flags=USBHSD->INT_FG; /* Capture completion now, without another IRQ-loop round. */
        }
        if(flags & USBHS_UIF_TRANSFER) {
            uint8_t st=USBHSD->INT_ST;
            uint16_t len=USBHSD->RX_LEN;
            /* Save shared status, close reusable EP0 buffers, then release the
             * event latch before calls/register bookkeeping. Isochronous OUT
             * and feedback IN can finish less than 2us apart. */
            if(!(st & USBHS_UIS_IS_NAK)) {
                if((st & 0x3fu)==USBHS_UIS_TOKEN_IN) USBHSD->UEP0_TX_CTRL |= USBHS_UEP_T_RES_NAK;
                if((st & 0x3fu)==USBHS_UIS_TOKEN_OUT) USBHSD->UEP0_RX_CTRL |= USBHS_UEP_R_RES_NAK;
            }
            USBHSD->INT_FG=USBHS_UIF_TRANSFER;
            USB_TransferEvent(st,len);
            continue;
        }
        if(flags & USBHS_UIF_FIFO_OV) {usb_fifo_overflow++;USBHSD->INT_FG=USBHS_UIF_FIFO_OV;continue;}
        if(flags & USBHS_UIF_HST_SOF) {USBHSD->INT_FG=USBHS_UIF_HST_SOF;continue;}
        if(flags & USBHS_UIF_SUSPEND) {
            USBHSD->INT_FG=USBHS_UIF_SUSPEND;
            if(USBHSD->MIS_ST & USBHS_UMS_SUSPEND) USBHS_DevSleepStatus|=2u;
            else USBHS_DevSleepStatus&=~2u;
            I2S_FeedbackReset();continue;
        }
        break;
    }
    if(ep0_out_pending) USBHSD->UEP0_RX_CTRL=USBHS_UEP_R_TOG_DATA1|USBHS_UEP_R_RES_NAK;
    usb_isr_drain[(drained>=4u)?3u:drained ? drained-1u : 0u]++;
    if(drained>=8u) usb_isr_drain_ovf++; /* Leave pending flags for the next entry. */
    {
        uint16_t elapsed=(uint16_t)((uint16_t)TIM8->CNT-started);
        usb_isr_cnt++;usb_isr_ticks+=elapsed;
        if(elapsed>usb_isr_max_ticks) usb_isr_max_ticks=elapsed;
    }
}
