#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把 CH32V307 的 ROM ISP bootloader（链接在 0x00000000）重定位到 **用户区 flash 末尾**，
生成一个可以单独烧到 0x0801C000 的镜像。

为什么另做这条路线：
  SRAM 版实测能跑、USB 也能打开，但时钟选路落到 HSI x16 = 128 MHz，USB 拿不到 48 MHz，
  PC 侧一直"无法识别的 USB 设备"。放回 flash 末端，执行环境与它被设计时最接近。

为什么 blob 不编进 APP 的 .hex：
  WCHISPTool 按下载文件的长度擦除（日志"擦除长度 27K"对应 26944 B 文件）。blob 和 APP
  在一个 .hex 里，APP 一长就把 blob 擦掉 —— 正是放在 0x08000000 那次翻车的原因。

重定位清单（每一条都踩过）：
  A. 复位处理里 4 条 auipc+addi 拼出的绝对地址：gp / sp / .data 源 / .data 目的
  B. 0x1EC0 的 lui+addi（绝对数据地址）
  C. 0x1ED4 的 3 个 lui 站点（后面跟 lw -300(a5)，偏移不变，只改 lui）
  D. 向量表 0x38 - 0x1D4 里所有"小地址"条目
  E. 地址表 0x1ED8 - 0x1F2C 的内容
  F. **两个 switch 跳转表基址** —— 这里有坑：
     原指令是 16 位的 `c.lui a2,2` 紧跟 16 位的 `c.slli a5,a5,2`。
     直接按 4 字节写 `lui a2,hi` 会把后面的 c.slli 一起吃掉，
     于是索引不再乘 4，`lw a5,0(a5)` 变成非对齐访问 ——
     实测症状：mcause=4（load address misaligned），PC 停在偏移 0x204 的 `j self`。
     所以这里改成"调用一个放在镜像尾部的小助手函数"：
       站点处：jal ra, helper  （4 字节）+ 5 个 c.nop 填满原块（共 14 字节）
       助手里：索引乘 4、取表基址、查表、jr
     助手用的是绝对基址，由本脚本按 FLASH_BASE 生成。
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "bootloader_ch32v307_1FFF8000.bin")
OUT_DIR = os.path.join(HERE, "..", "prebuilt")

FLASH_BASE = 0x0801C000      # 镜像落点（4 KB 对齐）
IMAGE_SIZE = 0x2100          # 原镜像有效内容
FINAL_SIZE = 0x3040          # 加上尾部助手函数后的总长度
HELPER_A = 0x3000            # switch(0x1ED8 表) 的助手
HELPER_B = 0x3020            # switch(0x1F04 表) 的助手

GP_VALUE = 0x20000838        # bootloader 自己的 .data/.bss 基址（RAM，固定）
SP_VALUE = 0x20002000        # bootloader 的栈顶（RAM，固定）
DATA_DST = 0x20000000        # .data 拷到 RAM 的哪里（固定）


def lui(rd, imm20):
    return ((imm20 & 0xFFFFF) << 12) | (rd << 7) | 0x37


def addi(rd, rs1, imm12):
    return ((imm12 & 0xFFF) << 20) | (rs1 << 15) | (rd << 7) | 0x13


def jal(rd, imm):
    i = imm & 0x1FFFFF
    b20 = (i >> 20) & 1
    b10_1 = (i >> 1) & 0x3FF
    b11 = (i >> 11) & 1
    b19_12 = (i >> 12) & 0xFF
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | ((rd & 0x1F) << 7) | 0x6F


def slli(rd, rs1, sh):
    return ((sh & 0x1F) << 20) | (rs1 << 15) | (1 << 12) | (rd << 7) | 0x13


def add(rd, rs1, rs2):
    return ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) | (rd << 7) | 0x33


