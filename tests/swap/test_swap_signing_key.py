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

"""Which key a swap may be signed with.

A swap is signed without any review on the device, and the key that signs is
chosen by the host in the first signing APDU. The application therefore only
signs a swap with an ed25519 key on a Tezos account path, and only an operation
whose source is that very key. These cases tamper with the signing request, one
way each, and expect the swap to be refused and Exchange to be started again.

Three are refused when the key is set up, answering 0x6A80: another curve, a path
of another coin, and a path with an unhardened component. The last one is refused
when the operation is checked, answering 0x6985: the operation names another
account of the same seed as its source.

The cases that must still be signed are covered elsewhere: every test in
test_swap_fa2.py signs with the three component path Exchange uses, and
test_replay_device_log.py with the four component path Ledger Live signs with.

What this does not cover is the account itself. Exchange does not tell the
application which account the user approved, so any account of the seed, with a
matching operation, is still signed. Closing that needs Exchange to provide it.

The snapshots of these cases do not exist until they are generated: run them
once with --golden_run on each device, see README.md.
"""

import pytest
from ragger.bip import pack_derivation_path
from ragger.error import ExceptionRAPDU

from . import cal_helper as cal
from .test_swap_fa2 import TezosNativeSwapTests
from .tezos_transaction import (
    CLA,
    EXC_REJECT,
    EXC_WRONG_VALUES,
    INS_SIGN,
    P1_FIRST,
    P1_LAST,
    P2_ED25519,
    P2_SECP256K1,
    STATUS_OK,
    SWAP_EC_ERROR_GENERIC,
    SWAP_ERR_KEY,
    SWAP_ERR_SOURCE,
    craft_native_transfer,
    swap_error,
)


class TezosSigningKeyTests(TezosNativeSwapTests):
    """A native tez swap whose signing request is tampered with.

    The attributes below are what the signing request carries. They default to
    what the swap flow itself uses, and each case changes one of them.
    """

    # The derivation path and the curve sent in the first signing APDU.
    sign_path = cal.XTZ_PACKED_DERIVATION_PATH
    sign_p2 = P2_ED25519
    # The key the operation names as its source.
    source_path = cal.XTZ_PACKED_DERIVATION_PATH

    def perform_final_tx(self, destination, send_amount, fees, memo):
        """Sign a transfer matching the swap, with the key and source set on the case."""
        _, source_hash = self._get_device_address(self.source_path)
        rapdu = self.backend.exchange(CLA, INS_SIGN, P1_FIRST, self.sign_p2, self.sign_path)
        assert rapdu.status == STATUS_OK
        payload = craft_native_transfer(source_hash, destination, send_amount, fees)
        self.backend.exchange(CLA, INS_SIGN, P1_LAST, self.sign_p2, data=payload)

    def _swap_then_sign_refused(self, status, error):
        """Run the swap and expect the signature to be refused with `status`.

        `error` is the data of the answer, which tells which check refused.
        """
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        with pytest.raises(ExceptionRAPDU) as e:
            self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        assert e.value.status == status
        assert e.value.data == error
        self.assert_exchange_is_started()

    def perform_test_swap_key_other_curve(self):
        """A key of another curve than ed25519 is refused: the swap is only ever checked for tz1."""
        self.sign_p2 = P2_SECP256K1
        self._swap_then_sign_refused(EXC_WRONG_VALUES, swap_error(SWAP_EC_ERROR_GENERIC, SWAP_ERR_KEY))

    def perform_test_swap_key_other_coin(self):
        """A key of another coin, here Ethereum's, is refused."""
        self.sign_path = pack_derivation_path("m/44'/60'/0'")
        self._swap_then_sign_refused(EXC_WRONG_VALUES, swap_error(SWAP_EC_ERROR_GENERIC, SWAP_ERR_KEY))

    def perform_test_swap_key_unhardened_path(self):
        """A path on the Tezos coin type with an unhardened account is refused."""
        self.sign_path = pack_derivation_path("m/44'/1729'/0")
        self._swap_then_sign_refused(EXC_WRONG_VALUES, swap_error(SWAP_EC_ERROR_GENERIC, SWAP_ERR_KEY))

    def perform_test_swap_key_other_source(self):
        """An operation sent from another account than the one signing it is refused.

        The amount, the fee and the destination are the ones of the swap, so only
        the source gives it away.
        """
        self.source_path = pack_derivation_path("m/44'/1729'/1'")
        self._swap_then_sign_refused(EXC_REJECT, swap_error(SWAP_EC_ERROR_GENERIC, SWAP_ERR_SOURCE))


# Use a class to reuse the same Speculos instance
class TestsTezosSwapSigningKey:
    @pytest.mark.parametrize(
        "test_to_run",
        ["swap_key_other_curve", "swap_key_other_coin", "swap_key_unhardened_path", "swap_key_other_source"],
    )
    def test_tezos_swap_signing_key(self, backend, exchange_navigation_helper, test_to_run):
        """A swap must not be signed with a key the host picked outside of the swap flow."""
        TezosSigningKeyTests(backend, exchange_navigation_helper).run_test(test_to_run)
