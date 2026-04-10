"""Standalone Tezos encoding/forging helpers (pytezos-compatible)."""

from decimal import Decimal
from hashlib import blake2b
from typing import Any, Dict, List, Optional, Union

import base58

Tb = bytes

base58_encodings = [
    (b"B", 51, Tb([1, 52]), 32, "block hash"),
    (b"o", 51, Tb([5, 116]), 32, "operation hash"),
    (b"Lo", 52, Tb([133, 233]), 32, "operation list hash"),
    (b"LLo", 53, Tb([29, 159, 109]), 32, "operation list list hash"),
    (b"P", 51, Tb([2, 170]), 32, "protocol hash"),
    (b"Co", 52, Tb([79, 199]), 32, "context hash"),
    (b"tz1", 36, Tb([6, 161, 159]), 20, "ed25519 public key hash"),
    (b"tz2", 36, Tb([6, 161, 161]), 20, "secp256k1 public key hash"),
    (b"tz3", 36, Tb([6, 161, 164]), 20, "p256 public key hash"),
    (b"tz4", 36, Tb([6, 161, 166]), 20, "BLS12-381 public key hash"),
    (b"KT1", 36, Tb([2, 90, 121]), 20, "originated smart contract address"),
    (b"txr1", 37, Tb([1, 128, 120, 31]), 20, "tx_rollup_l2_address"),
    (b"sr1", 36, Tb([6, 124, 117]), 20, "originated smart rollup address"),
    (b"src1", 54, Tb([17, 165, 134, 138]), 32, "smart rollup commitment hash"),
    (b"srs1", 54, Tb([17, 165, 235, 240]), 32, "smart rollup state hash"),
    (b"srib1", 55, Tb([3, 255, 138, 145, 110]), 32, "smart rollup inbox"),
    (b"srib2", 55, Tb([3, 255, 138, 145, 140]), 32, "smart rollup merkelized payload"),
    (b"id", 30, Tb([153, 103]), 16, "cryptobox public key hash"),
    (b"expr", 54, Tb([13, 44, 64, 27]), 32, "script expression"),
    (b"edsk", 54, Tb([13, 15, 58, 7]), 32, "ed25519 seed"),
    (b"edpk", 54, Tb([13, 15, 37, 217]), 32, "ed25519 public key"),
    (b"spsk", 54, Tb([17, 162, 224, 201]), 32, "secp256k1 secret key"),
    (b"p2sk", 54, Tb([16, 81, 238, 189]), 32, "p256 secret key"),
    (b"edesk", 88, Tb([7, 90, 60, 179, 41]), 56, "ed25519 encrypted seed"),
    (b"spesk", 88, Tb([9, 237, 241, 174, 150]), 56, "secp256k1 encrypted secret key"),
    (b"p2esk", 88, Tb([9, 48, 57, 115, 171]), 56, "p256 encrypted secret key"),
    (b"sppk", 55, Tb([3, 254, 226, 86]), 33, "secp256k1 public key"),
    (b"p2pk", 55, Tb([3, 178, 139, 127]), 33, "p256 public key"),
    (b"SSp", 53, Tb([38, 248, 136]), 33, "secp256k1 scalar"),
    (b"GSp", 53, Tb([5, 92, 0]), 33, "secp256k1 element"),
    (b"edsk", 98, Tb([43, 246, 78, 7]), 64, "ed25519 secret key"),
    (b"edsig", 99, Tb([9, 245, 205, 134, 18]), 64, "ed25519 signature"),
    (b"spsig", 99, Tb([13, 115, 101, 19, 63]), 64, "secp256k1 signature"),
    (b"p2sig", 98, Tb([54, 240, 44, 52]), 64, "p256 signature"),
    (b"sig", 96, Tb([4, 130, 43]), 64, "generic signature"),
    (b"Net", 15, Tb([87, 82, 0]), 4, "chain id"),
    (b"nce", 53, Tb([69, 220, 169]), 32, "seed nonce hash"),
    (b"btz1", 37, Tb([1, 2, 49, 223]), 20, "blinded public key hash"),
    (b"vh", 52, Tb([1, 106, 242]), 32, "block_payload_hash"),
    (b"BLsig", 142, Tb([40, 171, 64, 207]), 96, "bls12_381 signature"),
    (b"BLpk", 76, Tb([6, 149, 135, 204]), 48, "bls12_381 public key"),
    (b"BLsk", 54, Tb([3, 150, 192, 40]), 32, "bls12_381 secret_key"),
    (b"BLesk", 88, Tb([2, 5, 30, 53, 25]), 56, "bls12_381 encrypted_secret_key"),
]

