


#ifndef TLV320ADC5120_H
#define TLV320ADC5120_H

#include <stdint.h>

#include "tlv320adc5120_port.h"

#ifndef adc5120_bus_write
#error "请在 tlv320adc5120_port.h 中定义 adc5120_bus_write（I2C 写函数名）"
#endif

#ifndef adc5120_bus_read
#error "请在 tlv320adc5120_port.h 中定义 adc5120_bus_read（I2C 读函数名）"
#endif

#ifndef adc5120_delay_ms
#error "请在 tlv320adc5120_port.h 中定义 adc5120_delay_ms（延时函数名，单位 ms）"
#endif

/* ============================================================================
 * 设备地址与内部时序
 * ==========================================================================*/

/** 7 位固定 I2C 从地址（1001110b，手册 8.5.1.1，不可更改） */
#define ADC5120_I2C_ADDR              0x4EU

/* 以下延时可在 tlv320adc5120_port.h 中覆盖定义 */

#ifndef ADC5120_DELAY_POWER_UP_MS
/** 上电后到首次 I2C 访问的等待：手册要求供电稳定后 >=2 ms（t1/t2），
 *  且器件上电即进入睡眠，唤醒前需 >=10 ms，取 10 ms 覆盖两种要求 */
#define ADC5120_DELAY_POWER_UP_MS     10U
#endif

#ifndef ADC5120_DELAY_RESET_MS
/** 复位后等待：复位会把 SLEEP_ENZ 恢复为 0（回到睡眠态），
 *  手册要求进入睡眠后 >=10 ms 才能发起唤醒的 I2C 访问 */
#define ADC5120_DELAY_RESET_MS        10U
#endif

#ifndef ADC5120_DELAY_WAKE_MS
/** 唤醒后等待：唤醒序列 >=1 ms；且手册 8.4.2 要求 CHx_CFG1~4
 *  等寄存器必须在退出睡眠后 >=10 ms 才能读写，取 10 ms */
#define ADC5120_DELAY_WAKE_MS         10U
#endif

/* ============================================================================
 * Page 0 寄存器地址（手册表 8-55）
 * 中断/AGC/DRC/BQ 等配置入口已裁剪；DRE 由 Init.DreEnable 控制；
 * PDM、GPIO 复用与 DSP_CFG0（抽取滤波器）随 PDM 功能一并开放。
 * ==========================================================================*/

#define ADC5120_REG_PAGE_CFG          0x00U  /**< 页选择寄存器（每页均有，写值即选页） */
#define ADC5120_REG_SW_RESET          0x01U  /**< 软件复位（bit0 自清零） */
#define ADC5120_REG_SLEEP_CFG         0x02U  /**< AREG / VREF 快充 / 睡眠唤醒 */
#define ADC5120_REG_ASI_CFG0          0x07U  /**< ASI 格式、字长、极性、发送边沿 */
#define ADC5120_REG_ASI_CFG1          0x08U  /**< ASI TX_OFFSET / bus keeper（保持默认） */
#define ADC5120_REG_ASI_CFG2          0x09U  /**< ASI 总线错误检测（保持默认） */
#define ADC5120_REG_ASI_CH1           0x0BU  /**< 通道 1 槽位分配 */
#define ADC5120_REG_ASI_CH2           0x0CU  /**< 通道 2 槽位分配 */
#define ADC5120_REG_ASI_CH3           0x0DU  /**< 通道 3 槽位分配（PDM 通道，复位 0x02） */
#define ADC5120_REG_ASI_CH4           0x0EU  /**< 通道 4 槽位分配（PDM 通道，复位 0x03） */
#define ADC5120_REG_MST_CFG0          0x13U  /**< 主/从、自动时钟、PLL、MCLK 频率 */
#define ADC5120_REG_MST_CFG1          0x14U  /**< 主机模式采样率、BCLK 比率 */
#define ADC5120_REG_ASI_STS           0x15U  /**< 只读：ASI 时钟自动检测结果 */
#define ADC5120_REG_CLK_SRC           0x16U  /**< MCLK 频率模式/比率、从模式根时钟 */
#define ADC5120_REG_PDMCLK_CFG        0x1FU  /**< PDM 时钟输出分频（PDMCLK_DIV） */
#define ADC5120_REG_PDMIN_CFG         0x20U  /**< PDMDIN1 采样沿（决定 CH1/CH2 用哪个沿） */
#define ADC5120_REG_GPIO_CFG0         0x21U  /**< GPIO1 功能码 + 驱动能力 */
#define ADC5120_REG_GPO_CFG0          0x22U  /**< GPO1(IN2M) 功能码 + 驱动能力 */
#define ADC5120_REG_GPI_CFG0          0x2BU  /**< GPI1(IN2P) / GPI2(MICBIAS) 功能码 */
#define ADC5120_REG_CM_TOL_CFG        0x3AU  /**< 两通道输入共模容差 */
#define ADC5120_REG_BIAS_CFG          0x3BU  /**< MICBIAS 值、ADC 满量程(VREF) */
#define ADC5120_REG_CH1_CFG0          0x3CU  /**< 通道1：输入类型/源/耦合/阻抗 */
#define ADC5120_REG_CH1_CFG1          0x3DU  /**< 通道1：模拟增益 */
#define ADC5120_REG_CH1_CFG2          0x3EU  /**< 通道1：数字音量 */
#define ADC5120_REG_CH1_CFG3          0x3FU  /**< 通道1：增益校准（保持默认 0dB） */
#define ADC5120_REG_CH1_CFG4          0x40U  /**< 通道1：相位校准（保持默认 0） */
#define ADC5120_REG_CH2_CFG0          0x41U  /**< 通道2：输入类型/源/耦合/阻抗 */
#define ADC5120_REG_CH2_CFG1          0x42U  /**< 通道2：模拟增益 */
#define ADC5120_REG_CH2_CFG2          0x43U  /**< 通道2：数字音量 */
#define ADC5120_REG_CH2_CFG3          0x44U  /**< 通道2：增益校准（保持默认 0dB） */
#define ADC5120_REG_CH2_CFG4          0x45U  /**< 通道2：相位校准（保持默认 0） */
#define ADC5120_REG_CH3_CFG2          0x48U  /**< 通道3：数字音量（PDM，无模拟增益） */
#define ADC5120_REG_CH3_CFG3          0x49U  /**< 通道3：增益校准（保持默认 0dB） */
#define ADC5120_REG_CH3_CFG4          0x4AU  /**< 通道3：相位校准（保持默认 0） */
#define ADC5120_REG_CH4_CFG2          0x4DU  /**< 通道4：数字音量（PDM，无模拟增益） */
#define ADC5120_REG_CH4_CFG3          0x4EU  /**< 通道4：增益校准（保持默认 0dB） */
#define ADC5120_REG_CH4_CFG4          0x4FU  /**< 通道4：相位校准（保持默认 0） */
#define ADC5120_REG_DSP_CFG0          0x6BU  /**< 抽取滤波器档位 / HPF 选择 */
#define ADC5120_REG_IN_CH_EN          0x73U  /**< 输入通道使能（须先于上电写入） */
#define ADC5120_REG_ASI_OUT_CH_EN     0x74U  /**< ASI 输出槽使能（须先于上电写入） */
#define ADC5120_REG_PWR_CFG           0x75U  /**< 上电控制（最后写入） */
#define ADC5120_REG_DEV_STS0          0x76U  /**< 只读：通道 1/2 上电状态 */
#define ADC5120_REG_DEV_STS1          0x77U  /**< 只读：器件模式状态 */

