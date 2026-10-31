#include "main.h"
#include "key_driver.h"
#include "ws2812_driver.h"
#include "dac_adc_app.h"
#include "hid_app.h"
#include "console_app.h"
#include "led_status.h"
#include "volume_app.h"

volatile uint8_t i2c_idle;
uint32_t system_reset_flags;

int main (void) {
    system_reset_flags = RCC->RSTSCKR;
    RCC_ClearFlag();
    NVIC_PriorityGroupConfig (NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    GPIO_Setup(); /* 初始化引脚 */
    KEY_Setup();

    UART7_Setup();  /* 串口+DMA初始化 */

    I2C_Setup();    /* I2C初始化 */

    TIM6_Setup();   /* 20ms 心跳：喂狗 + 扫按键 */
    WS2812_Setup(); /* 先刷黑 */

    I2S_Setup();    /* I2S3 播放 + I2S2 录音 + MCLK PWM */

    printf ("========== CH32V305GBU6 Audio Bridge ==========\r\n");
    printf ("========== firmware %s ==========\r\n", FIRMWARE_VERSION);

#if I2C_ENABLE && USB_AUDIO_ENABLE
    /* DAC = ES9038Q2M @0x48，ADC = TLV320ADC5120 @0x4E */
    {
        uint8_t required = (USB_PLAY_ENABLE ? 1u : 0u) | (USB_CAPTURE_ENABLE ? 2u : 0u);
        uint8_t r = DAC_ADC_Setup (48000);
        // 检查是否两者都成功 (二进制 0000 0011 = 0x03)
        if ((r & required) == required) {
            printf ("========== DAC/ADC init OK (Status=0x%02X) =========\r\n", (unsigned)r);
        } else {
            // 如果没全成功，打印具体哪个位没亮
            printf ("========== DAC/ADC Init Status=0x%02X (bit0=DAC bit1=ADC, 1=OK) =========\r\n",
                    (unsigned)r);
        }
    }
#else
    printf ("==========   DAC/ADC  init skipped   ==========\r\n");
#endif

    Volume_Init();

    printf ("==========        enum  start        ==========\r\n");
    USBHS_RCC_Init();
    USBHS_Device_Init (ENABLE); /* 初始化USB，之后的枚举全部在USB中断中进行*/

    OAMP_Power (USB_AUDIO_ENABLE != 0);
    if (USB_PLAY_ENABLE)
        HAMP_MUTE_OFF();
    else
        HAMP_MUTE_ON();

    IWDG_Setup(); /* 最后开狗：前面初始化不受 2s 限制 */

    while (1) {
        USB_StreamService();
        Console_Service();  /* 串口行编辑终端：help 看命令表 */

        USB_ApplyPending(); /* 主机 SET_CUR 登记的采样率，在这里真正落硬件 */
        USB_StreamService();

        /* FU 音量/静音落硬件（USB 中断只置脏标志，见 volume_app.c 顶部）。
         * 放在主循环最前面：它关系到"主机拖一下音量条要多久听到变化"，
         * 而一次 I2C 写几十微秒，趁 USB_ApplyPending 之后总线空闲做。 */
        Volume_Handler();
        USB_StreamService();

        Key_Event_Handler();

        if (Tim6_Sync) {
            Tim6_Sync = false;
            Key_Process (&Key_1);
            Key_Process (&Key_2);
            Key_Process (&Key_3);

            /* 反馈控制从 SOF 中断挪到这里 */
            (void)I2S_FeedbackUpdate (0, 0);
        }
        HID_Poll();       /* 松开上一份 HID 报告 */
        Led_RateUpdate(); /* 两颗灯 = 播放/录音采样率 */
        I2S_PlayGuard();  /* 环高/低水位守护 */
        /* Check RAM pointer alignment, hardware configuration and DMA progress.
         * These checks cannot classify white noise from arbitrary host PCM. */
        (void)I2S_PlayHealthCheck();
        I2S_CapHealthCheck();
        I2S_StreamWatch(); /* USB 掉线 → 静音 + 清环 */
        UART7_TxPoll();    /* 串口发送自愈 */
        /* 系统的活着证明：只有走到这里才喂狗 */
        IWDG_Feed();
    }
}
