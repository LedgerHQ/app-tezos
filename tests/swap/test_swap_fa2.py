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

"""Swapping an FA2 token held on Tezos (LIVE-36514).

A swap paying with a token signs an FA2 `transfer` call rather than a tez
transfer: the operation carries no tez, its destination is the token contract,
and the recipient and the amount are inside the Michelson parameters. The
application used to compare the operation against the swap parameters as if it
were a native transfer, and answered 0x6985 to every such signature.

swap_check_validity() in app/src/handle_swap.c now branches on whether the
Exchange application provided a coin configuration. Without one the
application cannot tell which token the operation moves, so it must keep
refusing - which is what happens with the CAL configuration in production
today, see USDT_NO_TOKEN_CONFIGURATION.
"""

import pytest
from ledger_app_clients.exchange.test_runner import ExchangeTestRunner
from ragger.error import ExceptionRAPDU

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
    blake2_hash_pubkey,
    craft_fa2_transfer,
    craft_native_transfer,
    encode_tz1,
)


class TezosFa2Tests(ExchangeTestRunner):
    """Swap paying with an FA2 token."""

    currency_configuration = cal.USDT_CURRENCY_CONFIGURATION

    # The token the final transaction transfers. Overridden below by the case
    # that transfers a token other than the one being swapped.
    token_contract_hash = cal.USDT_CONTRACT_HASH
    token_id = cal.USDT_TOKEN_ID

    # Payin address of the swap provider, i.e. the FA2 `to_` of the transfer.
    valid_destination_1 = "tz1drcp5n9mkJ2eDQQsHBBSDetKJ9s5aKox4"
    valid_destination_2 = "tz1TNKZitanSzHmkxVRriDdMU7qrQukae9Yr"

    # Address of the Speculos seed on cal.XTZ_PATH. It is both the refund
    # address of the swap and the FA2 `from_` of the transfer.
    valid_refund = "tz1YPjCVqgimTAPmxZX9egDeTFRCmrTRqmp9"

    # 100.106851 USDt, the amount of the reported failure.
    valid_send_amount_1 = 100106851
    valid_send_amount_2 = 446739662
    # Operation fees, in mutez.
    valid_fees_1 = 2000
    valid_fees_2 = 10078

    fake_refund = "abcdabcd"
    fake_payout = "abcdabcd"

    signature_refusal_error_code = EXC_REJECT

    def _get_device_address(self):
        """Read the device public key and derive its tz1 address."""
        rapdu = self.backend.exchange(CLA, INS_GET_PUBLIC_KEY, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
        assert rapdu.status == STATUS_OK
        source_hash = blake2_hash_pubkey(rapdu.data[2:])
        return encode_tz1(source_hash), source_hash

    def _sign(self, payload):
        """Send a payload through the two packets of an INS_SIGN exchange.

        A non-9000 answer raises, so the tampering cases can expect it.
        """
        rapdu = self.backend.exchange(CLA, INS_SIGN, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
        assert rapdu.status == STATUS_OK
        self.backend.exchange(CLA, INS_SIGN, P1_LAST, P2_ED25519, data=payload)

    def perform_final_tx(self, destination, send_amount, fees, memo):
        """Sign an FA2 `transfer` of `send_amount` tokens to `destination`."""
        source, source_hash = self._get_device_address()
        assert source == self.valid_refund, f"Unexpected device address {source}, expected {self.valid_refund}"

        self._sign(
            craft_fa2_transfer(source, source_hash, self.token_contract_hash, destination, self.token_id, send_amount, fees)
        )

    def perform_test_swap_fa2_valid_1(self):
        """The reported flow: swapping USDt must be signed."""
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        try:
            self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        except ExceptionRAPDU as e:
            pytest.fail(
                f"The application refused to sign the FA2 token transfer with SW "
                f"0x{e.status:04x}. The operation moves {self.valid_send_amount_1} units "
                f"of USDt to {self.valid_destination_1}, both taken from the Michelson "
                f"parameters, and carries 0 mutez."
            )
        self.assert_exchange_is_started()

    def perform_test_swap_fa2_wrong_token(self):
        """Transferring a token other than the one quoted must be refused."""
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        with pytest.raises(ExceptionRAPDU) as e:
            self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        assert e.value.status == self.signature_refusal_error_code
        self.assert_exchange_is_started()


class TezosFa2WrongTokenTests(TezosFa2Tests):
    """Swap quoted for USDt, but the operation transfers QUIPU instead.

    Both tokens are in the application registry and both have 6 decimals, so
    only the ticker from the coin configuration tells them apart.
    """

    token_contract_hash = cal.QUIPU_CONTRACT_HASH


class TezosFa2NoTokenConfigTests(TezosFa2Tests):
    """The situation in production: the CAL sends no sub-coin configuration.

    The application is then told nothing about the token, so it cannot check
    that the operation moves the token the swap was quoted for. Refusing is
    the only safe answer.
    """

    currency_configuration = cal.USDT_NO_TOKEN_CONFIGURATION

    def perform_test_swap_fa2_no_config(self):
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        with pytest.raises(ExceptionRAPDU) as e:
            self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        assert e.value.status == self.signature_refusal_error_code
        self.assert_exchange_is_started()


class TezosNativeSwapTests(TezosFa2Tests):
    """Control case: the same flow with a native tez transfer.

    It shares everything with TezosFa2Tests except the final transaction, so a
    failure here means the test setup is broken rather than the application.
    """

    currency_configuration = cal.XTZ_CURRENCY_CONFIGURATION
    valid_send_amount_1 = 10000000

    def perform_final_tx(self, destination, send_amount, fees, memo):
        _, source_hash = self._get_device_address()
        self._sign(craft_native_transfer(source_hash, destination, send_amount, fees))

    def perform_test_swap_native_valid_1(self):
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        self.assert_exchange_is_started()


# Use a class to reuse the same Speculos instance
class TestsTezosSwap:
    @pytest.mark.parametrize("test_to_run", ["swap_native_valid_1"])
    def test_tezos_native_swap_control(self, backend, exchange_navigation_helper, test_to_run):
        """Sanity check: a native tez swap goes through with this exact setup."""
        TezosNativeSwapTests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["swap_fa2_valid_1"])
    def test_tezos_fa2(self, backend, exchange_navigation_helper, test_to_run):
        TezosFa2Tests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["swap_wrong_amount", "swap_wrong_destination", "swap_wrong_fees"])
    def test_tezos_fa2_tampered(self, backend, exchange_navigation_helper, test_to_run):
        """A tampered FA2 transfer must not be signed."""
        TezosFa2Tests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["swap_fa2_wrong_token"])
    def test_tezos_fa2_wrong_token(self, backend, exchange_navigation_helper, test_to_run):
        """Transferring another registered token must not be signed."""
        TezosFa2WrongTokenTests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["swap_fa2_no_config"])
    def test_tezos_fa2_without_token_config(self, backend, exchange_navigation_helper, test_to_run):
        """An FA2 swap with no sub-coin configuration must stay refused."""
        TezosFa2NoTokenConfigTests(backend, exchange_navigation_helper).run_test(test_to_run)