/* ============================================================================
 * 配置宏（HAL 风格：调用处直接看宏名即可知道配置含义）
 * ==========================================================================*/

/* ---- 复位模式（ADC5120_Reset / Init.ResetMode） --------------------------*/
#define ADC5120_RESET_NONE            0x00U  /**< 不复位（外部已复位或仅重配） */
#define ADC5120_RESET_SOFTWARE        0x01U  /**< 软件复位（写 SW_RESET 0x01 D0） */
#define ADC5120_RESET_HARDWARE        0x02U  /**< 硬件复位（需提供可选宏 adc5120_hw_reset） */

/* ---- 页号 ----------------------------------------------------------------*/
#define ADC5120_PAGE_0                0x00U  /**< 配置寄存器页 */

/* ---- SLEEP_CFG (0x02) -----------------------------------------------------*/
#define ADC5120_AREG_EXTERNAL         0x00U  /**< 外部 1.8V AREG（AVDD=1.8V 且 AREG 与 AVDD 短接） */
#define ADC5120_AREG_INTERNAL         0x01U  /**< 片内 LDO 产生 AREG（AVDD=3.3V 时使用） */
#define ADC5120_VREF_QCHG_3P5MS       0x00U  /**< VREF 外部电容快充 3.5 ms（默认，配 1uF） */
#define ADC5120_VREF_QCHG_10MS        0x01U  /**< VREF 快充 10 ms */
#define ADC5120_VREF_QCHG_50MS        0x02U  /**< VREF 快充 50 ms */
#define ADC5120_VREF_QCHG_100MS       0x03U  /**< VREF 快充 100 ms */

/* ---- ASI_CFG0 (0x07)：串行音频接口格式 -----------------------------------*/
#define ADC5120_ASI_FORMAT_TDM        0x00U  /**< TDM（DSP）模式 */
#define ADC5120_ASI_FORMAT_I2S        0x01U  /**< I2S 模式 */
#define ADC5120_ASI_FORMAT_LJ         0x02U  /**< 左对齐（LJ）模式 */

#define ADC5120_ASI_WLEN_16BIT        0x00U  /**< 16 bit（手册建议 10k/20k 输入阻抗时优先用此档） */
#define ADC5120_ASI_WLEN_20BIT        0x01U  /**< 20 bit */
#define ADC5120_ASI_WLEN_24BIT        0x02U  /**< 24 bit */
#define ADC5120_ASI_WLEN_32BIT        0x03U  /**< 32 bit（复位默认） */

#define ADC5120_POL_NORMAL            0x00U  /**< 极性按标准协议（复位默认） */
#define ADC5120_POL_INVERTED          0x01U  /**< 极性反转（FSYNC_POL / BCLK_POL 共用取值） */

#define ADC5120_TX_EDGE_DEFAULT       0x00U  /**< 数据发送边沿跟随 BCLK_POL 默认沿 */
#define ADC5120_TX_EDGE_INVERTED      0x01U  /**< 发送边沿反转（半个 BCLK 周期延迟） */

#define ADC5120_TX_FILL_ZERO          0x00U  /**< 未使用周期发 0（复位默认） */
#define ADC5120_TX_FILL_HIZ           0x01U  /**< 未用周期输出高阻 */

