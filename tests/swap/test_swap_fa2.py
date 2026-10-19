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
refusing an FA2 transfer - which is what happens with the CAL configuration in
production today, see USDT_NO_TOKEN_CONFIGURATION.

The same missing configuration makes the swap look like a tez swap, so a plain
tez transfer is signed for it. That is the open limitation app-tezos-dev#66, pinned by
TezosNativeTransferForTokenSwapTests.
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
    INS_SIGN_WITH_HASH,
    P1_FIRST,
    P1_LAST,
    P2_ED25519,
    STATUS_OK,
    address_bytes,
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

    def _get_device_address(self, packed_path=cal.XTZ_PACKED_DERIVATION_PATH):
        """Read the device public key at `packed_path` and derive its tz1 address."""
        rapdu = self.backend.exchange(CLA, INS_GET_PUBLIC_KEY, P1_FIRST, P2_ED25519, packed_path)
        assert rapdu.status == STATUS_OK
        source_hash = blake2_hash_pubkey(rapdu.data[2:])
        return encode_tz1(source_hash), source_hash

    def _sign(self, payload, ins=INS_SIGN):
        """Send a payload through the two packets of a signing exchange.

        `ins` is INS_SIGN, or INS_SIGN_WITH_HASH which reaches the same checks.
        A non-9000 answer raises, so the tampering cases can expect it.
        """
        rapdu = self.backend.exchange(CLA, ins, P1_FIRST, P2_ED25519, cal.XTZ_PACKED_DERIVATION_PATH)
        assert rapdu.status == STATUS_OK
        self.backend.exchange(CLA, ins, P1_LAST, P2_ED25519, data=payload)

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


class TezosFa2UnregisteredContractTests(TezosFa2Tests):
    """Swap quoted for USDt, but the operation calls a contract that is not in the registry.

    The entrypoint and the parameters are chosen by the host, so an FA2-shaped
    `transfer` to a contract the registry does not list says nothing about what
    it moves. The application must not decode it as a token transfer, and so
    has no validated recipient or amount to compare with the swap.
    """

    token_contract_hash = cal.UNREGISTERED_CONTRACT_HASH

    def perform_test_swap_fa2_unregistered_contract(self):
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        with pytest.raises(ExceptionRAPDU) as e:
            self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        assert e.value.status == self.signature_refusal_error_code
        self.assert_exchange_is_started()


class TezosFa2ForeignOwnerTests(TezosFa2Tests):
    """Swap of USDt whose tokens belong to another account.

    Everything the application used to check is valid: the operation is sent by
    the key that signs, the contract, token, amount, recipient and fee are the
    quoted ones. Only the FA2 `from_`, the owner whose balance the contract
    debits, is not the device's account. If that account made the signer an FA2
    operator the contract would accept the transfer, and the user would pay
    with tokens that are not theirs without ever seeing it. Nothing but the
    application ties `from_` to the signer, so it has to refuse.
    """

    # An account that is not the device's.
    foreign_owner = TezosFa2Tests.valid_destination_2
    # The instruction the transfer is sent with.
    sign_ins = INS_SIGN

    def _owner(self, signer):
        """`from_` of the transfer, as the case builds it: a string, as Ledger Live sends it."""
        return self.foreign_owner

    def perform_final_tx(self, destination, send_amount, fees, memo):
        signer, source_hash = self._get_device_address()
        assert signer == self.valid_refund, f"Unexpected device address {signer}, expected {self.valid_refund}"
        self._sign(
            craft_fa2_transfer(
                self._owner(signer), source_hash, self.token_contract_hash, destination, self.token_id, send_amount, fees
            ),
            self.sign_ins,
        )

    def _swap_then_sign_refused(self):
        self.perform_valid_swap_from_custom(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        with pytest.raises(ExceptionRAPDU) as e:
            self.perform_coin_specific_final_tx(self.valid_destination_1, self.valid_send_amount_1, self.valid_fees_1, "")
        assert e.value.status == self.signature_refusal_error_code
        self.assert_exchange_is_started()

    def perform_test_swap_fa2_foreign_owner(self):
        """Another owner's tokens, sent with INS_SIGN, must not be signed."""
        self._swap_then_sign_refused()

    def perform_test_swap_fa2_foreign_owner_with_hash(self):
        """The same, sent with INS_SIGN_WITH_HASH."""
        self.sign_ins = INS_SIGN_WITH_HASH
        self._swap_then_sign_refused()

    def perform_test_swap_fa2_foreign_owner_binary(self):
        """The same, with the owner given as the bytes of an address rather than a string."""
        self._owner = lambda signer: address_bytes(self.foreign_owner)
        self._swap_then_sign_refused()

    def perform_test_swap_fa2_owner_with_nul(self):
        """The signer's address followed by a NUL and more is not the signer's.

        Read as a C string it is the signer, so it has to be refused before that
        comparison is made.
        """
        self._owner = lambda signer: signer + "\x00" + "abc"
        self._swap_then_sign_refused()


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


class TezosNativeTransferForTokenSwapTests(TezosNativeSwapTests):
    """Known limitation (app-tezos-dev#66): a tez transfer is signed for a swap quoted in USDt.

    The CAL sends no sub-coin configuration for USDt, so the swap reaches the
    application exactly like a tez swap. The host then sends a plain tez
    transfer with the quoted amount, fee and recipient, and nothing the
    application can check differs: 100106851 USDt base units become
    100.106851 XTZ, and the Exchange screen prints them as XTZ.

    This pins what happens today. It has to flip to a refusal once Exchange
    passes the ticker, or once the CAL gives USDt a sub-coin configuration.
    """

    currency_configuration = cal.USDT_NO_TOKEN_CONFIGURATION
    # The amount of the USDt swap, not the 10 tez of the control case.
    valid_send_amount_1 = TezosFa2Tests.valid_send_amount_1


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

    @pytest.mark.parametrize("test_to_run", ["swap_fa2_unregistered_contract"])
    def test_tezos_fa2_unregistered_contract(self, backend, exchange_navigation_helper, test_to_run):
        """An FA2-shaped transfer to a contract outside the registry must not be signed."""
        TezosFa2UnregisteredContractTests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize(
        "test_to_run",
        [
            "swap_fa2_foreign_owner",
            "swap_fa2_foreign_owner_with_hash",
            "swap_fa2_foreign_owner_binary",
            "swap_fa2_owner_with_nul",
        ],
    )
    def test_tezos_fa2_foreign_owner(self, backend, exchange_navigation_helper, test_to_run):
        """Tokens of another owner than the signer must not be signed."""
        TezosFa2ForeignOwnerTests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["swap_fa2_no_config"])
    def test_tezos_fa2_without_token_config(self, backend, exchange_navigation_helper, test_to_run):
        """An FA2 swap with no sub-coin configuration must stay refused."""
        TezosFa2NoTokenConfigTests(backend, exchange_navigation_helper).run_test(test_to_run)

    @pytest.mark.parametrize("test_to_run", ["swap_native_valid_1"])
    def test_tezos_native_transfer_for_token_swap(self, backend, exchange_navigation_helper, test_to_run):
        """Known limitation (app-tezos-dev#66): a tez transfer is signed for a swap quoted in USDt with no sub-coin configuration."""
        TezosNativeTransferForTokenSwapTests(backend, exchange_navigation_helper).run_test(test_to_run)
