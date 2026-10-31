/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v30x_usbhs_device.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/11/20
 * Description        : USB2.0 High speed device operation related header files
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#ifndef __CH32V30X_USBHS_DEVICE_H__
#define __CH32V30X_USBHS_DEVICE_H__

/*******************************************************************************/
/* Header File */
#include "debug.h"
#include "settings.h"
#include "string.h"
#include "ch32v30x_usb.h"
#include "usb_desc.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************/
/* Macro Definition */

/* General */
#define pUSBHS_SetupReqPak ((PUSB_SETUP_REQ)USBHS_EP0_Buf)

#define DEF_UEP_IN 0x80
#define DEF_UEP_OUT 0x00
/* Endpoint Number */
#define DEF_UEP0 0x00
#define DEF_UEP1 0x01
#define DEF_UEP2 0x02
#define DEF_UEP3 0x03
#define DEF_UEP4 0x04

/* USB SPEED TYPE */
#define USBHS_SPEED_TYPE_MASK ((uint8_t)(0x03))
#define USBHS_SPEED_LOW ((uint8_t)(0x02))
#define USBHS_SPEED_FULL ((uint8_t)(0x00))
#define USBHS_SPEED_HIGH ((uint8_t)(0x01))

/* Test mode */
#define TEST_MASK 0x03
#define TEST_SE0 0x00
#define TEST_PACKET 0x01
#define TEST_J 0x02
#define TEST_K 0x03

/* Audio class request */
#define AUDIO_REQ_GET_MSK 0x80
#define AUDIO_REQ_CUR 1
#define AUDIO_REQ_RANGE 2
#define CS_SAM_FREQ_CONTROL 0x01
#define CS_CLOCK_VALID_CONTROL 0x02


/* 音频功能里的两个 Feature Unit（实体 ID 见 usb_desc.h）：
 *   AUDIO_FU_ID     = FU2，播放树（IT1 -> FU2 -> OT3），映射 ES9038 Reg15/16
 *   AUDIO_FU_CAP_ID = FU8，录音树（IT7 -> FU8 -> OT6），映射 ADC5120 PGA/数字音量
 * 两个都在 master 声道（CN=0）声明 Mute + Volume，见 usb_desc.c。 */

/* UAC2 的 Feature Unit 控制选择器（Audio 2.0 附录 A.17.7） */
#define FU_MUTE_CONTROL 0x01
#define FU_VOLUME_CONTROL 0x02

void USB_ApplyPending (void);

extern volatile uint32_t USBHS_CapRateReq;
extern volatile uint32_t USBHS_CapSetCurCnt;
extern volatile uint32_t USBHS_CapSetCurLast;
/* 采样率 SET_CUR 现场（诊断用，cap/usb 命令打印） */
extern volatile uint32_t USBHS_ScCnt;
extern volatile uint32_t USBHS_ScFreq;
extern volatile uint8_t  USBHS_ScEnt;
extern volatile uint8_t  USBHS_ScClk;
/******************************************************************************/
/* Variable Declaration */

/* SetUp Request Values */
extern const uint8_t *pUSBHS_Descr;

/* Setup Request */
extern volatile uint8_t USBHS_SetupReqCode;
extern volatile uint8_t USBHS_SetupReqType;
extern volatile uint16_t USBHS_SetupReqValue;
extern volatile uint16_t USBHS_SetupReqIndex;
extern volatile uint16_t USBHS_SetupReqLen;

/* USB Device Status */
extern volatile uint8_t USBHS_DevConfig;
extern volatile uint8_t USBHS_DevAddr;
extern volatile uint8_t USBHS_DevSleepStatus;
extern volatile uint8_t USBHS_DevEnumStatus;
extern volatile uint8_t USBHS_DevSpeed;
extern volatile uint8_t  USBHS_CaptureActive;  /* 录音流是否已开（SET_INTERFACE(3,alt>0) 置 1） */
/* 每个接口当前选中的 alternate setting：0=AC、1=AS播放、2=AS录音、3=HID */
extern volatile uint8_t  USBHS_DevAltSetting[5];