/* ---- 槽位分配 ASI_CH1/CH2 (0x0B/0x0C)，取值 0~63 -------------------------
 * TDM 模式：槽位在整帧内顺序编号 0~63，立体声用 SLOT_0 / SLOT_1。
 * I2S/LJ 模式：槽位分成左右两个集合——bit5=0 属左半帧（左槽 0~31），
 *   bit5=1 属右半帧（右槽 0~31）。标准立体声 I2S 每半帧只有 1 个字，
 *   因此右声道必须写 SLOT_RIGHT_0(=32)；若写成 SLOT_1 会落到左半帧的
 *   第 2 个字上，右声道将没有数据（手册表 8-4）。
 * ------------------------------------------------------------------------*/
#define ADC5120_ASI_SLOT_0            0x00U  /**< TDM slot0 / I2S·LJ 左 slot0 */
#define ADC5120_ASI_SLOT_1            0x01U  /**< TDM slot1 / I2S·LJ 左 slot1 */
#define ADC5120_ASI_SLOT_2            0x02U  /**< TDM slot2 / I2S·LJ 左 slot2 */
#define ADC5120_ASI_SLOT_3            0x03U  /**< TDM slot3 / I2S·LJ 左 slot3 */
#define ADC5120_ASI_SLOT_RIGHT_0      0x20U  /**< I2S·LJ 右 slot0（= TDM slot32），立体声 I2S/LJ 右声道用 */
#define ADC5120_ASI_SLOT_RIGHT_1      0x21U  /**< I2S·LJ 右 slot1（= TDM slot33） */

/* ---- MST_CFG0 (0x13)：主从与自动时钟 -------------------------------------*/
#define ADC5120_ASI_SLAVE             0x00U  /**< 从模式：BCLK/FSYNC 由主机提供（复位默认） */
#define ADC5120_ASI_MASTER            0x01U  /**< 主机模式：BCLK/FSYNC 由本芯片输出 */

#define ADC5120_FS_FAMILY_48K         0x00U  /**< fs 为 48 kHz 系列的整数倍/分频（复位默认） */
#define ADC5120_FS_FAMILY_44K1        0x01U  /**< fs 为 44.1 kHz 系列的整数倍/分频 */

/* ---- MST_CFG1 (0x14)：主机模式采样率 / BCLK 比率 -------------------------
 * FS_RATE 一档同时对应 44.1k 系列与 48k 系列（由 FsFamily 区分）。
 * ------------------------------------------------------------------------*/
#define ADC5120_FS_RATE_8K            0x00U  /**< 7.35 / 8 kHz */
#define ADC5120_FS_RATE_16K           0x01U  /**< 14.7 / 16 kHz */
#define ADC5120_FS_RATE_24K           0x02U  /**< 22.05 / 24 kHz */
#define ADC5120_FS_RATE_32K           0x03U  /**< 29.4 / 32 kHz */
#define ADC5120_FS_RATE_48K           0x04U  /**< 44.1 / 48 kHz */
#define ADC5120_FS_RATE_96K           0x05U  /**< 88.2 / 96 kHz */
#define ADC5120_FS_RATE_192K          0x06U  /**< 176.4 / 192 kHz */
#define ADC5120_FS_RATE_384K          0x07U  /**< 352.8 / 384 kHz */
#define ADC5120_FS_RATE_768K          0x08U  /**< 705.6 / 768 kHz */

#define ADC5120_BCLK_RATIO_16         0x00U  /**< BCLK/FSYNC = 16 */
#define ADC5120_BCLK_RATIO_24         0x01U  /**< BCLK/FSYNC = 24 */
#define ADC5120_BCLK_RATIO_32         0x02U  /**< BCLK/FSYNC = 32 */
#define ADC5120_BCLK_RATIO_48         0x03U  /**< BCLK/FSYNC = 48 */
#define ADC5120_BCLK_RATIO_64         0x04U  /**< BCLK/FSYNC = 64 */
#define ADC5120_BCLK_RATIO_96         0x05U  /**< BCLK/FSYNC = 96 */
#define ADC5120_BCLK_RATIO_128        0x06U  /**< BCLK/FSYNC = 128 */
#define ADC5120_BCLK_RATIO_192        0x07U  /**< BCLK/FSYNC = 192 */
#define ADC5120_BCLK_RATIO_256        0x08U  /**< BCLK/FSYNC = 256 */
#define ADC5120_BCLK_RATIO_384        0x09U  /**< BCLK/FSYNC = 384 */
#define ADC5120_BCLK_RATIO_512        0x0AU  /**< BCLK/FSYNC = 512 */
#define ADC5120_BCLK_RATIO_1024       0x0BU  /**< BCLK/FSYNC = 1024 */
#define ADC5120_BCLK_RATIO_2048       0x0CU  /**< BCLK/FSYNC = 2048 */

/* ---- MST_CFG0 (0x13)：主机模式 MCLK 输入频率 ------------------------------*/
#define ADC5120_MCLK_12MHZ            0x00U  /**< MCLK = 12 MHz */
#define ADC5120_MCLK_12P288MHZ        0x01U  /**< MCLK = 12.288 MHz */
#define ADC5120_MCLK_13MHZ            0x02U  /**< MCLK = 13 MHz（复位默认） */
#define ADC5120_MCLK_16MHZ            0x03U  /**< MCLK = 16 MHz */
#define ADC5120_MCLK_19P2MHZ          0x04U  /**< MCLK = 19.2 MHz */
#define ADC5120_MCLK_19P68MHZ         0x05U  /**< MCLK = 19.68 MHz */
#define ADC5120_MCLK_24MHZ            0x06U  /**< MCLK = 24 MHz */
#define ADC5120_MCLK_24P576MHZ        0x07U  /**< MCLK = 24.576 MHz */

