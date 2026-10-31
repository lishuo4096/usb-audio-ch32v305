#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pack_bootloader.py —— 把 WCH 官方 ISP bootloader 打包到固件 bin 的末尾

干什么
  1. 按芯片型号自动算出用户区 flash 大小，因此自动算出 blob 该落在哪里
     （CH32V30x 系列：303/307 = 256 KB -> 0x0803C000；305 = 128 KB -> 0x0801C000）
  2. 把 bootloader/ch32v30x_isp_rom_1FFF8000.bin 重定位到那个落点
  3. 打成两种产物：
       app_only.bin       —— 只有 APP，用于之后每次走 ISP 下载（不含 blob）
       first_install.bin  —— APP + 中间填充 + blob，用于**第一次**用 SDI 一次性烧写
     第一次烧完之后，ISP 下载只写 APP 区，blob 永远留在末尾，可以反复下载。

为什么分成两个文件
  WCHISPTool 是"按下载文件的长度擦除"的（日志里"擦除长度 27K"对应 26944 B 的文件）。
  如果把 blob 和 APP 放进同一个 hex 给 ISP 下载，擦除会一路擦到 blob 身上，
  正在运行的 bootloader 就被自己擦掉了 —— 这就是把 blob 放在 0x08000000 时翻车的原因。
  所以：blob 只烧一次（SDI），之后 ISP 下载的永远是 app_only.bin。

用法
  python pack_bootloader.py --chip CH32V305 --app build/app.bin
  python pack_bootloader.py --chip 307 --app app.hex --out out --emit-header
  python pack_bootloader.py --chip 305 --app app.bin --base 0x0801C000 --flash-kb 128

  常用参数
    --chip        型号，可写 305 / CH32V305 / CH32V305GBU6 / 307 ...
    --flash-kb    直接指定用户区 flash 大小（覆盖型号推断）
    --base        直接指定 blob 落点（默认自动算）
    --app         APP 固件，.bin 或 Intel .hex
    --app-base    .bin 的装载地址，默认 0x08000000
    --out         输出目录，默认 ./out
    --emit-header 额外生成 ch32_bl_config.h（把型号/容量/落点固化成宏）
    --force       允许 APP 越过 blob 落点（默认拒绝）
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROM_BIN_CANDIDATES = [
    os.path.join(HERE, "..", "bootloader", "ch32v30x_isp_rom_1FFF8000.bin"),
    os.path.join(HERE, "..", "bootloader_ch32v307_1FFF8000.bin"),
    os.path.join(HERE, "..", "..", "bootloader", "ch32v30x_isp_rom_1FFF8000.bin"),
]

FLASH_START = 0x08000000
IMAGE_SIZE = 0x2100     # 原镜像有效内容
FINAL_SIZE = 0x3040     # 重定位后总长（尾部加了两个助手函数）
HELPER_A = 0x3000
HELPER_B = 0x3020
GP_VALUE = 0x20000838
SP_VALUE = 0x20002000
DATA_DST = 0x20000000
ALIGN = 0x4000

# CH32V30x 系列用户区 flash 容量（KB）
CHIP_FLASH_KB = {
    303: 256,
    305: 128,
    307: 256,
}


# --------------------------------------------------------------------------
# 指令编码
# --------------------------------------------------------------------------
def lui(rd, imm20):
    return ((imm20 & 0xFFFFF) << 12) | (rd << 7) | 0x37


def addi(rd, rs1, imm12):
    return ((imm12 & 0xFFF) << 20) | (rs1 << 15) | (rd << 7) | 0x13


def jal(rd, imm):
    i = imm & 0x1FFFFF
    return (((i >> 20) & 1) << 31) | (((i >> 1) & 0x3FF) << 21) | \
           (((i >> 11) & 1) << 20) | (((i >> 12) & 0xFF) << 12) | \
           ((rd & 0x1F) << 7) | 0x6F


def slli(rd, rs1, sh):
    return ((sh & 0x1F) << 20) | (rs1 << 15) | (1 << 12) | (rd << 7) | 0x13


def add(rd, rs1, rs2):
    return ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) | (rd << 7) | 0x33


