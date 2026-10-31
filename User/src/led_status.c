#include "led_status.h"
#include "main.h"
#include "i2s_app.h"
#include "ws2812_driver.h"
#include "usbhs_device.h"


static void Rate_Hue (uint32_t rate, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t hue;

    switch (rate) {
    case 48000u:  hue = 96u;  break;
    case 96000u:  hue = 64u;  break;
    case 192000u: hue = 128u; break;
    case 384000u: hue = 160u; break;
    default:      hue = 0u;   break;
    }
    WS2812_HSV (hue, 255, 90, r, g, b);
}

void Led_RateUpdate (void) {
    static uint32_t last_play;
    static uint32_t last_cap;
    static uint8_t  inited;
    static uint8_t last_online;
    uint8_t online=USB_HostReady();
    uint8_t r, g, b;

    if (ws2812_busy || (inited && last_online==online && last_play == i2s_rate && last_cap == i2s_cap_rate)) {
        return;
    }
    inited    = 1;
    last_online=online;
    last_play = i2s_rate;
    last_cap  = i2s_cap_rate;

    if (!online) {
        WS2812_Fill(0,0,0);
        WS2812_Show();
        return;
    }
    Rate_Hue (i2s_rate, &r, &g, &b);            /* LED0 = 播放 */
    WS2812_SetPixel (0, r, g, b);
    Rate_Hue (i2s_cap_rate, &r, &g, &b);        /* LED1 = 录音 */
    WS2812_SetPixel (1, r, g, b);
    WS2812_Show ();
}
