#!/usr/bin/env python3
"""Merge gcov coverage for src/*.c across the three test binaries (GCC gcov).

Each test binary compiles a copy of the same #included src/*.c files, so their
.gcov files overlap. This script ORs line/branch coverage across the binaries
and prints a per-file report. With --out DIR it also writes one merged .gcov
file per source into DIR, which SonarQube imports via
sonar.cfamily.gcov.reportsPath.
"""

import os
import re
import sys

SRC_FILES = [
    "xaps-settings.c",
    "xaps-utils.c",
    "xaps-push-notification-plugin.c",
    "xaps-imap-plugin.c",
]

LINE_RE = re.compile(r"^(\s*(-|\$+|\d+\*?|#####|=====)):\s*(\d+):(.*)$")
FUNC_RE = re.compile(r"^\s*function\s+(.+?)\s+called\s+(\d+)")
#  branch 0 taken 3 (fallthrough) / branch 0 never executed
BRANCH_RE = re.compile(r"^\s*branch\s+(\d+)\s+(taken\s+(\d+)|never executed)\s*(.*)$")


class Gcov:
    base: "Gcov"  # set by merge_copies()
    def __init__(self):
        self.raw = []          # (count_str, lineno, text) for every line
        self.lines = {}        # lineno -> [covered, max_count]
        self.branches = {}     # lineno -> [covered, ...] (one bool per branch)
        self.funcs = {}        # name -> called count
        self.cur_line = None

    @staticmethod
    def parse(path):
        g = Gcov()
        with open(path) as f:
            for raw in f:
                line = raw.rstrip("\n")
                m = FUNC_RE.match(line)
                if m:
                    g.funcs[m.group(1)] = max(g.funcs.get(m.group(1), 0),
                                               int(m.group(2)))
                    g.raw.append((None, None, line))
                    continue
                m = BRANCH_RE.match(line)
                if m:
                    cov = m.group(2) != "never executed"
                    if g.cur_line is not None:
                        idx = int(m.group(1))
                        lst = g.branches.setdefault(g.cur_line, [])
                        while len(lst) <= idx:
                            lst.append(False)
                        lst[idx] = lst[idx] or cov
                    # Record the source line so emit_gcov() can rewrite this
                    # branch to reflect merged coverage from other binaries.
                    g.raw.append((None, g.cur_line, line))
                    continue
                m = LINE_RE.match(line)
                if not m:
                    g.raw.append((None, None, line))
                    continue
                cnt_s, lineno, text = m.group(2), int(m.group(3)), m.group(4)
                if cnt_s == "-":
                    g.cur_line = None
                    g.raw.append((cnt_s, lineno, text))
                    continue
                g.cur_line = lineno
                # GCC marks partially executed lines with a trailing '*'
                covered = cnt_s not in ("#####", "=====") and not cnt_s.startswith("$")
                count = int(cnt_s.rstrip("*")) if covered else 0
                prev = g.lines.get(lineno, [False, 0])
                g.lines[lineno] = [prev[0] or covered, max(prev[1], count)]
                g.raw.append((cnt_s, lineno, text))
        return g


def merge_copies(src, dirs):
    gcovs = [Gcov.parse(os.path.join(d, src + ".gcov"))
             for d in dirs if os.path.exists(os.path.join(d, src + ".gcov"))]
    if not gcovs:
        return None
    # base = the copy that exercised the most lines
    base = max(gcovs, key=lambda g: sum(1 for c, _ in g.lines.values() if c))
    merged = Gcov()
    for g in gcovs:
        for name, cnt in g.funcs.items():
            merged.funcs[name] = max(merged.funcs.get(name, 0), cnt)
    for g in gcovs:
        for ln, (cov, cnt) in g.lines.items():
            prev = merged.lines.get(ln, [False, 0])
            merged.lines[ln] = [prev[0] or cov, max(prev[1], cnt)]
        for ln, lst in g.branches.items():
            mlst = merged.branches.setdefault(ln, [])
            for i, cov in enumerate(lst):
                if i >= len(mlst):
                    mlst.append(cov)
                else:
                    mlst[i] = mlst[i] or cov
    merged.base = base
    return merged


