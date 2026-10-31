/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.h
 * Author             : WCH / modified
 * Version            : V2.0.0
 * Date               : 2026/10
 * Description        : header file of usb_desc.c
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#ifndef USER_USB_DESC_H_
#define USER_USB_DESC_H_

#include "debug.h"

/******************************************************************************/
/* global define */
/* file version */

#define DEF_FILE_VERSION USB_DEVICE_VERSION
/* USB device descriptor, device serial number (bcdDevice) */
#define DEF_IC_PRG_VER DEF_FILE_VERSION

#define SAMPLE_FREQ_4B(frq) (uint8_t)(frq), (uint8_t)((frq >> 8)), \
                            (uint8_t)((frq >> 16)), (uint8_t)((frq >> 24))
#define USBD_AUDIO_FREQ_384 384000U
#define USBD_AUDIO_FREQ_192 192000U
#define USBD_AUDIO_FREQ_96 96000U
#define USBD_AUDIO_FREQ_48 48000U
#define AUDIO_PACKET_SZE(sze) (uint8_t)(sze), (uint8_t)((sze >> 8))
/* 播放时钟 = Clock Source 4 */
#define AUDIO_CLOCK_ID 0x04

#define AUDIO_FU_ID 0x02     /* 播放（source = IT1） */
#define AUDIO_FU_CAP_ID 0x08 /* 录音（source = IT7） */

#define AUDIO_CLOCK_ID_CAP 0x05


#define AUDIO_CSOURCE_ID_PLAY AUDIO_CLOCK_ID
#define AUDIO_CSOURCE_ID_CAP  AUDIO_CLOCK_ID_CAP

/* 主机眼里只有一路时钟 —— 采样率必须两路同时改 */

/******************************************************************************/
/* HID 消费者控制功能（音量加/减、播放暂停）
 * 放在配置描述符最后：interface 3，单一 IN 中断端点 0x84（8 字节）。
 * HID 是标准类，不需要 IAD —— Windows 会把它枚举成一个独立的 HID 功能。 */
#define USB_HID_IF USB_IF_HID
#define USB_HID_EP 0x84
#define DEF_USB_EP4_SIZE            64
/* 报告描述符：1 字节位图，bit0=播放/暂停(0xCD)、bit1=音量+(0xE9)、bit2=音量-(0xEA) */
#define USB_HID_REPORT_DESC_LEN 77
extern const uint8_t USB_HID_ReportDesc[USB_HID_REPORT_DESC_LEN];

/******************************************************************************/
/* usb device endpoint size define */
#define DEF_USBD_UEP0_SIZE 64 /* usb hs/fs device end-point 0 size */
/* HS */
#define DEF_USBD_HS_PACK_SIZE 512      /* usb hs device max bulk/int pack size */
#define DEF_USBD_HS_ISO_PACK_SIZE 1024 /* usb hs device max iso pack size */
/* FS */
#define DEF_USBD_FS_PACK_SIZE 64       /* usb fs device max bulk/int pack size */
#define DEF_USBD_FS_ISO_PACK_SIZE 1023 /* usb fs device max iso pack size */
/* LS */
#define DEf_USBD_LS_UEP0_SIZE 8  /* usb ls device end-point 0 size */
#define DEF_USBD_LS_PACK_SIZE 64 /* usb ls device max int pack size */

/* HS end-point size */
#define DEF_USB_EP1_HS_SIZE DEF_USBD_HS_ISO_PACK_SIZE
#define DEF_USB_EP1_FD_HS_SIZE 4
#define DEF_USB_EP2_HS_SIZE DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP3_HS_SIZE DEF_USBD_HS_PACK_SIZE
/* FS end-point size */
#define DEF_USB_EP1_FS_SIZE DEF_USBD_FS_ISO_PACK_SIZE /* 1023, isochronous */
#define DEF_USB_EP2_FS_SIZE DEF_USBD_FS_PACK_SIZE
#define DEF_USB_EP3_FS_SIZE DEF_USBD_FS_PACK_SIZE

/******************************************************************************/
/* 描述符长度：配置描述符改为运行时按 settings.h 生成，所以长度是变量。
 * 注意：这两个都是「当前生效那张表」的长度，HS/FS 只有 wMaxPacketSize
 * 不同，长度永远相等。 */
extern uint8_t  MyCfgDescr_HS[];
#define MyCfgDescr_FS MyCfgDescr_HS
extern uint16_t USB_CfgDesc_Len;

