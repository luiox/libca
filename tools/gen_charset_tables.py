#!/usr/bin/env python3
## 字符编码内置表生成器（WHATWG encoding 标准 DBCS index → C++ .inc）。
##
## 数据源与同源性：
##   WHATWG encoding 标准的 DBCS index（index-gb18030 / index-big5 / index-shift_jis /
##   index-euc_kr）与 python 内置 codec 的双字节映射同源（encoding_rs、Go x/text、
##   Chromium 均出自这套 index），因此本脚本直接用 python codec 生成，并对抽样
##   断言与 WHATWG 已发布向量一致，再全量自洽校验后落盘。
##
## 生成物（.inc）入库到 libca/str/src/libca/str/detail/，构建不依赖 python 与网络。
##
## 用法：
##   python tools/gen_charset_tables.py --charset gb18030
##   python tools/gen_charset_tables.py --charset all
##   python tools/gen_charset_tables.py --charset gb18030 --out <path>
##
## 表设计（每编码一个 .inc，同一骨架）：
##   - DOUBLE_DECODE：双字节序列 → 码点（u16），按 WHATWG pointer 公式线性索引；
##   - ENCODE_CP / ENCODE_SEQ：码点（升序）→ 双字节序列，消费方二分查找；
##   - 0x00-0x7F ASCII 不进表，消费方直通；
##   - 编码各自的算法区段（如 GB18030 四字节线性映射、Shift_JIS 半角片假名单字节）
##     不进表，由消费方纯算法处理，本脚本负责生成/校验这些区段的元数据。
##
## 已入库：gb18030（本批）。big5 / shift_jis / euc-kr 留开关待接：双字节骨架已
## 支持，接入 C++ 消费方前须先补齐其编解码器特例（big5 有 3 个双码点序列，
## shift_jis 有半角片假名与 PUA 特例，见本文件 CHARSET_CONFIGS 注释）。
##
## 校验策略：
##   - gb18030：全量校验（双字节 23940 序列、四字节全部 1,087,996 个可编码码点、
##     全部 1,587,600 个指针位置的解码行为逐一对拍 python codec）；
##   - 其余编码：抽样 2000 点对拍（接入消费方时应升级为全量）。

import argparse
import random
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT_DIR = REPO_ROOT / "libca/str/src/libca/str/detail"

# 十六进制行宽与每行元素数（生成物风格：12 项/行）。
PER_LINE = 12


class DBCSConfig:
    """单一 DBCS 编码的表参数。

    lead_ranges / trail_ranges：双字节 lead/trail 的合法区段（闭区间列表）。
    trail_index(trail)：trail → 表内行内索引（0 起）。
    pointer(lead, trail)：WHATWG pointer 公式（线性索引）。
    codec：python 编解码器名（WHATWG 同源口径：shift_jis 对应 cp932）。
    spot_vectors：硬编码对拍向量 [(bytes, code_point)]，须含 WHATWG 已发布值。
    notes：写进生成物头部的说明。
    """

    def __init__(self, name, codec, lead_ranges, trail_ranges, trail_index,
                 spot_vectors, notes, codec_name_for_comment):
        self.name = name
        self.codec = codec
        self.lead_ranges = lead_ranges
        self.trail_ranges = trail_ranges
        self._trail_index = trail_index
        self.spot_vectors = spot_vectors
        self.notes = notes
        self.codec_name_for_comment = codec_name_for_comment

    def trail_index(self, trail):
        return self._trail_index(trail)

    @property
    def leads(self):
        out = []
        for lo, hi in self.lead_ranges:
            out.extend(range(lo, hi + 1))
        return out

    @property
    def trails(self):
        out = []
        for lo, hi in self.trail_ranges:
            out.extend(range(lo, hi + 1))
        return out


def _gb18030_trail_index(trail):
    # WHATWG index-gb18030：trail 0x40-0x7E 用 trail-0x40，0x80-0xFE 用 trail-0x41。
    return trail - 0x40 if trail <= 0x7E else trail - 0x41


def _big5_trail_index(trail):
    # WHATWG index-big5：trail 0x40-0x7E 用 trail-0x40，0xA1-0xFE 用 trail-0xA1+0x3F。
    return trail - 0x40 if trail <= 0x7E else trail - 0xA1 + 0x3F


