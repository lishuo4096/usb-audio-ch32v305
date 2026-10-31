/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.c
 * Author             : WCH / modified
 * Version            : V2.0.0
 * Date               : 2026/10
 * Description        : USB device descriptor, configuration descriptor,
 *                      string descriptors and other descriptors.
 *
 *  V2 改动：
 *   - 配置描述符不再手写字节数组，改为按 app_config.h 的支持矩阵在运行时
 *     生成。wTotalLength 由生成器自己数，永远不会写错。
 *   - HS/FS 只有 wMaxPacketSize 不同，共用一张表，请求时现场 patch，
 *     省 1.4KB RAM。
 *   - alt setting 与位深的映射表一并生成，SET_INTERFACE 直接查表，
 *     不再写死 32/16/24。
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/

#include <usb_desc.h>
#include "settings.h"
#include <string.h>

/******************************************************************************/
/*                    USB 设备 ID / 名称配置（用户配置区）                      */
/* VID/PID：0x 十六进制或十进制均可，也可通过 -DUSB_VID=... 覆盖。              */
/* 名称字符串：直接改成想要的 ASCII 文本，自动转成 USB UTF-16LE 字符串          */
/******************************************************************************/
/* Device Descriptor */
const uint8_t MyDevDescr[] = {
    0x12,                     // bLength
    0x01,                     // bDescriptorType (Device)
    0x00,
    0x02,                     // bcdUSB 2.00
    USB_AUDIO_ENABLE ? 0xEF : 0, // bDeviceClass
    USB_AUDIO_ENABLE ? 0x02 : 0, // bDeviceSubClass
    USB_AUDIO_ENABLE ? 0x01 : 0, // bDeviceProtocol
    DEF_USBD_UEP0_SIZE,       // bMaxPacketSize0 64
    (uint8_t)USB_VID,
    (uint8_t)(USB_VID >> 8),  // idVendor
    (uint8_t)USB_PID,
    (uint8_t)(USB_PID >> 8),  // idProduct
    DEF_IC_PRG_VER,
    0x00,                     // bcdDevice 0.01
    0x01,                     // iManufacturer (String Index)
    0x02,                     // iProduct (String Index)
    0x03,                     // iSerialNumber (String Index)
    0x01,                     // bNumConfigurations 1
};

#define MCU_UID_BASE 0x1FFFF7E8UL
#define MCU_UID_LEN 12u
#define USB_PID_UID_BAND 0x6000u
#define USB_PID_UID_MASK 0x0FFFu

static uint8_t uid_bytes[MCU_UID_LEN];
static uint8_t serial_desc[2 + 2 * (sizeof (USB_SERIAL_PREFIX) + 1 + 2 * MCU_UID_LEN)];
static uint8_t uid_ready;

uint8_t MyDevDescrDyn[sizeof (MyDevDescr)];
uint16_t USB_DerivedPid;

static void devdescr_build (void);

/******************************************************************************/
/*                        运行时生成的描述符与表                               */
/******************************************************************************/

/* 单一配置描述符缓冲：HS 与 FS 只差 wMaxPacketSize 两个字段，请求时 patch
 * 预算：12 档 alt x 53B x 2 个 AS + 137B 固定部分 = 1409B；
 * 留出余量到 2048B，即使 settings.h 全开也不会写穿。 */
#define USB_CFG_DESC_MAX 2048

uint8_t MyCfgDescr_HS[USB_CFG_DESC_MAX];

/* 当前这份描述符是按「允许的最高采样率」建的。
 * HS 可以跑到 384k；FS 每帧（bInterval=1）只有一个 1023 字节的等时包，
 * 384k/32bit 要 3072 字节/ms，物理上发不出来，所以 FS 只留 <=96k。
 * USB_GetConfigDesc() 发现速度变了会按新上限重建。 */
static uint32_t s_rate_cap = 384000U;

#if USB_AUDIO_ENABLE
static uint32_t AltRateCap (uint32_t depth) {
    if (USB_PLAY_ENABLE && USB_CAPTURE_ENABLE && s_rate_cap <= 96000U && depth >= 32U) {
        return 48000U;
    }
    return s_rate_cap;
}
#endif

uint16_t USB_CfgDesc_Len;

/* 1 = 共享描述符缓冲/能力表当前装的是 OTHER_SPEED_CONFIGURATION 的
 * 「对侧速度」那一份。见 USB_RestoreSpeedDesc() 的说明。 */
uint8_t USB_DescIsOtherSpeed;

uint32_t USB_RateList[4];
uint8_t USB_RateCount;

uint8_t USB_PlayAltDepth[USB_ALT_MAX];
uint8_t USB_PlayAltCount;
uint8_t USB_CapAltDepth[USB_ALT_MAX];
uint32_t USB_PlayAltRate[USB_ALT_MAX];
uint32_t USB_CapAltRate[USB_ALT_MAX];
uint8_t USB_CapAltCount;

uint8_t USB_SampleFreqPlay[4];
uint8_t USB_SampleFreqCap[4];

uint8_t USB_RangeDesc_HS[2 + 8 * 12];
uint16_t USB_RangeDesc_HS_Len;
uint8_t USB_RangeDesc_FS[2 + 8 * 12];
uint16_t USB_RangeDesc_FS_Len;

/* wMaxPacketSize 字段在描述符里的偏移，生成时每个 alt 各记一个，请求时全量 patch */
static uint16_t s_off_ep_play[USB_ALT_MAX];
static uint16_t s_off_ep_cap[USB_ALT_MAX];
/* 反馈端点的 wMaxPacketSize 偏移：HS=4 字节(Q16.16)，FS=3 字节(Q10.14) */
static uint16_t s_off_fb_play[USB_ALT_MAX];
static uint16_t s_off_fb_cap[USB_ALT_MAX];

