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

"""The swap cases of the Exchange application, with native tez.

These are the cases the Exchange application runs for the Tezos application,
ported from test/python/test_tezos.py of https://github.com/LedgerHQ/app-exchange-dev
along with their snapshots, so that they run here and no longer only there.
They go through the Exchange flows - swap, sell and fund - with a valid
transaction, and with a wrong amount, destination or fee, which the Tezos
application has to refuse.

The final transaction is the one the Exchange application tests sign: a reveal
of the key, then a transfer, see craft_revealed_native_transfer(). The cases
that carry a memo or go through THORSwap are left out, as Tezos supports
neither.

test_swap_fa2.py has the cases of this repository: a token, a control with a
single transaction, and the tampering of the signing request.
"""

import pytest
from ledger_app_clients.exchange.test_runner import ALL_TESTS_EXCEPT_MEMO_AND_THORSWAP, ExchangeTestRunner

from . import cal_helper as cal
from .tezos_transaction import (
    CLA,
    EXC_REJECT,
    INS_GET_PUBLIC_KEY,
    INS_SIGN,
    P1_FIRST,
    P1_LAST,
    P2_ED25519,
    STATUS_OK,
    craft_revealed_native_transfer,
    encode_tz1,
)


class TezosExchangeTests(ExchangeTestRunner):
    """The Exchange application tests of Tezos."""

    currency_configuration = cal.XTZ_CURRENCY_CONFIGURATION
    valid_destination_1 = encode_tz1(bytes.fromhex("e6330795ffe18f873b83cb13662442b87bd98c22"))
    valid_destination_2 = encode_tz1(bytes.fromhex("e6330795ffe18f873b83cb13662442b87bd98c40"))
    valid_refund = "tz1YPjCVqgimTAPmxZX9egDeTFRCmrTRqmp9"
    valid_send_amount_1 = 10000000
    valid_send_amount_2 = 446739662
    valid_fees_1 = 100
    valid_fees_2 = 10078
    fake_refund = "abcdabcd"
    fake_payout = "abcdabcd"
    signature_refusal_error_code = EXC_REJECT

    def perform_final_tx(self, destination, send_amount, fees, memo):
        """Sign a reveal then a transfer of `send_amount` mutez to `destination`.

        A non-9000 answer raises, so the cases that tamper with the transaction
        can expect the refusal. Tezos has no memo, so `memo` is not used.
        """
        # The device public key is part of the reveal.
        rapdu = self.backend.exchange(CLA, INS_GET_PUBLIC_KEY, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
        assert rapdu.status == STATUS_OK
        public_key = rapdu.data[2:]

        # Init a signature on the derivation path, then send the operations.
        rapdu = self.backend.exchange(CLA, INS_SIGN, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
        assert rapdu.status == STATUS_OK
        payload = craft_revealed_native_transfer(public_key, destination, send_amount, fees)
        self.backend.exchange(CLA, INS_SIGN, P1_LAST, P2_ED25519, data=payload)


# Use a class to reuse the same Speculos instance
class TestsTezos:
    @pytest.mark.parametrize("test_to_run", ALL_TESTS_EXCEPT_MEMO_AND_THORSWAP)
    def test_tezos(self, backend, exchange_navigation_helper, test_to_run):
        TezosExchangeTests(backend, exchange_navigation_helper).run_test(test_to_run)