def _euc_kr_trail_index(trail):
    # WHATWG index-euc_kr：trail 0x41-0xFE 用 trail-0x41。
    return trail - 0x41


def _sjis_trail_index(trail):
    # WHATWG index-shift_jis：trail 0x40-0x7E 用 trail-0x40，0x80-0xFC 用 trail-0x41。
    return trail - 0x40 if trail <= 0x7E else trail - 0x41


CHARSET_CONFIGS = {
    # GB18030（本批入库）：双字节 126 lead × 190 trail = 23940 项，无空洞、
    # 码点唯一；四字节区段是"按已赋值码点序的稠密线性排名"，由
    # derive_gb18030_ranges 推导为分段线性表。
    "gb18030": DBCSConfig(
        name="gb18030",
        codec="gb18030",
        lead_ranges=[(0x81, 0xFE)],
        trail_ranges=[(0x40, 0x7E), (0x80, 0xFE)],
        trail_index=_gb18030_trail_index,
        spot_vectors=[
            (b"\x81\x40", 0x4E02),          # index-gb18030[0]（WHATWG 已发布值）
            (b"\xD6\xD0", 0x4E2D),          # 中
            (b"\xCE\xC4", 0x6587),          # 文
            (b"\xA2\xE3", 0x20AC),          # 欧元符号（GB18030 编码；CP936 用 0x80）
            (b"\xAA\xA1", 0xE000),          # PUA 区段起点
            (b"\x81\x30\x81\x30", 0x0080),  # U+0080 走四字节
            (b"\x95\x32\x82\x36", 0x20000), # U+20000 走四字节
            (b"\x94\x39\xFC\x36", 0x1F600), # U+1F600 走四字节
        ],
        notes=[
            "双字节表 23940 项无空洞、码点唯一；0x80/0xFF 单字节非法。",
            "四字节指针空间为已赋值码点的稠密排名，由 derive_gb18030_ranges 推导",
            "为分段线性区间表（见 GB18030_RANGE_*）；指针上限见 GB18030_MAX_POINTER。",
            "指针洞 [39420, 188999]（BMP 块与增补平面块之间的保留段）解码无效。",
        ],
        codec_name_for_comment="gb18030",
    ),
    # Big5（未入库）：WHATWG index-big5 与 python "big5" 同源。注意 big5 有 3 个
    # 双码点序列（0x8862/0x8864/0x88A3 → U+00CA/U+0304 等），本骨架以 0xFFFF 留洞，
    # 接入消费方时须特判；须升级为全量对拍。
    "big5": DBCSConfig(
        name="big5",
        codec="big5",
        lead_ranges=[(0x81, 0xFE)],
        trail_ranges=[(0x40, 0x7E), (0xA1, 0xFE)],
        trail_index=_big5_trail_index,
        spot_vectors=[
            (b"\xA1\x40", 0x3000),  # 全角空格
            (b"\xA4\xA4", 0x4E2D),  # 中
            (b"\xC0\x74", 0x9F9C),  # 龜
        ],
        notes=[
            "注意：0x8862/0x8864/0x88A3 三个序列映射为两个码点（WHATWG big5 特例），",
            "本表以 0xFFFF 留洞，消费方须特判；当前为抽样校验。",
        ],
        codec_name_for_comment="big5",
    ),
    # Shift_JIS（未入库）：WHATWG shift_jis 与 python "cp932" 同源（≠ python
    # "shift_jis"）。半角片假名 0xA1-0xDF 是单字节区段，不进双字节表，消费方
    # 纯算法处理（cp ←→ 0xA1-0xDF 与 U+FF61-U+FF9F 直映射）；须升级为全量对拍。
    "shift_jis": DBCSConfig(
        name="shift_jis",
        codec="cp932",
        lead_ranges=[(0x81, 0x9F), (0xE0, 0xFC)],
        trail_ranges=[(0x40, 0x7E), (0x80, 0xFC)],
        trail_index=_sjis_trail_index,
        spot_vectors=[
            (b"\x81\x40", 0x3000),  # 全角空格
            (b"\x93\x86", 0x4E2D),  # 中
            (b"\x83\x41", 0xFF61),  # 。→ 半角? 实为 U+FF61 起始的单字节区段对拍锚
        ],
        notes=[
            "注意：半角片假名 0xA1-0xDF ↔ U+FF61-U+FF9F 为单字节区段，不进本表；",
            "WHATWG shift_jis 与 python cp932 同源（≠ python shift_jis）；当前为抽样校验。",
        ],
        codec_name_for_comment="cp932",
    ),
    # EUC-KR（未入库）：WHATWG index-euc_kr 与 python "euc_kr" 同源（Wansung）；
    # 94×94 全平面表，无特例；须升级为全量对拍。
    "euc-kr": DBCSConfig(
        name="euc-kr",
        codec="euc_kr",
        lead_ranges=[(0x81, 0xFE)],
        trail_ranges=[(0x41, 0xFE)],
        trail_index=_euc_kr_trail_index,
        spot_vectors=[
            (b"\xB0\xA1", 0xAC00),  # 가
            (b"\xC7\xD1", 0xD55C),  # 한
        ],
        notes=["Wansung（KS X 1001）全平面表；当前为抽样校验。"],
        codec_name_for_comment="euc_kr",
    ),
}


