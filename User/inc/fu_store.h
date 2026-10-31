#ifndef __FU_STORE_H__
#define __FU_STORE_H__

#include "debug.h"

/* Link.ld 提供的边界（低地址 = 0x0000_0000 空间的代码区别名）*/
extern uint8_t _fu_store_start[];
extern uint8_t _fu_store_end[];

#define FU_SLOT_SIZE 16u                    /* 一条记录 16 字节 = 8 个半字 */
#define FU_SLOTS     (4096u / FU_SLOT_SIZE) /* 4KB 扇区里 256 个槽 */

/* FPEC 认 0x0800_0000 空间，所以统一加基址；读写同源 */
#define FU_FLASH_BASE (((uint32_t)(uintptr_t)_fu_store_start) + 0x08000000u)

#define FU_REC_MAGIC 0x46555332u /* FUS2: mute + persistent channel flags */
#define FU_REC_MAGIC_LEGACY 0x46555331u

/* 一条记录：正好 16 字节 = 8 个半字，可以按半字逐个编程。
 * seq 用 16 位：65536 次保存才回绕，按"每天改一次音量"算是 180 年；
 * 比较新旧用模差，回绕也不会判错。 */
typedef struct __attribute__ ((packed)) {
    uint32_t magic;      /* FU_REC_MAGIC（最后写，作"完成标志"）*/
    uint16_t seq;        /* 单调递增，模 65536 */
    int16_t  dac_l_m256; /* 播放左声道，单位 1/256 dB */
    int16_t  dac_r_m256; /* 播放右声道 */
    int16_t  adc_m256;   /* 录音（两声道同值）*/
    uint8_t  flags;   /* FU 静音（不含零点检测/掉线的自动静音）*/
    uint8_t  adc_mute;
    uint16_t crc;        /* CRC16-CCITT，覆盖本字段之前的 14 字节 */
} FuRecord;              /* 16 字节 */
typedef char FuRecord_SizeCheck[(sizeof(FuRecord)==16u) ? 1 : -1];

/* 扫描全部槽，取 seq 最新且 magic+CRC 正确的记录。
 * 返回 1 = 找到（*out 有效），0 = 没有有效记录。 */
uint8_t FuStore_Load (FuRecord *out);

uint8_t FuStore_Save (const FuRecord *rec, uint8_t allow_erase);

/* 诊断 */
uint32_t FuStore_WriteCount (void);
uint32_t FuStore_Seq (void);
uint32_t FuStore_Slot (void);        /* 下一个要写的槽号（>=FU_SLOTS 表示已满）*/
uint8_t  FuStore_SlotBlank (uint32_t slot);

#endif /* __FU_STORE_H__ */