/******************************************************************************/
/*                              字节写入器                                     */
/******************************************************************************/
static uint8_t *s_p;

static void bd8 (uint8_t v) {
    *s_p++ = v;
}

static void bd16 (uint16_t v) {
    *s_p++ = (uint8_t)(v & 0xFF);
    *s_p++ = (uint8_t)(v >> 8);
}

/* 记录当前偏移位置的 wMaxPacketSize 字段地址 */
#if USB_AUDIO_ENABLE
static uint16_t bd_mark_ep (uint16_t *slot) {
    *slot = (uint16_t)(s_p - MyCfgDescr_HS);
    return *slot;
}
#endif

/******************************************************************************/
/*                      单个 AS alternate setting 生成                         */
/******************************************************************************/
/* bInterfaceNumber / bAlternateSetting / 端点 / 位深 / bTerminalLink */
/* UAC2：一个 alt setting = 一种位深，不含任何采样率信息
 * 采样率属于 Clock Source（播放 clock 4 / 录音 clock 5），由
 * SET_CUR(SAMPLING_FREQ_CONTROL) 单独修改，因此两个方向可以各跑各的
 * 频率，切频率也绝不碰 alt setting。CT7601SR 就是这个结构。 */
#if USB_AUDIO_ENABLE
static void bd_as_alt (uint8_t ifc, uint8_t alt, uint8_t termlink,
                       uint8_t ep, uint8_t fb, uint8_t depth,
                       uint16_t *off_ep, uint16_t *off_fb) {
    /* Standard AS Interface Descriptor */
    bd8 (0x09);
    bd8 (0x04);
    bd8 (ifc);
    bd8 (alt);
    bd8 ((uint8_t)(fb ? 0x02 : 0x01)); /* bNumEndpoints: 数据 (+ 反馈) */
    bd8 (0x01);                        /* bInterfaceClass: Audio */
    bd8 (0x02);                        /* bInterfaceSubClass: AudioStreaming */
    bd8 (0x20);                        /* bInterfaceProtocol: IP 2.0 */
    bd8 (0x00);                        /* iInterface */

    /* AS Interface Descriptor (General) —— UAC2: bmFormats/bmChannelConfig 各 4B */
    bd8 (0x10);
    bd8 (0x24);
    bd8 (0x01);
    bd8 (termlink);
    bd8 (0x00); /* bmControls */
    bd8 (0x01); /* bFormatType = FORMAT_TYPE_I */
    bd8 (0x01);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* bmFormats = PCM */
    bd8 (0x02); /* bNrChannels */
    bd8 (0x03);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* bmChannelConfig = FL|FR */
    bd8 (0x00); /* iChannelNames */

    bd8 (0x06);
    bd8 (0x24);
    bd8 (0x02);
    bd8 (0x01);
    bd8 ((uint8_t)(depth / 8)); /* bSubslotSize */
    bd8 (depth);                /* bBitResolution */

    /* Standard AS Isochronous Data Endpoint */
    bd8 (0x07);
    bd8 (0x05);
    bd8 (ep);
    bd8 (0x05);
    bd_mark_ep (off_ep);
    bd16 (0);   /* wMaxPacketSize placeholder, 请求时 patch */
    bd8 (0x01); /* bInterval */

    /* CS Endpoint Descriptor */
    bd8 (0x08);
    bd8 (0x25);
    bd8 (0x01);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    if (fb != 0x00) {
        bd8 (0x07);
        bd8 (0x05);
        bd8 (fb);
        bd8 (0x11); /* iso + feedback */
        bd_mark_ep (off_fb);
        bd16 (0);   /* wMaxPacketSize: HS=4 / FS=3，按速度 patch */
        bd8 (0x01); /* bInterval */
    }
}
#endif

/******************************************************************************/
/*                              配置描述符生成                                 */
/******************************************************************************/

/* 一档采样率 × 一档位深 -> 一个 alt setting */
typedef struct {
    uint32_t rate;
    uint8_t depth;
} RateDepth_t;

