#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#ifndef true
#define true 1
#define false 0
#endif

/* USB functions are independent; HID also works without audio. */
#ifndef USB_PLAY_ENABLE
#define USB_PLAY_ENABLE 1
#endif
#ifndef USB_CAPTURE_ENABLE
#define USB_CAPTURE_ENABLE 1
#endif
#ifndef USB_HID_ENABLE
#define USB_HID_ENABLE 1
#endif
#ifndef USB_FU_ENABLE
#define USB_FU_ENABLE 1
#endif
#ifndef I2C_ENABLE
#define I2C_ENABLE 1
#endif
#ifndef USB_VID
#define USB_VID 0x1A86
#endif
#ifndef USB_PID_BASE
#define USB_PID_BASE 0x6005
#endif
#ifndef USB_PRODUCT_NAME
#define USB_PRODUCT_NAME "CH32V305 Audio Bridge V1.0"
#endif
#ifndef USB_MANUFACTURER_NAME
#define USB_MANUFACTURER_NAME "Nanjing Qinheng Microelectronics"
#endif
#ifndef USB_SERIAL_PREFIX
#define USB_SERIAL_PREFIX "CH32V305GBU6-0"
#endif
#ifndef USB_DEVICE_VERSION
#define USB_DEVICE_VERSION 0x0C
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "V1.0"
#endif
#ifndef USB_PID_FROM_UID
#define USB_PID_FROM_UID 0
#endif
#ifndef USB_FORCE_FS
#define USB_FORCE_FS 0
#endif
#ifndef DAC_OUTPUT_CROSS
#define DAC_OUTPUT_CROSS 1
#endif
#ifndef PLAY_SWAP_DEFAULT
#define PLAY_SWAP_DEFAULT 1
#endif
#ifndef CAPTURE_SWAP_DEFAULT
#define CAPTURE_SWAP_DEFAULT 0
#endif
#ifndef AMP_MUTE_GPIO_EN
#define AMP_MUTE_GPIO_EN 0
#endif
#ifndef DAC_SOFT_MUTE_EN
#define DAC_SOFT_MUTE_EN 1
#endif
#ifndef ZERO_MUTE_ACTIVE_LEVEL
#define ZERO_MUTE_ACTIVE_LEVEL 0
#endif
#ifndef PLAY_UNMUTE_DELAY_US
#define PLAY_UNMUTE_DELAY_US 10000u
#endif
#ifndef AMP_GPIO_SETTLE_US
#define AMP_GPIO_SETTLE_US 1000u
#endif
#ifndef AMP_MUTE_WAIT_US
#define AMP_MUTE_WAIT_US 12000u
#endif
#ifndef DAC_VOLUME_RAMP_RATE
#define DAC_VOLUME_RAMP_RATE 7u
#endif
/* Logical host dB -> DAC attenuation. 20% now matches the former 5%.
 * 0dB still selects maximum output; set both factors to 1 for calibrated dB.
 * The host dB label is a control coordinate with this custom curve enabled. */