/* ---- MST_CFG0 (0x13)：自动时钟下是否启用 PLL ------------------------------*/
#define ADC5120_PLL_AUTO_ON           0x00U  /**< 自动时钟配置中启用 PLL（复位默认，推荐） */
#define ADC5120_PLL_AUTO_OFF          0x01U  /**< 自动时钟配置中关闭 PLL（低功耗，性能略降） */

/* ---- CLK_SRC (0x16)：MCLK 频率指定方式 -----------------------------------*/
#define ADC5120_MCLK_FREQ_FIXED       0x00U  /**< MCLK 频率由 MclkFreq 固定档位给出（复位默认） */
#define ADC5120_MCLK_RATIO_TO_FSYNC   0x01U  /**< MCLK 频率 = FSYNC × MclkFsyncRatio（如 11.2896 MHz） */

#define ADC5120_MCLK_RATIO_64         0x00U  /**< MCLK/FSYNC = 64 */
#define ADC5120_MCLK_RATIO_256        0x01U  /**< MCLK/FSYNC = 256 */
#define ADC5120_MCLK_RATIO_384        0x02U  /**< MCLK/FSYNC = 384（复位默认） */
#define ADC5120_MCLK_RATIO_512        0x03U  /**< MCLK/FSYNC = 512 */
#define ADC5120_MCLK_RATIO_768        0x04U  /**< MCLK/FSYNC = 768 */
#define ADC5120_MCLK_RATIO_1024       0x05U  /**< MCLK/FSYNC = 1024 */
#define ADC5120_MCLK_RATIO_1536       0x06U  /**< MCLK/FSYNC = 1536 */
#define ADC5120_MCLK_RATIO_2304       0x07U  /**< MCLK/FSYNC = 2304 */

/* ---- CLK_SRC (0x16)：从模式且关闭 PLL时的音频根时钟 ----------------------*/
#define ADC5120_ROOT_CLK_BCLK         0x00U  /**< 以 BCLK 为音频根时钟（复位默认） */
#define ADC5120_ROOT_CLK_MCLK         0x01U  /**< 以 MCLK(GPIO/GPIx) 为音频根时钟 */

/* ---- BIAS_CFG (0x3B)：ADC 满量程（按最低 AVDD 电压选择） -----------------*/
#define ADC5120_VREF_2P75V            0x00U  /**< VREF=2.75V：差分 2Vrms / 单端 1Vrms（复位默认） */
#define ADC5120_VREF_2P5V             0x01U  /**< VREF=2.5V：差分 1.818Vrms / 单端 0.909Vrms */
#define ADC5120_VREF_1P375V           0x02U  /**< VREF=1.375V：差分 1Vrms / 单端 0.5Vrms */

/* ---- BIAS_CFG (0x3B)：MICBIAS 电压选择 -----------------------------------*/
#define ADC5120_MBIAS_VREF            0x00U  /**< MICBIAS = VREF（复位默认） */
#define ADC5120_MBIAS_VREF_1P096      0x01U  /**< MICBIAS = VREF × 1.096 */
#define ADC5120_MBIAS_VCM_IN1M        0x02U  /**< MICBIAS = VCM = IN1M（单端输入配置用） */
#define ADC5120_MBIAS_VCM_IN2M        0x03U  /**< MICBIAS = VCM = IN2M（单端输入配置用） */
#define ADC5120_MBIAS_VCM_AVG_INM     0x04U  /**< MICBIAS = (IN1M+IN2M)/2（单端输入配置用） */
#define ADC5120_MBIAS_VCM_INTERNAL    0x05U  /**< MICBIAS = 内部粗略共模 */
#define ADC5120_MBIAS_AVDD            0x06U  /**< MICBIAS = AVDD */
#define ADC5120_MBIAS_GPI2            0x07U  /**< MICBIAS 脚改作 GPI2（可复用为 PDMDIN2）；
                                              *   用 CH3/CH4 时必须选此档，且 MicbiasPower
                                              *   必须保持 ADC5120_MICBIAS_POWER_DOWN */

/* ---- PWR_CFG (0x75)：MICBIAS 电源 -----------------------------------------*/
#define ADC5120_MICBIAS_POWER_DOWN    0x00U  /**< 关断 MICBIAS（不接驻极体/MEMS 偏置时用这个） */
#define ADC5120_MICBIAS_POWER_UP      0x01U  /**< 上电 MICBIAS */

/* ---- 通道配置（ADC5120_ChannelInitTypeDef 各字段） -----------------------*/
#define ADC5120_CH_DISABLE            0x00U  /**< 关闭该输入通道 / ASI 输出槽 */
#define ADC5120_CH_ENABLE             0x01U  /**< 使能该输入通道 / ASI 输出槽 */

#define ADC5120_INPUT_MIC             0x00U  /**< 麦克风输入（复位默认） */
#define ADC5120_INPUT_LINE            0x01U  /**< 线路输入 */

#define ADC5120_INPUT_DIFFERENTIAL    0x00U  /**< 模拟差分输入 INxP/INxM（复位默认） */
#define ADC5120_INPUT_SINGLE_ENDED    0x01U  /**< 模拟单端输入（同时需正确配置 MICBIAS 值） */
#define ADC5120_INPUT_PDM             0x02U  /**< 数字麦克风 PDM 输入（CH1/CH2 专用）；
                                              *   写入后该通道的 INxP/INxM 模拟前端被旁路，
                                              *   CH2 选此值后 IN2P/IN2M 才释放为 GPI1/GPO1 */

#define ADC5120_COUPLING_AC           0x00U  /**< 输入交流耦合（复位默认，推荐） */
#define ADC5120_COUPLING_DC           0x01U  /**< 输入直流耦合 */

