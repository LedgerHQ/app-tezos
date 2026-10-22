#!/usr/bin/env python3
"""Write the operation vectors of the unit tests as binary fuzzing seeds.

The vectors of unit-tests/test_operation_parser.c are hex strings, some of
them built from #define'd prefixes. They are extracted from the C source, so
the seeds follow the tests.
"""

import argparse
import hashlib
import re
from pathlib import Path

DEFAULT_SOURCE = Path(__file__).resolve().parent.parent / "unit-tests" / "test_operation_parser.c"

# A C string literal (without escapes) or an identifier
TOKEN = re.compile(r'"([^"\\]*)"|([A-Za-z_]\w*)')
COMMENT_OR_STRING = re.compile(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*', re.DOTALL)
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+([^\n]*)", re.MULTILINE)
VECTOR = re.compile(r"\bchar\s+str\[\]\s*=\s*([^;]*);")


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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out_dir", type=Path, help="directory to write the seeds to")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="C file holding the vectors")
    args = parser.parse_args()

    source = strip_comments(args.source.read_text()).replace("\\\n", " ")

    macros: dict[str, str] = {}
    for name, body in DEFINE.findall(source):
        value = concat(body, macros)
        if value is not None:
            macros[name] = value

    args.out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for expr in VECTOR.findall(source):
        value = concat(expr, macros)
        if value is None:
            continue
        try:
            data = bytes.fromhex(value)
        except ValueError:
            continue
        (args.out_dir / hashlib.sha1(data, usedforsecurity=False).hexdigest()).write_bytes(data)
        count += 1

    if count == 0:
        raise SystemExit(f"no vector found in {args.source}")
    print(f"{count} seeds written to {args.out_dir}")


if __name__ == "__main__":
    main()
