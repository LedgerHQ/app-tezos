#!/usr/bin/env python3
# Copyright 2026 Trilitech <contact@trili.tech>
#
# Standalone script to generate hex strings for FA2-related C unit tests in
# tests/unit/ctest/tests_parser.c. Uses only the standard library plus base58.
#
# Usage: python3 scripts/gen_fa2_ctest_hex.py
#
# Requires: pip install base58

from __future__ import annotations

import base58

# --- Minimal Tezos operation / Micheline forging (subset of pytezos) ---

operation_tags = {"transaction": 108}


def forge_tag(operation_tag: int) -> bytes:
    return operation_tag.to_bytes(1, "big")


def forge_array(data: bytes, len_bytes: int = 4) -> bytes:
    return len(data).to_bytes(len_bytes, "big") + data


def forge_nat(value: int) -> bytes:
    if value < 0:
        raise ValueError("Value cannot be negative.")
    buf = bytearray()
    more = True
    while more:
        byte = value & 0x7F
        value >>= 7
        if value:
            byte |= 0x80
        else:
            more = False
        buf.append(byte)
    return bytes(buf)


def forge_int(value: int) -> bytes:
    res = bytearray()
    i = abs(value)
    res.append((i & 0b00111111) | (0b11000000 if value < 0 else 0b10000000))
    i >>= 6
    while i != 0:
        res.append((i & 0b01111111) | 0b10000000)
        i >>= 7
    res[-1] &= 0b01111111
    return bytes(res)


def forge_address(value: str, tz_only: bool = False) -> bytes:
    prefix_len = 4 if value.startswith("txr1") else 3
    prefix = value[:prefix_len]
    address = base58.b58decode_check(value)[prefix_len:]
    if prefix == "tz1":
        res = b"\x00\x00" + address
    elif prefix == "tz2":
        res = b"\x00\x01" + address
    elif prefix == "tz3":
        res = b"\x00\x02" + address
    elif prefix == "tz4":
        res = b"\x00\x03" + address
    elif prefix == "KT1":
        res = b"\x01" + address + b"\x00"
    elif prefix == "txr1":
        res = b"\x02" + address + b"\x00"
    elif prefix == "sr1":
        res = b"\x03" + address + b"\x00"
    else:
        raise ValueError(f"Can't forge address: unknown prefix `{prefix}`")
    return res[1:] if tz_only else res


def get_tag(args_len: int, annots_len: int) -> bytes:
    tag = min(args_len * 2 + 3 + (1 if annots_len > 0 else 0), 9)
    return bytes([tag])


prim_tags = {"Pair": b"\x07"}


def forge_micheline(data) -> bytes:
    if isinstance(data, list):
        return b"\x02" + forge_array(b"".join(map(forge_micheline, data)))
    if isinstance(data, dict):
        if data.get("prim"):
            args_len = len(data.get("args", []))
            annots_len = len(data.get("annots", []))
            res = [get_tag(args_len, annots_len), prim_tags[data["prim"]]]
            if args_len > 0:
                args = b"".join(map(forge_micheline, data["args"]))
                if args_len < 3:
                    res.append(args)
                else:
                    res.append(forge_array(args))
            if annots_len > 0:
                res.append(forge_array(" ".join(data["annots"]).encode()))
            elif args_len >= 3:
                res.append(b"\x00" * 4)
            return b"".join(res)
        if data.get("bytes") is not None:
            return b"\x0a" + forge_array(bytes.fromhex(data["bytes"]))
        if data.get("int") is not None:
            return b"\x00" + forge_int(int(data["int"]))
        if data.get("string") is not None:
            return b"\x01" + forge_array(data["string"].encode())
        raise AssertionError(data)
    raise AssertionError(data)


reserved_entrypoints = {
    "default": b"\x00",
    "root": b"\x01",
    "do": b"\x02",
    "set_delegate": b"\x03",
    "remove_delegate": b"\x04",
    "deposit": b"\x05",
}


def forge_entrypoint(entrypoint: str) -> bytes:
    if entrypoint in reserved_entrypoints:
        return reserved_entrypoints[entrypoint]
    return b"\xff" + forge_array(entrypoint.encode(), len_bytes=1)


def has_parameters(content: dict) -> bool:
    if not content.get("parameters"):
        return False
    p = content["parameters"]
    return not (p["entrypoint"] == "default" and p["value"] == {"prim": "Unit"})