void USB_BuildDescriptors (void) {
    /* 设备描述符与序列号只跟芯片 UID 有关，这里先建一遍，
     * 之后 USB_GetDeviceDesc() / USB_GetSerialDesc() 直接返回。 */
    devdescr_build();
    static const RateDepth_t rd[] = {
#if RATE_384K_32BIT
        {384000U, 32},
#endif
#if RATE_384K_24BIT
        {384000U, 24},
#endif
#if RATE_384K_16BIT
        {384000U, 16},
#endif
#if RATE_192K_32BIT
        {192000U, 32},
#endif
#if RATE_192K_24BIT
        {192000U, 24},
#endif
#if RATE_192K_16BIT
        {192000U, 16},
#endif
#if RATE_96K_32BIT
        {96000U,  32},
#endif
#if RATE_96K_24BIT
        {96000U,  24},
#endif
#if RATE_96K_16BIT
        {96000U,  16},
#endif
#if RATE_48K_32BIT
        {48000U,  32},
#endif
#if RATE_48K_24BIT
        {48000U,  24},
#endif
#if RATE_48K_16BIT
        {48000U,  16},
#endif
    };
    const uint8_t n_rd = (uint8_t)(sizeof (rd) / sizeof (rd[0]));
    uint8_t i, k;
    uint16_t off_total;
#if USB_AUDIO_ENABLE
    uint16_t off_ac_total;
#endif
    uint8_t *p_start;

    USB_PlayAltCount=USB_CapAltCount=1u;
    memset (MyCfgDescr_HS, 0, USB_CFG_DESC_MAX);
    s_p = MyCfgDescr_HS;
    p_start = s_p;

    /*---------------- Configuration ----------------*/
    bd8 (0x09);
    bd8 (0x02);
    off_total = (uint16_t)(s_p - p_start);
    bd16 (0);                             /* wTotalLength，末尾回填 */
    bd8 ((uint8_t)USB_INTERFACE_COUNT); /* bNumInterfaces: AC + 播放AS + 录音AS (+ HID) */
    bd8 (0x01);                           /* bConfigurationValue */
    bd8 (0x00);                           /* iConfiguration */
    bd8 (0xC0);                           /* self-powered, no remote wakeup */
    bd8 (0xFA);                           /* bMaxPower = 500mA */

#if USB_AUDIO_ENABLE
    /*---------------- Interface Association：唯一一个音频功能 ----------------*/
    bd8 (0x08);
    bd8 (0x0B);
    bd8 (0x00); /* bFirstInterface = 0 */
    bd8 (1u + USB_PLAY_ENABLE + USB_CAPTURE_ENABLE);
    bd8 (0x01); /* bFunctionClass = Audio */
    bd8 (0x00); /* bFunctionSubClass = AudioControl */
    bd8 (0x20); /* bFunctionProtocol = IP 2.0 */
    bd8 (0x00); /* iFunction */

    /*---------------- Audio Control interface ----------------*/
    bd8 (0x09);
    bd8 (0x04);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* bNumEndpoints = 0 */
    bd8 (0x01);
    bd8 (0x01);
    bd8 (0x20);
    bd8 (0x00);

    /* AC Header */
    bd8 (0x09);
    bd8 (0x24);
    bd8 (0x01);
    bd8 (0x00);
    bd8 (0x02); /* bcdADC = 0x0200 */
    bd8 (0x01); /* bCategory */
    off_ac_total = (uint16_t)(s_p - p_start);
    bd16 (0);   /* wTotalLength，末尾回填 */
    bd8 (0x00); /* bmControls */

#endif
#if USB_PLAY_ENABLE
    /* Input Terminal 1: USB Streaming -> 播放通路 */
    bd8 (0x11);
    bd8 (0x24);
    bd8 (0x02);
    bd8 (0x01);
    bd8 (0x01);
    bd8 (0x01);                  /* wTerminalType = 0x0101 USB streaming */
    bd8 (0x00);                  /* bAssocTerminal */
    bd8 (AUDIO_CSOURCE_ID_PLAY); /* bCSourceID：播放树 -> Clock 4 */
    bd8 (0x02);                  /* bNrChannels */
    bd8 (0x03);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* iChannelNames */
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* bmControls + iTerminal */

#if USB_FU_ACTIVE
    
    bd8 (0x12);   /* bLength = 6 + 3*4 */
    bd8 (0x24);   /* CS_INTERFACE */
    bd8 (0x06);   /* FEATURE_UNIT */
    bd8 (AUDIO_FU_ID);
    bd8 (0x01);   /* bSourceID = IT1 */
    bd8 (0x0F);   /* D1..0 Mute RW + D3..2 Volume RW */
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    for (i = 0; i < 8; i++) {
        bd8 (0x00); /* bmaControls(1)/bmaControls(2) = 无（走 master） */
    }
    bd8 (0x00);     /* iFeature */

#endif
    /* Output Terminal 3: -> Headphone */
    bd8 (0x0C);
    bd8 (0x24);
    bd8 (0x03);
    bd8 (0x03);
    bd8 (0x02);
    bd8 (0x03);                  /* wTerminalType = 0x0302 Headphone */
    bd8 (0x00);                  /* bAssocTerminal */
    bd8 (USB_FU_ACTIVE ? AUDIO_FU_ID : 0x01); /* playback source */
    bd8 (AUDIO_CSOURCE_ID_PLAY); /* bCSourceID：播放树 -> Clock 4 */
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* bmControls + iTerminal */

#endif
#if USB_PLAY_ENABLE
    /* Clock Source 4：内部可编程，频率可读写 */
    bd8 (0x08);
    bd8 (0x24);
    bd8 (0x0A);
    bd8 (AUDIO_CLOCK_ID);
    bd8 (0x03); /* bmAttributes = internal programmable */
    bd8 (0x07); /* bmControls：D1..0=11 频率可读写，D3..2=01 时钟有效只读 */
                /* 位对编码（ADC-2）：00 不存在 / 01 只读 / 11 可编程，
                 * 与本机 CT7601SR 完全一致。firmware 里 CS 4/5 都已实现
                 * GET_CUR(0x02) 返回 1，声明与实现闭环。 */
    bd8 (0x00); /* bAssocTerminal */
    bd8 (0x00); /* iClockSource */

#endif
#if USB_CAPTURE_ENABLE
    
    bd8 (0x08);
    bd8 (0x24);
    bd8 (0x0A);
    bd8 (AUDIO_CLOCK_ID_CAP);
    bd8 (0x03); /* bmAttributes = internal programmable */
    bd8 (0x07); /* bmControls = 频率可读写 + 时钟有效只读（同 CS4） */
    bd8 (0x00); /* bAssocTerminal */
    bd8 (0x00); /* iClockSource */

#endif

#if USB_CAPTURE_ENABLE
    /* Input Terminal 7：线路输入 -> 录音通路
     * 0x0603 = Line connector（线路输入），不是 0x0201 Microphone。
     * Windows 按 wTerminalType 决定录音端点在「声音」面板里显示成
     * "线路输入" 还是 "麦克风"，也影响是否套 +20dB 麦放增益。 */
    bd8 (0x11);
    bd8 (0x24);
    bd8 (0x02);
    bd8 (0x07);                 /* bTerminalID */
    bd8 (0x03);                 /* wTerminalType 低字节 */
    bd8 (0x06);                 /* wTerminalType 高字节 -> 0x0603 Line connector */
    bd8 (0x00);                 /* bAssocTerminal */
    bd8 (AUDIO_CSOURCE_ID_CAP); /* bCSourceID：录音树 -> Clock 5 */
    bd8 (0x02);                 /* bNrChannels */
    bd8 (0x03);                 /* bmChannelConfig = FL|FR */
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00); /* iChannelNames */
    bd8 (0x00); /* bmControls lo */
    bd8 (0x00); /* bmControls hi */
    bd8 (0x00); /* iTerminal */