#define ADC5120_INPUT_Z_2K5           0x00U  /**< 输入阻抗约 2.5 kΩ（复位默认） */
#define ADC5120_INPUT_Z_10K           0x01U  /**< 输入阻抗约 10 kΩ */
#define ADC5120_INPUT_Z_20K           0x02U  /**< 输入阻抗约 20 kΩ */

#define ADC5120_CM_TOL_DEFAULT        0x00U  /**< 共模容差：AC 100mVpp / DC 2.82Vpp（复位默认） */
#define ADC5120_CM_TOL_1VPP           0x01U  /**< 共模容差 1Vpp（AC/DC 均可） */
#define ADC5120_CM_TOL_RAIL           0x02U  /**< 0~AVDD 全摆幅（仅 10k/20k 输入阻抗可用） */

/** 模拟增益编码：(dB) 0~42，步进 0.5 dB；负值需 10k/20k 输入阻抗（最低 -11 dB）。
 *  例：ADC5120_GAIN_DB(20) = 20 dB，ADC5120_GAIN_DB(-6) = -6 dB */
#define ADC5120_GAIN_DB(db) \
    ((uint8_t)(((db) < 0) ? (uint16_t)((((uint16_t)(-(double)(db) * 2.0)) << 1) | 0x01U) \
                          : (uint16_t)(((uint16_t)((double)(db) * 2.0)) << 1)))

/** 数字音量编码：-100 dB ~ +27 dB，步进 0.5 dB。
 *  例：ADC5120_VOLUME_DB(0) = 0 dB（复位默认），ADC5120_VOLUME_DB(-12.5) */
#define ADC5120_VOLUME_DB(db)         ((uint8_t)(201 + (int)((double)(db) * 2.0)))
#define ADC5120_VOLUME_MUTE           0x00U  /**< 静音（CH1/CH2_CFG2 写 0x00） */


/* ---- PDM 总开关（ADC5120_PdmInitTypeDef.Enable） -------------------------*/
#define ADC5120_PDM_DISABLE           0x00U  /**< 不使用 PDM：Init 内完全不碰 PDM/GPIO 寄存器 */
#define ADC5120_PDM_ENABLE            0x01U  /**< 启用 PDM：写 PDMCLK_CFG/PDMIN_CFG/GPIO 复用 */

/* ---- PDMCLK_CFG (0x1F)：PDMCLK_DIV[1:0] ----------------------------------
 * 注释为「44.1 kHz 系列 / 48 kHz 系列」对应的实际时钟频率；
 * PDM 麦克风通常取 64 × fs，48 kHz 用 ADC5120_PDMCLK_2P8224M（= 3.072 MHz）。
 * ------------------------------------------------------------------------*/
#define ADC5120_PDMCLK_2P8224M        0x00U  /**< 2.8224 / 3.072 MHz（复位默认，48k 推荐） */
#define ADC5120_PDMCLK_1P4112M        0x01U  /**< 1.4112 / 1.536 MHz */
#define ADC5120_PDMCLK_0P7056M        0x02U  /**< 705.6 / 768 kHz */
#define ADC5120_PDMCLK_5P6448M        0x03U  /**< 5.6448 / 6.144 MHz（手册限制：仅 CH1/CH2） */

/* ---- PDMIN_CFG (0x20)：PDMDIN1_EDGE -------------------------------------*/
#define ADC5120_PDM_CH1_NEG_EDGE      0x00U  /**< CH1 采负沿、CH2 采正沿（复位默认） */
#define ADC5120_PDM_CH1_POS_EDGE      0x01U  /**< CH1 采正沿、CH2 采负沿（麦克风接错沿时改这个） */

/* ---- GPIO1_CFG / GPO1_CFG 功能码（4 bit，占寄存器 bit7:4） ---------------
 * 注意：GPIO1 与 GPO1 的功能码不是从 0 连续编号，见手册表 8-74 / 8-75。
 * ------------------------------------------------------------------------*/
#define ADC5120_GPO_FUNC_DISABLE      0x0U   /**< 引脚禁用 */
#define ADC5120_GPO_FUNC_GPO          0x1U   /**< 通用输出 */
#define ADC5120_GPO_FUNC_IRQ          0x2U   /**< 中断输出（GPIO1 复位默认） */
#define ADC5120_GPO_FUNC_PDMCLK       0x4U   /**< PDM 时钟输出 */
#define ADC5120_GPO_FUNC_PD_ALL       0x7U   /**< 关断全部 ADC/PDM 通道（仅 GPIO1） */
#define ADC5120_GPO_FUNC_MICBIAS_EN   0x8U   /**< MICBIAS 开关输入（仅 GPIO1） */
#define ADC5120_GPO_FUNC_GPI          0x9U   /**< 通用输入（仅 GPIO1） */
#define ADC5120_GPO_FUNC_MCLK         0xAU   /**< MCLK 输入（仅 GPIO1） */
#define ADC5120_GPO_FUNC_SDIN         0xBU   /**< ASI 级联输入（仅 GPIO1） */
#define ADC5120_GPO_FUNC_PDMDIN1      0xCU   /**< PDMDIN1：PDM 数据输入 CH1/CH2（仅 GPIO1） */
#define ADC5120_GPO_FUNC_PDMDIN2      0xDU   /**< PDMDIN2：PDM 数据输入 CH3/CH4（仅 GPIO1） */

