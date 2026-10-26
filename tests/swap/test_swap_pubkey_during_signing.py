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

"""A public key is not read while a swap is being signed.

In a swap the application stays in the same state from the moment Exchange
starts it, so that state does not tell whether a signing is under way. The
public key is written where the hash of the operation and the state of its
signing are kept: answering a public key request between two packets of a
signing would overwrite them.

The request must therefore be refused, whatever the instruction (with or
without a prompt), and the refusal must leave the signing as it was: the last
packet is then sent and the swap is signed as if nothing had happened.

The snapshots of this case are the ones of the control case of
test_swap_fa2.py: the screens are those of a swap that is signed. Regenerate
them with --golden_run, see README.md.
"""

import pytest
from ragger.error import ExceptionRAPDU

from . import cal_helper as cal
from .test_swap_fa2 import TezosNativeSwapTests
from .tezos_transaction import (
    CLA,
    EXC_UNEXPECTED_STATE,
    INS_GET_PUBLIC_KEY,
    INS_PROMPT_PUBLIC_KEY,
    INS_SIGN,
    P1_FIRST,
    P1_LAST,
    P2_ED25519,
    STATUS_OK,
    craft_native_transfer,
)


class TezosPubkeyDuringSigningTests(TezosNativeSwapTests):
    """A native tez swap, with public key requests between the packets of its signing."""

    def perform_final_tx(self, destination, send_amount, fees, memo):
        """Sign a transfer matching the swap, asking for the public key in the middle."""
        _, source_hash = self._get_device_address()

        rapdu = self.backend.exchange(CLA, INS_SIGN, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
        assert rapdu.status == STATUS_OK

        # The one without a prompt first: if it were answered, the one with a
        # prompt would wait for a user that is not there.
        for ins in (INS_GET_PUBLIC_KEY, INS_PROMPT_PUBLIC_KEY):
            with pytest.raises(ExceptionRAPDU) as e:
                self.backend.exchange(CLA, ins, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
            assert e.value.status == EXC_UNEXPECTED_STATE

        payload = craft_native_transfer(source_hash, destination, send_amount, fees)
        self.backend.exchange(CLA, INS_SIGN, P1_LAST, P2_ED25519, data=payload)

    def perform_test_swap_pubkey_during_signing(self):
        """The swap is signed even though the public key was asked for in the middle of its signing."""
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.assert_exchange_is_started()


# Use a class to reuse the same Speculos instance
class TestsTezosSwapPubkeyDuringSigning:
    @pytest.mark.parametrize("test_to_run", ["swap_pubkey_during_signing"])
    def test_tezos_swap_pubkey_during_signing(self, backend, exchange_navigation_helper, test_to_run):
        """The public key must not be read while a swap is being signed."""
        TezosPubkeyDuringSigningTests(backend, exchange_navigation_helper).run_test(test_to_run)