def lw(rd, rs1, off):
    return ((off & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) | (2 << 12) | ((rd & 0x1F) << 7) | 0x03


def jalr_x0(rs1, off):
    return ((off & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) | 0x67


def split_hi_lo(addr):
    hi = ((addr + 0x800) >> 12) & 0xFFFFF
    lo = addr - (hi << 12)
    return hi, lo


# --------------------------------------------------------------------------
# 重定位
# --------------------------------------------------------------------------
def relocate(rom_bytes, base):
    """把链接在 0x00000000 的官方 bootloader 重定位到 base。返回 0x3040 字节。"""
    raw = rom_bytes[:IMAGE_SIZE]
    if len(raw) != IMAGE_SIZE:
        raise ValueError("源镜像只有 %d 字节，不足 0x%X" % (len(raw), IMAGE_SIZE))

    img = bytearray(raw) + bytearray(b"\x07" * (FINAL_SIZE - IMAGE_SIZE))

    def rd32(o):
        return struct.unpack_from("<I", img, o)[0]

    def wr32(o, v):
        struct.pack_into("<I", img, o, v & 0xFFFFFFFF)

    def wr16(o, v):
        struct.pack_into("<H", img, o, v & 0xFFFF)

    # A. 复位处理里 4 条 auipc+addi 拼出的绝对地址
    hi, lo = split_hi_lo(GP_VALUE)
    wr32(0x16D2, lui(3, hi));  wr32(0x16D6, addi(3, 3, lo))
    hi, lo = split_hi_lo(SP_VALUE)
    wr32(0x16DA, lui(2, hi));  wr32(0x16DE, addi(2, 2, lo))
    hi, lo = split_hi_lo(base + 0x20B0)
    wr32(0x16E2, lui(10, hi)); wr32(0x16E6, addi(10, 10, lo))
    hi, lo = split_hi_lo(DATA_DST)
    wr32(0x16EA, lui(11, hi)); wr32(0x16EE, addi(11, 11, lo))

    # B. 0x1EC0 绝对数据地址
    hi, lo = split_hi_lo(base + 0x1EC0)
    wr32(0x04E2, lui(15, hi)); wr32(0x04E6, addi(15, 15, lo))

    # C. 0x1ED4 的 3 个 lui 站点（后面跟 lw -300(a5)，偏移不变）
    hi = ((base + 0x1ED4 + 300) >> 12) & 0xFFFFF
    for off in (0x0BD6, 0x0CC4, 0x0CF0):
        wr32(off, lui(15, hi))

    # D. 向量表
    for off in range(0x38, 0x1D8, 4):
        w = rd32(off)
        if w != 0 and w < IMAGE_SIZE:
            wr32(off, w + base)

    # E. 地址表内容
    for off in range(0x1ED8, 0x1F30, 4):
        w = rd32(off)
        if w != 0 and w < IMAGE_SIZE:
            wr32(off, w + base)

    # F. 两个 switch 跳转表站点 -> 改成调用尾部助手
    def emit_helper(off, table_off):
        hi, lo = split_hi_lo(base + table_off)
        wr32(off + 0x00, slli(15, 15, 2))
        wr32(off + 0x04, lui(12, hi))
        wr32(off + 0x08, addi(12, 12, lo))
        wr32(off + 0x0C, add(15, 15, 12))
        wr32(off + 0x10, lw(15, 15, 0))
        wr32(off + 0x14, jalr_x0(15, 0))

    emit_helper(HELPER_A, 0x1ED8)
    emit_helper(HELPER_B, 0x1F04)

    for site, helper in ((0x0ECC, HELPER_A), (0x13D8, HELPER_B)):
        wr32(site, jal(1, helper - site))
        for k in range(4, 14, 2):
            wr16(site + k, 0x0001)   # c.nop 填满原 14 字节块

    return bytes(img)


# --------------------------------------------------------------------------
# 固件读取：.bin / Intel .hex
# --------------------------------------------------------------------------
def load_firmware(path, app_base):
    ext = os.path.splitext(path)[1].lower()
    if ext == ".hex":
        mem = {}
        for ln_no, line in enumerate(open(path, "r"), 1):
            line = line.strip()
            if not line or line[0] != ":":
                continue
            rec = bytes.fromhex(line[1:])
            n, addr, typ = rec[0], (rec[1] << 8) | rec[2], rec[3]
            if typ == 0x00:
                for i in range(n):
                    mem[addr + i] = rec[4 + i]
            elif typ == 0x01:
                break
            elif typ == 0x04:
                raise ValueError("第 %d 行：不支持扩展线性地址记录（V30x 用不到）" % ln_no)
        if not mem:
            raise ValueError("hex 里没有数据")
        lo = min(mem)
        hi = max(mem)
        buf = bytearray(hi - lo + 1)
        for a, b in mem.items():
            buf[a - lo] = b
        return bytes(buf), lo
    with open(path, "rb") as f:
        return f.read(), app_base


# --------------------------------------------------------------------------
# 主流程
# --------------------------------------------------------------------------
def parse_chip(text):
    """从 305 / CH32V305 / CH32V305GBU6 / CH32V307VCT6 之类的写法里认出型号

    注意不能直接取前三位数字：'CH32V305GBU6' 里的数字是 3,2,3,0,5,6 = "323056"，
    前三位成了 "323"。所以先把数字全抠出来，再找 303/305/307 子串。
    """
    digits = "".join(ch for ch in str(text) if ch.isdigit())
    for key in ("303", "305", "307"):
        if key in digits:
            return int(key)
    raise ValueError("认不出型号：%r（只支持 CH32V303 / CH32V305 / CH32V307；"
                     "其它容量请用 --flash-kb 直接指定）" % text)


def main():
    ap = argparse.ArgumentParser(description="把 WCH 官方 ISP bootloader 打包到固件 bin 末尾")
    ap.add_argument("--chip", default="305", help="型号：305 / CH32V305 / CH32V305GBU6 / 307 / 303")
    ap.add_argument("--flash-kb", type=int, help="直接指定用户区 flash 大小（KB）")
    ap.add_argument("--base", help="直接指定 blob 落点，如 0x0801C000")
    ap.add_argument("--app", help="APP 固件：.bin 或 Intel .hex")
    ap.add_argument("--app-base", default="0x08000000", help=".bin 的装载地址，默认 0x08000000")
    ap.add_argument("--rom", help="官方 ROM bootloader 镜像路径（默认自动找 bootloader/ 下那个）")
    ap.add_argument("--out", default="out", help="输出目录，默认 ./out")
    ap.add_argument("--emit-header", action="store_true", help="额外生成 ch32_bl_config.h")
    ap.add_argument("--force", action="store_true", help="允许 APP 越过 blob 落点")
    args = ap.parse_args()

    chip = parse_chip(args.chip)
    flash_kb = args.flash_kb or CHIP_FLASH_KB[chip]

    if args.base:
        base = int(args.base, 0)
    else:
        base = FLASH_START + ((flash_kb * 1024 - FINAL_SIZE) & ~(ALIGN - 1))

    # 找 ROM 镜像
    rom_path = args.rom
    if not rom_path:
        for cand in ROM_BIN_CANDIDATES:
            if os.path.exists(cand):
                rom_path = cand
                break
    if not rom_path or not os.path.exists(rom_path):
        sys.exit("找不到官方 bootloader 镜像，用 --rom 指定（应为 28 KB 的 0x1FFF8000 dump）")
    rom = open(rom_path, "rb").read()

    print("芯片        : CH32V%d（用户区 flash %d KB）" % (chip, flash_kb))
    print("ROM 镜像    : %s（%d B）" % (os.path.abspath(rom_path), len(rom)))
    print("blob 落点   : 0x%08X  (占用 0x%08X - 0x%08X)" % (base, base, base + FINAL_SIZE - 1))
    print("APP 上限    : < 0x%08X（%d KB）" % (base, (base - FLASH_START) // 1024))

    blob = relocate(rom, base)
    if blob[0] != 0x6F or (struct.unpack_from("<I", blob, 0)[0] != 0x6D20106F):
        print("  ! 警告：重定位后首字不是 0x6D20106F，检查 --base 是否合理")

    os.makedirs(args.out, exist_ok=True)
    tag = "%08X" % base
    blob_path = os.path.join(args.out, "bootloader_%s.bin" % tag)
    with open(blob_path, "wb") as f:
        f.write(blob)

    if args.app:
        app, app_base = load_firmware(args.app, int(args.app_base, 0))
        app_end = app_base + len(app)
        print("APP         : %s（%d B，0x%08X - 0x%08X）" % (os.path.basename(args.app), len(app),
                                                            app_base, app_end - 1))
        if app_base != FLASH_START:
            print("  ! 提醒：APP 装载地址不是 0x08000000，ISP 下载通常要求它从 0x08000000 开始")
        if app_end > base and not args.force:
            sys.exit("APP 越过 blob 落点（0x%08X > 0x%08X）。缩小固件，或用 --base/--flash-kb 调整，"
                     "或加 --force 强制（不推荐）" % (app_end - 1, base - 1))

        app_only = os.path.join(args.out, "app_only.bin")
        with open(app_only, "wb") as f:
            f.write(app)

        off = base - FLASH_START
        first = bytearray(b"\xFF" * off)
        first[0:len(app)] = app
        first += blob
        first_path = os.path.join(args.out, "first_install.bin")
        with open(first_path, "wb") as f:
            f.write(bytes(first))

        print("输出        :")
        print("  %s  （%d B，ISP 下载用）" % (app_only, len(app)))
        print("  %s  （%d B，SDI 一次性烧写用）" % (first_path, len(first)))
        print("  %s  （%d B，单独烧 blob 用）" % (blob_path, len(blob)))
    else:
        print("输出        :")
        print("  %s  （%d B，单独烧 blob 用）" % (blob_path, len(blob)))

    if args.emit_header:
        hp = os.path.join(args.out, "ch32_bl_config.h")
        with open(hp, "w", encoding="utf-8", newline="\n") as f:
            f.write("/* 由 pack_bootloader.py 自动生成，勿手改 */\n")
            f.write("#ifndef __CH32_BL_CONFIG_H\n#define __CH32_BL_CONFIG_H\n\n")
            f.write("#define CH32_BL_CHIP          %d\n" % chip)
            f.write("#define CH32_BL_FLASH_SIZE_KB %d\n" % flash_kb)
            f.write("#define CH32_BL_BASE          0x%08XUL\n" % base)
            f.write("#define CH32_BL_APP_LIMIT     0x%08XUL\n" % (base - 1))
            f.write("\n#endif\n")
        print("  %s" % hp)

    print()
    print("下一步：")
    print("  1) SDI 烧 first_install.bin 到 0x08000000（第一次，之后不需要再烧 blob）")
    print("  2) 以后每次改固件：只烧 app_only.bin（SDI 或走 ISP 下载都行）")
    print("  3) APP 里调 ch32_bl_enter() 即可进入 ISP；落点宏用 CH32_BL_BASE")


if __name__ == "__main__":
    main()
