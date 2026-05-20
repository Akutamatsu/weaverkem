#!/usr/bin/env python3
"""Aggregate Section 5 benchmark logs into paper tables."""
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEAVER = ROOT / "bench_section5"
MLKEM = WEAVER / "mlkem"


def parse_weaver_runs(text: str) -> list[dict]:
    blocks = re.split(r"^=== run \d+.*===\s*$", text, flags=re.MULTILINE)
    runs = []
    for block in blocks:
        if "component benchmark" not in block:
            continue
        cur, pending = {}, None
        for line in block.splitlines():
            s = line.strip()
            if s.endswith(":") and not s.startswith("median") and "cycles" not in s:
                if any(x in s for x in ("layout", "===", "---")):
                    continue
                pending = s[:-1]
            elif s.startswith("median:") and pending:
                cur[pending] = int(s.split()[1])
                pending = None
        if cur:
            runs.append(cur)
    return runs


def parse_mlkem_runs(text: str) -> list[dict]:
    blocks = re.split(r"^=== run \d+ ===\s*$", text, flags=re.MULTILINE)
    runs = []
    for block in blocks:
        if "median:" not in block:
            continue
        cur, pending = {}, None
        for line in block.splitlines():
            s = line.strip()
            if s.endswith(":") and not s.startswith("median"):
                pending = s[:-1]
            elif s.startswith("median:") and pending:
                cur[pending] = int(s.split()[1])
                pending = None
        if cur:
            runs.append(cur)
    return runs


def med(runs: list[dict], key: str) -> int:
    vals = [r[key] for r in runs if key in r]
    return int(statistics.median(vals)) if vals else 0


def load_weaver(tag: str) -> list[dict]:
    p = WEAVER / f"{tag}.txt"
    return parse_weaver_runs(p.read_text()) if p.exists() else []


def load_mlkem(tag: str) -> list[dict]:
    p = MLKEM / f"{tag}.txt"
    return parse_mlkem_runs(p.read_text()) if p.exists() else []


def ratio(ref: int, avx: int) -> str:
    return f"{ref/avx:.2f}×" if avx else "-"


def pct_save(ref_b: int, other_b: int) -> str:
    return f"{(ref_b - other_b) * 100 / ref_b:.0f}%"