extern volatile uint32_t usb_ep4_ok;    /* EP4：主机确实取走一份报告的次数 */
extern volatile uint32_t usb_ep4_poll;  /* EP4：主机轮询总次数（含被 NAK 的） */
extern volatile uint32_t usb_ep4_pend;  /* EP4：其中落在 armed 期间、即"问了没完成"的次数 */
extern volatile uint32_t usb_ep4_sof;   /* SOF 个数，用来核对 poll 的量级 */
extern volatile uint32_t usb_ep1_out_evt;   /* EP1 OUT ??? */
extern volatile uint32_t usb_ep1_out_notog; /* ?? TOG_OK ? 0 ?????????????? */
extern volatile uint32_t usb_setup_cnt;  /* SETUP 进入次数 */
extern volatile uint32_t usb_setup_w0;   /* bRequestType|bRequest<<8|wValue<<16 */
extern volatile uint32_t usb_setup_w1;   /* wIndex|wLength<<16 */
extern volatile uint32_t usb_ep4_to;    /* EP4：握手超时被兜底退回 NAK 的次数（应长期为 0） */

/* USB 中断负载仪表（见 usbhs_device.c 里的说明） */
extern volatile uint32_t usb_isr_cnt;    /* USB 中断进入次数 */
extern volatile uint32_t usb_isr_ticks;
extern volatile uint32_t usb_isr_sof_ticks;
extern volatile uint32_t usb_isr_sof_cnt;
extern volatile uint32_t usb_isr_out_ticks;
extern volatile uint32_t usb_isr_out_cnt;
extern volatile uint8_t  usb_int_busy;   /* 当前 RB_UC_INT_BUSY 是否打开 */
extern volatile uint32_t usb_ep0_out_count, usb_ep0_out_last, usb_ep0_out_data, usb_ep0_out_skip;
 
extern volatile uint32_t usb_isr_drain[4];   /* [0]=1 个 [1]=2 个 [2]=3 个 [3]>=4 个 */
extern volatile uint32_t usb_isr_drain_ovf;  /* 排空循环到上限仍未排空（必须为 0） */
extern volatile uint32_t usb_fmt_cnt;        /* 格式重建次数 */
extern volatile uint32_t usb_fmt_ret;        /* 最后一次结果：bit0..1 1=成功 2=失败，bit8=播放侧 */
extern volatile uint32_t usb_fmt_heal;       /* 硬件自愈触发次数 */

/* Feature Unit 诊断：主机最后一次对 FU 下了什么 */
extern volatile uint32_t usb_fu_set_cnt;
extern volatile uint8_t  usb_fu_set_ent;
extern volatile uint8_t  usb_fu_set_sel;
extern volatile uint8_t  usb_fu_set_ch;
extern volatile int16_t  usb_fu_set_val;

void USB_SetIntBusy (uint8_t en);
void USB_RequestPlayRecovery(void);
void USB_RequestCapRecovery(void);
void USB_StreamService(void);
uint8_t USB_HostReady(void);
extern volatile uint32_t usb_rx_queue_drop, usb_rx_queue_stale, usb_rx_queue_peak;
extern volatile uint32_t usb_cap_queue_under, usb_cap_packets, usb_fifo_overflow;
extern volatile uint32_t usb_isr_max_ticks;
extern volatile uint32_t usb_xfer_max_ticks;


/* Endpoint Buffer */
extern __attribute__((aligned(4))) uint8_t USBHS_EP0_Buf[];
extern __attribute__((aligned(4))) uint8_t USBHS_EP1_Rx_Buf[][DEF_USB_EP1_HS_SIZE];

/********************************************************************************/
/* Function Declaration */
extern void USBHS_RCC_Init(void);
extern void USBHS_Device_Endp_Init(void);
/* HID 报告发送（hid_app.c 调用）：数据经 EP4 IN 上传，空闲时端点保持 NAK */
uint8_t USB_GetFuMute (void);
int16_t USB_GetFuVolL (void);
int16_t USB_GetFuVolR (void);

void USB_HID_Send (const uint8_t *data, uint8_t len);

extern void USBHS_Device_Init(FunctionalState sta);
extern void USBHS_IRQHandler(void);

#ifdef __cplusplus
}
#endif

extern volatile uint8_t USBHS_ForceFS;

#endif
