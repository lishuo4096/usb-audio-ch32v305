#include "fu_store.h"
#include "ch32v30x_flash.h"
#include <stddef.h>
#include <string.h>

/*=====================================================================
 *  FU 记录的追加写日志（4KB 扇区 / 16 字节 x 256 槽）
 *  设计取舍（为什么是追加写、为什么 magic 最后写）全在 fu_store.h 顶部。
 *=====================================================================*/

#define FU_CRC_SEED 0xFFFFu
#define FU_CRC_POLY 0x1021u

/* 这 4KB 就是 Link.ld 里 .fustore 段的内容：全是 0xFF（已擦除态）。
 * 用 GNU 的区间指定初始化器让编译器直接吐 4096 个 0xFF 进 .rodata 的
 * .fustore 段 —— 等价于"向编译器申请一段 0xFF 的 code 变量"。
 * used + KEEP 保证不会被 --gc-sections 扔掉。 */
__attribute__ ((section (".fustore"), used, aligned (256)))
const uint8_t g_fu_store_reserve[4096] = {
    [0 ... 4095] = 0xFFu
};

static uint16_t fu_crc16 (const uint8_t *p, uint32_t n) {
    uint16_t crc = FU_CRC_SEED;
    uint32_t i;
    uint8_t b;

    for (i = 0u; i < n; i++) {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (b = 0u; b < 8u; b++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ FU_CRC_POLY);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static uint32_t fu_slot_addr (uint32_t slot) {
    return FU_FLASH_BASE + slot * FU_SLOT_SIZE;
}

/* 擦除后的单元：手册说半字读 0xE339；也兼容 0xFFFF */
static uint8_t fu_hw_blank (uint16_t hw) {
    return (uint8_t)((hw == 0xFFFFu) || (hw == 0xE339u));
}

uint8_t FuStore_SlotBlank (uint32_t slot) {
    if (slot >= FU_SLOTS) {
        return 0u;
    }
    /* An interrupted write can leave payload programmed and magic erased. */
    for(uint32_t i=0u;i<FU_SLOT_SIZE;i+=2u)
        if(!fu_hw_blank(*(volatile uint16_t *)(uintptr_t)(fu_slot_addr(slot)+i))) return 0u;
    return 1u;
}

static uint8_t rec_is_valid (const FuRecord *r) {
    uint16_t crc;

    if (r->magic != FU_REC_MAGIC && r->magic != FU_REC_MAGIC_LEGACY) {
        return 0u;
    }
    crc = fu_crc16 ((const uint8_t *)r, (uint32_t)offsetof (FuRecord, crc));
    return (uint8_t)((crc == r->crc) ? 1u : 0u);
}

/* 模 16 位比较：a 比 b 新 */
static uint8_t fu_seq_newer (uint16_t a, uint16_t b) {
    return (uint8_t)(((int16_t)(uint16_t)(a - b)) > 0);
}

static uint16_t s_slot; /* 下一个要写的槽（>= FU_SLOTS 表示满；FU_SLOTS=256 放不进 uint8）*/
static uint16_t s_seq;
static uint32_t s_writes;

/*--------------------------------------------------------------------
 *  读：扫全 256 槽，取 seq 最新且 magic+CRC 都对的
 *--------------------------------------------------------------------*/
uint8_t FuStore_Load (FuRecord *out) {
    uint32_t i;
    uint8_t found = 0u;
    uint32_t best = 0u;
    FuRecord r;

    s_seq = 0u;
    s_slot = FU_SLOTS;

    for (i = 0u; i < FU_SLOTS; i++) {
        memcpy (&r, (const void *)(uintptr_t)fu_slot_addr (i), sizeof (r));
        if (rec_is_valid (&r) == 0u) {
            continue;
        }
        if ((found == 0u) || (fu_seq_newer (r.seq, s_seq) != 0u)) {
            found = 1u;
            best = i;
            s_seq = r.seq;
        }
    }

    /* 游标 = 第一个空槽：被截断/写坏的槽会被跳过，而不是反复覆盖它 */
    s_slot = FU_SLOTS;
    for (i = 0u; i < FU_SLOTS; i++) {
        if (FuStore_SlotBlank (i) != 0u) {
            s_slot = (uint16_t)i;
            break;
        }
    }

    if (found == 0u) {
        return 0u;
    }
    if (out != NULL) {
        memcpy (out, (const void *)(uintptr_t)fu_slot_addr (best), sizeof (*out));
    }
    return 1u;
}

/*--------------------------------------------------------------------
 *  整片擦除（4KB 扇区，16~20ms）：只在电源正常时走
 *--------------------------------------------------------------------*/
static uint8_t fu_erase_all (void) {
    FLASH_Status st;

    FLASH_Unlock ();
    FLASH_ClearFlag (FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR | FLASH_FLAG_BANK1_BSY);
    st = FLASH_ErasePage (FU_FLASH_BASE);
    FLASH_Lock ();

    if (st != FLASH_COMPLETE) {
        return 0u;
    }
    return FuStore_SlotBlank (0u); /* 回读确认第一槽确实回到空态 */
}

/*--------------------------------------------------------------------
 *  写一个槽：8 个半字，magic 最后写（掉电截断时不留半条记录）
 *--------------------------------------------------------------------*/
/* 记录必须正好 16 字节：编译期断言（不成立时数组长度为负，编译直接报错）*/
typedef char fu_rec_size_check[(sizeof (FuRecord) == FU_SLOT_SIZE) ? 1 : -1];

static uint8_t fu_program_slot (uint32_t addr, const FuRecord *r) {
    /* 打包结构体按 1 字节对齐，直接取 uint16_t* 会有非对齐风险
     * （-Waddress-of-packed-member）。先整段复制到本地半字数组，
     * 再逐半字编程 —— 顺便让编程顺序可控（magic 最后写）。 */
    uint16_t hw[FU_SLOT_SIZE / 2u];
    uint8_t i;
    FLASH_Status st = FLASH_COMPLETE;

    memcpy (hw, r, sizeof (hw));

    FLASH_Unlock ();
    FLASH_ClearFlag (FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR | FLASH_FLAG_BANK1_BSY);
    /* 先写 seq/载荷/CRC（半字 2..7），再写 magic（半字 0..1）*/
    for (i = 2u; (i < (FU_SLOT_SIZE / 2u)) && (st == FLASH_COMPLETE); i++) {
        st = FLASH_ProgramHalfWord (addr + (uint32_t)i * 2u, hw[i]);
    }
    for (i = 0u; (i < 2u) && (st == FLASH_COMPLETE); i++) {
        st = FLASH_ProgramHalfWord (addr + (uint32_t)i * 2u, hw[i]);
    }
    FLASH_Lock ();

    return (uint8_t)((st == FLASH_COMPLETE) ? 1u : 0u);
}

uint8_t FuStore_Save (const FuRecord *rec, uint8_t allow_erase) {
    FuRecord nrec;
    FuRecord chk;
    uint32_t addr;
    uint32_t slot;

    if (rec == NULL) {
        return 0u;
    }

    /* 当前槽不可写（用满 / 该槽不空）：需要整片擦除，只有 allow_erase 才做 */
    slot = s_slot;
    if ((slot >= FU_SLOTS) || (FuStore_SlotBlank (slot) == 0u)) {
        if (allow_erase == 0u) {
            return 0u; /* 掉电中断里不做 16~20ms 的擦除 */
        }
        if (fu_erase_all () == 0u) {
            return 0u;
        }
        slot = 0u;
        s_slot = 0u;
    }

    nrec.magic = FU_REC_MAGIC;
    nrec.seq = (uint16_t)(s_seq + 1u);
    nrec.dac_l_m256 = rec->dac_l_m256;
    nrec.dac_r_m256 = rec->dac_r_m256;
    nrec.adc_m256 = rec->adc_m256;
    nrec.flags = rec->flags;
    nrec.adc_mute = rec->adc_mute;
    nrec.crc = fu_crc16 ((const uint8_t *)&nrec, (uint32_t)offsetof (FuRecord, crc));

    addr = fu_slot_addr (slot);
    if (fu_program_slot (addr, &nrec) == 0u) {
        return 0u;
    }

    /* 回读校验：magic+CRC 都对才算成功 */
    memcpy (&chk, (const void *)(uintptr_t)addr, sizeof (chk));
    if ((rec_is_valid (&chk) == 0u) || (chk.seq != nrec.seq)) {
        return 0u;
    }

    s_seq = nrec.seq;
    s_slot = (uint16_t)(slot + 1u);
    s_writes++;
    return 1u;
}

uint32_t FuStore_WriteCount (void) {
    return s_writes;
}

uint32_t FuStore_Seq (void) {
    return (uint32_t)s_seq;
}

uint32_t FuStore_Slot (void) {
    return (uint32_t)s_slot;
}
