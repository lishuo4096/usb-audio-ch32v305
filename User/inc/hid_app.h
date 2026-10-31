/********************************** (C) COPYRIGHT *******************************
 * File Name          : hid_app.h
 * Description        : USB HID 消费者控制（媒体键）应用层。
 *                      报告格式见 usb_desc.c 的 USB_HID_ReportDesc：
 *                      byte0 = 播放类键，byte1 = 音量类键（共 16 bit + Report ID）
 *******************************************************************************/
#ifndef __HID_APP_H
#define __HID_APP_H

#include <stdint.h>

/* 与 usb_desc.c 里 Report Descriptor 的 Usage 一一对应 */
/* byte0：第一组 81 02（CD/B5/B6/B7）+ 第二组 81 22（B0/B1/B3/B4） */
#define HID_KEY_PLAY_PAUSE      0x0001u  /* bit0  Consumer 0xCD Play/Pause */
#define HID_KEY_NEXT            0x0002u  /* bit1  Consumer 0xB5 Scan Next Track */
#define HID_KEY_PREV            0x0004u  /* bit2  Consumer 0xB6 Scan Previous Track */
#define HID_KEY_STOP            0x0008u  /* bit3  Consumer 0xB7 Stop */
/* byte1：第三组 81 02（E9/EA）+ 第四组 81 02（E2） */
#define HID_KEY_VOL_UP          0x0100u  /* bit0  Consumer 0xE9 Volume Increment */
#define HID_KEY_VOL_DOWN        0x0200u  /* bit1  Consumer 0xEA Volume Decrement */
#define HID_KEY_MUTE            0x0400u  /* bit2  Consumer 0xE2 Mute */

void HID_PlayPause (void);
uint8_t HID_SendKey(uint16_t bits);
void HID_Next (void);
void HID_Prev (void);
void HID_Stop (void);
void HID_VolUp (void);
void HID_VolDown (void);
void HID_Mute (void);

/* 主循环轮询：按下报告被主机取走后补发松开报告 */
void HID_Poll (void);

/* 当前键位（调试用，串口 'e' 命令打印） */
uint16_t HID_GetBits (void);

#endif /* __HID_APP_H */