def build_double_byte_table(config):
    """枚举全部双字节序列，经 python codec 解码得 (pointer → 码点) 解码表。

    返回 (decode, holes, duplicates)：
      decode：dict pointer → code point；holes：解码失败（表洞）的 pointer 列表；
      duplicates：同码点多序列（编码表取首个，即最小序列）。
    """
    codec_name = config.codec
    decode = {}
    holes = []
    for lead in config.leads:
        for trail in config.trails:
            pointer = (lead - 0x81) * len(config.trails) + config.trail_index(trail)
            try:
                cp = ord(bytes([lead, trail]).decode(codec_name))
            except UnicodeDecodeError:
                holes.append(pointer)
                continue
            decode.setdefault(pointer, cp)  # setdefault：编码表取最小序列
    return decode, holes


def check_spot_vectors(config):
    """断言 python codec 与硬编码 WHATWG 已发布向量一致（同源性的抽样证据）。"""
    for raw, expected_cp in config.spot_vectors:
        got = raw.decode(config.codec)
        if ord(got) != expected_cp:
            raise AssertionError(
                "spot vector mismatch for %s: bytes %s decode to U+%04X, want U+%04X"
                % (config.name, raw.hex(), ord(got), expected_cp))


def derive_gb18030_ranges():
    """从 python gb18030 codec 推导四字节线性区间表（WHATWG index-gb18030-ranges 同源）。

    GB18030 四字节指针空间是「已赋值码点（非 ASCII / 非双字节 / 非代理项）按码点
    序的稠密排名」，码点有洞处指针跳变，因此整体是分段线性的：指针 207 段。
    返回按 start_cp 升序的 [(start_cp, start_pointer), ...]。
    """
    samples = []
    double_byte_cps = set(build_double_byte_table(CHARSET_CONFIGS["gb18030"])[0].values())
    for cp in range(0x80, 0x110000):
        if 0xD800 <= cp <= 0xDFFF:
            continue  # 代理项不参与映射
        if cp in double_byte_cps:
            continue
        try:
            b = chr(cp).encode("gb18030")
        except UnicodeEncodeError:
            raise AssertionError("cp U+%04X unexpectedly unencodable" % cp)
        if len(b) != 4:
            raise AssertionError("cp U+%04X expected four-byte encoding" % cp)
        pointer = (((b[0] - 0x81) * 10 + (b[1] - 0x30)) * 1260
                   + (b[2] - 0x81) * 10 + (b[3] - 0x30))
        samples.append((cp, pointer))

    ranges = []
    prev_cp = prev_ptr = None
    for cp, pointer in samples:
        # 断段条件：cp 或 pointer 任一不再连续即开新段。注意 cp 空洞（双字节 /
        # 未赋值 / 代理项）即使指针稠密也必须断段，否则线性插值会把指针映到
        # 空洞码点（如 U+00A4 是双字节映射，四字节指针 36 必须直接落到 U+00A5）。
        if prev_cp is None or cp != prev_cp + 1 or pointer != prev_ptr + 1:
            ranges.append((cp, pointer))
        prev_cp, prev_ptr = cp, pointer
    return ranges