prim_tags = {
    "parameter": b"\x00",
    "storage": b"\x01",
    "code": b"\x02",
    "False": b"\x03",
    "Elt": b"\x04",
    "Left": b"\x05",
    "None": b"\x06",
    "Pair": b"\x07",
    "Right": b"\x08",
    "Some": b"\x09",
    "True": b"\x0a",
    "Unit": b"\x0b",
    "PACK": b"\x0c",
    "UNPACK": b"\x0d",
    "BLAKE2B": b"\x0e",
    "SHA256": b"\x0f",
    "SHA512": b"\x10",
    "ABS": b"\x11",
    "ADD": b"\x12",
    "AMOUNT": b"\x13",
    "AND": b"\x14",
    "BALANCE": b"\x15",
    "CAR": b"\x16",
    "CDR": b"\x17",
    "CHECK_SIGNATURE": b"\x18",
    "COMPARE": b"\x19",
    "CONCAT": b"\x1a",
    "CONS": b"\x1b",
    "__CREATE_ACCOUNT__": b"\x1c",
    "CREATE_CONTRACT": b"\x1d",
    "IMPLICIT_ACCOUNT": b"\x1e",
    "DIP": b"\x1f",
    "DROP": b"\x20",
    "DUP": b"\x21",
    "EDIV": b"\x22",
    "EMPTY_MAP": b"\x23",
    "EMPTY_SET": b"\x24",
    "EQ": b"\x25",
    "EXEC": b"\x26",
    "FAILWITH": b"\x27",
    "GE": b"\x28",
    "GET": b"\x29",
    "GT": b"\x2a",
    "HASH_KEY": b"\x2b",
    "IF": b"\x2c",
    "IF_CONS": b"\x2d",
    "IF_LEFT": b"\x2e",
    "IF_NONE": b"\x2f",
    "INT": b"\x30",
    "LAMBDA": b"\x31",
    "LE": b"\x32",
    "LEFT": b"\x33",
    "LOOP": b"\x34",
    "LSL": b"\x35",
    "LSR": b"\x36",
    "LT": b"\x37",
    "MAP": b"\x38",
    "MEM": b"\x39",
    "MUL": b"\x3a",
    "NEG": b"\x3b",
    "NEQ": b"\x3c",
    "NIL": b"\x3d",
    "NONE": b"\x3e",
    "NOT": b"\x3f",
    "NOW": b"\x40",
    "OR": b"\x41",
    "PAIR": b"\x42",
    "PUSH": b"\x43",
    "RIGHT": b"\x44",
    "SIZE": b"\x45",
    "SOME": b"\x46",
    "SOURCE": b"\x47",
    "SENDER": b"\x48",
    "SELF": b"\x49",
    "STEPS_TO_QUOTA": b"\x4a",
    "SUB": b"\x4b",
    "SWAP": b"\x4c",
    "TRANSFER_TOKENS": b"\x4d",
    "SET_DELEGATE": b"\x4e",
    "UNIT": b"\x4f",
    "UPDATE": b"\x50",
    "XOR": b"\x51",
    "ITER": b"\x52",
    "LOOP_LEFT": b"\x53",
    "ADDRESS": b"\x54",
    "CONTRACT": b"\x55",
    "ISNAT": b"\x56",
    "CAST": b"\x57",
    "RENAME": b"\x58",
    "bool": b"\x59",
    "contract": b"\x5a",
    "int": b"\x5b",
    "key": b"\x5c",
    "key_hash": b"\x5d",
    "lambda": b"\x5e",
    "list": b"\x5f",
    "map": b"\x60",
    "big_map": b"\x61",
    "nat": b"\x62",
    "option": b"\x63",
    "or": b"\x64",
    "pair": b"\x65",
    "set": b"\x66",
    "signature": b"\x67",
    "string": b"\x68",
    "bytes": b"\x69",
    "mutez": b"\x6a",
    "timestamp": b"\x6b",
    "unit": b"\x6c",
    "operation": b"\x6d",
    "address": b"\x6e",
    "SLICE": b"\x6f",
    "DIG": b"\x70",
    "DUG": b"\x71",
    "EMPTY_BIG_MAP": b"\x72",
    "APPLY": b"\x73",
    "chain_id": b"\x74",
    "CHAIN_ID": b"\x75",
    "LEVEL": b"\x76",
    "SELF_ADDRESS": b"\x77",
    "never": b"\x78",
    "NEVER": b"\x79",
    "UNPAIR": b"\x7a",
    "VOTING_POWER": b"\x7b",
    "TOTAL_VOTING_POWER": b"\x7c",
    "KECCAK": b"\x7d",
    "SHA3": b"\x7e",
    "PAIRING_CHECK": b"\x7f",
    "bls12_381_g1": b"\x80",
    "bls12_381_g2": b"\x81",
    "bls12_381_fr": b"\x82",
    "sapling_state": b"\x83",
    "sapling_transaction_deprecated": b"\x84",
    "SAPLING_EMPTY_STATE": b"\x85",
    "SAPLING_VERIFY_UPDATE": b"\x86",
    "ticket": b"\x87",
    "TICKET_DEPRECATED": b"\x88",
    "READ_TICKET": b"\x89",
    "SPLIT_TICKET": b"\x8a",
    "JOIN_TICKETS": b"\x8b",
    "GET_AND_UPDATE": b"\x8c",
    "chest": b"\x8d",
    "chest_key": b"\x8e",
    "OPEN_CHEST": b"\x8f",
    "VIEW": b"\x90",
    "view": b"\x91",
    "constant": b"\x92",
    "SUB_MUTEZ": b"\x93",
    "tx_rollup_l2_address": b"\x94",
    "MIN_BLOCK_TIME": b"\x95",
    "sapling_transaction": b"\x96",
    "EMIT": b"\x97",
    "Lambda_rec": b"\x98",
    "LAMBDA_REC": b"\x99",
    "TICKET": b"\x9a",
    "BYTES": b"\x9b",
    "NAT": b"\x9c",
    "Ticket": b"\x9d",
    "Stack_elt": b"\xee",
    "Big_map": b"\xee",
    "input": b"\xee",
    "output": b"\xee",
    "sender": b"\xee",
    "amount": b"\xee",
    "balance": b"\xee",
    "self": b"\xee",
    "now": b"\xee",
    "source": b"\xee",
    "big_maps": b"\xee",
    "DUMP": b"\xee",
    "PRINT": b"\xee",
    "DEBUG": b"\xee",
    "DROP_ALL": b"\xee",
    "BEGIN": b"\xee",
    "COMMIT": b"\xee",
    "RUN": b"\xee",
    "EXPAND": b"\xee",
    "PATCH": b"\xee",
    "RESET": b"\xee",
    "BIG_MAP_DIFF": b"\xee",
}