#if USB_FU_ACTIVE
    
    bd8 (0x12);             /* bLength = 6 + 3*4 */
    bd8 (0x24);             /* CS_INTERFACE */
    bd8 (0x06);             /* FEATURE_UNIT */
    bd8 (AUDIO_FU_CAP_ID);
    bd8 (0x07);             /* bSourceID = IT7 */
    bd8 (0x0F);             /* D1..0 Mute RW + D3..2 Volume RW */
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);
    for (i = 0; i < 8; i++) {
        bd8 (0x00);
    }
    bd8 (0x00);             /* iFeature */

#endif
    /* Output Terminal 6: 录音线路输入 -> USB Streaming */
    bd8 (0x0C);
    bd8 (0x24);
    bd8 (0x03);
    bd8 (0x06);                 /* bTerminalID */
    bd8 (0x01);                 /* wTerminalType = 0x0101 USB streaming */
    bd8 (0x01);
    bd8 (0x00);                 /* bAssocTerminal */
    bd8 (USB_FU_ACTIVE ? AUDIO_FU_CAP_ID : 0x07); /* capture source */
    bd8 (AUDIO_CSOURCE_ID_CAP); /* bCSourceID：录音树 -> Clock 5 */
    bd8 (0x00);                 /* bmControls lo */
    bd8 (0x00);                 /* bmControls hi */
    bd8 (0x00);                 /* iTerminal */

#endif
#if USB_AUDIO_ENABLE
    
    {
        uint16_t ac_len = (uint16_t)(s_p - (p_start + off_ac_total - 6));
        MyCfgDescr_HS[off_ac_total] = (uint8_t)(ac_len & 0xFF);
        MyCfgDescr_HS[off_ac_total + 1] = (uint8_t)(ac_len >> 8);
    }

#endif
#if USB_PLAY_ENABLE
    /*---------------- 播放 AS 接口（interface 1）：每个 alt 一种位深 ----------------
     * alt 1 = 32bit，alt 2 = 24bit，alt 3 = 16bit（按 settings.h 过滤），
     * 每档 Format 里列出该位深下所有支持的采样率。 */
    bd8 (0x09);
    bd8 (0x04);
    bd8 (USB_IF_PLAY);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x01);
    bd8 (0x02);
    bd8 (0x20);
    bd8 (0x00);

    USB_PlayAltDepth[0] = 0;
    USB_PlayAltRate[0] = 0;
    USB_PlayAltCount = 1;
    {
        static const uint8_t dtab[3] = {32u, 24u, 16u};
        uint8_t di;

        for (di = 0; di < 3u; di++) {
            uint32_t rt[8];
            uint8_t nrt = 0;
            uint8_t alt;

            for (i = 0; i < n_rd; i++) {
                if (rd[i].depth == dtab[di] && rd[i].rate <= AltRateCap (dtab[di]) &&
                    nrt < 8u) {
                    rt[nrt++] = rd[i].rate;
                }
            }
            if (nrt == 0u) {
                continue; /* 该位深被 settings.h 全关，或超出本速度的带宽上限 */
            }
            alt = USB_PlayAltCount;
            if (alt >= USB_ALT_MAX) {
                break;
            }
            if ((uint32_t)(s_p - p_start) + 2u * 53u > USB_CFG_DESC_MAX) {
                break;
            }
            /* 单个播放 alt 的字节数固定 53 = AS接口9 + AS通用16 + 格式6
             * + 数据端点7 + CS端点8 + 反馈端点7（与采样率无关了） */
            bd_as_alt (USB_IF_PLAY, alt, 0x01, 0x01, 0x82, dtab[di],
                       &s_off_ep_play[alt], &s_off_fb_play[alt]);
            USB_PlayAltDepth[alt] = dtab[di];
            USB_PlayAltRate[alt] = rt[0];
            USB_PlayAltCount++;
        }
    }

#endif


#if USB_CAPTURE_ENABLE
    /* 录音 AS 接口（interface 2）：每个 alt 一种位深，bTerminalLink 指向 OT6 */
    bd8 (0x09);
    bd8 (0x04);
    bd8 (USB_IF_CAP);
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x01);
    bd8 (0x02);
    bd8 (0x20);
    bd8 (0x00);

    USB_CapAltDepth[0] = 0;
    USB_CapAltRate[0] = 0;
    USB_CapAltCount = 1;
    {
        static const uint8_t dtab[3] = {32u, 24u, 16u};
        uint8_t di;

        for (di = 0; di < 3u; di++) {
            uint32_t rt[8];
            uint8_t nrt = 0;
            uint8_t alt;

            for (i = 0; i < n_rd; i++) {
                if (rd[i].depth == dtab[di] && rd[i].rate <= AltRateCap (dtab[di]) &&
                    nrt < 8u) {
                    rt[nrt++] = rd[i].rate;
                }
            }
            if (nrt == 0u) {
                continue; /* 该位深被 settings.h 全关，或超出本速度的带宽上限 */
            }
            alt = USB_CapAltCount;
            if (alt >= USB_ALT_MAX) {
                break;
            }
            if ((uint32_t)(s_p - p_start) + 2u * 46u > USB_CFG_DESC_MAX) {
                break;
            }
            /* 单个录音 alt = 9+16+6+7+8 = 46；录音侧没有反馈端点 */
            bd_as_alt (USB_IF_CAP, alt, 0x06, 0x83, 0x00, dtab[di],
                       &s_off_ep_cap[alt], &s_off_fb_cap[alt]);
            USB_CapAltDepth[alt] = dtab[di];
            USB_CapAltRate[alt] = rt[0];
            USB_CapAltCount++;
        }
    }