def verify_gb18030(ranges):
    """全量对拍 gb18030：双字节 / 四字节编解码逐项 vs python codec。失败即抛异常。"""
    codec = CHARSET_CONFIGS["gb18030"].codec

    # 1) 双字节解码表逐项对拍 + 结构断言。
    decode, holes = build_double_byte_table(CHARSET_CONFIGS["gb18030"])
    if holes:
        raise AssertionError("gb18030 double-byte table has holes: %r" % holes[:8])
    if len(decode) != 23940:
        raise AssertionError("gb18030 double-byte table size %d != 23940" % len(decode))
    cps = list(decode.values())
    if len(set(cps)) != len(cps):
        raise AssertionError("gb18030 double-byte table has duplicate code points")
    if max(cps) > 0xFFFF:
        raise AssertionError("gb18030 double-byte table has non-BMP code point")

    # 2) 双字节编码：每个表内码点 python 编码须等于表内最小序列。
    seq_by_cp = {}
    for pointer, cp in decode.items():
        seq_by_cp[cp] = pointer  # pointer 即可还原序列（见 pointer_to_double_bytes）
    for cp, pointer in seq_by_cp.items():
        lead, trail = pointer_to_double_bytes(pointer)
        if chr(cp).encode(codec) != bytes([lead, trail]):
            raise AssertionError("gb18030 encode mismatch for U+%04X" % cp)

    # 3) 四字节编码：全部可编码码点的 (区间表 → 指针 → 四字节) 与 python 一致。
    max_ptr = ranges[-1][1] + (0x10FFFF - ranges[-1][0])
    for cp in range(0x80, 0x110000):
        if 0xD800 <= cp <= 0xDFFF or cp in seq_by_cp:
            continue
        want = chr(cp).encode(codec)
        pointer = lookup_pointer_by_cp(ranges, cp)
        if pointer is None or pointer > max_ptr:
            raise AssertionError("gb18030 four-byte range lookup miss for U+%04X" % cp)
        if pointer_to_four_bytes(pointer) != want:
            raise AssertionError("gb18030 four-byte encode mismatch for U+%04X" % cp)

    # 4) 四字节解码：全部指针位置（含超上限）的 (指针 → 区间表 → 码点) 与 python 一致。
    theoretical_max = ((0xFE - 0x81) * 10 + 9) * 1260 + (0xFE - 0x81) * 10 + 9
    for pointer in range(0, theoretical_max + 1):
        raw = pointer_to_four_bytes(pointer)
        try:
            expected = ord(raw.decode(codec))
        except UnicodeDecodeError:
            expected = None
        got = lookup_cp_by_pointer(ranges, pointer, max_ptr)
        if got != expected:
            raise AssertionError(
                "gb18030 four-byte decode mismatch at pointer %d: got %r want %r"
                % (pointer, got, expected))

    return max_ptr


def pointer_to_double_bytes(pointer):
    """gb18030 pointer → 双字节序列（trail 0x40-0x7E 后接 0x80-0xFE）。"""
    lead = 0x81 + pointer // 190
    idx = pointer % 190
    trail = 0x40 + idx if idx <= 0x7E - 0x40 else 0x41 + idx
    return lead, trail


def pointer_to_four_bytes(pointer):
    """gb18030 四字节指针 → 4 字节序列（b1/b3 ∈ 0x81-0xFE，b2/b4 ∈ 0x30-0x39）。"""
    b1 = 0x81 + pointer // 12600
    b2 = 0x30 + pointer % 12600 // 1260
    b3 = 0x81 + pointer % 1260 // 10
    b4 = 0x30 + pointer % 10
    return bytes([b1, b2, b3, b4])


def lookup_pointer_by_cp(ranges, cp):
    """区间表编码侧：cp → 四字节指针；不在任何区间返回 None。"""
    lo, hi, ans = 0, len(ranges) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        if ranges[mid][0] <= cp:
            ans = mid
            lo = mid + 1
        else:
            hi = mid - 1
    if ans is None:
        return None
    start_cp, start_ptr = ranges[ans]
    return start_ptr + (cp - start_cp)


