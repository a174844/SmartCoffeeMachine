#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
拉取 STM32F1 固件构建所需的上游源码到 third_party/。

和 SmartGarden 那边同样的取舍：CMSIS 是外部依赖，两万行级别的代码放进仓库
只会把本项目的 diff 淹掉，所以 third_party/ 被 .gitignore 忽略，
构建前跑一次这个脚本即可。

只访问 api.github.com（列目录）和 raw.githubusercontent.com（取文件）。

用法：
    python tools/fetch_deps.py            # 拉到 third_party/
    python tools/fetch_deps.py --force    # 已存在也重新拉
"""

import argparse
import json
import os
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / "third_party"

# 依赖清单。ref 固定到提交号，保证不同时候拉到的是同一份代码。
DEPS = [
    {
        "dest": "cmsis-core",
        "repo": "ARM-software/CMSIS_5",
        "ref": "55b19837f5703e418ca37894d5745b1dc05e4c91",
        "dirs": ["CMSIS/Core/Include"],
        "exts": (".h",),
        # 只取头文件，且把 CMSIS/Core/Include 这一层剥掉。
        # Cortex-M3 的设备头 stm32f1xx.h 会去 #include "core_cm3.h"。
        "strip": "CMSIS/Core/Include",
    },
    {
        "dest": "cmsis-device-f1",
        "repo": "STMicroelectronics/cmsis-device-f1",
        "ref": "c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22",
        "dirs": ["Include"],
        "exts": (".h",),
        # Include/ 里是全部型号（f100/f101/f102/f103/f105/f107），一共十几个
        # 头文件、几十 KB，没必要挑，挑漏了 stm32f1xx.h 的条件包含会断。
        "files": [
            # 中容量启动文件：STM32F103x8/xB（64-128KB Flash）。
            # 用 _md（medium density）那一份，对应 Blue Pill 一类的 C8T6。
            ("Source/Templates/gcc/startup_stm32f103xb.s", "startup_stm32f103xb.s"),
            # SystemInit() + SystemCoreClock 变量
            ("Source/Templates/system_stm32f1xx.c", "system_stm32f1xx.c"),
        ],
    },
]

API = "https://api.github.com/repos/{repo}/contents/{path}?ref={ref}"
RAW = "https://raw.githubusercontent.com/{repo}/{ref}/{path}"


def _headers():
    """带上令牌（若有）以避开 api.github.com 的匿名限流（60 次/小时）"""
    h = {"User-Agent": "smartcoffeemachine-fetch"}
    tok = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if tok:
        h["Authorization"] = "Bearer " + tok.strip()
    return h


def http_get(url, timeout=60):
    req = urllib.request.Request(url, headers=_headers())
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def list_dir(repo, ref, path):
    """列出一级目录下所有文件（跳过子目录）"""
    raw = http_get(API.format(repo=repo, path=path, ref=ref))
    items = json.loads(raw.decode("utf-8"))
    if isinstance(items, dict):
        raise RuntimeError("list %s/%s failed: %s" % (repo, path, items.get("message")))
    return [it["path"] for it in items if it["type"] == "file"]


def fetch_one(repo, ref, path, out_path, force):
    if out_path.exists() and not force:
        return "skip"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    data = http_get(RAW.format(repo=repo, ref=ref, path=path))
    out_path.write_bytes(data)
    return "ok"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="已存在的文件也重新拉")
    args = ap.parse_args()

    jobs = []          # (repo, ref, src_path, out_path)
    for dep in DEPS:
        repo, ref, dest = dep["repo"], dep["ref"], DEST / dep["dest"]
        exts, strip = dep["exts"], dep.get("strip")

        for d in dep.get("dirs", []):
            try:
                files = list_dir(repo, ref, d)
            except Exception as e:                       # noqa: BLE001
                print("列目录失败 %s/%s: %s" % (repo, d, e), file=sys.stderr)
                return 1
            for p in files:
                if not p.endswith(exts):
                    continue
                rel = p[len(strip) + 1:] if strip else p
                jobs.append((repo, ref, p, dest / rel))

        for p, rel in dep.get("files", []):
            jobs.append((repo, ref, p, dest / rel))

    print("待拉取 %d 个文件 -> %s" % (len(jobs), DEST))
    stats = {"ok": 0, "skip": 0, "fail": 0}

    def worker(job):
        repo, ref, src, out = job
        try:
            return fetch_one(repo, ref, src, out, args.force), None
        except Exception as e:                            # noqa: BLE001
            return "fail", "%s: %s" % (src, e)

    with ThreadPoolExecutor(max_workers=8) as pool:
        for r, err in pool.map(worker, jobs):
            stats[r] += 1
            if err:
                print("  失败 %s" % err, file=sys.stderr)

    print("完成：下载 %d，已存在 %d，失败 %d"
          % (stats["ok"], stats["skip"], stats["fail"]))
    return 1 if stats["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