#endif

#if USB_HID_ENABLE
    /*---------------- HID 消费者控制接口（interface 4） ----------------
     * 音量加减、播放/暂停。让 KEY1/2/3 能直接控制主机侧的播放器。
     * 没有 IAD：HID 是标准类，Windows 按接口类 0x03 自己认。 */
    bd8 (0x09);
    bd8 (0x04);
    bd8 (USB_HID_IF);
    bd8 (0x00);
    bd8 (0x01); /* bNumEndpoints = 1 */
    bd8 (0x03); /* bInterfaceClass = HID */
    bd8 (0x00);
    bd8 (0x00);
    bd8 (0x00);

    /* HID 描述符（0x21）：bcdHID 1.11、1 个附加描述符 = 报告描述符 */
    bd8 (0x09);
    bd8 (0x21);
    bd8 (0x11);
    bd8 (0x01);
    bd8 (0x00); /* bCountryCode */
    bd8 (0x01); /* bNumDescriptors */
    bd8 (0x22); /* bDescriptorType = REPORT */
    bd8 ((uint8_t)(USB_HID_REPORT_DESC_LEN & 0xFF));
    bd8 ((uint8_t)(USB_HID_REPORT_DESC_LEN >> 8));

    bd8 (0x07);
    bd8 (0x05);
    bd8 (USB_HID_EP);
    bd8 (0x03); /* interrupt */
    bd8 (0x40); /* wMaxPacketSize = 64，与 QCC3095 一致 */
    bd8 (0x00);
    bd8 (0x08); /* bInterval = 8（HS 16ms / FS 8ms） */
#endif          /* USB_HID_ENABLE */

    /*---------------- wTotalLength 回填 ----------------*/
    USB_CfgDesc_Len = (uint16_t)(s_p - p_start);
    MyCfgDescr_HS[off_total] = (uint8_t)(USB_CfgDesc_Len & 0xFF);
    MyCfgDescr_HS[off_total + 1] = (uint8_t)(USB_CfgDesc_Len >> 8);

    /*---------------- 采样率表（降序，去重） ----------------*/
    USB_RateCount = 0;
    for (i = 0; i < n_rd; i++) {
        uint8_t dup = 0;
        if (rd[i].rate > s_rate_cap) {
            continue; /* 超出本速度能承载的上限 */
        }
        for (k = 0; k < USB_RateCount; k++) {
            if (USB_RateList[k] == rd[i].rate) {
                dup = 1;
                break;
            }
        }
        if (!dup) {
            USB_RateList[USB_RateCount++] = rd[i].rate;
        }
    }
    if (USB_RateCount == 0) { /* 理论上不会发生，兜底 */
        USB_RateList[0] = 48000U;
        USB_RateCount = 1;
    }

    /* 当前采样率：这个值代表「I2S 硬件真正在跑的频率」，只能由 SET_CUR /
     * 换频路径改。这里只在它还没被设过（上电全 0）时给一个默认值。 */
    {
        uint32_t cur = USB_GetCurrentRate();
        uint32_t ccur = USB_GetCapRate();
        uint8_t found = 0;

        for (i = 0; i < USB_RateCount; i++) {
            if (USB_RateList[i] == cur) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (cur == 0u) {
                cur = USB_RateList[0];
            }
        }
        found = 0;
        for (i = 0; i < USB_RateCount; i++) {
            if (USB_RateList[i] == ccur) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (ccur == 0u) {
                ccur = USB_RateList[0];
            }
        }
        USB_SampleFreqPlay[0] = (uint8_t)(cur & 0xFF);
        USB_SampleFreqPlay[1] = (uint8_t)(cur >> 8);
        USB_SampleFreqPlay[2] = (uint8_t)(cur >> 16);
        USB_SampleFreqPlay[3] = (uint8_t)(cur >> 24);
        USB_SampleFreqCap[0] = (uint8_t)(ccur & 0xFF);
        USB_SampleFreqCap[1] = (uint8_t)(ccur >> 8);
        USB_SampleFreqCap[2] = (uint8_t)(ccur >> 16);
        USB_SampleFreqCap[3] = (uint8_t)(ccur >> 24);
    }

    /*---------------- 采样率范围描述符 ----------------
     * 格式沿用原工程：{N(2B LE), N x (wMIN 4B, wMAX 4B, wRES 4B)}
     * HS 表给全部支持的采样率；FS 表只给 <= 96k 的（1023 字节包上限）。 */
    {
        uint8_t n_hs = 0, n_fs = 0;
        uint8_t *h = USB_RangeDesc_HS + 2;
        uint8_t *f = USB_RangeDesc_FS + 2;

        for (i = 0; i < USB_RateCount; i++) {
            uint32_t r = USB_RateList[i];
            h[0] = (uint8_t)r;
            h[1] = (uint8_t)(r >> 8);
            h[2] = (uint8_t)(r >> 16);
            h[3] = (uint8_t)(r >> 24);
            h[4] = h[0];
            h[5] = h[1];
            h[6] = h[2];
            h[7] = h[3];
            h[8] = 0; /* dRES = 0：UAC2 规定离散单点用 min==max、res=0，
                       * 写 1 会被主机当成「分辨率 1Hz 的连续区间」 */
            h[9] = 0;
            h[10] = 0;
            h[11] = 0;
            h += 12;
            n_hs++;

            if (r <= 96000U) {
                f[0] = (uint8_t)r;
                f[1] = (uint8_t)(r >> 8);
                f[2] = (uint8_t)(r >> 16);
                f[3] = (uint8_t)(r >> 24);
                f[4] = f[0];
                f[5] = f[1];
                f[6] = f[2];
                f[7] = f[3];
                f[8] = 0; /* dRES = 0，同 HS 表 */
                f[9] = 0;
                f[10] = 0;
                f[11] = 0;
                f += 12;
                n_fs++;
            }
        }
        if (n_fs == 0) { /* FS 一档都没有时，至少给 48k */
            uint32_t r = 48000U;
            f[0] = (uint8_t)r;
            f[1] = (uint8_t)(r >> 8);
            f[2] = 0;
            f[3] = 0;
            f[4] = f[0];
            f[5] = f[1];
            f[6] = 0;
            f[7] = 0;
            f[8] = 0;
            f[9] = 0;
            f[10] = 0;
            f[11] = 0;
            n_fs = 1;
        }
        USB_RangeDesc_HS[0] = n_hs;
        USB_RangeDesc_HS[1] = 0;
        USB_RangeDesc_HS_Len = (uint16_t)(2 + n_hs * 12);
        USB_RangeDesc_FS[0] = n_fs;
        USB_RangeDesc_FS[1] = 0;
        USB_RangeDesc_FS_Len = (uint16_t)(2 + n_fs * 12);
    }
}

