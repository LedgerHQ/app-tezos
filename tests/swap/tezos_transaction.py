# Copyright 2026 Ledger SAS

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Minimal Tezos operation encoding, enough to craft the transactions a swap signs."""

from hashlib import blake2b, sha256

from base58 import b58decode, b58encode

CLA = 0x80
INS_GET_PUBLIC_KEY = 0x02
INS_SIGN = 0x04
P1_FIRST = 0x00
P1_LAST = 0x81
P2_ED25519 = 0x00

MAGIC_BYTE_UNSAFE = 0x03
OPERATION_TAG_TRANSACTION = 0x6C

STATUS_OK = 0x9000
EXC_REJECT = 0x6985

# Micheline "prim with 2 arguments, no annotation" (0x07) applied to Pair (0x07)
MICHELINE_PAIR = b"\x07\x07"

TZ1_PREFIX = bytes.fromhex("06a19f")


def encode_tz1(blake2_hashed_pubkey: bytes) -> str:
    """Base58 encode a 20 byte public key hash as a tz1 address."""
    checksum = sha256(sha256(TZ1_PREFIX + blake2_hashed_pubkey).digest()).digest()[:4]
    return b58encode(TZ1_PREFIX + blake2_hashed_pubkey + checksum).decode()


def decode_tz1(address: str) -> bytes:
    """Extract the 20 byte public key hash out of a tz1 address."""
    return b58decode(address)[3:-4]


def blake2_hash_pubkey(pubkey: bytes) -> bytes:
    """Hash a public key the way Tezos derives an address from it."""
    return blake2b(pubkey, digest_size=20).digest()


def zarith(value: int) -> bytes:
    """Unsigned zarith: 7 bits per byte, the MSB being the continuation bit."""
    assert value >= 0
    if value == 0:
        return b"\x00"
    out = b""
    while value:
        byte = value & 0x7F
        value >>= 7
        out += bytes([byte | 0x80]) if value else bytes([byte])
    return out


def zarith_signed(value: int) -> bytes:
    """Signed zarith, as used by Micheline ints: bit 6 of the first byte is the sign."""
    negative = value < 0
    value = abs(value)
    first = value & 0x3F
    value >>= 6
    out = bytes([first | (0x40 if negative else 0x00) | (0x80 if value else 0x00)])
    while value:
        byte = value & 0x7F
        value >>= 7
        out += bytes([byte | 0x80]) if value else bytes([byte])
    return out


def micheline_int(value: int) -> bytes:
    return b"\x00" + zarith_signed(value)


def micheline_string(value: str) -> bytes:
    raw = value.encode()
    return b"\x01" + len(raw).to_bytes(4, "big") + raw


def micheline_sequence(payload: bytes) -> bytes:
    return b"\x02" + len(payload).to_bytes(4, "big") + payload


def fa2_transfer_parameters(source: str, destination: str, token_id: int, amount: int) -> bytes:
    """Parameters of an FA2 `transfer` entrypoint call.

    Pair(from_, [Pair(to_, Pair(token_id, amount))]), which is what Ledger Live
    sends for a token transfer.
    """
    transfer = (MICHELINE_PAIR + micheline_string(destination) + MICHELINE_PAIR +
                micheline_int(token_id) + micheline_int(amount))
    return micheline_sequence(MICHELINE_PAIR + micheline_string(source) +
                              micheline_sequence(transfer))


def _operation_header(source_hash: bytes, fees: int) -> bytes:
    """Magic byte, branch and the manager fields shared by every transaction."""
    payload = bytes([MAGIC_BYTE_UNSAFE])
    payload += bytes(32)  # branch, not checked by the application
    payload += bytes([OPERATION_TAG_TRANSACTION])
    payload += b"\x00" + source_hash  # implicit tz1 source
    payload += zarith(fees)
    payload += zarith(174728483)  # counter
    payload += zarith(3551)  # gas limit
    payload += zarith(0)  # storage limit
    return payload


def craft_native_transfer(source_hash: bytes, destination: str, amount: int, fees: int) -> bytes:
    """A plain tez transfer to an implicit account."""
    payload = _operation_header(source_hash, fees)
    payload += zarith(amount)
    payload += b"\x00\x00" + decode_tz1(destination)  # implicit tz1 destination
    payload += b"\x00"  # no parameters
    return payload


def craft_fa2_transfer(source: str, source_hash: bytes, contract_hash: bytes, destination: str,
                       token_id: int, amount: int, fees: int) -> bytes:
    """An FA2 `transfer` call on a token contract.

    The operation carries no tez, its destination is the token contract, and
    the recipient and the amount are inside the Michelson parameters.
    """
    payload = _operation_header(source_hash, fees)
    payload += zarith(0)
    payload += b"\x01" + contract_hash + b"\x00"  # originated KT1 destination
    payload += b"\xff"  # parameters present
    payload += b"\xff" + bytes([len("transfer")]) + b"transfer"
    parameters = fa2_transfer_parameters(source, destination, token_id, amount)
    payload += len(parameters).to_bytes(4, "big") + parameters
    return payload