/* ---- GPI1 / GPI2 功能码（3 bit：GPI1 占 bit6:4，GPI2 占 bit2:0） --------*/
#define ADC5120_GPI_FUNC_DISABLE      0x0U   /**< 引脚禁用（复位默认） */
#define ADC5120_GPI_FUNC_GPI          0x1U   /**< 通用输入 */
#define ADC5120_GPI_FUNC_MCLK         0x2U   /**< MCLK 输入 */
#define ADC5120_GPI_FUNC_SDIN         0x3U   /**< ASI 级联输入 */
#define ADC5120_GPI_FUNC_PDMDIN1      0x4U   /**< PDMDIN1：PDM 数据输入 CH1/CH2 */
#define ADC5120_GPI_FUNC_PDMDIN2      0x5U   /**< PDMDIN2：PDM 数据输入 CH3/CH4 */
#define ADC5120_GPI_FUNC_PD_ALL       0x7U   /**< 关断全部 ADC/PDM 通道 */

/* ---- 引脚驱动能力（GPOx_DRV / GPIO1_DRV） --------------------------------
 * PDMCLK 是高速方波，必须用推挽档 ADC5120_PIN_DRV_PUSH_PULL。
 * ------------------------------------------------------------------------*/
#define ADC5120_PIN_DRV_HIZ           0x0U   /**< 高阻 */
#define ADC5120_PIN_DRV_PUSH_PULL     0x1U   /**< 主动拉低 + 主动拉高（时钟输出用这个） */
#define ADC5120_PIN_DRV_OD_WEAK_HIGH  0x2U   /**< 主动拉低 + 弱上拉（GPIO1 复位默认） */
#define ADC5120_PIN_DRV_OPEN_DRAIN    0x3U   /**< 主动拉低 + 高阻 */

/* ---- PDM 时钟输出引脚（ADC5120_PdmInitTypeDef.ClockPin） ----------------*/
#define ADC5120_PDMCLK_PIN_GPO1       0x00U  /**< IN2M/GPO1（默认，TI EVM 方案） */
#define ADC5120_PDMCLK_PIN_GPIO1      0x01U  /**< 专用 GPIO1（此时 PDMDIN 只能用 GPI1/GPI2） */

/* ---- PDM 数据输入引脚（ADC5120_PdmInitTypeDef.DataPin） -----------------*/
#define ADC5120_PDMDIN_PIN_GPI1_GPI2  0x00U  /**< IN2P/GPI1=PDMDIN1，MICBIAS/GPI2=PDMDIN2（默认） */
#define ADC5120_PDMDIN_PIN_GPIO1      0x01U  /**< GPIO1=PDMDIN1（此时 PDMCLK 只能用 GPO1） */

/* ---- DSP_CFG0 (0x6B)：DECI_FILT[1:0] 抽取滤波器档位 ----------------------
 * 模拟输入和 PDM 输入共用这条抽取滤波器（手册 8.3.6.7）。
 * ------------------------------------------------------------------------*/
#define ADC5120_DECI_FILT_KEEP        0xFFU  /**< 不修改，保持芯片当前值（复位为线性相位） */
#define ADC5120_DECI_FILT_LINEAR      0x00U  /**< 线性相位（复位默认，通带相位最平） */
#define ADC5120_DECI_FILT_LOW_LAT     0x01U  /**< 低延迟（群延迟更小，语音/ANC 常用） */
#define ADC5120_DECI_FILT_ULTRA_LOW   0x02U  /**< 超低延迟 */

/* ============================================================================
 * 状态读回（DEV_STS0/DEV_STS1/ASI_STS）
 * ==========================================================================*/

#define ADC5120_FLAG_CH1_POWERED      0x00000001U  /**< 通道 1 已上电（DEV_STS0 0x76 D7） */
#define ADC5120_FLAG_CH2_POWERED      0x00000002U  /**< 通道 2 已上电（DEV_STS0 0x76 D6） */
#define ADC5120_FLAG_MODE_SLEEP       0x00000004U  /**< 睡眠/软件关断（DEV_STS1 0x77 MODE_STS=4） */
#define ADC5120_FLAG_MODE_ACTIVE      0x00000008U  /**< 活动模式（DEV_STS1 0x77 MODE_STS=6 或 7） */
#define ADC5120_FLAG_ASI_CLK_VALID    0x00000010U  /**< ASI 时钟自动检测有效（ASI_STS 0x15） */

/** 两通道都已上电且器件处于活动模式（不含 ASI 时钟，时钟由主机提供，可能尚未启动） */
#define ADC5120_FLAG_READY \
    (ADC5120_FLAG_CH1_POWERED | ADC5120_FLAG_CH2_POWERED | ADC5120_FLAG_MODE_ACTIVE)

/** ASI_STS 中采样率检测无效值（未检测到合法 FSYNC） */
#define ADC5120_FS_RATE_STS_INVALID   0x0FU
/** ASI_STS 中 BCLK 比率检测无效值 */
#define ADC5120_BCLK_RATIO_STS_INVALID 0x0FU

/* ============================================================================
 * 类型定义
 * ==========================================================================*/

/** 库函数返回值（0 = 成功，与 HAL_StatusTypeDef 风格一致） */
typedef enum {
    ADC5120_OK                 = 0,  /**< 成功 */
    ADC5120_ERROR              = 1,  /**< 总线或参数错误 */
    ADC5120_ERROR_NOT_PRESENT  = 2,  /**< 在线探测失败（无应答） */
    ADC5120_ERROR_NOT_READY    = 3   /**< 已配置但状态标志未达预期 */
} ADC5120_StatusTypeDef;