/******************************************************************************/
/*                       按速度取配置描述符（现场 patch）                       */
/******************************************************************************/
static void patch_maxpkt (uint16_t off, uint16_t v) {
    MyCfgDescr_HS[off] = (uint8_t)(v & 0xFF);
    MyCfgDescr_HS[off + 1] = (uint8_t)(v >> 8);
}

/* 把共享描述符缓冲与随之重建的能力表恢复到当前总线速度那一份。 */
void USB_RestoreSpeedDesc (uint8_t hs_speed) {
    uint32_t want = hs_speed ? 384000U : ((USB_PLAY_ENABLE && USB_CAPTURE_ENABLE && EN_32BIT) ? 48000U : 96000U);

    if (want != s_rate_cap) {
        s_rate_cap = want;
        USB_BuildDescriptors();
    }
    if(!hs_speed) {
        uint32_t p=USB_GetCurrentRate(),c=USB_GetCapRate();
        USB_SetCurrentRate(p>want ? 48000u : p,c>want ? 48000u : c);
    }
}

uint8_t *USB_GetConfigDesc (uint8_t hs_speed, uint8_t other_speed, uint16_t *len) {
    uint8_t is_hs;
    uint8_t i;

    /* other_speed 请求时，「对侧」速度才是要填进去的参数 */
    is_hs = other_speed ? (uint8_t)(!hs_speed) : hs_speed;

    /* 描述符里的 tSamFreq 列表必须跟本速度真能发出来的速率一致：
     * FS 只有 1023 字节/帧，384k/32bit 要 3072 字节。 */
    {
        uint32_t want = is_hs ? 384000U : ((USB_PLAY_ENABLE && USB_CAPTURE_ENABLE && EN_32BIT) ? 48000U : 96000U);

        if (want != s_rate_cap) {
            s_rate_cap = want;
            USB_BuildDescriptors();
        }
        /* 只有 OTHER_SPEED 查询才会把这份共享缓冲换成「对侧速度」那一份。
         * 传输是分多包做完的，中途不能恢复，所以这里只标记，
         * 等 EP0 发完最后一个字节再让 usbhs_device.c 调 USB_RestoreSpeedDesc()。 */
        USB_DescIsOtherSpeed = other_speed ? 1u : 0u;
    }

    {
        uint32_t fps = is_hs ? 8000u : 1000u;
        uint32_t lim = is_hs ? 1024u : 1023u;

        for (i = 1; i < USB_PlayAltCount && i < USB_ALT_MAX; i++) {
            uint32_t d = USB_PlayAltDepth[i];
            uint32_t f = USB_PlayAltRate[i];
            uint32_t v = (d && f) ? ((f / fps) * (d / 8u) * 2u) : 0u;
            /* 再留一个样本的余量：usbaudio2 会按「可能多来一个样本」
             * 校验端点能不能承载这一档，包长刚好等于理论值（例如
             * 384k/32bit = 384 字节）时它会把这一档从下拉框里剔掉。 */
            v += (d / 8u) * 2u;
            if (v > lim) {
                v = lim;
            }
            patch_maxpkt (s_off_ep_play[i], (uint16_t)v);
        }
        for (i = 1; i < USB_CapAltCount && i < USB_ALT_MAX; i++) {
            uint32_t d = USB_CapAltDepth[i];
            uint32_t f = USB_CapAltRate[i];
            uint32_t v = (d && f) ? ((f / fps) * (d / 8u) * 2u) : 0u;
            v += (d / 8u) * 2u; /* 同上：留一个样本余量 */
            if (v > lim) {
                v = lim;
            }
            patch_maxpkt (s_off_ep_cap[i], (uint16_t)v);
        }
    }
    /* 反馈端点：HS 是 4 字节 Q16.16（每微帧样本数），
     * FS 是 3 字节 Q10.14（每帧样本数）—— 长度写错主机会当成协议错误 */
    for (i = 1; i < USB_PlayAltCount && i < USB_ALT_MAX; i++) {
        if (s_off_fb_play[i] != 0u) {
            patch_maxpkt (s_off_fb_play[i], is_hs ? 4u : 3u);
        }
    }
    /* 录音侧已不生成反馈端点，off 恒 0 —— patch 0 会写坏配置描述符头 */
    for (i = 1; i < USB_CapAltCount && i < USB_ALT_MAX; i++) {
        if (s_off_fb_cap[i] != 0u) {
            patch_maxpkt (s_off_fb_cap[i], is_hs ? 4u : 3u);
        }
    }

    /* bDescriptorType：0x02 = CONFIGURATION，0x07 = OTHER_SPEED_CONFIGURATION */
    MyCfgDescr_HS[1] = other_speed ? 0x07 : 0x02;

    *len = USB_CfgDesc_Len;
    return MyCfgDescr_HS;
}