def lw(rd, rs1, off):
    return ((off & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) | (2 << 12) | ((rd & 0x1F) << 7) | 0x03


def jalr_x0(rs1, off):
    return ((off & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) | (0 << 12) | 0x67


def split_hi_lo(addr):
    hi = ((addr + 0x800) >> 12) & 0xFFFFF
    lo = addr - (hi << 12)
    return hi, lo


def main():
    if not os.path.exists(SRC):
        sys.exit("找不到源镜像: %s" % SRC)
    raw = open(SRC, "rb").read()[:IMAGE_SIZE]
    if len(raw) != IMAGE_SIZE:
        sys.exit("源镜像长度不足 0x%X（实际 %d）" % (IMAGE_SIZE, len(raw)))

    img = bytearray(raw) + bytearray(b"\x07" * (FINAL_SIZE - IMAGE_SIZE))

    def rd32(off):
        return struct.unpack_from("<I", img, off)[0]

    def wr32(off, val):
        struct.pack_into("<I", img, off, val & 0xFFFFFFFF)

    def wr16(off, val):
        struct.pack_into("<H", img, off, val & 0xFFFF)

    # ---- A. 复位处理里的 4 条绝对地址 ----
    hi, lo = split_hi_lo(GP_VALUE)
    wr32(0x16D2, lui(3, hi));   wr32(0x16D6, addi(3, 3, lo))
    hi, lo = split_hi_lo(SP_VALUE)
    wr32(0x16DA, lui(2, hi));   wr32(0x16DE, addi(2, 2, lo))
    hi, lo = split_hi_lo(FLASH_BASE + 0x20B0)          # .data 源
    wr32(0x16E2, lui(10, hi));  wr32(0x16E6, addi(10, 10, lo))
    hi, lo = split_hi_lo(DATA_DST)                     # .data 目的
    wr32(0x16EA, lui(11, hi));  wr32(0x16EE, addi(11, 11, lo))

    # ---- B. 0x1EC0 绝对数据地址 ----
    hi, lo = split_hi_lo(FLASH_BASE + 0x1EC0)
    wr32(0x04E2, lui(15, hi)); wr32(0x04E6, addi(15, 15, lo))

    # ---- C. 0x1ED4 的 3 个 lui 站点 ----
    hi = ((FLASH_BASE + 0x1ED4 + 300) >> 12) & 0xFFFFF
    for off in (0x0BD6, 0x0CC4, 0x0CF0):
        wr32(off, lui(15, hi))

    # ---- D. 向量表 ----
    vt = 0
    for off in range(0x38, 0x1D8, 4):
        w = rd32(off)
        if w != 0 and w < IMAGE_SIZE:
            wr32(off, w + FLASH_BASE)
            vt += 1

    # ---- E. 地址表内容 0x1ED8 - 0x1F2C ----
    tb = 0
    for off in range(0x1ED8, 0x1F30, 4):
        w = rd32(off)
        if w != 0 and w < IMAGE_SIZE:
            wr32(off, w + FLASH_BASE)
            tb += 1

    # ---- F. 两个 switch 站点改成调用助手 ----
    def emit_helper(off, table_off):
        """索引在 a5 里，把表基址装进 a2、乘 4、查表、jr"""
        hi, lo = split_hi_lo(FLASH_BASE + table_off)
        wr32(off + 0x00, slli(15, 15, 2))     # a5 <<= 2
        wr32(off + 0x04, lui(12, hi))         # a2 = hi
        wr32(off + 0x08, addi(12, 12, lo))    # a2 += lo
        wr32(off + 0x0C, add(15, 15, 12))     # a5 += a2
        wr32(off + 0x10, lw(15, 15, 0))       # a5 = *(a5)
        wr32(off + 0x14, jalr_x0(15, 0))      # jr a5

    emit_helper(HELPER_A, 0x1ED8)
    emit_helper(HELPER_B, 0x1F04)

    for site, helper in ((0x0ECC, HELPER_A), (0x13D8, HELPER_B)):
        wr32(site, jal(1, (FLASH_BASE + helper) - (FLASH_BASE + site)))
        for k in range(4, 14, 2):             # 原块共 14 字节，余下用 c.nop 填满
            wr16(site + k, 0x0001)

    os.makedirs(OUT_DIR, exist_ok=True)
    tag = "%08X" % FLASH_BASE
    bin_path = os.path.join(OUT_DIR, "boot_isp_%s.bin" % tag)
    hex_path = os.path.join(OUT_DIR, "boot_isp_%s.hex" % tag)
    open(bin_path, "wb").write(bytes(img))

    with open(hex_path, "w", newline="\n") as f:
        for i in range(0, len(img), 16):
            chunk = img[i:i + 16]
            addr = (FLASH_BASE + i) & 0xFFFF
            rec = [len(chunk), (addr >> 8) & 0xFF, addr & 0xFF, 0x00] + list(chunk)
            cks = (-sum(rec)) & 0xFF
            f.write(":" + "".join("%02X" % b for b in rec) + "%02X\n" % cks)
        f.write(":00000001FF\n")

    print("源镜像   : %s" % os.path.abspath(SRC))
    print("落点     : 0x%08X  (占用 0x%08X - 0x%08X)" % (FLASH_BASE, FLASH_BASE, FLASH_BASE + FINAL_SIZE - 1))
    print("  gp     -> 0x%08X" % GP_VALUE)
    print("  sp     -> 0x%08X" % SP_VALUE)
    print("  .data  : 0x%08X -> 0x%08X" % (FLASH_BASE + 0x20B0, DATA_DST))
    print("  向量表 : %d 项 + BASE" % vt)
    print("  地址表 : %d 项 + BASE" % tb)
    print("  助手   : 0x%08X (表 0x1ED8) / 0x%08X (表 0x1F04)" % (FLASH_BASE + HELPER_A, FLASH_BASE + HELPER_B))
    print("输出     : %s" % bin_path)
    print("擦除扇区 : %d - %d" % ((FLASH_BASE - 0x08000000) // 1024,
                                   (FLASH_BASE - 0x08000000 + FINAL_SIZE) // 1024))
    print("APP 上限 : 0x08000000 - 0x%08X = %d KB" % (FLASH_BASE - 1, (FLASH_BASE - 0x08000000) // 1024))


if __name__ == "__main__":
    main()