#ifndef DAC_VOLUME_CURVE_NUM
#define DAC_VOLUME_CURVE_NUM 91u
#endif
#ifndef DAC_VOLUME_CURVE_DEN
#define DAC_VOLUME_CURVE_DEN 49u
#endif
#if DAC_VOLUME_CURVE_DEN == 0 || DAC_VOLUME_CURVE_NUM < DAC_VOLUME_CURVE_DEN
#error Invalid DAC volume curve
#endif
#ifndef DAC_LOCK_SPEED
#define DAC_LOCK_SPEED 15u
#endif
#ifndef DAC_DPLL_BW_SERIAL
#define DAC_DPLL_BW_SERIAL 15u
#endif
#ifndef ZERO_DETECT_ENABLE
#define ZERO_DETECT_ENABLE 1
#endif
#ifndef ZERO_DETECT_THRESH
#define ZERO_DETECT_THRESH 644245u
#endif
#ifndef ZERO_MUTE_HOLD_TICKS
#define ZERO_MUTE_HOLD_TICKS 25u
#endif
#ifndef ADC_DEFAULT_GAIN_DB
#define ADC_DEFAULT_GAIN_DB 12
#endif
#ifndef ADC_LIMIT_SAVED_GAIN_ON_BOOT
#define ADC_LIMIT_SAVED_GAIN_ON_BOOT 1
#endif
#ifndef ADC_DRE_ENABLE
#define ADC_DRE_ENABLE 1 /* Stereo DRE at 48/96k; disabled at 192/384k. */
#endif
#ifndef ADC_DRE_CONFIG
#define ADC_DRE_CONFIG 0x4Bu /* -36dB trigger, up to 24dB additional gain. */
#endif
#ifndef ADC_INPUT_IMPEDANCE_OHMS
#define ADC_INPUT_IMPEDANCE_OHMS 2500u
#endif
#if ADC_INPUT_IMPEDANCE_OHMS != 2500u && ADC_INPUT_IMPEDANCE_OHMS != 10000u && ADC_INPUT_IMPEDANCE_OHMS != 20000u
#error ADC_INPUT_IMPEDANCE_OHMS must be 2500, 10000 or 20000
#endif
#ifndef FU_SAVE_ON_CHANGE
#define FU_SAVE_ON_CHANGE 1
#endif
#ifndef PLC_ENABLE
#define PLC_ENABLE 1
#endif
#ifndef PLC_DECAY_Q15
#define PLC_DECAY_Q15 20316u
#endif
#ifndef PLC_RECOVER_Q15
#define PLC_RECOVER_Q15 5898u
#endif
#ifndef PLC_SILENCE_Q15
#define PLC_SILENCE_Q15 328u
#endif
#ifndef FEEDBACK_TAU_MS
#define FEEDBACK_TAU_MS 100
#endif
#ifndef FEEDBACK_TRIM_MAX_PCT
#define FEEDBACK_TRIM_MAX_PCT 1
#endif
#ifndef PLAY_TARGET_US
#define PLAY_TARGET_US 2000
#endif
#ifndef UART7_BAUD
#define UART7_BAUD 115200
#endif
#ifndef UART7_RX_RING_BYTES
#define UART7_RX_RING_BYTES 1024
#endif
#ifndef UART7_RX_USE_DMA
#define UART7_RX_USE_DMA 0
#endif
#ifndef UART7_RX_DMA_CH
#define UART7_RX_DMA_CH DMA2_Channel9
#endif
#ifndef UART7_TX_RING_BYTES
#define UART7_TX_RING_BYTES 4096u
#endif
#ifndef UART7_TX_DMA_CH
#define UART7_TX_DMA_CH DMA2_Channel8
#endif
#ifndef UART7_TX_SPIN_LIMIT
#define UART7_TX_SPIN_LIMIT 400000u
#endif
#ifndef TIM6_PERIOD_MS
#define TIM6_PERIOD_MS 20
#endif
#ifndef TIM6_PRESCALER
#define TIM6_PRESCALER (12288 - 1)
#endif
#ifndef IWDG_ENABLE
#define IWDG_ENABLE 1
#endif
#ifndef IWDG_PRESCALER
#define IWDG_PRESCALER IWDG_Prescaler_64
#endif
#ifndef IWDG_RELOAD
#define IWDG_RELOAD 1250
#endif
#ifndef I2C2_CLOCK_SPEED
#define I2C2_CLOCK_SPEED 400000
#endif
#ifndef I2C2_TIMEOUT_LOOPS
#define I2C2_TIMEOUT_LOOPS 200000
#endif
#ifndef I2C2_TIMEOUT_US
#define I2C2_TIMEOUT_US 2000u
#endif
#if I2C2_TIMEOUT_US > 30000u || I2C2_TIMEOUT_US == 0u
#error "I2C timeout must fit the TIM8 timebase"
#endif
#ifndef DAC_I2C_ADDR7
#define DAC_I2C_ADDR7 0x48u
#endif
#ifndef ADC_I2C_ADDR7
#define ADC_I2C_ADDR7 0x4Eu
#endif
#ifndef WS2812_LED_NUM
#define WS2812_LED_NUM 2u
#endif
#ifndef WS2812_TIM_ARR
#define WS2812_TIM_ARR 152u
#endif
#ifndef WS2812_T0H
#define WS2812_T0H 43u
#endif
#ifndef WS2812_T1H
#define WS2812_T1H 98u
#endif
#ifndef WS2812_RESET_SLOTS
#define WS2812_RESET_SLOTS 60u
#endif
/* Common rate set x common bit-depth set; 48k is the FS baseline. */
#ifndef USB_RATE_96K_ENABLE
#define USB_RATE_96K_ENABLE 1
#endif
#ifndef USB_RATE_192K_ENABLE
#define USB_RATE_192K_ENABLE 1
#endif
#ifndef USB_RATE_384K_ENABLE
#define USB_RATE_384K_ENABLE 1
#endif
#ifndef USB_PCM16_ENABLE
#define USB_PCM16_ENABLE 1
#endif
#ifndef USB_PCM24_ENABLE
#define USB_PCM24_ENABLE 1
#endif
#ifndef USB_PCM32_ENABLE
#define USB_PCM32_ENABLE 1
#endif
#define RATE_48K_16BIT USB_PCM16_ENABLE
#define RATE_48K_24BIT USB_PCM24_ENABLE
#define RATE_48K_32BIT USB_PCM32_ENABLE
#define RATE_96K_16BIT (USB_RATE_96K_ENABLE && USB_PCM16_ENABLE)
#define RATE_96K_24BIT (USB_RATE_96K_ENABLE && USB_PCM24_ENABLE)
#define RATE_96K_32BIT (USB_RATE_96K_ENABLE && USB_PCM32_ENABLE)
#define RATE_192K_16BIT (USB_RATE_192K_ENABLE && USB_PCM16_ENABLE)
#define RATE_192K_24BIT (USB_RATE_192K_ENABLE && USB_PCM24_ENABLE)
#define RATE_192K_32BIT (USB_RATE_192K_ENABLE && USB_PCM32_ENABLE)
#define RATE_384K_16BIT (USB_RATE_384K_ENABLE && USB_PCM16_ENABLE)
#define RATE_384K_24BIT (USB_RATE_384K_ENABLE && USB_PCM24_ENABLE)
#define RATE_384K_32BIT (USB_RATE_384K_ENABLE && USB_PCM32_ENABLE)