def lookup_cp_by_pointer(ranges, pointer, max_ptr):
    """区间表解码侧：pointer → 码点；越界或落在指针洞返回 None。

    指针洞：BMP 块止于指针 39419（U+FFFF），增补平面块起于指针 189000
    （U+10000），中间 [39420, 188999] 是 GB18030 保留指针段，任何映射都无效。
    判据：段内插值结果达到/越过下一段 start_cp 即为洞（不产生码点）。
    """
    if pointer > max_ptr:
        return None
    lo, hi, ans = 0, len(ranges) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        if ranges[mid][1] <= pointer:
            ans = mid
            lo = mid + 1
        else:
            hi = mid - 1
    start_cp, start_ptr = ranges[ans]
    cp = start_cp + (pointer - start_ptr)
    if ans + 1 < len(ranges) and cp >= ranges[ans + 1][0]:
        return None  # 指针洞（保留段）
    return cp


def sample_verify(config, decode, samples=2000):
    """非入库编码的抽样对拍：随机双字节序列 + 随机码点。"""
    rng = random.Random(20260915)
    leads, trails = config.leads, config.trails
    codec_name = config.codec
    for _ in range(samples):
        lead, trail = rng.choice(leads), rng.choice(trails)
        raw = bytes([lead, trail])
        try:
            expected = ord(raw.decode(codec_name))
        except UnicodeDecodeError:
            expected = None
        pointer = (lead - 0x81) * len(trails) + config.trail_index(trail)
        got = decode.get(pointer)
        if got != expected:
            raise AssertionError("%s decode mismatch at %s" % (config.name, raw.hex()))
    # 编码侧抽样：码点 → 最小序列。
    seq_by_cp = {}
    for pointer, cp in decode.items():
        seq_by_cp.setdefault(cp, pointer)
    cps = sorted(seq_by_cp)
    for _ in range(samples):
        cp = rng.choice(cps)
        pointer = seq_by_cp[cp]
        lead = 0x81 + pointer // len(trails)
        idx = pointer % len(trails)
        # 反查 trail：在合法 trail 列表里按索引取。
        trail = trails[idx]
        if chr(cp).encode(codec_name) != bytes([lead, trail]):
            raise AssertionError("%s encode mismatch for U+%04X" % (config.name, cp))


def format_array(values, per_line=PER_LINE):
    lines = []
    for i in range(0, len(values), per_line):
        chunk = ", ".join("0x%04X" % v for v in values[i:i + per_line])
        lines.append("    " + chunk + ",")
    return "\n".join(lines)


def format_array32(values, per_line=PER_LINE):
    lines = []
    for i in range(0, len(values), per_line):
        chunk = ", ".join("0x%08X" % v for v in values[i:i + per_line])
        lines.append("    " + chunk + ",")
    return "\n".join(lines)


