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

"""Replay the signing APDUs of a swap captured from a device.

Nothing here builds an APDU. The requests come out of
logs/apdu_20260904_105509.log, recorded from a device swapping 31 USDt, and are
sent exactly as recorded. The swap parameters below are the ones of that same
capture, so the application compares the replayed bytes against the values it
compared them against on the device.

Only the requests addressed to the Tezos application are replayable. The
Exchange leg of the capture cannot be: its partner credentials are signed with
Ledger's production key, and the transaction proposal embeds the device
transaction id of that session, which Exchange mints anew on every run and
which is covered by the partner's signature. So the swap itself is set up
through the Exchange client, and the device review is navigated and captured
the way the other tests in this suite do it.
"""

import re
from pathlib import Path

import pytest
from ragger.backend import RaisePolicy

from . import cal_helper as cal
from .test_swap_fa2 import TezosFa2Tests

LOG = Path(__file__).parent / "logs" / "apdu_20260904_105509.log"

# The Tezos application answers CLA 0x80. Everything else in the capture is
# addressed to Exchange or to the dashboard.
TEZOS_CLA = "80"
# Of those, the ones that sign: the derivation path, then the operation.
SIGN_INS = "04"

STATUS_OK = 0x9000

# Values decoded from the same capture, see apdu_comparison.md:
#   payin address of the swap, i.e. the `to_` of the FA2 transfer
CAPTURED_PAYIN_ADDRESS = "tz1fvBBFB8tcCj1MqtuqSRkVJbf4ZLQob9GY"
#   amount_to_provider, and the amount in the Michelson parameters
CAPTURED_TOKEN_AMOUNT = 31000000
#   the fee of the swap, which the reveal and the transaction share
CAPTURED_TOTAL_FEE = 4000


def read_capture(path=LOG):
    """Read the capture into (request, status word) pairs, in order."""
    exchanges = []
    request = None
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        direction, _, payload = line.partition(" ")
        payload = payload.strip().lower()
        assert re.fullmatch(r"[0-9a-f]*", payload), f"not hex: {payload}"
        if direction == "=>":
            assert request is None, f"two requests in a row, second is {payload}"
            request = payload
        elif direction == "<=":
            assert request is not None, f"an answer without a request: {payload}"
            exchanges.append((request, int(payload[-4:], 16)))
            request = None
    assert request is None, "the capture ends on a request with no answer"
    return exchanges


def signing_requests(path=LOG):
    """The signing requests of the capture, as recorded."""
    requests = [(bytes.fromhex(req), sw) for req, sw in read_capture(path) if req[0:2] == TEZOS_CLA and req[2:4] == SIGN_INS]
    assert requests, "the capture holds no signing request"
    return requests


class TezosReplayTests(TezosFa2Tests):
    """Replay the captured signing APDUs against the captured swap.

    The refund address is the one thing that cannot come from the capture:
    Exchange has the application derive it and compare, so it has to be the
    address of the Speculos seed, while the `from_` inside the captured
    operation is the address of the captured device. The application does not
    check the source of an operation against the signing key, so this does not
    change what is being replayed.
    """

    valid_destination_1 = CAPTURED_PAYIN_ADDRESS
    valid_send_amount_1 = CAPTURED_TOKEN_AMOUNT
    valid_fees_1 = CAPTURED_TOTAL_FEE

    # The partner of the captured swap, so the review reads as it did there.
    partner_name = "Changelly"

    # What the last replayed request is expected to answer.
    expected_final_status = STATUS_OK

    def perform_final_tx(self, destination, send_amount, fees, memo):
        """Send the captured signing requests, unmodified."""
        assert destination == CAPTURED_PAYIN_ADDRESS
        assert send_amount == CAPTURED_TOKEN_AMOUNT
        assert fees == CAPTURED_TOTAL_FEE

        requests = signing_requests()
        for index, (apdu, captured_status) in enumerate(requests):
            last = index == len(requests) - 1
            expected = self.expected_final_status if last else STATUS_OK

            self.backend.raise_policy = RaisePolicy.RAISE_NOTHING
            try:
                rapdu = self.backend.exchange_raw(apdu)
            finally:
                self.backend.raise_policy = RaisePolicy.RAISE_ALL_BUT_0x9000

            assert rapdu.status == expected, (
                f"request {index + 1} of {len(requests)} diverges.\n"
                f"  sent      {apdu.hex()}\n"
                f"  expected  0x{expected:04x}\n"
                f"  answered  0x{rapdu.status:04x}\n"
                f"  the device in the capture answered 0x{captured_status:04x}"
            )

    def perform_test_replay(self):
        """The captured operation must be signed once the token is identified."""
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.assert_exchange_is_started()


class TezosReplayAsCapturedTests(TezosReplayTests):
    """The capture as it happened, coin configuration included.

    The CAL sends no sub-coin configuration for USDt on Tezos, so the
    application is told neither the ticker nor the decimals of the token and
    cannot check that the operation moves the token the swap was quoted for.
    Refusing is the only safe answer, and this reproduces the 0x6985 the device
    answered.
    """

    currency_configuration = cal.USDT_NO_TOKEN_CONFIGURATION
    expected_final_status = 0x6985

    def perform_test_replay_as_captured(self):
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.assert_exchange_is_started()


# Use a class to reuse the same Speculos instance
class TestsTezosReplay:
    @pytest.mark.parametrize("test_to_run", ["replay"])
    def test_replay(self, backend, exchange_navigation_helper, test_to_run):
        """LIVE-36514: the captured operation must be signed."""
        TezosReplayTests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["replay_as_captured"])
    def test_replay_as_captured(self, backend, exchange_navigation_helper, test_to_run):
        """The captured refusal, reproduced down to the status word."""
        TezosReplayAsCapturedTests(backend, exchange_navigation_helper).run_test(test_to_run)
