#include "main.h"
#include "key_driver.h"
#include "hid_app.h"
#include "board.h"
#include "ws2812_driver.h"

/*---------------------------------------------------------------------
 *  三个按键实例。引脚必须写字面量：GPIO_Desc 是复合字面量，
 *  不能用在静态初始化里。改引脚时 main.h 和这里要一起改。
 *      KEY1 = PC8, KEY2 = PD2, KEY3 = PB14（按下为低）
 *-------------------------------------------------------------------*/
volatile Key_Dev_t Key_1 = {.GPIOx = GPIOC, .GPIO_Pin = GPIO_Pin_8};
volatile Key_Dev_t Key_2 = {.GPIOx = GPIOD, .GPIO_Pin = GPIO_Pin_2};
volatile Key_Dev_t Key_3 = {.GPIOx = GPIOB, .GPIO_Pin = GPIO_Pin_14};

/*---------------------------------------------------------------------
 *  三键脚：上拉输入（按下为低）
 *-------------------------------------------------------------------*/
void KEY_Setup (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8; /* KEY1 = PC8  */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; /* KEY2 = PD2  */
    GPIO_Init (GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14; /* KEY3 = PB14 */
    GPIO_Init (GPIOB, &GPIO_InitStructure);
}

static void status_apply (uint8_t r, uint8_t g, uint8_t b) {
    WS2812_Fill (r, g, b);
    WS2812_Show();
}

/*---------------------------------------------------------------------
 *  事件消费：在主循环里跑，绝不放进中断（会牵动 USB/I2C/串口）
 *
 *  KEY1 单击 播放/暂停   双击 静音    三击 停止    长按 关状态灯
 *  KEY2 单击 音量减      双击 上一首  长按 关功放
 *  KEY3 单击 音量加      双击 下一首  长按 开功放
 *-------------------------------------------------------------------*/
static uint8_t boot_chord;
static uint32_t boot_chord_start;
void Key_Event_Handler (void) {
    uint8_t both=(GPIO_ReadInputDataBit(Key_2.GPIOx,Key_2.GPIO_Pin)==Bit_RESET &&
                  GPIO_ReadInputDataBit(Key_3.GPIOx,Key_3.GPIO_Pin)==Bit_RESET);
    if(both && !boot_chord) {boot_chord=1u;boot_chord_start=tim6_tick;}
    if(boot_chord) {
        Key_2.Event=Key_3.Event=KEY_EVENT_NONE;
        if(both && boot_chord==1u && (uint32_t)(tim6_tick-boot_chord_start)>=50u) {
            boot_chord=2u; Board_EnterBootloader();
        }
        if(!both) {
            boot_chord_start=tim6_tick;
            if(GPIO_ReadInputDataBit(Key_2.GPIOx,Key_2.GPIO_Pin)==Bit_SET &&
               GPIO_ReadInputDataBit(Key_3.GPIOx,Key_3.GPIO_Pin)==Bit_SET) boot_chord=0u;
        }
    }
    if (Key_1.Event != KEY_EVENT_NONE) {
        switch (Key_1.Event) {
        case KEY_EVENT_SINGLE:
            HID_PlayPause();
            printf ("Key1 SingleKey -> HID Play/Pause\r\n");
            break;
        case KEY_EVENT_DOUBLE:
            HID_Mute();
            printf ("Key1 DoubleKey -> HID Mute\r\n");
            break;
        case KEY_EVENT_TRIPLE:
            HID_Stop();
            printf ("Key1 TripleKey -> HID Stop\r\n");
            break;
        case KEY_EVENT_LONG:
            status_apply (0, 0, 0);
            printf ("Key1 LongKey -> LED off\r\n");
            break;
        default:
            break;
        }
        Key_1.Event = KEY_EVENT_NONE;
    }

    if (Key_2.Event != KEY_EVENT_NONE) {
        switch (Key_2.Event) {
        case KEY_EVENT_SINGLE:
            HID_VolDown();
            printf ("Key2 SingleKey -> HID Volume Down\r\n");
            break;
        case KEY_EVENT_DOUBLE:
            HID_Prev();
            printf ("Key2 DoubleKey -> HID Previous Track\r\n");
            break;
        case KEY_EVENT_LONG:
            OAMP_Power (false);
            printf ("Key2 LongKey -> OAMP off\r\n");
            break;
        default:
            break;
        }
        Key_2.Event = KEY_EVENT_NONE;
    }

    if (Key_3.Event != KEY_EVENT_NONE) {
        switch (Key_3.Event) {
        case KEY_EVENT_SINGLE:
            HID_VolUp();
            printf ("Key3 SingleKey -> HID Volume Up\r\n");
            break;
        case KEY_EVENT_DOUBLE:
            HID_Next();
            printf ("Key3 DoubleKey -> HID Next Track\r\n");
            break;
        case KEY_EVENT_LONG:
            OAMP_Power (true);
            printf ("Key3 LongKey -> OAMP on\r\n");
            break;
        default:
            break;
        }
        Key_3.Event = KEY_EVENT_NONE;
    }
}

/*---------------------------------------------------------------------
 *  数采样寄存器里的上升沿个数（松开次数）
 *-------------------------------------------------------------------*/
static uint8_t Rise_Edge_Counter (uint32_t Input) {
    uint32_t Mask;

    if (Input == 0xFFFFFFFFu) {
        return KEY_EVENT_NONE;
    }
    Mask = ~Input & (Input << 1);
    Mask = (Mask & 0x55555555u) + ((Mask >> 1) & 0x55555555u);
    Mask = (Mask & 0x33333333u) + ((Mask >> 2) & 0x33333333u);
    Mask = (Mask + (Mask >> 4)) & 0x0F0F0F0Fu;
    Mask = Mask + (Mask >> 8);
    Mask = Mask + (Mask >> 16);
    Mask = Mask & 0x0000003Fu;
    return (uint8_t)Mask;
}

/*---------------------------------------------------------------------
 *  每次调用采 1 个样本；main.c主循环中按TIM6 20ms时序同步执行
 *  所以攒满 32 个样本约 640ms 出一份判定。
 *  连续 8 次高电平（已松开）提前收尾。
 *-------------------------------------------------------------------*/
void Key_Process (volatile Key_Dev_t *btn) {
    bool Read_Temp;

    if (btn == 0) {
        return;
    }
    if(boot_chord && (btn==&Key_2 || btn==&Key_3)) {
        btn->Event=KEY_EVENT_NONE;btn->Is_Sampling=btn->Start_Sampling=false;
        btn->Sampling_Counter=btn->Key_Release_Counter=0u;
        btn->Previous_Key=btn->Now_Key=true;
        return;
    }

    Read_Temp = (GPIO_ReadInputDataBit (btn->GPIOx, btn->GPIO_Pin) == Bit_SET);

    /* 1. 检测下降沿（按下），触发一轮采样 */
    if (!btn->Is_Sampling) {
        btn->Previous_Key = btn->Now_Key;
        btn->Now_Key = Read_Temp;

        if (!btn->Now_Key && btn->Previous_Key) {
            btn->Start_Sampling = true;
        } else {
            btn->Sampling_Counter = 0;
            btn->Sampling_Temp = 0;
            btn->Key_Release_Counter = 0;
            return;
        }
    }

    /* 2. 采样 32 次；连续 8 次高电平提前收尾 */
    if (btn->Start_Sampling) {
        if (btn->Sampling_Counter < 32u) {
            btn->Is_Sampling = true;
            btn->Sampling_Temp <<= 1;

            if (Read_Temp) {
                btn->Sampling_Temp |= 0x01u;
                btn->Key_Release_Counter++;
                if (btn->Key_Release_Counter >= 8u) {
                    uint8_t remain = (uint8_t)(32u - btn->Sampling_Counter);
                    btn->Key_Release_Counter = 0;
                    btn->Sampling_Temp = (btn->Sampling_Temp << remain) |
                                         ((1UL << remain) - 1UL);
                    btn->Sampling_Counter = 32u;
                }
            } else {
                btn->Key_Release_Counter = 0;
            }
            btn->Sampling_Counter++;
        }

        if (btn->Sampling_Counter >= 32u) {
            uint8_t edges;

            btn->Is_Sampling = false;
            btn->Start_Sampling = false;
            btn->Sampling_Counter = 0;

            edges = Rise_Edge_Counter (btn->Sampling_Temp);
            switch (edges) {
            case 0: btn->Event = KEY_EVENT_LONG; break;
            case 1: btn->Event = KEY_EVENT_SINGLE; break;
            case 2: btn->Event = KEY_EVENT_DOUBLE; break;
            case 3: btn->Event = KEY_EVENT_TRIPLE; break;
            default: btn->Event = KEY_EVENT_NONE; break;
            }
        }
    }
}
