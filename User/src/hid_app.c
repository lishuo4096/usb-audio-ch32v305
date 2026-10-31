/********************************** (C) COPYRIGHT *******************************
 * File Name          : hid_app.c
 * Description        : USB HID 消费者控制（媒体键）应用层。
 *
 *   KEY1 单击 -> 播放/暂停   KEY1 双击 -> 静音    KEY1 三击 -> 停止
 *   KEY2 单击 -> 音量减      KEY2 双击 -> 上一首
 *   KEY3 单击 -> 音量加      KEY3 双击 -> 下一首
 *
 *******************************************************************************/
#include "settings.h"
#include "hid_app.h"
#include "usbhs_device.h"
#include <stdbool.h>
#include "tim6_app.h"
#include <string.h>

#if !USB_HID_ENABLE
uint8_t HID_SendKey(uint16_t bits) {(void)bits;return 0u;}
/* HID 关闭时保留空实现，按键/主循环调用点不必改动 */
void HID_PlayPause (void) { }
void HID_Next (void) { }
void HID_Prev (void) { }
void HID_Stop (void) { }
void HID_VolUp (void) { }
void HID_VolDown (void) { }
void HID_Mute (void) { }
void HID_Poll (void) { }
uint16_t HID_GetBits (void) { return 0u; }
#else

/* 报告要"被主机取走"才算送达。取走与否靠 usb_ep4_ok 判断，超时兜底重发。 */
#define HID_GIVE_UP_TICKS 25u /* 500ms（TIM6 tick=20ms） */
#define HID_RETRY_MAX 10u     /* 报告被兜底丢弃后最多重发几次 */

static volatile uint16_t hid_bits; /* bit 映射见 hid_app.h */

/* 0=空闲 1=按下报告待取走 2=松开报告待取走 */
static uint8_t hid_state;
static uint8_t hid_retry;
static uint32_t hid_tick;
static uint32_t hid_taken; /* 发送瞬间的 usb_ep4_ok 快照 */
static uint32_t hid_lost;  /* 发送瞬间的 usb_ep4_to 快照 */
static uint16_t hid_queue[8];
static uint8_t hid_read,hid_write;
static uint8_t hid_offline=1u;
uint8_t HID_SendKey(uint16_t bits) {
    if (!USB_HostReady() || !bits || (bits & ~0x070Fu) ||
        (uint8_t)(hid_write-hid_read)>=8u) return 0u;
    hid_queue[hid_write++ & 7u]=bits;
    return 1u;
}

static void HidSend (void) {
    /* 报告格式与 usb_desc.c 的 77 字节描述符严格一致：
     * [0]=Report ID 1, [1]=byte0, [2]=byte1（InputReportByteLength=3） */
    uint8_t rpt[3];

    rpt[0] = 0x01u;
    rpt[1] = (uint8_t)(hid_bits & 0xFFu);
    rpt[2] = (uint8_t)((hid_bits >> 8) & 0xFFu);

    /* 先取快照再发，否则会漏掉"发完立刻被取走"的那一次 */
    hid_taken = usb_ep4_ok;
    hid_lost = usb_ep4_to;
    USB_HID_Send (rpt, 3);
}

static void HidPress (uint16_t bit) {
    hid_bits = bit;
    hid_state = 1u;
    hid_retry = 0u;
    hid_tick = tim6_tick;
    HidSend();
}

void HID_PlayPause (void) { (void)HID_SendKey (HID_KEY_PLAY_PAUSE); }
void HID_Next (void)      { (void)HID_SendKey (HID_KEY_NEXT); }
void HID_Prev (void)      { (void)HID_SendKey (HID_KEY_PREV); }
void HID_Stop (void)      { (void)HID_SendKey (HID_KEY_STOP); }
void HID_VolUp (void)     { (void)HID_SendKey (HID_KEY_VOL_UP); }
void HID_VolDown (void)   { (void)HID_SendKey (HID_KEY_VOL_DOWN); }
void HID_Mute (void)      { (void)HID_SendKey (HID_KEY_MUTE); }

uint16_t HID_GetBits (void) {
    return hid_bits;
}

void HID_Poll (void) {
    uint32_t dt;
    if (!USB_HostReady()) {
        hid_read=hid_write=hid_state=0u;hid_bits=0u;
        hid_offline=1u;
        return;
    }
    if(hid_offline) {
        hid_offline=0u;hid_state=2u;hid_bits=0u;hid_retry=0u;hid_tick=tim6_tick;HidSend();
        return;
    }

    if (hid_state == 0u) {
        if(hid_read!=hid_write) HidPress(hid_queue[hid_read++ & 7u]);
        return;
    }
    dt = tim6_tick - hid_tick; /* 无符号差值，回绕安全 */
    if ((int32_t)dt < 0) {
        dt = 0u;
    }

    /* 按下报告：被取走 -> 发松开；被兜底丢弃或超时 -> 重发同一份按下 */
    if (hid_state == 1u) {
        if (usb_ep4_ok != hid_taken) {
            hid_bits = 0u;
            hid_state = 2u;
            hid_tick = tim6_tick;
            HidSend();
        } else if ((usb_ep4_to != hid_lost) || (dt >= HID_GIVE_UP_TICKS)) {
            if (hid_retry < HID_RETRY_MAX) {
                hid_retry++;
                hid_tick = tim6_tick;
                HidSend();
            } else {
                hid_bits = 0u;
                hid_state = 0u;
            }
        }
        return;
    }

    /* 松开报告：丢了会让主机以为键一直按着，所以同样重发 */
    if (usb_ep4_ok != hid_taken) {
        hid_bits = 0u;
        hid_state = 0u;
    } else if ((usb_ep4_to != hid_lost) || (dt >= HID_GIVE_UP_TICKS)) {
        if (hid_retry < HID_RETRY_MAX) {
            hid_retry++;
            hid_tick = tim6_tick;
            HidSend();
        } else {
            hid_bits = 0u;
            hid_state = 0u;
        }
    }
}

#endif                     /* USB_HID_ENABLE */
