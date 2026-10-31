#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 README 用的静态 SVG 图（banner + 3 张框图）。

为什么不用 mermaid-cli：本机无外网、无 chromium 依赖，装不上。
README.md 里保留了等价的 mermaid 源码（GitHub 原生渲染）；
本脚本产出的是同一批图的静态 SVG，供不支持 mermaid 的平台（嘉立创开源硬件平台）使用。

无第三方依赖，输出确定性（同样的输入 -> 逐字节相同的 SVG），方便 diff。

    python tools/gen_diagrams.py

输出到 ../docs/images/
"""

import os

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "docs", "images")

# ---------------------------------------------------------------- palette
BG0, BG1, BG2 = "#0b1020", "#111a33", "#0d1226"
INK, SUB, DIM = "#e8eefc", "#9fb3d6", "#6b7fa3"
LINE = "#3b4a6b"

TONE = {
    "host": "#6366f1",
    "usb":  "#8b5cf6",
    "mcu":  "#3b82f6",
    "dac":  "#14b8a6",
    "adc":  "#0ea5e9",
    "ana":  "#f59e0b",
    "hamp": "#fb923c",
    "pwr":  "#ef4444",
    "ldo":  "#f43f5e",
    "dbg":  "#7c8ba1",
    "led":  "#22c55e",
    "io":   "#eab308",
    "clk":  "#a855f7",
}

FONT = ('-apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans SC", '
        '"Microsoft YaHei", "PingFang SC", "Helvetica Neue", sans-serif')
MONO = '"SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace'


def esc(s):
    return (str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def tw(s, size):
    """粗略文字宽度：CJK 按 1 em，窄字符 0.34，大写/数字 0.62，其余 0.56"""
    w = 0.0
    for ch in str(s):
        o = ord(ch)
        if o > 0x2E80:                       # CJK / 全角
            w += size * 1.00
        elif ch in "ilj|.,:;'`!()[]":
            w += size * 0.34
        elif ch.isupper() or ch.isdigit():
            w += size * 0.62
        else:
            w += size * 0.56
    return w


class Fig:
    def __init__(self, w, h, title="", subtitle="", tag=""):
        self.w, self.h = w, h
        self.body = []
        self.tones = set()
        self.rects = []          # (x, y, w, h, label) —— 供 lint 查重叠/溢出
        self.segs = []           # ((x1,y1),(x2,y2)) —— 供 lint 查"连线穿框"
        self.warn = []
        if title:
            self.text(40, 46, title, 21, INK, "start", "700", spacing="0.5")
        if subtitle:
            self.text(40, 72, subtitle, 12.5, SUB, "start", "400")
        if tag:
            self._right_tag(tag)

    # ---------------------------------------------------------- primitives
    def _right_tag(self, s):
        w = tw(s, 10.5) + 22
        x = self.w - 40 - w
        self.body.append(
            f'<rect x="{x:.1f}" y="{34}" width="{w:.1f}" height="22" rx="11" '
            f'fill="#8b5cf6" fill-opacity="0.13" stroke="#8b5cf6" stroke-opacity="0.45"/>')
        self.text(x + w / 2, 49, s, 10.5, "#c4b5fd", "middle", "600")

    def text(self, x, y, s, size=12, fill=INK, anchor="middle", weight="400",
             spacing=None, mono=False, opacity=None):
        extra = ""
        if spacing:
            extra += f' letter-spacing="{spacing}"'
        if opacity is not None:
            extra += f' opacity="{opacity}"'
        fam = MONO if mono else FONT
        self.body.append(
            f'<text x="{x:.1f}" y="{y:.1f}" font-family=\'{fam}\' font-size="{size}" '
            f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}"{extra}>'
            f'{esc(s)}</text>')

    def box(self, x, y, w, h, title, sub=None, sub2=None, sub3=None, tone="mcu",
            rx=11, bar=True, sub_size=10.5, title_size=13.5):
        c = TONE.get(tone, LINE)
        self.tones.add(tone)
        self.body.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" '
            f'fill="{c}" fill-opacity="0.13" stroke="{c}" stroke-opacity="0.78" '
            f'stroke-width="1.4"/>')
        if bar:
            self.body.append(
                f'<rect x="{x + 1.2}" y="{y + 9}" width="3" height="{h - 18}" rx="1.5" '
                f'fill="{c}" fill-opacity="0.85"/>')

        subs = [s for s in (sub, sub2, sub3) if s]
        self.rects.append((x, y, w, h, title))
        for s in [title] + subs:
            size = title_size if s is title else sub_size
            if tw(s, size) > w - 14:
                self.warn.append(f'text overflow in "{title}": "{s}" '
                                 f'{tw(s, size):.0f}px > {w - 14}px')
        n = 1 + len(subs)
        lh = title_size + 4.5
        total = title_size + sum(sub_size + 4.5 for _ in subs)
        ty = y + h / 2 - total / 2 + title_size * 0.82
        self.text(x + w / 2, ty, title, title_size, INK, "middle", "700")
        cy = ty + sub_size + 3.0
        for s in subs:
            self.text(x + w / 2, cy, s, sub_size, SUB, "middle", "400")
            cy += sub_size + 4.5

    def chip(self, x, y, w, h, title, sub=None, tone="io", sub_size=9.5,
             title_size=11.5):
        c = TONE.get(tone, LINE)
        self.tones.add(tone)
        self.rects.append((x, y, w, h, title))
        for s, size in ((title, title_size), (sub, sub_size)):
            if s and tw(s, size) > w - 12:
                self.warn.append(f'chip text overflow in "{title}": "{s}" '
                                 f'{tw(s, size):.0f}px > {w - 12}px')
        self.body.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" '
            f'fill="{c}" fill-opacity="0.15" stroke="{c}" stroke-opacity="0.7" '
            f'stroke-width="1.2"/>')
        if sub:
            self.text(x + w / 2, y + h / 2 - 2.5, title, title_size, INK, "middle", "600")
            self.text(x + w / 2, y + h / 2 + 11, sub, sub_size, SUB, "middle", "400")
        else:
            self.text(x + w / 2, y + h / 2 + 4, title, title_size, INK, "middle", "600")

    def arrow(self, pts, tone="line", dash=False, label=None, label_pos=None,
              width=1.5, both=False, label_size=10, label_tone=None):
        c = TONE.get(tone, LINE) if tone in TONE else LINE
        if tone in TONE:
            self.tones.add(tone)
        key = tone if tone in TONE else "line"
        d = " ".join(f"{p[0]:.1f},{p[1]:.1f}" for p in pts)
        da = ' stroke-dasharray="5 4"' if dash else ""
        mk = f' marker-end="url(#a_{key})"'
        if both:
            mk += f' marker-start="url(#a_{key}_s)"'
        self.body.append(
            f'<polyline points="{d}" fill="none" stroke="{c}" stroke-opacity="0.85" '
            f'stroke-width="{width}" stroke-linejoin="round"{da}{mk}/>')
        self.segs.extend(zip(pts, pts[1:]))
        if label:
            if label_pos:
                lx, ly = label_pos
            else:
                lx, ly = _mid(pts)
            lw = tw(label, label_size) + 10
            self.body.append(
                f'<rect x="{lx - lw / 2:.1f}" y="{ly - label_size + 0.5:.1f}" '
                f'width="{lw:.1f}" height="{label_size + 4}" rx="3" fill="{BG2}"/>')
            self.text(lx, ly, label, label_size,
                      label_tone or c, "middle", "600")

    def legend(self, x, y, items, size=10.5):
        cx = x
        for tone, txt in items:
            c = TONE.get(tone, LINE)
            self.body.append(
                f'<rect x="{cx}" y="{y - 8}" width="10" height="10" rx="2.5" '
                f'fill="{c}" fill-opacity="0.35" stroke="{c}" stroke-opacity="0.9" '
                f'stroke-width="1"/>')
            self.text(cx + 16, y + 0.5, txt, size, SUB, "start", "400")
            cx += 16 + tw(txt, size) + 22

    def divider(self, y, x0=40, x1=None):
        x1 = self.w - 40 if x1 is None else x1
        self.body.append(
            f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}" stroke="{LINE}" '
            f'stroke-opacity="0.55" stroke-width="1"/>')

    def note(self, x, y, s, size=10.5, fill=None):
        self.text(x, y, s, size, fill or DIM, "start", "400")

    # -------------------------------------------------------------- render
    def render(self):
        defs = [f'<linearGradient id="bg" x1="0" y1="0" x2="0.35" y2="1">'
                f'<stop offset="0" stop-color="{BG0}"/>'
                f'<stop offset="0.55" stop-color="{BG1}"/>'
                f'<stop offset="1" stop-color="{BG0}"/></linearGradient>',
                f'<radialGradient id="glow" cx="0.82" cy="0.12" r="0.75">'
                f'<stop offset="0" stop-color="#3b82f6" stop-opacity="0.20"/>'
                f'<stop offset="1" stop-color="#3b82f6" stop-opacity="0"/></radialGradient>',
                '<pattern id="grid" width="28" height="28" patternUnits="userSpaceOnUse">'
                '<path d="M28 0H0V28" fill="none" stroke="#ffffff" stroke-opacity="0.035" '
                'stroke-width="1"/></pattern>']
        for t in sorted(self.tones):
            c = TONE[t]
            defs.append(
                f'<marker id="a_{t}" viewBox="0 0 10 10" refX="9.2" refY="5" '
                f'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">'
                f'<path d="M0,0 L10,5 L0,10 z" fill="{c}"/></marker>')
            defs.append(
                f'<marker id="a_{t}_s" viewBox="0 0 10 10" refX="9.2" refY="5" '
                f'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">'
                f'<path d="M10,0 L0,5 L10,10 z" fill="{c}"/></marker>')
        defs.append(
            f'<marker id="a_line" viewBox="0 0 10 10" refX="9.2" refY="5" '
            f'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="{LINE}"/></marker>')
        defs.append(
            f'<marker id="a_line_s" viewBox="0 0 10 10" refX="9.2" refY="5" '
            f'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">'
            f'<path d="M10,0 L0,5 L10,10 z" fill="{LINE}"/></marker>')

        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" '
                f'height="{self.h}" viewBox="0 0 {self.w} {self.h}" '
                f'role="img" aria-label="diagram">\n'
                f'<defs>{"".join(defs)}</defs>\n'
                f'<rect width="{self.w}" height="{self.h}" rx="0" fill="url(#bg)"/>\n'
                f'<rect width="{self.w}" height="{self.h}" fill="url(#glow)"/>\n'
                f'<rect width="{self.w}" height="{self.h}" fill="url(#grid)"/>\n'
                + "\n".join(self.body) + "\n</svg>\n")

    def lint(self, name=""):
        """框重叠 + 越界检查。没有浏览器可用，只能靠几何自检。"""
        r = self.rects
        for i in range(len(r)):
            x, y, w, h, t = r[i]
            if x < 0 or y < 0 or x + w > self.w or y + h > self.h:
                self.warn.append(f'"{t}" 越界: ({x},{y},{w}x{h}) vs {self.w}x{self.h}')
            for j in range(i + 1, len(r)):
                x2, y2, w2, h2, t2 = r[j]
                ox = min(x + w, x2 + w2) - max(x, x2)
                oy = min(y + h, y2 + h2) - max(y, y2)
                if ox > 0 and oy > 0:
                    self.warn.append(f'重叠: "{t}" x "{t2}" ({ox:.0f}x{oy:.0f}px)')
        for wmsg in self.warn:
            print(f"  ! {name}: {wmsg}")
        if not self.warn:
            print(f"  ok {name}: 无重叠 / 无溢出 / 无越界")

    def lint_wires(self, name=""):
        """连线不得穿过任何框（端点贴边不算穿透，框内缩 4px 判定）"""
        bad = []
        for (x1, y1), (x2, y2) in self.segs:
            for bx, by, bw, bh, lbl in self.rects:
                if _seg_hits_rect((x1, y1), (x2, y2),
                                  (bx + 4, by + 4, bw - 8, bh - 8)):
                    bad.append(f'连线 ({x1:.0f},{y1:.0f})→({x2:.0f},{y2:.0f}) '
                               f'穿过 "{lbl}"')
        for b in bad:
            print(f"  ! {name}: {b}")
        if not bad:
            print(f"  ok {name}: 连线不穿框")

    def save(self, name):
        os.makedirs(OUT, exist_ok=True)
        p = os.path.join(OUT, name)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(self.render())
        print(f"  {name:16s} {os.path.getsize(p):>7d} B")


def _seg_hits_rect(p, q, r):
    """Liang-Barsky：线段是否与矩形 (x,y,w,h) 相交"""
    x, y, w, h = r
    if w <= 0 or h <= 0:
        return False
    dx, dy = q[0] - p[0], q[1] - p[1]
    t0, t1 = 0.0, 1.0
    for edge_p, edge_q in ((-dx, p[0] - x), (dx, x + w - p[0]),
                           (-dy, p[1] - y), (dy, y + h - p[1])):
        if edge_p == 0:
            if edge_q < 0:
                return False
        else:
            t = edge_q / edge_p
            if edge_p < 0:
                t0 = max(t0, t)
            else:
                t1 = min(t1, t)
            if t0 > t1:
                return False
    return True


def _mid(pts):
    """折线上 50% 处，用来放边标签"""
    segs = []
    total = 0.0
    for a, b in zip(pts, pts[1:]):
        d = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
        segs.append((a, b, d))
        total += d
    acc = 0.0
    for a, b, d in segs:
        if acc + d >= total / 2:
            t = (total / 2 - acc) / (d or 1)
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        acc += d
    return pts[-1]


# =====================================================================
#  1) banner
# =====================================================================
def banner():
    W, H = 1200, 300
    f = Fig(W, H)
    # 波形装饰
    import math
    pts = []
    for i in range(0, 521):
        x = 620 + i
        env = math.exp(-((i - 280) / 300.0) ** 2)
        y = 150 - 46 * env * math.sin(i / 17.0) - 12 * env * math.sin(i / 6.3)
        pts.append(f"{x:.1f},{y:.1f}")
    f.body.append(
        f'<polyline points="{" ".join(pts)}" fill="none" stroke="url(#wave)" '
        f'stroke-width="2.2" stroke-opacity="0.85" stroke-linejoin="round"/>')
    # 频谱柱
    for i, hgt in enumerate([10, 18, 30, 46, 62, 44, 28, 40, 56, 34, 20, 12, 8]):
        x = 640 + i * 26
        f.body.append(
            f'<rect x="{x}" y="{236 - hgt}" width="7" height="{hgt}" rx="3.5" '
            f'fill="#22d3ee" fill-opacity="0.30"/>')

    f.body.append(
        f'<rect x="40" y="56" width="{W - 80}" height="188" rx="18" '
        f'fill="#0a0f1e" fill-opacity="0.55" stroke="#3b82f6" stroke-opacity="0.30"/>')

    f.text(76, 118, "CH32V305", 46, INK, "start", "800", spacing="1")
    f.text(76, 152, "USB Audio Bridge", 26, "#8b9dc4", "start", "300", spacing="2")
    f.body.append('<line x1="76" y1="172" x2="560" y2="172" stroke="#3b82f6" '
                  'stroke-opacity="0.45" stroke-width="1"/>')
    f.text(76, 196, "UAC2 播放 + 录音 · ES9038Q2M + TLV320ADC5120 · 板载调试桥",
           12.5, SUB, "start", "400")

    chips = [("USB HS", "#8b5cf6"), ("384 kHz / 32-bit", "#14b8a6"),
             ("±5 V 模拟电源", "#f59e0b"), ("70 × 35 mm", "#22c55e")]
    x = 76
    for s, c in chips:
        w = tw(s, 11) + 24
        f.body.append(
            f'<rect x="{x}" y="210" width="{w:.1f}" height="24" rx="12" '
            f'fill="{c}" fill-opacity="0.14" stroke="{c}" stroke-opacity="0.55"/>')
        f.text(x + w / 2, 226, s, 11, c, "middle", "600")
        x += w + 10

    # 让波形渐变可用
    svg = f.render().replace(
        "</defs>",
        '<linearGradient id="wave" x1="0" y1="0" x2="1" y2="0">'
        '<stop offset="0" stop-color="#22d3ee" stop-opacity="0.15"/>'
        '<stop offset="0.5" stop-color="#22d3ee" stop-opacity="0.8"/>'
        '<stop offset="1" stop-color="#8b5cf6" stop-opacity="0.2"/></linearGradient>'
        "</defs>")
    f.lint("banner")
    f.lint_wires("banner")
    _write("banner.svg", svg)


# =====================================================================
#  2) 系统架构
# =====================================================================
def arch():
    W, H = 1160, 620
    f = Fig(W, H, "系统架构", "三条独立链路：USB 音频 · 模拟音频 · 调试桥",
            tag="System Architecture")
    C = [30, 250, 480, 710, 940]
    BW = 190

    # --- 播放链
    f.box(C[0], 96, BW, 66, "PC / 主机", "USB2-C · UAC2 音频", tone="host")
    f.box(C[1], 96, BW, 150, "CH32V305GBU6", "RV32IMAC @ 122.88 MHz",
          "USB2 HS · I2S3 主 / I2S2 从", "I2C2 主 · UART7 · SWD", tone="mcu")
    f.box(C[2], 96, BW, 66, "ES9038Q2M", "32-bit DAC → 电流输出", tone="dac")
    f.box(C[3], 96, BW, 66, "OPA1632 ×2", "I/V 转换 · VOCM = GND", tone="ana")
    f.box(C[4], 96, BW, 66, "H1 排针", "差分 L+/L−/R+/R−", tone="io")

    f.box(C[3], 196, BW, 60, "OPA1622", "差分→单端 · 增益 0.6", tone="hamp")
    f.box(C[4], 196, BW, 60, "AUX1 · 3.5 mm", "耳机 / 线路输出", tone="io")

    f.box(C[2], 196, BW, 52, "X2 · 100 MHz", "有源晶振 → ES9038 XI", tone="clk")

    # --- 录音链
    f.box(C[4], 320, BW, 66, "AUX2 · 3.5 mm", "录音输入 · 直流耦合", tone="io")
    f.box(C[3], 320, BW, 66, "OPA1632 ×2", "单端→差分 · 增益 1", tone="ana")
    f.box(C[2], 320, BW, 66, "TLV320ADC5120", "2ch ADC → I2S2 从", tone="adc")

    # --- 调试 / 灯（第 4 行：灯 · 调试主机 · 桥）
    f.box(C[0], 430, BW, 60, "SK6812 ×2", "指示播放 / 录音采样率", tone="led")
    f.box(C[1], 430, BW, 60, "调试主机", "USB1-C", tone="dbg")
    f.box(C[2], 430, BW, 60, "CH549F", "USB ↔ SWD + UART7", tone="dbg")

    # --- 连线
    f.arrow([(220, 129), (250, 129)], "usb")
    f.arrow([(440, 120), (480, 120)], "mcu")
    f.arrow([(670, 129), (710, 129)], "dac")
    f.arrow([(900, 129), (940, 129)], "ana")
    f.arrow([(805, 162), (805, 196)], "ana")                        # I/V → 耳放
    f.arrow([(900, 226), (940, 226)], "hamp")
    f.arrow([(940, 353), (900, 353)], "io")                         # AUX2 → 前级
    f.arrow([(710, 353), (670, 353)], "ana")
    f.arrow([(575, 196), (575, 162)], "clk")                        # 晶振 → DAC

    # ADC → MCU：走 470 这条横向走廊
    f.arrow([(480, 353), (470, 353), (470, 246), (440, 246)], "adc")
    # MCU → CH549（SWD/UART7）：y=400 走廊
    f.arrow([(455, 246), (455, 400), (575, 400), (575, 430)], "dbg")
    # 调试主机 → CH549（USB1-C）
    f.arrow([(440, 460), (480, 460)], "dbg")
    # MCU → SK6812（PA5）：x=232 竖向走廊
    f.arrow([(250, 214), (232, 214), (232, 460), (220, 460)], "led")

    # --- I2C2 主干 + 两个分支（虚线）
    f.arrow([(440, 206), (465, 206)], dash=True, tone="line", width=1.2)
    f.arrow([(465, 206), (465, 150), (480, 150)], dash=True, tone="line", width=1.2)
    f.arrow([(465, 206), (465, 330), (480, 330)], dash=True, tone="line", width=1.2)

    f.divider(516)
    f.note(40, 540, "USB2-C 音频（HS 480 Mbps，EP1 OUT / EP2 反馈 / EP3 IN / EP4 HID）"
                    "　·　I2S3 播放（主发）　·　I2S2 录音（从收）　·　I2C2 控制")
    f.note(40, 562, "MCU 与 DAC 之间无 MCLK 连线：ES9038 由 X2 独立供时钟，DPLL 异步模式；"
                    "CH549F 只负责 SWD + 串口，与应用固件无关。")
    f.legend(40, 596, [("mcu", "主控"), ("dac", "DAC"), ("adc", "ADC"),
                       ("ana", "I/V & 前级"), ("hamp", "耳放"), ("dbg", "调试桥"),
                       ("clk", "时钟"), ("led", "状态灯"), ("line", "I2C2（虚线）")])
    f.lint("arch")
    f.lint_wires("arch")
    _write("arch.svg", f.render())


# =====================================================================
#  3) 电源树
# =====================================================================
def power():
    W, H = 1160, 660
    f = Fig(W, H, "电源树", "单路 USB 5 V 输入 → 6 条 3.3 V 轨 + ±6.5 V → ±5 V 模拟轨",
            tag="Power Tree")

    # 顶部链路
    f.box(40, 96, 210, 58, "USB2 VBUS_IN", "5 V · 总线供电", tone="pwr")
    f.box(320, 96, 210, 58, "U15 TPS2051B", "常开 · 0.5 A 限流", tone="ldo")
    f.box(600, 96, 190, 58, "VBUS 5 V", "整板总轨", tone="pwr")
    f.arrow([(250, 125), (320, 125)], "pwr")
    f.arrow([(530, 125), (600, 125)], "ldo")

    # 数字分支
    f.arrow([(695, 154), (695, 186), (470, 186), (470, 210)], "pwr")
    f.text(560, 180, "常开 · 上电即有", 10, DIM, "middle", "600")
    f.body.append('<rect x="330" y="210" width="300" height="34" rx="9" '
                  'fill="#3b82f6" fill-opacity="0.13" stroke="#3b82f6" '
                  'stroke-opacity="0.7" stroke-width="1.3"/>')
    f.text(480, 232, "3.3 V 轨（6 条）", 12.5, INK, "middle", "700")

    dig = [("MCU_3V3", "MCU + CH549 + 灯"), ("DAC_DVCC", "ES9038 + X2"),
           ("DAC_AVCCL", "ES9038 左声道"), ("DAC_AVCCR", "ES9038 右声道"),
           ("ADC_AVDD", "ADC 模拟"), ("ADC_IOVDD", "ADC 数字 IO")]
    cw, gap = 146, 8
    for i, (t, s) in enumerate(dig):
        r, c = divmod(i, 3)
        x = 330 + c * (cw + gap)
        y = 258 + r * 62
        f.chip(x, y, cw, 54, t, s, "ldo")

    # 模拟分支
    f.arrow([(695, 154), (695, 186), (1000, 186), (1000, 210)], "pwr")
    f.text(990, 180, "受 PC11 门控", 10, DIM, "middle", "600")
    f.body.append('<rect x="850" y="210" width="300" height="34" rx="9" '
                  'fill="#f59e0b" fill-opacity="0.13" stroke="#f59e0b" '
                  'stroke-opacity="0.7" stroke-width="1.3"/>')
    f.text(1000, 232, "模拟轨", 12.5, INK, "middle", "700")

    f.box(850, 258, 300, 52, "U3 TPS2051B", "EN = PC11（高 = 通）", tone="pwr")
    f.arrow([(1000, 310), (1000, 332)], "pwr")
    f.box(850, 332, 300, 52, "U20 LT3471", "+6.5 V boost / −6.5 V Cuk", tone="pwr")

    ana = [("OAMP_+5V", "U7/U9/U11/U12"), ("HAMP_+5V", "U8 OPA1622"),
           ("OAMP_−5V", "I/V + 前级"), ("HAMP_−5V", "耳放")]
    for i, (t, s) in enumerate(ana):
        r, c = divmod(i, 2)
        x = 850 + c * 154
        y = 404 + r * 62
        f.chip(x, y, 146, 54, t, s, "ana")
    f.arrow([(923, 384), (923, 404)], "ana")
    f.arrow([(1077, 384), (1077, 404)], "ana")
    f.text(1000, 540, "每路 ±5 V 成对，共 4 颗 LDO（U24/U16/U23/U13）",
           9.5, DIM, "middle", "400")

    # 实测卡片
    f.body.append('<rect x="40" y="258" width="270" height="252" rx="14" '
                  'fill="#0a0f1e" fill-opacity="0.6" stroke="#3b82f6" '
                  'stroke-opacity="0.28"/>')
    f.text(58, 288, "实测状况", 13.5, INK, "start", "700")
    f.body.append('<line x1="58" y1="300" x2="292" y2="300" stroke="#3b4a6b" '
                  'stroke-opacity="0.6"/>')
    rows = [("USB 输入", "5 V / 约 400 mA"),
            ("500 mA 预算", "占 80 %"),
            ("板温", "40 ~ 50 °C"),
            ("ES9038 表面", "53 °C（受热传导）"),
            ("模拟中间轨", "±6.5 V（目标 ±5 V）"),
            ("LDO 白耗", "约 170 mW ≈ 整机 9 %")]
    y = 324
    for k, v in rows:
        f.text(58, y, k, 10.5, DIM, "start", "400")
        f.text(292, y, v, 10.5, INK, "end", "600")
        y += 26
    f.text(58, y + 6, "RD 余量告急：两颗 SK6812 全白", 9.5, "#f59e0b", "start", "500")
    f.text(58, y + 22, "可再吃 20~40 mA，别常开。", 9.5, "#f59e0b", "start", "500")
    f.text(58, y + 38, "功耗来自说明文档实测值。", 9.5, DIM, "start", "400")

    f.divider(592)
    f.note(40, 616, "复位期间 PC11 = 低 → ±5 V 全断，四颗 OPA1632 与 OPA1622 均无电；"
                    "任何「DAC 没输出」先确认 PC11 已拉高。")
    f.note(40, 638, "OAMP_±5 V 门控无外部上下拉，默认关；DAC_RST（PA6）低有效；"
                    "四颗 OPA1632 的 ENABLE 悬空 = 常开，固件不可单独关断。")
    f.lint("power")
    f.lint_wires("power")
    _write("power.svg", f.render())


# =====================================================================
#  4) 配置派生
# =====================================================================
def config():
    W, H = 1160, 630
    f = Fig(W, H, "编译期配置与派生", "settings.h 一处开关，描述符 / 接口号 / USB PID 全部自动跟随",
            tag="Configuration")

    f.box(40, 236, 190, 96, "settings.h", "全部配置的唯一入口", tone="usb")
    f.text(135, 352, "也可用 -D 覆盖", 9.5, DIM, "middle", "400")

    groups = [("功能开关", "PLAY / CAPTURE / HID / FU / I2C", "mcu", 110),
              ("速率 × 位深", "48 / 96 / 192 / 384 k × 16 / 24 / 32 bit", "dac", 200),
              ("板级与音质", "增益 / 静音 / DPLL / 零检测 / 缓冲区", "ana", 290),
              ("身份", "VID / PID / 名称 / 序列号 / 版本", "host", 380)]
    for t, s, tone, y in groups:
        f.box(320, y, 330, 62, t, s, tone=tone)
        f.arrow([(230, 284), (272, 284), (272, y + 31), (320, y + 31)], "line", width=1.2)

    outs = [("USB PID 自动派生", "PROFILE_CODE << 8", 110, "usb"),
            ("接口号 / 端点 / alt 表", "连续编号，随开关增减", 200, "mcu"),
            ("GET_RANGE 采样率表", "离散单点，min=max", 290, "dac"),
            ("编译期 #error 校验", "位深全关 / PID 溢出 / 延时越界", 380, "pwr")]
    for t, s, y, tone in outs:
        f.box(760, y, 360, 62, t, s, tone=tone)
        f.arrow([(650, y + 31), (760, y + 31)], tone, width=1.2)

    f.body.append('<rect x="40" y="470" width="700" height="96" rx="12" '
                  'fill="#14b8a6" fill-opacity="0.10" stroke="#14b8a6" '
                  'stroke-opacity="0.45"/>')
    f.text(60, 496, "运行期可改（掉电保存在 Flash 4 KB 区）", 12, "#5eead4", "start", "700")
    f.text(60, 520, "vol dac / vol adc · mute dac/adc · swap play/cap · automute on/off · save",
           10.5, SUB, "start", "400", mono=True)
    f.text(60, 542, "17 条串口命令（help 看全部）· 16 字节记录 + CRC16 · 256 槽顺序写日志",
           10, DIM, "start", "400")

    f.divider(586)
    f.note(40, 608, "改速率集合或位深集合时要顺手改 USB_PID_BASE（自动 PID 不含这两个维度），"
                    "否则 Windows 会复用旧拓扑缓存。")
    f.lint("config")
    f.lint_wires("config")
    _write("config.svg", f.render())


# =====================================================================
#  4) 音频数据流
# =====================================================================
def flow():
    W, H = 1160, 540
    f = Fig(W, H, "音频数据流", "播放走等时 OUT，录音走等时 IN；速率靠反馈端点闭环",
            tag="Data Path")

    # --- 播放链
    pw, pp = 150, 158
    x0 = 40
    f.box(x0 + 0 * pp, 110, pw, 66, "EP1 OUT", "等时 1024 B/µframe", tone="usb")
    f.box(x0 + 1 * pp, 110, pw, 66, "接收队列", "24 bit → 32 bit 槽", tone="mcu")
    f.box(x0 + 2 * pp, 110, pw, 66, "播放环", "DMA2_CH2 环形", tone="mcu")
    f.box(x0 + 3 * pp, 110, pw, 66, "I2S3 主", "BCLK / LRCK / SDATA", tone="ana")
    f.box(x0 + 4 * pp, 110, pw, 66, "ES9038Q2M", "DPLL 异步", tone="dac")
    f.box(x0 + 5 * pp, 110, pw, 66, "OPA1632 I/V", "VOCM = GND", tone="ana")
    f.box(x0 + 6 * pp, 110, pw, 66, "输出", "H1 排针 / AUX1", tone="io")
    for i in range(6):
        f.arrow([(x0 + i * pp + pw, 143), (x0 + (i + 1) * pp, 143)], "mcu")

    # --- 反馈（EP2 在最左，伺服在中，PLC 在右）
    f.box(40, 250, 150, 60, "EP2 IN", "反馈 Q16.16 → 主机", tone="usb")
    f.box(200, 250, 180, 60, "水位伺服", "TIM8 时基 · τ = 100 ms", tone="mcu")
    f.box(400, 250, 140, 60, "PLC", "衰减重放补缺口", tone="mcu")
    f.arrow([(200, 280), (190, 280)], "usb", dash=True)
    f.arrow([(431, 176), (431, 215), (290, 215), (290, 250)], "mcu", dash=True)
    f.arrow([(470, 250), (470, 176)], "mcu", dash=True)
    f.text(482, 322, "环占用偏高 → 让主机少送；偏低 → 多送", 9.5, DIM, "start", "400")

    # --- 录音链
    rw, rp = 170, 182
    f.box(x0 + 0 * rp, 390, rw, 68, "AUX2 3.5 mm", "录音输入 · 直流耦合", tone="io")
    f.box(x0 + 1 * rp, 390, rw, 68, "OPA1632 前级", "单端→差分 · 增益 1", tone="ana")
    f.box(x0 + 2 * rp, 390, rw, 68, "TLV320ADC5120", "2ch · VREF 2.75 V", tone="adc")
    f.box(x0 + 3 * rp, 390, rw, 68, "I2S2 从", "随 ADC 的 BCLK/FSYNC", tone="ana")
    f.box(x0 + 4 * rp, 390, rw, 68, "录音环 + DMA", "读指针伺服", tone="mcu")
    f.box(x0 + 5 * rp, 390, rw, 68, "EP3 IN", "等时录音", tone="usb")
    for i in range(5):
        f.arrow([(x0 + i * rp + rw, 424), (x0 + (i + 1) * rp, 424)], "adc")

    f.divider(490)
    f.note(40, 514, "播放环永不停止 DMA：换采样率只重建环形指针，BCLK 不断 → 不会出现「换档一声爆音」。"
                    "录音侧靠读指针伺服对齐 DMA 写指针。")
    f.lint("flow")
    f.lint_wires("flow")
    _write("flow.svg", f.render())


# =====================================================================
#  5) 上电顺序
# =====================================================================
def seq():
    W, H = 1160, 470
    f = Fig(W, H, "上电顺序", "固件实际执行顺序 —— 静音与模拟电源是刻意最后才放开的",
            tag="Power-up Sequence")

    bw, bp = 196, 216
    x0 = 40
    top = [
        ("① 上电 / 复位", "所有 GPIO 为输入", "pwr"),
        ("② GPIO 初始化", "压静音 · DAC_RST 高", "pwr"),
        ("③ 外设初始化", "UART7 / I2C2 / TIM6 / I2S", "mcu"),
        ("④ DAC 复位", "RESETB 低 100 µs 后释放", "dac"),
        ("⑤ ADC 初始化", "TLV320ADC5120 退出 SLEEP", "adc"),
    ]
    bot = [
        ("⑨ 开看门狗", "IWDG 约 2 s", "led"),
        ("⑧ 解静音", "播放流起来后再放开", "hamp"),
        ("⑦ ±5 V 上电", "PC11 = 1 → OAMP_VBUS", "ana"),
        ("⑥ 取回音量", "Flash → 硬件，USB 前", "ana"),
    ]
    for i, (t, s, tone) in enumerate(top):
        f.box(x0 + i * bp, 110, bw, 78, t, s, tone=tone)
    for i in range(4):
        f.arrow([(x0 + i * bp + bw, 149), (x0 + (i + 1) * bp, 149)], "line")

    for i, (t, s, tone) in enumerate(bot):
        f.box(x0 + (4 - i) * bp, 260, bw, 78, t, s, tone=tone)
    for i in range(3):
        f.arrow([(x0 + (4 - i) * bp, 299), (x0 + (3 - i) * bp + bw, 299)], "line")

    # 蛇形拐点：⑤ 底 → ⑥ 顶
    f.arrow([(x0 + 4 * bp + bw / 2, 188), (x0 + 4 * bp + bw / 2, 260)], "adc")

    f.divider(372)
    f.note(40, 396, "红线：② 与 ⑦ 是两条「必须先关住、最后才放开」的门 —— "
                    "静音脚（HAMP_MUTE 高 = 使能）与模拟电源（PC11 高 = 通）。")
    f.note(40, 418, "⑥ 必须在 ⑦ 之前：ES9038 的寄存器与音量影子在模拟轨上电前就已写完，"
                    "避免上电瞬间输出直流台阶。")
    f.lint("seq")
    f.lint_wires("seq")
    _write("seq.svg", f.render())


def _write(name, svg):
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, name)
    with open(p, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(svg)
    print(f"  {name:16s} {os.path.getsize(p):>7d} B")


if __name__ == "__main__":
    print("writing ->", OUT)
    banner()
    arch()
    power()
    config()
    flow()
    seq()
    print("done.")