/******************************************************************************/
/*                           字符串 / 其它描述符                               */
/******************************************************************************/

/* Language Descriptor */
const uint8_t MyLangDescr[] = {0x04, 0x03, 0x09, 0x04};

const uint8_t USB_HID_ReportDesc[USB_HID_REPORT_DESC_LEN] = {
    /* 整体照抄 QCC3095（VID 0A12 / PID 4007）的 Consumer 集合。
     *   byte0 bit0 Play/Pause(0xCD) bit1 Next(0xB5) bit2 Prev(0xB6) bit3 Stop(0xB7)
     *         bit4 Play(0xB0) bit5 Pause(0xB1) bit6 FF(0xB3) bit7 Rewind(0xB4)
     *   byte1 bit0 Vol+(0xE9) bit1 Vol-(0xEA) bit2 Mute(0xE2) 其余填充 */
    0x05, 0x0C,                         /* Usage Page (Consumer)            */
    0x09, 0x01,                         /* Usage (Consumer Control)         */
    0xA1, 0x01,                         /* Collection (Application)         */
    0x85, 0x01,                         /*   Report ID (1)                  */
    0x09, 0xCD,                         /*   Usage (Play/Pause)             */
    0x09, 0xB5,                         /*   Usage (Scan Next Track)        */
    0x09, 0xB6,                         /*   Usage (Scan Previous Track)    */
    0x09, 0xB7,                         /*   Usage (Stop)                   */
    0x15, 0x00, 0x25, 0x01,             /*   Logical 0..1                   */
    0x75, 0x01, 0x95, 0x04,             /*   1 bit x 4                      */
    0x81, 0x02,                         /*   Input (Data,Var,Abs)           */
    0x09, 0xB0,                         /*   Usage (Play)                   */
    0x09, 0xB1,                         /*   Usage (Pause)                  */
    0x09, 0xB3,                         /*   Usage (Fast Forward)           */
    0x09, 0xB4,                         /*   Usage (Rewind)                 */
    0x15, 0x00, 0x25, 0x01,             /*   Logical 0..1                   */
    0x75, 0x01, 0x95, 0x04,             /*   1 bit x 4                      */
    0x81, 0x22,                         /*   Input (Data,Var,Abs,NoPref)    */
    0x09, 0xE9,                         /*   Usage (Volume Increment)       */
    0x09, 0xEA,                         /*   Usage (Volume Decrement)       */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x02,
    0x81, 0x02,                         /*   Input (Data,Var,Abs)           */
    0x09, 0xE2,                         /*   Usage (Mute)                   */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01,
    0x81, 0x02,                         /*   Input (Data,Var,Abs)  静音为一次性绝对量 */
    0x75, 0x05, 0x95, 0x01, 0x81, 0x03, /* 5 位填充 -> 共 2 字节 */
    0xC0                                /* End Collection                   */
};

/* Build a USB string descriptor (bLength + 0x03 + UTF-16LE chars) from an
 * ASCII name configured at the top of this file. Single-threaded use only
 * (called from the USBHS IRQ). */
static uint8_t string_desc_buf[128];

static const uint8_t *BuildStringDesc (const char *ascii) {
    uint8_t n = 0;
    while (ascii[n] != '\0' && n < 63) {
        n++;
    }
    string_desc_buf[0] = (uint8_t)(2 + n * 2);
    string_desc_buf[1] = 0x03;
    for (uint8_t i = 0; i < n; i++) {
        string_desc_buf[2 + i * 2] = (uint8_t)ascii[i];
        string_desc_buf[3 + i * 2] = 0;
    }
    return string_desc_buf;
}

const uint8_t *USB_GetManufacturerDesc (void) {
    return BuildStringDesc (USB_MANUFACTURER_NAME);
}

const uint8_t *USB_GetProductDesc (void) {
    return BuildStringDesc (USB_PRODUCT_NAME);
}

/* Serial Number Information */
const uint8_t MySerNumInfo[] = {0x16, 0x03, '0', 0, '1', 0, '2', 0, '3', 0,
                                '4', 0, '5', 0, '6', 0, '8', 0, '9', 0, 'F', 0};

/* ---------------------------------------------------------------------
 * 读 96 位唯一 ID。地址读不到（全 0x00 / 全 0xFF）时退回一个固定值 ——
 * 序列号宁可重复也不能是空串：空串会让 Windows 每次都当新设备。
 * ------------------------------------------------------------------- */