operation_tags = {
    "endorsement": 0,
    "endorsement_with_slot": 10,
    "proposals": 5,
    "ballot": 6,
    "seed_nonce_revelation": 1,
    "double_endorsement_evidence": 2,
    "double_baking_evidence": 3,
    "activate_account": 4,
    "failing_noop": 17,
    "reveal": 107,
    "transaction": 108,
    "origination": 109,
    "delegation": 110,
    "register_global_constant": 111,
    "transfer_ticket": 158,
    "smart_rollup_add_messages": 201,
    "smart_rollup_execute_outbox_message": 206,
}

reserved_entrypoints = {
    "default": b"\x00",
    "root": b"\x01",
    "do": b"\x02",
    "set_delegate": b"\x03",
    "remove_delegate": b"\x04",
    "deposit": b"\x05",
}


def scrub_input(v: Union[str, bytes]) -> bytes:
    """Normalize input to bytes."""
    if isinstance(v, bytes):
        return v
    if isinstance(v, str):
        try:
            return bytes.fromhex(v.removeprefix("0x"))
        except ValueError:
            return v.encode("ascii")
    raise TypeError(
        f"A bytes-like object is required (also str), not `{type(v).__name__}`"
    )


def blake2b_32(v: Union[str, bytes] = b""):
    """Return a 32-byte BLAKE2b digest of v."""
    return blake2b(scrub_input(v), digest_size=32)


def base58_decode(v: Union[str, bytes]) -> bytes:
    """Decode a base58check-encoded Tezos value, stripping the prefix."""
    if isinstance(v, str):
        v = v.encode()
    try:
        prefix_len = next(
            len(encoding[2])
            for encoding in base58_encodings
            if len(v) == encoding[1] and v.startswith(encoding[0])
        )
    except StopIteration as exc:
        raise ValueError("Invalid encoding, prefix or length mismatch.") from exc
    return base58.b58decode_check(v)[prefix_len:]


def base58_encode(v: bytes, prefix: bytes) -> bytes:
    """Encode bytes as a base58check Tezos value with the given prefix."""
    try:
        encoding = next(
            encoding
            for encoding in base58_encodings
            if len(v) == encoding[3] and prefix == encoding[0]
        )
    except StopIteration as exc:
        raise ValueError("Invalid encoding, prefix or length mismatch.") from exc
    return base58.b58encode_check(encoding[2] + v)


