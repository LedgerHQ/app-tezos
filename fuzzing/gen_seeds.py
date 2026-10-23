#!/usr/bin/env python3
"""Write the seeds of the fuzz targets, one directory per target under OUT_DIR.

- operation_parser: the operation vectors of the unit tests, and the operations
  and micheline expressions of the sample generator when it has been run. The
  vectors of the unit tests are hex strings, some of them built from #define'd
  prefixes, so they are extracted from the C source and follow the tests.
- micheline_parser: a few expressions, and the samples of the generator.
- the other targets: a few inputs that reach what they check straight away, in
  the layout of their harness.
"""

import argparse
import hashlib
import re
from collections.abc import Iterator
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = ROOT / "unit-tests" / "test_operation_parser.c"
DEFAULT_SAMPLES = ROOT / "tests" / "samples"

# A C string literal (without escapes) or an identifier
TOKEN = re.compile(r'"([^"\\]*)"|([A-Za-z_]\w*)')
COMMENT_OR_STRING = re.compile(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*', re.DOTALL)
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+([^\n]*)", re.MULTILINE)
VECTOR = re.compile(r"\bchar\s+str\[\]\s*=\s*([^;]*);")

MAGIC_OPERATION = b"\x03"
MAGIC_MICHELINE = b"\x05"

# Micheline: an int, a string, sequences, primitives, bytes, annotations
MICHELINE_SEEDS = [
    "0000",  # int 0
    "0045",  # int -5
    "01000000036162 63",  # string "abc"
    "0200000000",  # empty sequence
    "02000000040001 0002",  # sequence of two ints
    "030b",  # Unit
    "07070001 0002",  # Pair 1 2
    "05090005",  # Some 5
    "0a00000003aabbcc",  # bytes
    "040b0000000325666f",  # Unit with an annotation
    "09200000000000000000",  # a primitive with any number of arguments
]


def strip_comments(source: str) -> str:
    return COMMENT_OR_STRING.sub(lambda m: m.group(0) if m.group(0).startswith('"') else " ", source)


def concat(expr: str, macros: dict[str, str]) -> str | None:
    """Value of an expression made only of string literals and of macros
    expanding to such strings, None for anything else."""
    if TOKEN.sub("", expr).strip():
        return None
    parts = []
    for literal, name in TOKEN.findall(expr):
        if name:
            if name not in macros:
                return None
            parts.append(macros[name])
        else:
            parts.append(literal)
    return "".join(parts)


def unit_test_vectors(path: Path) -> Iterator[bytes]:
    source = strip_comments(path.read_text()).replace("\\\n", " ")

    macros: dict[str, str] = {}
    for name, body in DEFINE.findall(source):
        value = concat(body, macros)
        if value is not None:
            macros[name] = value

    for expr in VECTOR.findall(source):
        value = concat(expr, macros)
        if value is None:
            continue
        try:
            yield bytes.fromhex(value)
        except ValueError:
            continue


def sample_lines(path: Path) -> Iterator[bytes]:
    """The samples of the generator, one hex string per line."""
    if not path.is_file():
        return
    for line in path.read_text().splitlines():
        try:
            data = bytes.fromhex(line.strip())
        except ValueError:
            continue
        if data:
            yield data


def formatting_seeds() -> Iterator[bytes]:
    """[function | canonical][output size][aux][payload]: each helper, on an
    input it accepts, with and without leading zero bytes."""
    plain = bytes(range(1, 97))
    zeros = bytes(2) + bytes(range(3, 97))
    for function in range(10):
        for aux in range(5):
            yield bytes([0x80 | function, 12, aux]) + (zeros if aux % 2 else plain)


def amounts_seeds() -> Iterator[bytes]:
    """[function][slack][decimals][symbol length][payload]"""
    digits = bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2])
    for function in range(7):
        for slack, decimals, symbol in ((5, 0, 0), (60, 6, 3), (40, 18, 5)):
            yield bytes([function, slack, decimals, symbol]) + digits
    # Zarith: 300, then -5
    yield bytes([6, 0, 0, 0, 0xAC, 0x04])
    yield bytes([6, 0, 0, 0, 0x45])


def fa2_seeds() -> Iterator[bytes]:
    """[lookup][bits][index or token id]"""
    yield bytes([0, 0, 0])
    yield bytes([1, 0xC0, 0])
    yield bytes([1, 0x80, 1])
    yield bytes([2, 0, 0])
    yield bytes([3, 0, 0])


def swap_key_policy_seeds() -> Iterator[bytes]:
    """[flags][length][components]"""
    yield bytes([0x0F, 3]) + bytes(12)
    yield bytes([0x0F, 4]) + bytes(16)
    yield bytes([0x01, 3]) + bytes(12)


def ui_strings_seeds() -> Iterator[bytes]:
    """Operations of [opcode][arg][arg][arg]"""

    def push(length: int, seed: int) -> bytes:
        return bytes([0, length >> 8, length & 0xFF, seed])

    yield push(40, 1) + push(100, 2) + push(300, 3) + bytes([2, 0, 0, 0]) + push(60, 4)
    yield push(20, 1) + bytes([4, 10, 5, 7]) + bytes([6, 0, 0, 0]) + bytes([3, 0, 0, 0])
    yield push(0, 1) + push(1, 2) + push(500, 3) + push(100, 4) + bytes([2, 0, 0, 0])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path, help="directory to write the seeds to")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="C file holding the vectors")
    parser.add_argument("--samples", type=Path, default=DEFAULT_SAMPLES, help="samples of the generator")
    args = parser.parse_args()

    counts: dict[str, int] = {}

    def write(target: str, data: bytes) -> None:
        target_dir = args.out_dir / target
        target_dir.mkdir(parents=True, exist_ok=True)
        (target_dir / hashlib.sha1(data, usedforsecurity=False).hexdigest()).write_bytes(data)
        counts[target] = counts.get(target, 0) + 1

    vectors = list(unit_test_vectors(args.source))
    if not vectors:
        raise SystemExit(f"no vector found in {args.source}")
    for vector in vectors:
        write("operation_parser", vector)

    samples = args.samples / "operations" / "nano" / "samples.hex"
    for operation in sample_lines(samples):
        # The samples start at the branch, the app is given the magic byte too
        write("operation_parser", MAGIC_OPERATION + operation)

    micheline_samples = args.samples / "micheline" / "nano" / "samples.hex"
    for expression in sample_lines(micheline_samples):
        write("micheline_parser", expression)
        write("operation_parser", MAGIC_MICHELINE + expression)

    for seed in MICHELINE_SEEDS:
        expression = bytes.fromhex(seed.replace(" ", ""))
        write("micheline_parser", expression)
        write("operation_parser", MAGIC_MICHELINE + expression)

    for target, seeds in (
        ("formatting", formatting_seeds),
        ("amounts", amounts_seeds),
        ("fa2_tokens", fa2_seeds),
        ("swap_key_policy", swap_key_policy_seeds),
        # The ring buffer is fuzzed in the size of each kind of device
        ("ui_strings", ui_strings_seeds),
        ("ui_strings_bagl", ui_strings_seeds),
        ("ui_strings_nanos", ui_strings_seeds),
    ):
        for seed in seeds():
            write(target, seed)

    for target, count in sorted(counts.items()):
        print(f"{count:5d} seeds for {target}")


if __name__ == "__main__":
    main()