static void uid_load (void) {
    static const uint8_t uid_fallback[MCU_UID_LEN] = {
        0x01, 0x23, 0x45, 0x68, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44};
    const volatile uint8_t *p = (const volatile uint8_t *)MCU_UID_BASE;
    uint8_t i;
    uint8_t all0 = 1;
    uint8_t allf = 1;

    for (i = 0; i < MCU_UID_LEN; i++) {
        uid_bytes[i] = p[i];
        if (uid_bytes[i] != 0x00) {
            all0 = 0;
        }
        if (uid_bytes[i] != 0xFF) {
            allf = 0;
        }
    }
    if (all0 || allf) {
        for (i = 0; i < MCU_UID_LEN; i++) {
            uid_bytes[i] = uid_fallback[i];
        }
    }

#if USB_PID_FROM_UID
    {
        uint16_t h = 0x5A3Cu;
        for (i = 0; i < MCU_UID_LEN; i++) {
            h = (uint16_t)(((uint16_t)(h << 1) ^ (uint16_t)(h >> 15) ^ uid_bytes[i]) & 0xFFFFu);
        }
        USB_DerivedPid = (uint16_t)(USB_PID_UID_BAND | (h & USB_PID_UID_MASK));
    }
#else
    USB_DerivedPid = (uint16_t)USB_PID;
#endif
}

/* "PREFIX-<24 位十六进制>" -> USB string descriptor */
static void serial_build (void) {
    static const char hexc[] = "0123456789ABCDEF";
    char ascii[sizeof (USB_SERIAL_PREFIX) + 1 + 2 * MCU_UID_LEN];
    uint8_t n = 0;
    uint8_t i;

    for (i = 0; i < (uint8_t)(sizeof (USB_SERIAL_PREFIX) - 1); i++) {
        ascii[n++] = USB_SERIAL_PREFIX[i];
    }
    ascii[n++] = '-';
    for (i = 0; i < MCU_UID_LEN; i++) {
        ascii[n++] = hexc[(uid_bytes[i] >> 4) & 0x0Fu];
        ascii[n++] = hexc[uid_bytes[i] & 0x0Fu];
    }

    serial_desc[0] = (uint8_t)(2u + (uint16_t)n * 2u);
    serial_desc[1] = 0x03; /* STRING */
    for (i = 0; i < n; i++) {
        serial_desc[2 + i * 2] = (uint8_t)ascii[i];
        serial_desc[3 + i * 2] = 0;
    }
}

static void devdescr_build (void) {
    uint8_t i;

    uid_load();
    serial_build();
    for (i = 0; i < sizeof (MyDevDescr); i++) {
        MyDevDescrDyn[i] = MyDevDescr[i];
    }
    MyDevDescrDyn[10] = (uint8_t)(USB_DerivedPid & 0xFF);        /* idProduct L */
    MyDevDescrDyn[11] = (uint8_t)((USB_DerivedPid >> 8) & 0xFF); /* idProduct H */
    uid_ready = 1;
}

const uint8_t *USB_GetDeviceDesc (void) {
    if (!uid_ready) {
        devdescr_build();
    }
    return MyDevDescrDyn;
}

const uint8_t *USB_GetSerialDesc (void) {
    if (!uid_ready) {
        devdescr_build();
    }
    return serial_desc;
}

/* Device Qualified Descriptor */
const uint8_t MyQuaDesc[] = {
    0x0A,  // bLength
    0x06,  // bDescriptorType
    0x00,  // bcdUSB 2.00
    0x02,
    USB_AUDIO_ENABLE ? 0xEF : 0, // bDeviceClass
    USB_AUDIO_ENABLE ? 0x02 : 0, // bDeviceSubClass
    USB_AUDIO_ENABLE ? 0x01 : 0, // bDeviceProtocol
    0x40,  // bMaxPacketSize
    0x01,  // bNumConfigurations
    0x00,  // bReserved
};

/* Device BOS Descriptor */
const uint8_t MyBOSDesc[] = {
    0x05,
    0x0F,
    0x0C,
    0x00,
    0x01,
    0x07,
    0x10,
    0x02,
    0x02,
    0x00,
    0x00,
    0x00,
};

/******************************************************************************/
/*                    当前采样率（GET_CUR 的内容 / boot 同步）                  */
/******************************************************************************/
void USB_SetCurrentRate (uint32_t play_rate, uint32_t cap_rate) {
    USB_SampleFreqPlay[0] = (uint8_t)(play_rate & 0xFF);
    USB_SampleFreqPlay[1] = (uint8_t)((play_rate >> 8) & 0xFF);
    USB_SampleFreqPlay[2] = (uint8_t)((play_rate >> 16) & 0xFF);
    USB_SampleFreqPlay[3] = (uint8_t)((play_rate >> 24) & 0xFF);
    USB_SampleFreqCap[0] = (uint8_t)(cap_rate & 0xFF);
    USB_SampleFreqCap[1] = (uint8_t)((cap_rate >> 8) & 0xFF);
    USB_SampleFreqCap[2] = (uint8_t)((cap_rate >> 16) & 0xFF);
    USB_SampleFreqCap[3] = (uint8_t)((cap_rate >> 24) & 0xFF);
}

uint32_t USB_GetCurrentRate (void) {
    return (uint32_t)USB_SampleFreqPlay[0] | ((uint32_t)USB_SampleFreqPlay[1] << 8) |
           ((uint32_t)USB_SampleFreqPlay[2] << 16) |
           ((uint32_t)USB_SampleFreqPlay[3] << 24);
}

uint32_t USB_GetCapRate (void) {
    return (uint32_t)USB_SampleFreqCap[0] | ((uint32_t)USB_SampleFreqCap[1] << 8) |
           ((uint32_t)USB_SampleFreqCap[2] << 16) |
           ((uint32_t)USB_SampleFreqCap[3] << 24);
}