def main() -> None:
    w = {}
    for t in ("512", "1024", "2048"):
        w[f"w{t}_avx"] = load_weaver(f"w{t}_avx")
        w[f"w{t}_ref"] = load_weaver(f"w{t}_ref")
    m = {
        "512_avx": load_mlkem("mlkem512_avx2"),
        "512_ref": load_mlkem("mlkem512_ref"),
        "768_avx": load_mlkem("mlkem768_avx2"),
        "768_ref": load_mlkem("mlkem768_ref"),
        "1024_avx": load_mlkem("mlkem1024_avx2"),
        "1024_ref": load_mlkem("mlkem1024_ref"),
    }

    env = (WEAVER / "environment.txt").read_text().strip()
    commit = (MLKEM / "commit.txt").read_text().strip() if (MLKEM / "commit.txt").exists() else ""

    def wk(mode, impl, key):
        return med(w[f"w{mode}_{impl}"], key)

    def mk(suffix_avx, key):
        return med(m[suffix_avx], key)

    lines = [
        "# Section 5 benchmark results",
        "",
        "## Environment",
        "```",
        env,
        "```",
        "",
        commit,
        "Medians over 5 runs; N_test=1000 per run.",
        "",
        "## Table 1 — End-to-end KEM (AVX2, median cycles)",
        "",
        "| Scheme | n | k | Classical Sec. | KeyGen | Encaps | Decaps |",
        "|--------|---|---|----------------|--------|--------|--------|",
        f"| ML-KEM-512 | 256 | 2 | 128-bit | {mk('512_avx','kyber_keypair'):,} | "
        f"{mk('512_avx','kyber_encaps'):,} | {mk('512_avx','kyber_decaps'):,} |",
        f"| Weaver-512 | 256 | 2 | 128-bit | {wk('512','avx','kem_keypair'):,} | "
        f"{wk('512','avx','kem_encaps'):,} | {wk('512','avx','kem_decaps'):,} |",
        f"| ML-KEM-768 | 256 | 3 | 192-bit | {mk('768_avx','kyber_keypair'):,} | "
        f"{mk('768_avx','kyber_encaps'):,} | {mk('768_avx','kyber_decaps'):,} |",
        f"| ML-KEM-1024 | 256 | 4 | 256-bit | {mk('1024_avx','kyber_keypair'):,} | "
        f"{mk('1024_avx','kyber_encaps'):,} | {mk('1024_avx','kyber_decaps'):,} |",
        f"| Weaver-1024 | 256 | 4 | 256-bit | {wk('1024','avx','kem_keypair'):,} | "
        f"{wk('1024','avx','kem_encaps'):,} | {wk('1024','avx','kem_decaps'):,} |",
        f"| Weaver-2048 | 512 | 4 | 512-bit | {wk('2048','avx','kem_keypair'):,} | "
        f"{wk('2048','avx','kem_encaps'):,} | {wk('2048','avx','kem_decaps'):,} |",
        "",
        "## Table 2 — Bandwidth (bytes)",
        "",
        "| Scheme | |pk| (B) | |ct| (B) | |pk|+|ct| (B) |",
        "|--------|-----------|-----------|-----------------|",
        "| ML-KEM-512 | 800 | 768 | 1568 |",
        "| Weaver-512 | 608 | 736 | 1344 |",
        "| ML-KEM-768 | 1184 | 1088 | 2272 |",
        "| ML-KEM-1024 | 1568 | 1568 | 3136 |",
        "| Weaver-1024 | 1184 | 1312 | 2496 |",
        "| Weaver-2048 | 2336 | 2688 | 5024 |",
        "",
        "## Table 3 — Weaver / ML-KEM cycle ratio (AVX2)",
        "",
        "| Comparison | KeyGen | Encaps | Decaps | pk saving | ct saving |",
        "|-----------|--------|--------|--------|-----------|-----------|",
    ]

    def cmp_row(name, wmode, mtag, pk_r, ct_r, pk_w, ct_w):
        wkp, wep, wdp = wk(wmode, "avx", "kem_keypair"), wk(wmode, "avx", "kem_encaps"), wk(wmode, "avx", "kem_decaps")
        mkp, mep, mdp = mk(mtag, "kyber_keypair"), mk(mtag, "kyber_encaps"), mk(mtag, "kyber_decaps")
        lines.append(
            f"| {name} | {wkp/mkp:.2f}× | {wep/mep:.2f}× | {wdp/mdp:.2f}× | "
            f"{pct_save(pk_r, pk_w)} | {pct_save(ct_r, ct_w)} |"
        )

    cmp_row("Weaver-512 / ML-KEM-512", "512", "512_avx", 800, 768, 608, 736)
    cmp_row("Weaver-1024 / ML-KEM-1024", "1024", "1024_avx", 1568, 1568, 1184, 1312)

    lines += [
        "",
        "## Table 4 — AVX2 vs reference speedup",
        "",
        "| Scheme | KeyGen | Encaps | Decaps |",
        "|--------|--------|--------|--------|",
        f"| ML-KEM-512 | {ratio(mk('512_ref','kyber_keypair'), mk('512_avx','kyber_keypair'))} | "
        f"{ratio(mk('512_ref','kyber_encaps'), mk('512_avx','kyber_encaps'))} | "
        f"{ratio(mk('512_ref','kyber_decaps'), mk('512_avx','kyber_decaps'))} |",
        f"| ML-KEM-1024 | {ratio(mk('1024_ref','kyber_keypair'), mk('1024_avx','kyber_keypair'))} | "
        f"{ratio(mk('1024_ref','kyber_encaps'), mk('1024_avx','kyber_encaps'))} | "
        f"{ratio(mk('1024_ref','kyber_decaps'), mk('1024_avx','kyber_decaps'))} |",
        f"| Weaver-512 | {ratio(wk('512','ref','kem_keypair'), wk('512','avx','kem_keypair'))} | "
        f"{ratio(wk('512','ref','kem_encaps'), wk('512','avx','kem_encaps'))} | "
        f"{ratio(wk('512','ref','kem_decaps'), wk('512','avx','kem_decaps'))} |",
        f"| Weaver-1024 | {ratio(wk('1024','ref','kem_keypair'), wk('1024','avx','kem_keypair'))} | "
        f"{ratio(wk('1024','ref','kem_encaps'), wk('1024','avx','kem_encaps'))} | "
        f"{ratio(wk('1024','ref','kem_decaps'), wk('1024','avx','kem_decaps'))} |",
        f"| Weaver-2048 | {ratio(wk('2048','ref','kem_keypair'), wk('2048','avx','kem_keypair'))} | "
        f"{ratio(wk('2048','ref','kem_encaps'), wk('2048','avx','kem_encaps'))} | "
        f"{ratio(wk('2048','ref','kem_decaps'), wk('2048','avx','kem_decaps'))} |",
        "",
        "## Table 5 — Weaver components (AVX2)",
        "",
        "| Component | W-512 | W-1024 | W-2048 |",
        "|-----------|-------|--------|--------|",
    ]
    comps = [
        ("poly_ntt (1 poly)", "poly_ntt"),
        ("poly_invntt_tomont (1 poly)", "poly_invntt"),
        ("polyvec_basemul_acc", "polyvec_basemul_acc"),
        ("gen_matrix", "gen_matrix"),
        ("kem_keypair", "kem_keypair"),
        ("kem_encaps", "kem_encaps"),
        ("kem_decaps", "kem_decaps"),
    ]
    for raw, short in comps:
        lines.append(
            f"| {short} | {med(w['w512_avx'], raw):,} | {med(w['w1024_avx'], raw):,} | {med(w['w2048_avx'], raw):,} |"
        )

    out = WEAVER / "RESULTS.md"
    out.write_text("\n".join(lines) + "\n")
    print(out.read_text())


if __name__ == "__main__":
    main()