def format_mutez(value: Optional[Union[int, Decimal]]) -> str:
    """Format a mutez value as a string."""
    if value is None:
        value = 0
    elif isinstance(value, Decimal):
        value = int(value * 10**6)
    elif isinstance(value, float):
        raise ValueError("Please use decimal instead of float")
    return str(value)


def forge_int(value: int) -> bytes:
    """Encode a signed integer in Micheline binary format."""
    res = bytearray()
    i = abs(value)
    res.append((i & 0b00111111) | (0b11000000 if value < 0 else 0b10000000))
    i >>= 6
    while i != 0:
        res.append((i & 0b01111111) | 0b10000000)
        i >>= 7
    res[-1] &= 0b01111111
    return bytes(res)


def forge_nat(value: int) -> bytes:
    """Encode a natural number in Micheline binary format."""
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


def forge_int_fixed(value: int, length: int) -> bytes:
    """Encode an integer as a fixed-length big-endian byte string."""
    return value.to_bytes(length, "big")


def forge_int16(value: int) -> bytes:
    """Encode an integer as a 2-byte big-endian value."""
    return value.to_bytes(2, "big")


def forge_int32(value: int) -> bytes:
    """Encode an integer as a 4-byte big-endian value."""
    return value.to_bytes(4, "big")


def forge_bool(value: bool) -> bytes:
    """Encode a boolean as a single byte."""
    return b"\xff" if value else b"\x00"


def forge_base58(value: str) -> bytes:
    """Decode a base58check-encoded value to raw bytes."""
    return base58_decode(value)


def forge_array(data: bytes, len_bytes: int = 4) -> bytes:
    """Prefix data with its length encoded as a big-endian integer."""
    return len(data).to_bytes(len_bytes, "big") + data


def forge_tag(operation_tag: int) -> bytes:
    """Encode an operation tag as a single byte."""
    return operation_tag.to_bytes(1, "big")


def get_tag(args_len: int, annots_len: int) -> bytes:
    """Compute the Micheline primitive tag byte."""
    tag = min(args_len * 2 + 3 + (1 if annots_len > 0 else 0), 9)
    return bytes([tag])


def forge_address(value: str, tz_only: bool = False) -> bytes:
    """Encode a Tezos address to its binary representation."""
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


def forge_public_key(value: str) -> bytes:
    """Encode a Tezos public key to its binary representation."""
    prefix = value[:4]
    res = base58.b58decode_check(value)[4:]
    if prefix == "edpk":
        return b"\x00" + res
    if prefix == "sppk":
        return b"\x01" + res
    if prefix == "p2pk":
        return b"\x02" + res
    if prefix == "BLpk":
        return b"\x03" + res
    raise ValueError(f"Unrecognized key type: #{prefix}")


def forge_micheline(data: Union[List, Dict]) -> bytes:  # pylint: disable=too-many-branches
    """Encode a Micheline expression to its binary representation."""
    res = []
    if isinstance(data, list):
        res.append(b"\x02")
        res.append(forge_array(b"".join(map(forge_micheline, data))))
    elif isinstance(data, dict):
        if data.get("prim"):
            args_len = len(data.get("args", []))
            annots_len = len(data.get("annots", []))
            res.append(get_tag(args_len, annots_len))
            res.append(prim_tags[data["prim"]])

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
        elif data.get("bytes") is not None:
            res.append(b"\x0a")
            res.append(forge_array(bytes.fromhex(data["bytes"])))
        elif data.get("int") is not None:
            res.append(b"\x00")
            res.append(forge_int(int(data["int"])))
        elif data.get("string") is not None:
            res.append(b"\x01")
            res.append(forge_array(data["string"].encode()))
        else:
            raise AssertionError(data)
    else:
        raise AssertionError(data)
    return b"".join(res)


def forge_script(script: Dict[str, Any]) -> bytes:
    """Encode a Tezos script (code + storage) to its binary representation."""
    code = forge_micheline(script["code"])
    storage = forge_micheline(script["storage"])
    return forge_array(code) + forge_array(storage)


def has_parameters(content: Dict[str, Any]) -> bool:
    """Return True if the operation content has non-default parameters."""
    if not content.get("parameters"):
        return False
    return not (
        content["parameters"]["entrypoint"] == "default"
        and content["parameters"]["value"] == {"prim": "Unit"}
    )


def forge_entrypoint(entrypoint: str) -> bytes:
    """Encode a contract entrypoint name to its binary representation."""
    if entrypoint in reserved_entrypoints:
        return reserved_entrypoints[entrypoint]
    return b"\xff" + forge_array(entrypoint.encode(), len_bytes=1)