def emit_gcov(merged, out_dir, src):
    """Rebuild a merged .gcov file for one source."""
    base = merged.base
    out = []
    for cnt_s, lineno, text in base.raw:
        if cnt_s is not None and cnt_s == "-":
            out.append("%9s:%5d:%s" % ("-", lineno, text))
            continue
        if cnt_s is None:  # header/blank/function/branch/call lines
            m = BRANCH_RE.match(text)
            if m:
                blst = merged.branches.get(lineno, [])
                idx = int(m.group(1))
                if idx < len(blst) and blst[idx]:
                    text = re.sub(r"taken \d+", "taken 1", text)
                out.append(text)
                continue
            if FUNC_RE.match(text) or text.startswith("call"):
                out.append(text)
                continue
            if text == "":
                out.append("")
            else:
                out.append("%9s:%s" % ("-", text))
            continue
        if lineno is None:
            out.append(text)
            continue
        if lineno == 0:
            out.append("%9s:   0:%s" % (cnt_s, text))
            continue
        if merged.lines.get(lineno, [False])[0]:
            cnt = merged.lines[lineno][1]
            out.append("%9d:%5d:%s" % (cnt, lineno, text))
        else:
            out.append("%9s:%5d:%s" % ("#####", lineno, text))
    out.append("")
    with open(os.path.join(out_dir, src + ".gcov"), "w") as f:
        f.write("\n".join(out))


def main():
    args = sys.argv[1:]
    out_dir = None
    if args and args[0] == "--out":
        out_dir = args[1]
        args = args[2:]
    dirs = [d for d in args if os.path.isdir(d)]

    merged = {}
    for src in SRC_FILES:
        m = merge_copies(src, dirs)
        if m is not None:
            merged[src] = m

    total_exec = total_covered = 0
    total_branch = total_branch_cov = 0
    total_func = total_func_cov = 0
    hdr = f"{'file':<32}{'lines':>8}{'covered':>8}{'line%':>8}{'br':>6}{'br-cov':>7}{'br%':>7}{'fn':>4}{'fn-cov':>7}{'fn%':>7}"
    print(hdr)
    print("-" * len(hdr))
    for src in SRC_FILES:
        if src not in merged:
            print(f"{src:<32}N/A")
            continue
        m = merged[src]
        n_exec = len(m.lines)
        n_cov = sum(1 for c, _ in m.lines.values() if c)
        b_total = sum(len(b) for b in m.branches.values())
        b_cov = sum(sum(b) for b in m.branches.values())
        fn_total = len(m.funcs)
        fn_cov = sum(1 for v in m.funcs.values() if v > 0)
        lpct = 100.0 * n_cov / n_exec if n_exec else 0.0
        bpct = 100.0 * b_cov / b_total if b_total else 0.0
        fpct = 100.0 * fn_cov / fn_total if fn_total else 0.0
        print(f"{src:<32}{n_exec:>8}{n_cov:>8}{lpct:>7.1f}%{b_total:>6}{b_cov:>7}{bpct:>6.1f}%{fn_total:>4}{fn_cov:>7}{fpct:>6.1f}%")
        total_exec += n_exec
        total_covered += n_cov
        total_branch += b_total
        total_branch_cov += b_cov
        total_func += fn_total
        total_func_cov += fn_cov
    print("-" * len(hdr))
    l = 100.0 * total_covered / total_exec if total_exec else 0.0
    b = 100.0 * total_branch_cov / total_branch if total_branch else 0.0
    f = 100.0 * total_func_cov / total_func if total_func else 0.0
    print(f"{'TOTAL':<32}{total_exec:>8}{total_covered:>8}{l:>7.1f}%{total_branch:>6}{total_branch_cov:>7}{b:>6.1f}%{total_func:>4}{total_func_cov:>7}{f:>6.1f}%")

    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        for src, m in merged.items():
            emit_gcov(m, out_dir, src)
        print(f"\nWrote merged .gcov files to {out_dir}")

    print()
    print("Uncovered functions:")
    for src in SRC_FILES:
        if src in merged:
            for name, cnt in sorted(merged[src].funcs.items()):
                if cnt == 0:
                    print(f"  {src}: {name}")
    print()
    print("Low-coverage lines (### = never executed):")
    for src in SRC_FILES:
        if src in merged:
            missing = sorted(ln for ln, (c, _) in merged[src].lines.items() if not c)
            if missing:
                print(f"  {src}: lines {missing}")


if __name__ == "__main__":
    main()