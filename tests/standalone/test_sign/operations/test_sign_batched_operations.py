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

"""Gathering of tests related to Batched operations."""

from typing import ClassVar

from utils.message import (
    Delegation,
    IncreasePaidStorage,
    OperationGroup,
    Origination,
    RegisterGlobalConstant,
    Reveal,
    ScRollupAddMessage,
    ScRollupExecuteOutboxMessage,
    ScRollupOriginate,
    SetDepositLimit,
    Transaction,
    TransferTicket,
    UpdateCompanionKey,
    UpdateConsensusKey,
)

from .helper import Field, Flow, TestOperation


class TestManagerOperation(TestOperation):
    """Common tests."""

    @property
    def op_class(self):
        return Transaction  # A Manager operation

    def skip_signature_check(self):
        return "no generic ManagerOperation"

    fields: ClassVar[list[Field]] = [
        Field(
            "source",
            "Source",
            [
                Field.Case("tz1ixvCiPJYyMjsp2nKBVaq54f6AdbV8hCKa", "tz1"),
                Field.Case("tz2CJBeWWLsUDjVUDqGZL6od3DeBCNzYXrXk", "tz2"),
                Field.Case("tz3fLwHKthqhTPK6Lar6CTXN1WbDETw1YpGB", "tz3"),
                Field.Case("tz4AcerThk5nGtWNBiSqJfZFeWtz6ZqJ6mTY", "tz4"),
                Field.Case("tz1Kp8NCAN5WWwvkWkMmQQXMRe68iURmoQ8w", "long-hash"),
            ],
        ),
        Field(
            "fee",
            "Fee",
            [
                Field.Case(0, "0"),
                Field.Case(1000, "1000"),
                Field.Case(1000000, "1000000"),
                Field.Case(1000000000, "1000000000"),
                Field.Case(0xFFFFFFFFFFFFFFFF, "max"),  # max uint64
            ],
        ),
        Field(
            "storage_limit",
            "Storage limit",
            [
                Field.Case(0, "min"),
                Field.Case(0xFFFFFFFFFFFFFFFFFFFF, "max"),
            ],
        ),
    ]


class TestOperationGroup(TestOperation):
    """Common tests."""

    @property
    def op_class(self):
        return OperationGroup

    def skip_signature_check(self):
        return "no empty OperationGroup"

    flows: ClassVar[list[Flow]] = [
        Flow(
            "many-transactions",
            operations=[
                Transaction(),
                Transaction(),
                Transaction(),
            ],
        ),
        Flow(
            "reveal-transaction",
            operations=[
                Reveal(),
                Transaction(),
            ],
        ),
        Flow(
            "all",
            operations=[
                Reveal(),
                Transaction(),
                Origination(),
                Delegation(),
                RegisterGlobalConstant(),
                SetDepositLimit(),
                IncreasePaidStorage(),
                UpdateConsensusKey(),
                UpdateCompanionKey(),
                TransferTicket(),
                ScRollupOriginate(),
                ScRollupAddMessage(),
                ScRollupExecuteOutboxMessage(),
            ],
        ),
    ]