/** 单通道初始化配置（Ch1 / Ch2 / Ch3 / Ch4 各一份）
 *  说明：Ch1 / Ch2 可配模拟输入或 PDM 输入（InputSrc 选 ADC5120_INPUT_PDM）；
 *        Ch3 / Ch4 是 PDM 专用通道，只有 Enable / Slot / Volume 三个字段有效，
 *        Gain / InputType / Coupling / InputZ / CmTolerance 会被忽略。 */
typedef struct {
    uint8_t Enable;       /**< ADC5120_CH_ENABLE / ADC5120_CH_DISABLE */
    uint8_t Slot;         /**< ADC5120_ASI_SLOT_0 / ADC5120_ASI_SLOT_RIGHT_0 等，0~63 */
    uint8_t InputType;    /**< ADC5120_INPUT_MIC / ADC5120_INPUT_LINE */
    uint8_t InputSrc;     /**< ADC5120_INPUT_DIFFERENTIAL / _SINGLE_ENDED / _PDM */
    uint8_t Coupling;     /**< ADC5120_COUPLING_AC / ADC5120_COUPLING_DC */
    uint8_t InputZ;       /**< ADC5120_INPUT_Z_2K5 / _10K / _20K */
    uint8_t CmTolerance;  /**< ADC5120_CM_TOL_DEFAULT / _1VPP / _RAIL */
    uint8_t Gain;         /**< ADC5120_GAIN_DB(x)，x 单位 dB，0.5 dB 步进 */
    uint8_t Volume;       /**< ADC5120_VOLUME_DB(x) 或 ADC5120_VOLUME_MUTE */
} ADC5120_ChannelInitTypeDef;


typedef struct {
    uint8_t Enable;            /**< ADC5120_PDM_ENABLE 才写 PDM 寄存器；_DISABLE 时本结构全部忽略 */
    uint8_t ClockDiv;          /**< ADC5120_PDMCLK_2P8224M / _1P4112M / _0P7056M / _5P6448M */
    uint8_t Din1Edge;          /**< ADC5120_PDM_CH1_NEG_EDGE / _CH1_POS_EDGE */
    uint8_t ClockPin;          /**< ADC5120_PDMCLK_PIN_GPO1 / _GPIO1 */
    uint8_t DataPin;           /**< ADC5120_PDMDIN_PIN_GPI1_GPI2 / _GPIO1 */
    uint8_t DecimationFilter;  /**< ADC5120_DECI_FILT_KEEP / _LINEAR / _LOW_LAT / _ULTRA_LOW */
} ADC5120_PdmInitTypeDef;

/** 器件初始化配置：所有取值均为 ADC5120_xxx 宏，调用处即可读懂配置含义。
 *  注意：结构体需完整初始化（建议用 C99 指定初始化器逐字段赋值），
 *  零值并不等于复位默认值（例如 Wlen 的零值是 16 bit 而非 32 bit）。 */
typedef struct {
    /* 复位 */
    uint8_t ResetMode;         /**< ADC5120_RESET_SOFTWARE / _HARDWARE / _NONE */

    /* 电源域 SLEEP_CFG (0x02) */
    uint8_t AregSupply;        /**< ADC5120_AREG_INTERNAL / ADC5120_AREG_EXTERNAL */
    uint8_t VrefQuickCharge;   /**< ADC5120_VREF_QCHG_3P5MS / _10MS / _50MS / _100MS */

    /* ASI 串行音频接口 */
    uint8_t AsiFormat;         /**< ADC5120_ASI_FORMAT_I2S / _TDM / _LJ */
    uint8_t AsiWordLength;     /**< ADC5120_ASI_WLEN_32BIT / _24BIT / _20BIT / _16BIT */
    uint8_t FsyncPolarity;     /**< ADC5120_POL_NORMAL / ADC5120_POL_INVERTED */
    uint8_t BclkPolarity;      /**< ADC5120_POL_NORMAL / ADC5120_POL_INVERTED */
    uint8_t TxEdge;            /**< ADC5120_TX_EDGE_DEFAULT / ADC5120_TX_EDGE_INVERTED */
    uint8_t TxFill;            /**< ADC5120_TX_FILL_ZERO / ADC5120_TX_FILL_HIZ */

    /* 时钟与主从（从模式 + 自动时钟下由片内自动检测 FSYNC/BCLK，
     * Mclk* 与 FsRate/BclkRatio 仅在主机模式下生效） */
    uint8_t MstSlvCfg;         /**< ADC5120_ASI_SLAVE / ADC5120_ASI_MASTER */
    uint8_t FsFamily;          /**< ADC5120_FS_FAMILY_48K / ADC5120_FS_FAMILY_44K1 */
    uint8_t FsRate;            /**< ADC5120_FS_RATE_48K 等（主机模式） */
    uint8_t BclkRatio;         /**< ADC5120_BCLK_RATIO_64 等（主机模式） */
    uint8_t MclkFreq;          /**< ADC5120_MCLK_12P288MHZ 等（主机模式） */
    uint8_t MclkFreqMode;      /**< ADC5120_MCLK_FREQ_FIXED / ADC5120_MCLK_RATIO_TO_FSYNC */
    uint8_t MclkFsyncRatio;    /**< ADC5120_MCLK_RATIO_384 等（MclkFreqMode=倍频方式时有效） */
    uint8_t PllEnable;         /**< ADC5120_PLL_AUTO_ON / ADC5120_PLL_AUTO_OFF */
    uint8_t SlaveRootClock;    /**< ADC5120_ROOT_CLK_BCLK / ADC5120_ROOT_CLK_MCLK（关 PLL 时用） */

    /* 模拟前端公共项 */
    uint8_t AdcFullScale;      /**< ADC5120_VREF_2P75V / _2P5V / _1P375V */
    uint8_t MicbiasVal;        /**< ADC5120_MBIAS_VREF 等（单端输入时按手册选 VCM 档） */
    uint8_t MicbiasPower;      /**< ADC5120_MICBIAS_POWER_DOWN / _POWER_UP */
    uint8_t DreEnable;         /**< 1: 模拟 CH1/2 开 DRE；AGC/DRC 保持关闭 */

    /* 四个通道（Ch3/Ch4 为 PDM 专用，仅 Enable / Slot / Volume 有效） */
    ADC5120_ChannelInitTypeDef Ch1;
    ADC5120_ChannelInitTypeDef Ch2;
    ADC5120_ChannelInitTypeDef Ch3;
    ADC5120_ChannelInitTypeDef Ch4;

    /* PDM 数字麦克风（Enable 为总开关，_DISABLE 时不访问任何 PDM/GPIO 寄存器） */
    ADC5120_PdmInitTypeDef Pdm;
} ADC5120_InitTypeDef;

