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

"""Coin configurations, as the CAL would provide them to the Exchange application."""

from ledger_app_clients.exchange.cal_helper import CurrencyConfiguration
from ragger.bip import pack_derivation_path
from ragger.utils import create_currency_config

XTZ_PATH = "m/44'/1729'/0'"
XTZ_PACKED_DERIVATION_PATH = pack_derivation_path(XTZ_PATH)

# Native tez.
XTZ_CURRENCY_CONFIGURATION = CurrencyConfiguration(
    ticker="XTZ",
    conf=create_currency_config("XTZ", "Tezos"),
    packed_derivation_path=XTZ_PACKED_DERIVATION_PATH,
)

# Tether USD, an FA2 token. Only the sub-coin configuration reaches the Tezos
# application - the Exchange application forwards nothing else - so it is what
# tells the application which token the swap was quoted for.
USDT_CURRENCY_CONFIGURATION = CurrencyConfiguration(
    ticker="USDt",
    conf=create_currency_config("USDt", "Tezos", sub_coin_config=("USDt", 6)),
    packed_derivation_path=XTZ_PACKED_DERIVATION_PATH,
)

# The same token as the CAL describes it today: no sub-coin configuration at
# all, which leaves the application unable to tell which token is being moved.
USDT_NO_TOKEN_CONFIGURATION = CurrencyConfiguration(
    ticker="USDt",
    conf=create_currency_config("USDt", "Tezos"),
    packed_derivation_path=XTZ_PACKED_DERIVATION_PATH,
)

# Contracts from the registry in app/src/parser/fa2_tokens.c.
# KT1XnTn74bUtxHfDtBmm2bGZAQfhPbvKWR8o, "Tether USD" / USDt, 6 decimals.
USDT_CONTRACT_HASH = bytes.fromhex("fe810959c3d6127a41cbd471e7cb4e91a61b780b")
USDT_TOKEN_ID = 0
# KT193D4vozYnhGJQVtw7CoxxqphqUEEwK6Vb, "Quipuswap Governance" / QUIPU. It has
# 6 decimals too, so only the ticker tells it apart from USDt.
QUIPU_CONTRACT_HASH = bytes.fromhex("05001a8af813094ee8bf162ac2093a8937e8ba83")
