#!/usr/bin/env python3
# Copyright 2024 Functori <contact@functori.com>
# Copyright 2024 Trilitech <contact@trili.tech>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Regression tests for Staking 2.0 wire encoding (Paris+ pseudo-operations).

See ``docs/PROTOCOL_ALIGNMENT_STAKING2.md`` §3.2 — these builtins must match
Octez ``Entrypoint_repr.smart_encoding`` so forged ops match wallets / nodes.
"""

from utils.message import Default, Transaction
from utils.tezos_encoding import forge_entrypoint


def test_builtin_entrypoint_bytes_match_protocol_table():
    """Verify built-in entrypoint bytes match Octez smart_encoding table."""
    assert forge_entrypoint("stake") == b"\x06"
    assert forge_entrypoint("unstake") == b"\x07"
    assert forge_entrypoint("finalize_unstake") == b"\x08"
    assert forge_entrypoint("set_delegate_parameters") == b"\x09"


def test_stake_entrypoint_uses_builtin_not_length_prefixed_name():
    """Paris+ ``stake`` is a single-byte smart entrypoint (6), not ``0xFF`` + ASCII name."""
    op = Transaction(
        source=Default.ED25519_PUBLIC_KEY_HASH,
        destination=Default.ED25519_PUBLIC_KEY_HASH,
        amount=1_000_000,
        entrypoint="stake",
        parameter={"prim": "Unit"},
    )
    raw = bytes(op)
    # Legacy test harness mistakenly used arbitrary name encoding for builtins 6–9.
    assert b"\xff\x05stake" not in raw