/** 器件句柄（HAL 句柄风格；后续可扩展运行期状态） */
typedef struct {
    ADC5120_InitTypeDef Init;  /**< 调用 ADC5120_Init 前由用户填充 */
} ADC5120_HandleTypeDef;

/** 器件状态快照（ADC5120_State_Get 输出） */
typedef struct {
    uint32_t Flags;        /**< ADC5120_FLAG_xxx 组合 */
    uint8_t  FsRateSts;    /**< 检测到的采样率档位，见 ADC5120_FS_RATE_xxx；
                                无效时为 ADC5120_FS_RATE_STS_INVALID */
    uint8_t  BclkRatioSts; /**< 检测到的 BCLK/FSYNC 比率，见 ADC5120_BCLK_RATIO_xxx；
                                无效时为 ADC5120_BCLK_RATIO_STS_INVALID */
} ADC5120_StateTypeDef;

/* ============================================================================
 * API
 * ==========================================================================*/

/**
 * @brief  完整初始化：在线探测 -> 复位 -> 唤醒 -> 全部配置 -> 上电 -> 状态校验
 * @note   调用前必须填充 hadc->Init；时序延时合计约 30 ms
 */
ADC5120_StatusTypeDef ADC5120_Init(ADC5120_HandleTypeDef *hadc);

/**
 * @brief  在线探测：读 PAGE_CFG 验证 I2C 应答（该芯片无 ID 寄存器）
 * @retval ADC5120_OK / ADC5120_ERROR_NOT_PRESENT / ADC5120_ERROR
 */
ADC5120_StatusTypeDef ADC5120_Probe(ADC5120_HandleTypeDef *hadc);

/**
 * @brief  复位
 * @param  ResetMode ADC5120_RESET_SOFTWARE（写 SW_RESET 寄存器 0x01）
 *                   或 ADC5120_RESET_HARDWARE（调用可选宏 adc5120_hw_reset()，
 *                   未在 port 头文件中定义该宏时返回 ADC5120_ERROR）
 *                   或 ADC5120_RESET_NONE（不复位）
 */
ADC5120_StatusTypeDef ADC5120_Reset(ADC5120_HandleTypeDef *hadc, uint8_t ResetMode);

/**
 * @brief  配置 ASI 串行音频接口与时钟/主从（SLEEP_CFG、ASI_CFG0、槽位、MST/CLK）
 * @note   仅改总线格式时可单独调用；完整流程请用 ADC5120_Init
 */
ADC5120_StatusTypeDef ADC5120_SAI_Config(ADC5120_HandleTypeDef *hadc);


ADC5120_StatusTypeDef ADC5120_PDM_Config(ADC5120_HandleTypeDef *hadc);

/**
 * @brief  配置模拟前端：共模容差、满量程、MICBIAS、四通道输入与增益音量
 * @note   CHx_CFG1~4 要求在退出睡眠后 >=10 ms 才能写（Init 内部已保证）；
 *         Ch3/Ch4 为 PDM 专用，只写数字音量与校准寄存器
 */
ADC5120_StatusTypeDef ADC5120_Input_Config(ADC5120_HandleTypeDef *hadc);

/**
 * @brief  通道使能、ASI 输出槽使能并上电（PWR_CFG 最后写入）
 */
ADC5120_StatusTypeDef ADC5120_Power_Config(ADC5120_HandleTypeDef *hadc);

/**
 * @brief  读取器件状态：通道上电标志、工作模式、ASI 时钟检测结果
 * @param  state 输出状态快照
 * @retval ADC5120_OK / ADC5120_ERROR（总线错误）
 */
/* 最近一次 State_Get 读到的 DEV_STS0(0x76) / DEV_STS1(0x77) 原始值 */
extern volatile uint8_t adc5120_dbg_sts0;
extern volatile uint8_t adc5120_dbg_sts1;

ADC5120_StatusTypeDef ADC5120_State_Get(ADC5120_HandleTypeDef *hadc,
                                        ADC5120_StateTypeDef *state);

/**
 * @brief  写 Page 0 单字节寄存器（库内部会先选页）
 */
ADC5120_StatusTypeDef ADC5120_Reg_Write(ADC5120_HandleTypeDef *hadc,
                                        uint8_t RegAddr, uint8_t Value);

/**
 * @brief  读 Page 0 单字节寄存器（库内部会先选页）
 */
ADC5120_StatusTypeDef ADC5120_Reg_Read(ADC5120_HandleTypeDef *hadc,
                                       uint8_t RegAddr, uint8_t *Value);

#endif /* TLV320ADC5120_H */
