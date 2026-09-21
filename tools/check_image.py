#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
校验链接出来的固件镜像：tools/build_arm.py 的产物是否真的"能启动"。

编译通过不等于镜像可用。这个脚本不看编译日志，直接读 ELF 里的字节做检查：

  1) 中断向量表头两项必须是 _estack（栈顶）和 Reset_Handler；
  2) 四个内核异常（HardFault / MemManage / BusFault / UsageFault）必须指向
     firmware/faults.c 里的实现，而不是启动文件里那个 Default_Handler 死循环。
     注意本工程**没有 RTOS**，所以 SVC / PendSV 指向 Default_Handler 是对的，
     这两个向量不参与检查（见下面的说明）；
  3) SysTick 必须指向 board_stm32f1.c 里的 SysTick_Handler ——
     board_delay_ms() 完全靠它推进 g_ms，指错了会直接死等；
  4) 镜像里不能出现 VFP 浮点指令（Cortex-M3 没有 FPU，出现即是编译选项被改坏）；
  5) 代码不能超过 64KB Flash 的末尾。

用法：python tools/check_image.py [build_arm/smartcoffeemachine.elf]
"""

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF = ROOT / "build_arm" / "smartcoffeemachine.elf"

FLASH_LIMIT = 0x08010000           # 64KB Flash 的末尾（C8T6）
CORE_VECTORS = [
    ("初始 SP (_estack)", None),
    ("Reset_Handler", "Reset_Handler"),
    ("NMI", "NMI_Handler"),
    ("HardFault", "HardFault_Handler"),
    ("MemManage", "MemManage_Handler"),
    ("BusFault", "BusFault_Handler"),
    ("UsageFault", "UsageFault_Handler"),
    ("(保留)", None),
    ("(保留)", None),
    ("(保留)", None),
    ("(保留)", None),
    ("SVC（本工程无 RTOS，未使用）", None),
    ("DebugMon", "DebugMon_Handler"),
    ("(保留)", None),
    ("PendSV（本工程无 RTOS，未使用）", None),
    ("SysTick（board_stm32f1.c 的 1ms 计数）", "SysTick_Handler"),
]


def parse(path):
    d = path.read_bytes()
    shoff = struct.unpack_from("<I", d, 32)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 46)

    raw = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize)
           for i in range(shnum)]
    shstr = d[raw[shstrndx][4]:raw[shstrndx][4] + raw[shstrndx][5]]

    def cstr(buf, off):
        return buf[off:buf.index(b"\0", off)].decode("utf-8", "replace")

    names = [cstr(shstr, r[0]) for r in raw]
    secs = {}
    for i, r in enumerate(raw):
        secs[names[i]] = {"type": r[1], "addr": r[3], "off": r[4],
                          "size": r[5], "link": r[6], "entsize": r[9]}

    # 符号表（SHT_SYMTAB = 2）；它自己的 sh_link 指向字符串表段
    syms = []
    for i, r in enumerate(raw):
        if r[1] != 2 or r[9] == 0:
            continue
        strtab = d[raw[r[6]][4]:raw[r[6]][4] + raw[r[6]][5]]
        for k in range(r[5] // r[9]):
            o = r[4] + k * r[9]
            nameoff, value, size, info, _other, _shndx = struct.unpack_from("<IIIBBH", d, o)
            if nameoff == 0:
                continue
            nm = cstr(strtab, nameoff)
            if nm:
                syms.append((value, size, info & 0xF, nm))
        break

    return d, secs, syms


def resolve(addr, syms):
    """找出所有地址落在 addr 上的函数符号名（别名会返回多个）。"""
    hit = [n for v, _s, k, n in syms if k == 2 and v and (v & ~1) == (addr & ~1)]
    return hit


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ELF
    if not path.exists():
        sys.exit("找不到 %s，先跑 python tools/build_arm.py" % path)

    d, secs, syms = parse(path)
    sym_by_name = {n: v for v, _s, k, n in syms if k == 2}

    fails = []

    # ---- 1. 向量表 ----
    vsec = secs[".isr_vector"]
    vec = struct.unpack_from("<%dI" % (vsec["size"] // 4), d, vsec["off"])

    estack = sym_by_name.get("_estack")
    reset = sym_by_name.get("Reset_Handler")

    print("中断向量表（前 %d 项）" % len(CORE_VECTORS))
    for i, (label, want) in enumerate(CORE_VECTORS):
        a = vec[i]
        names = resolve(a, syms) if a else []
        shown = names[0] if names else "(空)"
        mark = ""

        if want is None:
            # 保留位应当为空；其余由下面的成组检查负责
            pass
        elif not names:
            mark = "  <-- 未填！"
            fails.append("向量 %d (%s) 是空的" % (i, label))
        elif want not in names:
            mark = "  <-- 期望 %s" % want
            fails.append("向量 %d (%s) 实际是 %s，期望 %s"
                         % (i, label, "/".join(names), want))

        print("  [%2d] 0x%08X  %-16s %s%s"
              % (i, a, ",".join(names)[:16] if names else "(空)", label, mark))

    if estack is not None and vec[0] != estack:
        fails.append("向量 0 是 0x%08X，应为 _estack=0x%08X" % (vec[0], estack))
    if vec[0] & 0x3:
        fails.append("初始 SP 未按 4 字节对齐")
    if reset is not None and (vec[1] & ~1) != (reset & ~1):
        fails.append("向量 1 不是 Reset_Handler")

    # ---- 2. 内核异常必须有真实实现，不能落在 Default_Handler ----
    #
    # 注意：Thumb 状态下向量里存的是"地址 | 1"，比较前要先把最低位抹掉。
    # 另外启动文件用 .thumb_set 把没实现的异常都指到了 Default_Handler，
    # 所以一个地址上可能有多个别名，必须按"地址"而不是"名字"来判断。
    #
    # 这里只查四个 Fault 和 SysTick。SVC / PendSV 本工程用不到
    # （没有 RTOS，不做任务切换），它们落在 Default_Handler 上是正确的，
    # 如果把这两个也纳入"必须实现"的检查，就会逼着人写两个空壳函数。
    dh = sym_by_name.get("Default_Handler")
    for idx, name in ((3, "HardFault"), (4, "MemManage"), (5, "BusFault"),
                      (6, "UsageFault"), (15, "SysTick")):
        if vec[idx] == 0:
            fails.append("%s 向量为空" % name)
        elif dh is not None and (vec[idx] & ~1) == (dh & ~1):
            fails.append("%s 仍指向 Default_Handler（死循环），没有实现" % name)

    # ---- 3. 不能有 VFP 浮点指令 ----
    # Cortex-M3 没有 FPU，链接脚本也按软浮点走，所以一条 VFP 指令都不该有。
    # （本工程确实用 float —— PID 和控制都是浮点运算 —— 但那些都会落到
    #   __aeabi_fadd / __aeabi_fmul 这类软件实现上，不会生成 VFP 指令。）
    # 出现 VFP 说明 -mcpu 被改成了带 FPU 的核，镜像烧到 M3 上会直接跑飞。
    # Thumb-2 里 VFP 指令的高半字形如 0xEExx / 0xEDxx，
    # 且第 9-11 位（协处理器号）为 0b101/0b110。
    text = d[secs[".text"]["off"]:secs[".text"]["off"] + secs[".text"]["size"]]
    vfp_hits = 0
    for i in range(0, len(text) - 3, 2):
        hw = struct.unpack_from("<H", text, i)[0]
        if (hw & 0x0E00) in (0x0A00, 0x0C00) and (hw & 0x0100) == 0 and \
           (hw >> 8) in (0xEE, 0xED):
            vfp_hits += 1
    if vfp_hits:
        fails.append("代码段里出现 %d 处疑似 VFP 指令（软浮点构建不该有）" % vfp_hits)

    # ---- 4. 不能超出 Flash ----
    end = max(s["addr"] + s["size"] for s in secs.values()
              if s["size"] and 0x08000000 <= s["addr"] < 0x08100000
              and s["type"] != 8)
    print("")
    print("代码末尾  0x%08X" % end)
    print("Flash 末尾 0x%08X（64KB）" % FLASH_LIMIT)
    if end > FLASH_LIMIT:
        fails.append("代码段 0x%08X 已越过 Flash 末尾 0x%08X" % (end, FLASH_LIMIT))

    print("")
    if fails:
        print("镜像校验未通过，%d 项：" % len(fails))
        for f in fails:
            print("  - " + f)
        return 1

    print("镜像校验通过：向量表、内核异常、指令集、Flash 边界都正常。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
