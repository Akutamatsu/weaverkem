#!/usr/bin/env python3
"""Parse bench_section5 logs and print LaTeX-ready tables."""
import re
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "bench_section5"

KEM_KEYS = ["kem_keypair", "kem_encaps", "kem_decaps"]
COMP_KEYS = [
    ("poly_ntt (1 poly)", "poly_ntt"),
    ("poly_invntt_tomont (1 poly)", "poly_invntt"),
    ("polyvec_basemul_acc", "polyvec_basemul_acc"),
    ("gen_matrix", "gen_matrix"),
    ("kem_keypair", "kem_keypair"),
    ("kem_encaps", "kem_encaps"),
    ("kem_decaps", "kem_decaps"),
]


def parse_runs(text: str) -> list[dict]:
    blocks = re.split(r"^=== run \d+.*===\s*$", text, flags=re.MULTILINE)
    runs = []
    for block in blocks:
        if "component benchmark" not in block:
            continue
        cur = {}
        pending = None
        for line in block.splitlines():
            stripped = line.strip()
            if stripped.endswith(":") and not stripped.startswith("median") and "cycles" not in stripped:
                if any(x in stripped for x in ("layout", "===", "---")):
                    continue
                pending = stripped[:-1]
            elif stripped.startswith("median:") and pending:
                cur[pending] = int(line.split()[1])
                pending = None
        if cur:
            runs.append(cur)
    return runs


def median_metric(runs: list[dict], key: str) -> int:
    vals = [r[key] for r in runs if key in r]
    if not vals:
        return 0
    return int(statistics.median(vals))


def load_tag(tag: str) -> list[dict]:
    p = OUT / f"{tag}.txt"
    if not p.exists():
        print(f"MISSING {p}", file=sys.stderr)
        return []
    return parse_runs(p.read_text())


def fmt(n: int) -> str:
    return f"{n:,}"


def ratio(a: int, b: int) -> str:
    if b == 0:
        return "-"
    return f"{a/b:.2f}×"


def main():
    tags = {
        "w512_avx": "Weaver-512 AVX",
        "w512_ref": "Weaver-512 Ref",
        "w1024_avx": "Weaver-1024 AVX",
        "w1024_ref": "Weaver-1024 Ref",
        "w2048_avx": "Weaver-2048 AVX",
        "w2048_ref": "Weaver-2048 Ref",
    }
    data = {t: load_tag(t) for t in tags}

    print("## Weaver KEM medians (cycles)\n")
    print("| Tag | KeyGen | Encaps | Decaps |")
    print("|-----|--------|--------|--------|")
    for t, label in tags.items():
        r = data[t]
        print(
            f"| {label} | {fmt(median_metric(r, 'kem_keypair'))} | "
            f"{fmt(median_metric(r, 'kem_encaps'))} | "
            f"{fmt(median_metric(r, 'kem_decaps'))} |"
        )

    print("\n## Weaver AVX2 speedup (AVX/ref)\n")
    print("| Scheme | KeyGen | Encaps | Decaps |")
    print("|--------|--------|--------|--------|")
    pairs = [
        ("Weaver-512", "w512_avx", "w512_ref"),
        ("Weaver-1024", "w1024_avx", "w1024_ref"),
        ("Weaver-2048", "w2048_avx", "w2048_ref"),
    ]
    for name, avx_t, ref_t in pairs:
        avx, ref = data[avx_t], data[ref_t]
        print(
            f"| {name} | "
            f"{ratio(median_metric(ref, 'kem_keypair'), median_metric(avx, 'kem_keypair'))} | "
            f"{ratio(median_metric(ref, 'kem_encaps'), median_metric(avx, 'kem_encaps'))} | "
            f"{ratio(median_metric(ref, 'kem_decaps'), median_metric(avx, 'kem_decaps'))} |"
        )

    print("\n## Component medians (AVX2)\n")
    print("| Component | W-512 | W-1024 | W-2048 |")
    print("|-----------|-------|--------|--------|")
    for raw, short in COMP_KEYS:
        v1 = median_metric(data["w512_avx"], raw)
        v3 = median_metric(data["w1024_avx"], raw)
        v5 = median_metric(data["w2048_avx"], raw)
        print(f"| {short} | {fmt(v1)} | {fmt(v3)} | {fmt(v5)} |")


if __name__ == "__main__":
    main()
