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

"""Gathering of tests related to Transaction operations.

Staking 2.0 doc-shaped **sign flows** live in ``test_sign_staking2.py``.
"""

from utils.message import Transaction
from .helper import Flow, Field, TestOperation, pytest_generate_tests  # pylint: disable=unused-import

# FA2 transfer parameter: single item, token_id=0 (clear-signing supported).
# Inner list(pair(uint64,uint64)) as direct Pair (common wallet encoding, e.g. Temple).
_FA2_TRANSFER_PARAMETER = [
    {'prim': 'Pair', 'args': [
        {'string': 'KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW'},
        [{'prim': 'Pair', 'args': [
            {'string': 'sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh'},
            {'prim': 'Pair', 'args': [
                {'int': 0},
                {'int': 200000}
            ]}
        ]}]
    ]}
]

# Same as above but inner txs item uses SEQ [ Pair(...) ] (alternative encoding).
_FA2_TRANSFER_PARAMETER_SEQ_WRAPPED = [
    {'prim': 'Pair', 'args': [
        {'string': 'KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW'},
        [{'prim': 'Pair', 'args': [
            {'string': 'sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh'},
            [{'prim': 'Pair', 'args': [
                {'int': 0},
                {'int': 200000}
            ]}]
        ]}]
    ]}
]

# FA2 transfer parameter: token_id!=0 (clear-signing supported when registered).
_FA2_TRANSFER_PARAMETER_TOKEN_ID_1 = [
    {'prim': 'Pair', 'args': [
        {'string': 'KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW'},
        [{'prim': 'Pair', 'args': [
            {'string': 'sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh'},
            {'prim': 'Pair', 'args': [
                {'int': 1},
                {'int': 100}
            ]}
        ]}]
    ]}
]

# FA2 transfer parameter: token_id not in registry (fallback to generic display).
_FA2_TRANSFER_UNREGISTERED_TOKEN_ID_PARAMETER = [
    {'prim': 'Pair', 'args': [
        {'string': 'KT1QWdbASvaTXW8GWfhfNh3JMjgXvnZAATJW'},
        [{'prim': 'Pair', 'args': [
            {'string': 'sr1MyCwR83hZphCSqaYSQApPxPMeyksJWWnh'},
            {'prim': 'Pair', 'args': [
                {'int': 42},
                {'int': 100}
            ]}
        ]}]
    ]}
]

_FA2_KT1_DESTINATION = 'KT18fp5rcTW7mbWDmzFwjLDUhs5MeJmagDSZ'
_FA2_KT1_TOKEN_ID_0_DESTINATION = 'KT193D4vozYnhGJQVtw7CoxxqphqUEEwK6Vb'


class TestTransaction(TestOperation):
    """Common tests."""

    @property
    def op_class(self):
        return Transaction

    flows = [
        Flow('basic', amount=5),
        Flow(
            'contract_call',
            destination=_FA2_KT1_TOKEN_ID_0_DESTINATION,
            entrypoint="transfer",
            parameter=_FA2_TRANSFER_PARAMETER,
        ),
        Flow(
            'contract_call_fa2_fallback',
            destination=_FA2_KT1_DESTINATION,
            entrypoint="transfer",
            parameter=_FA2_TRANSFER_UNREGISTERED_TOKEN_ID_PARAMETER,
        ),
        Flow(
            'contract_call_fa2_token_id_not_0',
            destination=_FA2_KT1_DESTINATION,
            entrypoint="transfer",
            parameter=_FA2_TRANSFER_PARAMETER_TOKEN_ID_1,
        ),
    ]

    fields = [
        Field("amount", "Amount", [
            Field.Case(0, "0"),
            Field.Case(1000, "1000"),
            Field.Case(1000000, "1000000"),
            Field.Case(1000000000, "1000000000"),
            Field.Case(0xFFFFFFFFFFFFFFFF, "max"),  # max uint64
        ]),
        Field("destination", "Destination", [
            Field.Case('tz1ixvCiPJYyMjsp2nKBVaq54f6AdbV8hCKa', "tz1"),
            Field.Case('tz2CJBeWWLsUDjVUDqGZL6od3DeBCNzYXrXk', "tz2"),
            Field.Case('tz3fLwHKthqhTPK6Lar6CTXN1WbDETw1YpGB', "tz3"),
            Field.Case('tz4DNQhMQaU9WMCVGwH6mQGGWqMNQHTjywDe', "tz4"),
            Field.Case('KT18amZmM5W7qDWVt2pH6uj7sCEd3kbzLrHT', "kt1"),
            Field.Case('tz1Kp8NCAN5WWwvkWkMmQQXMRe68iURmoQ8w', "long-hash"),
        ]),
        Field("entrypoint", "Entrypoint", [
            # `parameter` is set to make sure `entrypoint` is displayed with `default`
            Field.Case('default', "default", parameter={'prim': 'None'}),
            Field.Case('root', "root"),
            Field.Case('do', "do"),
            Field.Case('set_delegate', "set_delegate"),
            Field.Case('remove_delegate', "remove_delegate"),
            Field.Case('deposit', "deposit"),
            Field.Case('stake', "stake"),
            Field.Case('unstake', "unstake"),
            Field.Case('finalize_unstake', "finalize_unstake"),
            Field.Case('set_delegate_parameters', "set_delegate_parameters"),
            Field.Case('custom_entrypoint', "custom_entrypoint"),
        ]),
        Field("parameter", "Parameter", [
            # `entrypoint` is set to make sure `parameter` is displayed with `unit`
            Field.Case({'prim': 'Unit'}, "unit", entrypoint='entrypoint'),
            Field.Case({'prim': 'Pair', 'args': [{'string': 'a'}, {'int': 1}]}, "basic"),
            # More test about Micheline in micheline tests
        ]),
        Field("parameter", "Transfer tokens to", [
            Field.Case(
                _FA2_TRANSFER_PARAMETER,
                "fa2_transfer_to",
                entrypoint='transfer',
                destination=_FA2_KT1_TOKEN_ID_0_DESTINATION,
            ),
            Field.Case(
                _FA2_TRANSFER_PARAMETER_SEQ_WRAPPED,
                "fa2_transfer_to_seq_wrapped",
                entrypoint='transfer',
                destination=_FA2_KT1_TOKEN_ID_0_DESTINATION,
            ),
            Field.Case(
                _FA2_TRANSFER_PARAMETER_TOKEN_ID_1,
                "fa2_transfer_to_token_id_1",
                entrypoint='transfer',
                destination=_FA2_KT1_DESTINATION,
            ),
        ]),
        Field("parameter", "Token Amount", [
            Field.Case(
                _FA2_TRANSFER_PARAMETER,
                "fa2_amount",
                entrypoint='transfer',
                destination=_FA2_KT1_TOKEN_ID_0_DESTINATION,
            ),
            Field.Case(
                _FA2_TRANSFER_PARAMETER_SEQ_WRAPPED,
                "fa2_amount_seq_wrapped",
                entrypoint='transfer',
                destination=_FA2_KT1_TOKEN_ID_0_DESTINATION,
            ),
            Field.Case(
                _FA2_TRANSFER_PARAMETER_TOKEN_ID_1,
                "fa2_amount_token_id_1",
                entrypoint='transfer',
                destination=_FA2_KT1_DESTINATION,
            ),
        ]),
    ]