#define DEF_USBD_DEVICE_DESC_LEN ((uint8_t)MyDevDescr[0])
#define DEF_USBD_CONFIG_FS_DESC_LEN (USB_CfgDesc_Len)
#define DEF_USBD_CONFIG_HS_DESC_LEN (USB_CfgDesc_Len)
#define DEF_USBD_REPORT_DESC_LEN 0
#define DEF_USBD_LANG_DESC_LEN ((uint16_t)MyLangDescr[0])
#define DEF_USBD_SN_DESC_LEN ((uint16_t)MySerNumInfo[0])
#define DEF_USBD_QUALFY_DESC_LEN ((uint16_t)MyQuaDesc[0])
#define DEF_USBD_BOS_DESC_LEN ((uint16_t)MyBOSDesc[2] + (uint16_t)(MyBOSDesc[3] << 8))
#define DEF_USBD_FS_OTH_DESC_LEN (USB_CfgDesc_Len)
#define DEF_USBD_HS_OTH_DESC_LEN (USB_CfgDesc_Len)

/******************************************************************************/
/* 最大 alt setting 数（由 settings.h 的矩阵决定，12 = 4 采样率 x 3 位深） */
#define USB_ALT_MAX 16

extern const uint8_t MyDevDescr[];
extern const uint8_t MyLangDescr[];
extern const uint8_t MySerNumInfo[];
extern const uint8_t MyQuaDesc[];
extern const uint8_t MyBOSDesc[];

/*----------------------------------------------------------------------------
 *  支持矩阵导出（全部由 settings.h 生成）
 *----------------------------------------------------------------------------*/
/* 采样率表，降序，USB_RateList[0] 是默认采样率 */
extern uint32_t USB_RateList[4];
extern uint8_t  USB_RateCount;

/* alt setting -> 位深（0 表示该 alt 是零带宽档） */
extern uint8_t  USB_PlayAltDepth[USB_ALT_MAX];
extern uint8_t  USB_PlayAltCount;
extern uint8_t  USB_CapAltDepth[USB_ALT_MAX];
extern uint8_t  USB_CapAltCount;

/* alt setting -> sample rate (0 = zero bandwidth alt) */
extern uint32_t USB_PlayAltRate[USB_ALT_MAX];
extern uint32_t USB_CapAltRate[USB_ALT_MAX];

/* GET_CUR(SAMPLING_FREQ) 返回的 4 字节，等于当前采样率 */
extern uint8_t  USB_SampleFreqPlay[4];
extern uint8_t  USB_SampleFreqCap[4];

/* GET_RANGE(SAMPLING_FREQ) 返回的原始载荷：
 * {N(2B LE), N x (wMIN 4B, wMAX 4B, wRES 4B)} —— 保持与原工程一致的格式 */
extern uint8_t  USB_RangeDesc_HS[];
extern uint16_t USB_RangeDesc_HS_Len;
extern uint8_t  USB_RangeDesc_FS[];
extern uint16_t USB_RangeDesc_FS_Len;

/*----------------------------------------------------------------------------
 *  API
 *----------------------------------------------------------------------------*/
/* 上电时调用一次：按 settings.h 生成配置描述符、范围表、alt 映射表 */
void USB_BuildDescriptors (void);

/* 1 = 共享描述符缓冲/能力表当前装的是 OTHER_SPEED 的对侧速度那一份。
 * 由 usbhs_device.c 在 EP0 的 IN 完成里检查并调用下面的恢复函数。 */
extern uint8_t  USB_DescIsOtherSpeed;
void USB_RestoreSpeedDesc (uint8_t hs_speed);

/* 取配置描述符。other_speed != 0 时返回 OTHER_SPEED 形态（type=0x07 且
 * wMaxPacketSize 用另一速度的值）。返回值即 pUSBHS_Descr 应指向的地址，
 * 长度写进 *len。 */
void USB_SetCurrentRate (uint32_t play_rate, uint32_t cap_rate);
uint32_t USB_GetCurrentRate (void);
uint32_t USB_GetCapRate (void);
uint8_t *USB_GetConfigDesc (uint8_t hs_speed, uint8_t other_speed, uint16_t *len);

/* String descriptors built from the names configured in usb_desc.c */
extern const uint8_t *USB_GetManufacturerDesc (void);
const uint8_t *USB_GetProductDesc (void);
const uint8_t *USB_GetSerialDesc (void);
const uint8_t *USB_GetDeviceDesc (void);
extern const uint8_t *USB_GetProductDesc (void);

#endif /* USER_USB_DESC_H_ */
