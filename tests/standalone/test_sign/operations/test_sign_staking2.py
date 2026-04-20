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

"""Staking 2.0 pseudo-operations (Paris+), as documented on-chain.

These are ordinary **tag-108 transactions** with built-in entrypoints
``stake`` / ``unstake`` / ``finalize_unstake`` / ``set_delegate_parameters``
(see Octez ``entrypoint_repr`` smart encoding).

**Sources (examples aligned with protocol docs):**

- Paris: https://tezos.gitlab.io/paris/staking.html
- Seoul: https://tezos.gitlab.io/seoul/staking.html (third-party ``finalize_unstake`` when source ≠ destination)
- In-repo: ``docs/PROTOCOL_ALIGNMENT_STAKING2.md``

Field-level matrix for generic transactions (including the same entrypoint
names) stays in ``test_sign_transaction.py``. This module only runs **full
sign flows** for the doc-shaped staking vectors.
"""

from typing import Optional

import pytest

from utils.account import Account
from utils.backend import TezosBackend
from utils.message import Transaction
from utils.navigator import TezosNavigator

from .helper import Flow, TestOperation, pytest_generate_tests  # pylint: disable=unused-import

# Self-transfer destination matching default manager source in tests.
_SELF = "tz1Ke2h7sDdakHJQh8WX4Z372du1KChsksyU"
# Distinct implicit for Seoul+ third-party finalize (fee payer ≠ staker).
_STAKER_OTHER = "tz1ixvCiPJYyMjsp2nKBVaq54f6AdbV8hCKa"


class TestStaking2DocExamples(TestOperation):
    """Sign flows for Staking 2.0 doc-shaped tag-108 operations."""

    def skip_signature_check(self) -> Optional[str]:
        """Same ``Transaction`` class as ``TestTransaction``; avoid duplicate hash/signature test."""
        return "Signature check for Transaction is already run in test_sign_transaction."

    @property
    def op_class(self):
        return Transaction

    # Flow names are stable: snapshot dirs are ``flow-staking2_*`` (see test_sign_transaction history).
    flows = [
        # Paris doc § pseudo-ops: stake — non-zero amount, self-transfer, Unit parameter.
        Flow(
            'staking2_stake_self',
            amount=1000000000,
            destination=_SELF,
            entrypoint='stake',
            parameter={'prim': 'Unit'},
        ),
        # unstake — positive amount, self-transfer, Unit (typical wallet encoding).
        Flow(
            'staking2_unstake_self',
            amount=500000000,
            destination=_SELF,
            entrypoint='unstake',
            parameter={'prim': 'Unit'},
        ),
        # finalize_unstake — classic self-call: zero amount, Unit.
        Flow(
            'staking2_finalize_unstake_self',
            amount=0,
            destination=_SELF,
            entrypoint='finalize_unstake',
            parameter={'prim': 'Unit'},
        ),
        # Seoul doc: third party pays fee; destination = staker implicit, zero amount, finalize_unstake.
        Flow(
            'staking2_finalize_unstake_sponsored',
            amount=0,
            destination=_STAKER_OTHER,
            entrypoint='finalize_unstake',
            parameter={'prim': 'Unit'},
        ),
        # set_delegate_parameters — zero amount self-call; Pair limits (illustrative values, Paris doc shape).
        Flow(
            'staking2_set_delegate_parameters',
            amount=0,
            destination=_SELF,
            entrypoint='set_delegate_parameters',
            parameter={'prim': 'Pair', 'args': [
                {'int': 4000000},
                {'prim': 'Pair', 'args': [
                    {'int': 20000000},
                    {'prim': 'Unit'}
                ]}
            ]},
        ),
    ]

    def test_operation_field(
            self,
            backend: TezosBackend,
            tezos_navigator: TezosNavigator,
            account: Account,
    ):
        """Omitted here: use ``TestTransaction`` for the shared field matrix."""
        pytest.skip(
            "Staking 2.0 entrypoint/amount/parameter field tests are covered by "
            "test_sign_transaction.TestTransaction."
        )
