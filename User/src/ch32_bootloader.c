/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32_bootloader.c
 * Description        : 主动跳进 WCH 官方 ROM ISP bootloader（放在用户区 flash 末尾）
 *
 * 本文件故意**只用裸寄存器访问**，不 include ch32v30x.h / debug.h，
 * 这样移植到任何 CH32V30x 工程都能直接编译，不会被标准库版本差异绊住。
 *******************************************************************************/
#include "ch32_bootloader.h"

/* ---------------------------------------------------------------------------
 * 用到的寄存器地址（CH32V30x 全系列一致）
 * ------------------------------------------------------------------------ */
#define BL_RD32(a)        (*(volatile uint32_t *)(a))
#define BL_WR32(a, v)     (*(volatile uint32_t *)(a) = (uint32_t)(v))

/* PFIC（中断控制器）：0xE000E000 + 0x180 = IRER[0]，每 4 字节关 32 个中断源 */
#define BL_PFIC_IRER(n)   BL_WR32(0xE000E180UL + 4UL * (n), 0xFFFFFFFFUL)
/* PFIC->CFGR，写 KEY3|(1<<7) 即触发系统复位（与 NVIC_SystemReset 同一实现） */
#define BL_PFIC_CFGR      0xE000E044UL
#define BL_NVIC_KEY3      0xBEEF0000UL

/* SysTick */
#define BL_SYSTICK_CTLR   0xE000F000UL
#define BL_SYSTICK_SR     0xE000F004UL
#define BL_SYSTICK_CNT    0xE000F008UL
#define BL_SYSTICK_CMP    0xE000F010UL

/* RCC */
#define BL_RCC_BASE       0x40021000UL
#define BL_RCC_HBPCENR    (BL_RCC_BASE + 0x14UL) /* AHB/HB 外设时钟，bit11 = USBHS */
#define BL_RCC_APB2PCENR  (BL_RCC_BASE + 0x18UL)
#define BL_RCC_APB1PCENR  (BL_RCC_BASE + 0x1CUL)

/* IWDG：写 0x5555 解锁，写 0x0000 尝试停 */
#define BL_IWDG_CTLR      0x40003000UL

#define BL_ARM            ((volatile uint32_t *)CH32_BL_MAGIC_ADDR)
#define BL_HEAD           ((volatile uint32_t *)CH32_BL_BASE)

/* ---------------------------------------------------------------------------
 * 内部：判断一个字像不像"镜像首字"
 *   正常 = 0x6D20106F（`j` 指令，低 7 位 == 0x6F）
 *   反例 = 全 0 / 全 FF / CH32V flash 擦除态 0xE339E339（半字 0xE339，字节 0x39）
 * ------------------------------------------------------------------------ */
static int bl_word_is_image_head(uint32_t w)
{
    if (w == 0x00000000UL || w == 0xFFFFFFFFUL) {
        return 0;
    }
    if (w == 0xE339E339UL || w == 0x39393939UL || (w & 0xFFFFUL) == 0xE339UL) {
        return 0;
    }
    return ((w & 0x7FUL) == 0x6FUL); /* JAL opcode */
}

int ch32_bl_present(void)
{
    return bl_word_is_image_head(BL_RD32(CH32_BL_BASE));
}

/* ---------------------------------------------------------------------------
 * 恢复到接近硬件复位的状态
 *
 * 顺序讲究：先关中断，再关中断源，最后关外设时钟。反过来会有一小段时间
 * 中断还能进来但外设已经没了，容易在关时钟时被中断打断。
 * ------------------------------------------------------------------------ */
void ch32_bl_restore_reset_state(void)
{
    int i;

    /* 1) 关全局中断（M 模式 mstatus.MIE） */
    /* WCH startup enters U mode; mstatus CSR writes trap there. */
    __asm volatile("csrrc zero, 0x800, %0" :: "r"(0x88u) : "memory");

    /* 2) 关掉 256 个中断源 */
    for (i = 0; i < 8; i++) {
        BL_PFIC_IRER(i);
    }

    /* 3) 停 SysTick，并清掉计数值与比较值 */
    BL_WR32(BL_SYSTICK_CTLR, 0);
    BL_WR32(BL_SYSTICK_CNT, 0);
    BL_WR32(BL_SYSTICK_CMP, 0);
    BL_WR32(BL_SYSTICK_SR, 0);

    /* 4) 关外设时钟（上电默认就是全关） */
    BL_WR32(BL_RCC_HBPCENR, 0);
    BL_WR32(BL_RCC_APB2PCENR, 0);
    BL_WR32(BL_RCC_APB1PCENR, 0);

    /* 5) 尝试停独立看门狗（bootloader 的 SystemInit 自己会重开）
     *    注：IWDG 一旦启动，部分器件不允许关，这里只是尽力而为。 */
    BL_WR32(BL_IWDG_CTLR, 0x00005555UL);
    BL_WR32(BL_IWDG_CTLR, 0x00000000UL);

    __asm volatile("fence" ::: "memory");
}

/* ---------------------------------------------------------------------------
 * startup 阶段调用（jal SystemInit 之前）
 *
 * 此时 gp/sp 已就绪、.data/.bss 已初始化、外设仍是复位默认态 ——
 * 正是 bootloader 最想要的起点。
 * ------------------------------------------------------------------------ */
void ch32_bl_jump_after_reset(void)
{
    /* 没有字据：正常启动，什么都不做 */
    if (*BL_ARM != CH32_BL_MAGIC) {
        return;
    }

    /* 先消费掉字据，避免"跳进去 -> 又复位回来 -> 再跳进去"的死循环 */
    *BL_ARM = 0;
    __asm volatile("fence" ::: "memory");

    /* 镜像不在了（被擦掉 / 从没烧过）就别跳，老实跑 APP */
    if (!bl_word_is_image_head(BL_RD32(CH32_BL_BASE))) {
        return;
    }

    /* 进 bootloader 前把中断与 SysTick 收拾干净 */
    __asm volatile("csrrc zero, 0x800, %0" :: "r"(0x88u) : "memory");
    BL_WR32(BL_SYSTICK_CTLR, 0);

    /* 镜像就是按 CH32_BL_BASE 链接的，原地址直接取指即可 */
    __asm volatile("jr %0" ::"r"((uint32_t)CH32_BL_BASE) : "memory");

    while (1) {
        /* 不会到这里 */
    }
}

/* ---------------------------------------------------------------------------
 * 应用层入口：确认镜像 -> 收拾现场 -> 写字据 -> 软复位
 * ------------------------------------------------------------------------ */
void ch32_bl_enter(void)
{
    if (!ch32_bl_present()) {
        /* 用不了标准库 printf 也没关系，静默返回由调用者兜底 */
        return;
    }

    ch32_bl_restore_reset_state();

    /* 立字据（系统复位不清 SRAM），然后复位。
     * 复位之后 startup 里的 ch32_bl_jump_after_reset() 会把控制权接过去。 */
    *BL_ARM = CH32_BL_MAGIC;
    __asm volatile("fence" ::: "memory");

    BL_WR32(BL_PFIC_CFGR, BL_NVIC_KEY3 | (1UL << 7));

    while (1) {
        /* 软复位没生效的话就停在这里，不会带着半初始化状态跑下去 */
    }
}
