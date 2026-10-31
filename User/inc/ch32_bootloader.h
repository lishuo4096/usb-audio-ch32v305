/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32_bootloader.h
 * Description        : 让 CH32V30x 的 APP 主动跳进 WCH 官方 ROM ISP bootloader
 *
 * 适用芯片：CH32V303 / CH32V305 / CH32V307（只有 V30x 的 bootloader 是这个位置）
 *
 * 背景
 *   CH32V305GBU6（QFN28）等小封装芯片没有 BOOT0 引脚，没法靠拉引脚进 ISP。
 *   而 V305 的"系统存储区" 0x1FFF8000-0x1FFFEFFF 在量产片上是**空的**
 *   （实测整片 0xE339 擦除态），所以也没法直接 jr 到 0x1FFF8000。
 *
 *   解决办法：把从 CH32V307 上读出来的官方 bootloader 镜像重定位到
 *   **用户区 flash 的末尾**，由 APP 主动跳过去。镜像已经在启动时算好位置，
 *   你只需要在工程里接入本文件 + ch32_bootloader.c，再在 startup 里插 6 条指令。
 *
 * 为什么必须"写字据 + 软复位"，不能直接 jr
 *   bootloader 的 SystemInit 假定自己处在刚上电的外设状态：它直接把 PLL 源改成
 *   HSE、然后死等 PLLRDY。从 APP 里直接 jr 过去时那个等待永远满足不了，
 *   实测当场把调试口一起卡死。所以流程是：
 *       APP 写一个字据到 RAM -> NVIC_SystemReset() -> 复位后 startup 读到字据
 *       -> 此时 RCC/外设全是上电默认态 -> 再 jr 过去
 *
 * 用法（三步）
 *   1) 把 inc/ch32_bootloader.h 拷到工程的 User/（或任意 include 路径）
 *      把 src/ch32_bootloader.c 加进工程
 *   2) 在 Startup/startup_ch32v30x_*.S 里，`jal SystemInit` 之前插入
 *      startup_patch.txt 里的 6 条指令
 *   3) 在 main() 里需要进 ISP 的地方调 ch32_bl_enter()
 *
 * 容量与落点（本文件自动算，不用手填）
 *   128 KB 片 -> blob 落 0x0801C000，APP 可用 112 KB
 *   256 KB 片 -> blob 落 0x0803C000，APP 可用 240 KB
 *   换芯片只改 CH32_BL_CHIP，或用 -DCH32_BL_FLASH_SIZE_KB=xx 直接指定容量
 *******************************************************************************/
#ifndef __CH32_BOOTLOADER_H
#define __CH32_BOOTLOADER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * 一、选芯片：只决定"用户区 flash 有多大"，blob 落点由它算出来
 * =========================================================================*/
#ifndef CH32_BL_CHIP
#define CH32_BL_CHIP 305 /* 303 / 305 / 307 */
#endif

#if   CH32_BL_CHIP == 303
#define CH32_BL_FLASH_DEFAULT_KB 256
#elif CH32_BL_CHIP == 305
#define CH32_BL_FLASH_DEFAULT_KB 128
#elif CH32_BL_CHIP == 307
#define CH32_BL_FLASH_DEFAULT_KB 256
#else
#error "CH32_BL_CHIP 只能是 303 / 305 / 307；其它容量请用 -DCH32_BL_FLASH_SIZE_KB=xx"
#endif

/* 想用 64K 之类的少数型号，直接 -DCH32_BL_FLASH_SIZE_KB=64 覆盖 */
#ifndef CH32_BL_FLASH_SIZE_KB
#define CH32_BL_FLASH_SIZE_KB CH32_BL_FLASH_DEFAULT_KB
#endif

/* ===========================================================================
 * 二、blob 落点（与 tools/pack_bootloader.py 完全同一套算法，改一个必须改另一个）
 *     base = 0x08000000 + ((FLASH_SIZE - BLOB_SIZE) & ~(ALIGN-1))
 *     128K -> 0x0801C000 ；256K -> 0x0803C000
 * =========================================================================*/
#define CH32_BL_BLOB_SIZE   0x3040UL   /* 重定位后镜像总长 12352 B（含尾部两个助手函数） */
#define CH32_BL_ALIGN       0x4000UL   /* 按 16 KB 向下对齐，便于按扇区擦写 */
#define CH32_BL_FLASH_START 0x08000000UL

#define CH32_BL_BASE \
    (CH32_BL_FLASH_START + \
     ((((uint32_t)CH32_BL_FLASH_SIZE_KB * 1024UL) - CH32_BL_BLOB_SIZE) & ~(CH32_BL_ALIGN - 1UL)))

/* APP 必须小于这个地址，否则会压到 blob；ISP 下载时也会把 blob 擦掉 */
#define CH32_BL_APP_LIMIT   (CH32_BL_BASE - 1UL)

/* 系统存储区里的原始地址。V305/V303 量产片上这里是空的，仅供文档参考 */
#define CH32_BL_ROM_BASE    0x1FFF8000UL
#define CH32_BL_ROM_SIZE    0x00007000UL /* 28 KB */

/* ===========================================================================
 * 三、字据（跨越复位传递"要进 ISP"这个意图）
 *     选 0x20007FFC：链接脚本保留顶端16字节，避免与APP数据、DMA和栈重叠。
 *     系统复位不清 SRAM，所以字据能活过 NVIC_SystemReset()。
 * =========================================================================*/
#define CH32_BL_MAGIC       0x49535031UL /* "ISP1" */
#define CH32_BL_MAGIC_ADDR  0x20007FFCUL /* Reserved by Ld/Link.ld. */

/* 镜像首字正常是 0x6D20106F（`j <复位处理>`，JAL opcode 0x6F） */
#define CH32_BL_HEAD_WORD   0x6D20106FUL

/* ===========================================================================
 * 四、对外接口
 * =========================================================================*/

/* 检查 blob 是否已经烧在 CH32_BL_BASE 处。返回 1=在，0=不在 */
int ch32_bl_present(void);

/* 把外设恢复到接近"硬件复位"的状态：关全局中断、关全部中断源、停 SysTick、
 * 清各外设时钟使能、尝试停 IWDG。
 *
 * 注意：**这一步不能替代软复位**。bootloader 的 SystemInit 要在 PLLRDY 上死等，
 *       只有真正走一遍复位、让 RCC 回到上电默认值，它才能起来。
 *       ch32_bl_enter() 内部会调它，一般不用你手动调。 */
void ch32_bl_restore_reset_state(void);

/* 需要进 ISP 时调这个：检查镜像 -> 清中断/外设 -> 写字据 -> NVIC_SystemReset()
 * 正常情况下不返回（复位后由 startup 里的 ch32_bl_jump_after_reset 接手）。
 * 只有在 blob 不存在时才返回，返回前会打印一行错误（若已初始化 printf）。 */
void ch32_bl_enter(void);

/* 给 startup 用的：读到字据 -> 清字据 -> 校验镜像 -> jr 到 CH32_BL_BASE
 * 没有字据 / 镜像不对 时立刻返回，不影响正常启动。
 * 必须在 .data/.bss 已初始化、gp/sp 已就绪之后调用（即 jal SystemInit 之前）。 */
void ch32_bl_jump_after_reset(void) __attribute__((used));

#ifdef __cplusplus
}
#endif

#endif /* __CH32_BOOTLOADER_H */