def forge_transaction(content: dict) -> bytes:
    res = forge_tag(operation_tags[content["kind"]])
    res += forge_address(content["source"], tz_only=True)
    res += forge_nat(int(content["fee"]))
    res += forge_nat(int(content["counter"]))
    res += forge_nat(int(content["gas_limit"]))
    res += forge_nat(int(content["storage_limit"]))
    res += forge_nat(int(content["amount"]))
    res += forge_address(content["destination"])
    if has_parameters(content):
        res += b"\x01"
        res += forge_entrypoint(content["parameters"]["entrypoint"])
        res += forge_array(forge_micheline(content["parameters"]["value"]))
    else:
        res += b"\x00"
    return res


# Same fixtures as tests/standalone/test_sign/operations/test_sign_transaction.py
_FA2_TRANSFER_PARAMETER = [
    {
        "prim": "Pair",
        "args": [
            {"string": "KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW"},
            [
                {
                    "prim": "Pair",
                    "args": [
                        {"string": "sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh"},
                        {
                            "prim": "Pair",
                            "args": [{"int": 0}, {"int": 200000}],
                        },
                    ],
                }
            ],
        ],
    }
]

_FA2_TRANSFER_FALLBACK_PARAMETER = [
    {
        "prim": "Pair",
        "args": [
            {"string": "KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW"},
            [
                {
                    "prim": "Pair",
                    "args": [
                        {"string": "sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh"},
                        {
                            "prim": "Pair",
                            "args": [{"int": 1}, {"int": 100}],
                        },
                    ],
                }
            ],
        ],
    }
]

_FA2_MULTI = [
    {
        "prim": "Pair",
        "args": [
            {"string": "KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW"},
            [
                {
                    "prim": "Pair",
                    "args": [
                        {"string": "sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh"},
                        {"prim": "Pair", "args": [{"int": 0}, {"int": 100}]},
                    ],
                },
                {
                    "prim": "Pair",
                    "args": [
                        {"string": "sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh"},
                        {"prim": "Pair", "args": [{"int": 0}, {"int": 200}]},
                    ],
                },
            ],
        ],
    }
]

_FA2_NEG = [
    {
        "prim": "Pair",
        "args": [
            {"string": "KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW"},
            [
                {
                    "prim": "Pair",
                    "args": [
                        {"string": "sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh"},
                        {"prim": "Pair", "args": [{"int": 0}, {"int": -1}]},
                    ],
                }
            ],
        ],
    }
]

_FA2_TRANSFER_TOKEN_ID_GT0_PARAMETER = [
    {
        "prim": "Pair",
        "args": [
            {"string": "KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW"},
            [
                {
                    "prim": "Pair",
                    "args": [
                        {"string": "sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh"},
                        {
                            "prim": "Pair",
                            "args": [{"int": 1}, {"int": 500000}],
                        },
                    ],
                }
            ],
        ],
    }
]

_DEFAULT_TX = {
    "kind": "transaction",
    "source": "tz1ixvCiPJYyMjsp2nKBVaq54f6AdbV8hCKa",
    "fee": 1000,
    "counter": 1,
    "gas_limit": 100,
    "storage_limit": 100,
    "amount": 0,
    "destination": "KT193D4vozYnhGJQVtw7CoxxqphqUEEwK6Vb",
    "parameters": {
        "entrypoint": "transfer",
        "value": _FA2_TRANSFER_PARAMETER,
    },
}


def forge_batch_hex(parameter_value: list, destination: str | None = None) -> str:
    content = dict(_DEFAULT_TX)
    if destination is not None:
        content["destination"] = destination
    content["parameters"] = {"entrypoint": "transfer", "value": parameter_value}
    body = forge_transaction(content)
    batch = b"\x03" + bytes(32) + body
    return batch.hex()


def main() -> None:
    print("/* FA2 clear-signing (token_id=0, registry contract): */")
    print(f'    "{forge_batch_hex(_FA2_TRANSFER_PARAMETER)}"')
    print()
    print("/* FA2 fallback (token_id != 0): */")
    print(f'    "{forge_batch_hex(_FA2_TRANSFER_FALLBACK_PARAMETER)}"')
    print()
    print("/* FA2 multi-item txs (fallback): */")
    print(f'    "{forge_batch_hex(_FA2_MULTI)}"')
    print()
    print("/* FA2 negative amount (fallback): */")
    print(f'    "{forge_batch_hex(_FA2_NEG)}"')
    print()
    print("/* FA2 clear-signing (token_id=1, wrapped token registry): */")
    print(
        f'    "{forge_batch_hex(_FA2_TRANSFER_TOKEN_ID_GT0_PARAMETER, "KT18fp5rcTW7mbWDmzFwjLDUhs5MeJmagDSZ")}"'
    )


if __name__ == "__main__":
    main()