/* Derived values: do not edit. Software volume on the host needs no FU. */
#define USB_FU_ACTIVE (USB_FU_ENABLE && I2C_ENABLE)
#define USB_AUDIO_ENABLE (USB_PLAY_ENABLE || USB_CAPTURE_ENABLE)
#define USB_PROFILE_CODE ((!USB_PLAY_ENABLE) | ((!USB_CAPTURE_ENABLE) << 1) | ((!USB_HID_ENABLE) << 2) | ((!USB_FU_ACTIVE) << 3) | ((!I2C_ENABLE) << 4))
#ifndef USB_PID
#define USB_PID (USB_PID_BASE + (USB_PROFILE_CODE << 8))
#endif
#if USB_PID > 0xFFFFu
#error "Derived USB PID overflow; lower USB_PID_BASE or override USB_PID"
#endif
#define USB_IF_PLAY (USB_PLAY_ENABLE ? 1u : 255u)
#define USB_IF_CAP (USB_CAPTURE_ENABLE ? (1u + USB_PLAY_ENABLE) : 255u)
#define USB_IF_HID (USB_HID_ENABLE ? (USB_AUDIO_ENABLE + USB_PLAY_ENABLE + USB_CAPTURE_ENABLE) : 255u)
#define USB_INTERFACE_COUNT (USB_AUDIO_ENABLE + USB_PLAY_ENABLE + USB_CAPTURE_ENABLE + USB_HID_ENABLE)
#define I2C_INIT_EN I2C_ENABLE
#if USB_AUDIO_ENABLE && !(USB_PCM16_ENABLE || USB_PCM24_ENABLE || USB_PCM32_ENABLE)
#error "Enable at least one PCM bit depth"
#endif
#if !USB_INTERFACE_COUNT
#error "Enable at least one USB function"
#endif
#if PLAY_UNMUTE_DELAY_US > 30000u || AMP_GPIO_SETTLE_US > 30000u || AMP_MUTE_WAIT_US > 30000u
#error "Mute delays must fit the 16-bit 960kHz timebase"
#endif
#if DAC_VOLUME_RAMP_RATE > 7u || DAC_LOCK_SPEED > 15u || DAC_DPLL_BW_SERIAL > 15u
#error "DAC setting outside register range"
#endif
#define EN_384K (RATE_384K_32BIT || RATE_384K_24BIT || RATE_384K_16BIT)
#define EN_192K (RATE_192K_32BIT || RATE_192K_24BIT || RATE_192K_16BIT)
#define EN_96K (RATE_96K_32BIT || RATE_96K_24BIT || RATE_96K_16BIT)
#define EN_48K (RATE_48K_32BIT || RATE_48K_24BIT || RATE_48K_16BIT)
#define EN_32BIT (RATE_384K_32BIT || RATE_192K_32BIT || RATE_96K_32BIT || RATE_48K_32BIT)
#define EN_24BIT (RATE_384K_24BIT || RATE_192K_24BIT || RATE_96K_24BIT || RATE_48K_24BIT)
#define EN_16BIT (RATE_384K_16BIT || RATE_192K_16BIT || RATE_96K_16BIT || RATE_48K_16BIT)
#define PLAY_ALT_COUNT (1 + EN_32BIT + EN_24BIT + EN_16BIT)
#define CAP_ALT_COUNT PLAY_ALT_COUNT
#endif