def render_inc(config, decode, ranges=None, max_ptr=None):
    """渲染单个编码的 .inc 内容（生成物头 + 数组 + 元数据）。"""
    symbol_prefix = config.name.replace("-", "_").upper()
    seq_by_cp = {}
    for pointer, cp in decode.items():
        seq_by_cp.setdefault(cp, pointer)  # 已由 setdefault 保证最小序列
    ordered = sorted(seq_by_cp.items())

    trails = config.trails

    def pointer_to_seq(pointer):
        # pointer → (lead << 8 | trail) 的打包字节序列（与消费方 C++ 解包约定一致）。
        lead = 0x81 + pointer // len(trails)
        return (lead << 8) | trails[pointer % len(trails)]

    hole_note = ""
    holes = [p for p in range(len(config.leads) * len(config.trails)) if p not in decode]
    if holes:
        hole_note = "\n// 表洞（解码为 0xFFFF 哨兵，消费方须特判）：%d 个" % len(holes)

    parts = []
    parts.append("""//
// %s 双字节码表 —— 由 tools/gen_charset_tables.py 生成，请勿手工编辑。
// 生成命令：python tools/gen_charset_tables.py --charset %s
// 数据源：python 内置 codec "%s"（与 WHATWG encoding 标准 index-%s 同源），
// 生成时已断言 WHATWG 抽样向量一致 + %s。
//
// 消费方须知：
//   - 先 include "libca/core/datatype.hpp"（u16/u32 别名）；
//   - 0x00-0x7F ASCII 不进表，消费方直通；
//   - 解码表按 WHATWG pointer 公式线性索引；编码表按码点升序二分查找。
//%s
//%s
//""" % (config.name, config.name, config.codec, config.name,
        "全量自洽校验" if config.name == "gb18030" else "抽样对拍校验",
        hole_note,
        "".join("\n// " + note for note in config.notes)))

    parts.append("#pragma once")
    parts.append("// clang-format off")
    parts.append("")
    parts.append("namespace ca::str::detail {")
    parts.append("namespace charset_tables {")
    parts.append("")
    parts.append("// 双字节解码表：pointer = (lead - 0x81) * %d + %s；"
                 % (len(config.trails), "trail 索引见脚本 trail_index 注释"))
    parts.append("// 值为码点（u16）。")
    decode_values = []
    for pointer in range(len(config.leads) * len(config.trails)):
        cp = decode.get(pointer, 0xFFFF)
        decode_values.append(cp)
    parts.append("constexpr u16 %s_DOUBLE_DECODE[%d] = {" % (symbol_prefix, len(decode_values)))
    parts.append(format_array(decode_values))
    parts.append("};")
    parts.append("")
    parts.append("// 编码表：按码点升序，二分查找 CP 后取同下标 SEQ（= lead << 8 | trail）。")
    parts.append("constexpr u16 %s_ENCODE_CP[%d] = {" % (symbol_prefix, len(ordered)))
    parts.append(format_array([cp for cp, _ in ordered]))
    parts.append("};")
    parts.append("constexpr u16 %s_ENCODE_SEQ[%d] = {" % (symbol_prefix, len(ordered)))
    parts.append(format_array([pointer_to_seq(pointer) for _, pointer in ordered]))
    parts.append("};")

    if ranges is not None:
        parts.append("")
        parts.append("// 四字节线性区间表（与 WHATWG index-gb18030-ranges 同源，%d 段）：" % len(ranges))
        parts.append("// pointer ∈ [PTR_k, PTR_{k+1}) → cp = CP_k + (pointer - PTR_k)；")
        parts.append("// pointer > MAX_POINTER（超过 U+10FFFF 的映射）非法。")
        parts.append("constexpr u32 %s_RANGE_CP[%d] = {" % (symbol_prefix, len(ranges)))
        parts.append(format_array32([cp for cp, _ in ranges]))
        parts.append("};")
        parts.append("constexpr u32 %s_RANGE_PTR[%d] = {" % (symbol_prefix, len(ranges)))
        parts.append(format_array32([ptr for _, ptr in ranges]))
        parts.append("};")
        parts.append("constexpr u32 %s_MAX_POINTER = 0x%08X;" % (symbol_prefix, max_ptr))

    parts.append("")
    parts.append("}  // namespace charset_tables")
    parts.append("}  // namespace ca::str::detail")
    parts.append("")
    return "\n".join(parts)


def default_out_path(config):
    return DEFAULT_OUT_DIR / ("%s_tables.inc" % config.name.replace("-", "_"))


def generate(charset, out_path=None):
    config = CHARSET_CONFIGS[charset]
    check_spot_vectors(config)
    decode, _holes = build_double_byte_table(config)

    ranges = None
    max_ptr = None
    if charset == "gb18030":
        ranges = derive_gb18030_ranges()
        max_ptr = verify_gb18030(ranges)

    sample_verify(config, decode)

    path = Path(out_path) if out_path else default_out_path(config)
    path.write_text(render_inc(config, decode, ranges, max_ptr), encoding="utf-8", newline="\n")

    n_pointers = len(config.leads) * len(config.trails)
    print("%-10s -> %s（双字节 %d 项，码点 %d 个%s）"
          % (charset, path, n_pointers, len(decode),
             ("，四字节区间 %d 段，max_ptr %d" % (len(ranges), max_ptr)) if ranges else ""))


def main():
    parser = argparse.ArgumentParser(description="生成字符编码内置表 .inc")
    parser.add_argument("--charset", required=True,
                        choices=sorted(CHARSET_CONFIGS) + ["all"],
                        help="要生成的编码（all = 全部）")
    parser.add_argument("--out", default=None, help="输出路径（默认入库路径）")
    args = parser.parse_args()

    if args.charset == "all":
        for name in sorted(CHARSET_CONFIGS):
            generate(name)
    else:
        generate(args.charset, args.out)


if __name__ == "__main__":
    main()
