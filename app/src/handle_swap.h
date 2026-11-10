/* Tezos Ledger application - Swap requirement

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 TriliTech <contact@trili.tech>
   Copyright 2023 Functori <contact@functori.com>

   With code excerpts from:
   - Legacy Tezos app, Copyright 2019 Obsidian Systems
   - Ledger Blue sample apps, Copyright 2016 Ledger

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

/**
 * @brief What a refused swap tells the Exchange application and the host.
 *
 * The data of the answer starts with a two bytes error code, as the SDK
 * specifies in swap_error_code_helpers.h: the first byte is a
 * swap_error_common_code_t (what does not match: the amount, the
 * destination, the fees...), the second one is one of these, which tells
 * which check of the application failed.
 *
 * The status words are not changed by this: they are the ones the
 * application has always answered to a refused swap, and the tests of
 * Exchange expect them.
 */
typedef enum {
    TZ_SWAP_ERR_BATCH          = 0x01,  /// More than a reveal and a tx
    TZ_SWAP_ERR_OPERATION      = 0x02,  /// Last operation not a transfer
    TZ_SWAP_ERR_FEE            = 0x03,  /// Not the fee of the swap
    TZ_SWAP_ERR_KEY            = 0x04,  /// Key not allowed for a swap
    TZ_SWAP_ERR_SOURCE         = 0x05,  /// Not sent from the signing key
    TZ_SWAP_ERR_AMOUNT         = 0x06,  /// Not the amount of the swap
    TZ_SWAP_ERR_DESTINATION    = 0x07,  /// Not the destination of the swap
    TZ_SWAP_ERR_TOKEN_TEZ      = 0x08,  /// A token swap that sends tez
    TZ_SWAP_ERR_TOKEN_TRANSFER = 0x09,  /// Not one complete FA2 transfer
    TZ_SWAP_ERR_TOKEN_UNKNOWN  = 0x0A,  /// Contract is not a known token
    TZ_SWAP_ERR_TOKEN_TICKER   = 0x0B,  /// Not the token of the swap
    TZ_SWAP_ERR_TOKEN_DECIMALS = 0x0C,  /// Not the decimals of the swap
    TZ_SWAP_ERR_PARAMETERS     = 0x0D,  /// A tez transfer with parameters
} tz_swap_error_t;

/**
 * @brief Called to check the validity of swap params previously communicated
 * by swap_copy_transaction_parameters which is called from Ledger SDK.
 *
 */
void swap_check_validity(void);
