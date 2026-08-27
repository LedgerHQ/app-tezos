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

#ifdef HAVE_SWAP

#include <buffer.h>
#include <format.h>
#include <io.h>
#include <string.h>
#include <swap.h>

#include "format.h"
#include "handle_swap.h"
#include "keys.h"
#include "utils.h"

#include "parser/fa2_tokens.h"
#include "parser/num_parser.h"

// based on app-exchange
#define TICKER           "XTZ"
#define ADDRESS_MAX_SIZE 63
/* the smallest unit is microtez */
#define DECIMALS         6
/* Room for the ticker of a token swap, see swap_parse_config() */
#define TICKER_MAX_SIZE  16

/* Check check_address_parameters_t.address_to_check against specified
 * parameters.
 *
 * Must set params.result to 0 on error, 1 otherwise */
void
swap_handle_check_address(check_address_parameters_t *params)
{
    TZ_PREAMBLE(("params=%p", params));

    if (params->address_to_check == NULL) {
        PRINTF("[ERROR] Address to check is null\n");
        goto bail;
    }

    if ((params->address_parameters_length == 0)
        || (params->address_parameters == NULL)) {
        PRINTF("[ERROR] Address parameters is null\n");
        goto bail;
    }

    char address[TZ_CAPTURE_BUFFER_SIZE] = {0};

    // Always tz1
    derivation_type_t derivation_type = DERIVATION_TYPE_ED25519;
    bip32_path_t      bip32_path;
    buffer_t          cdata = {.ptr    = params->address_parameters,
                               .size   = params->address_parameters_length,
                               .offset = 0u};

    TZ_LIB_CHECK(read_bip32_path(&bip32_path, &cdata));
    cx_ecfp_public_key_t pubkey;
    TZ_LIB_CHECK(derive_pk(&pubkey, derivation_type, &bip32_path));
    TZ_LIB_CHECK(
        derive_pkh(&pubkey, derivation_type, address, sizeof(address)));
    if (strcmp(params->address_to_check, address) != 0) {
        PRINTF("[ERROR] Check address fail: %s !=  %s\n",
               params->address_to_check, address);
        goto bail;
    }

    params->result = 1;
    FUNC_LEAVE();
    return;
end:
bail:
    params->result = 0;
    FUNC_LEAVE();
}

/* Format printable amount including the ticker from specified parameters.
 *
 * Must set empty printable_amount on error, printable amount otherwise */
void
swap_handle_get_printable_amount(get_printable_amount_parameters_t *params)
{
    FUNC_ENTER(("params=%p", params));

    uint64_t amount;
    char     ticker[TICKER_MAX_SIZE] = TICKER;
    uint8_t  decimals                = DECIMALS;

    /* Fees are always paid in tez, even when swapping a token. Without a coin
     * configuration the currency is tez too. */
    if (!params->is_fee && (params->coin_configuration != NULL)
        && (params->coin_configuration_length > 0)) {
        if (!swap_parse_config(params->coin_configuration,
                               params->coin_configuration_length, ticker,
                               sizeof(ticker), &decimals)) {
            PRINTF("[ERROR] Fail to parse coin configuration\n");
            goto error;
        }
    }

    if (!swap_str_to_u64(params->amount, params->amount_length, &amount)) {
        PRINTF("[ERROR] Fail to parse amount\n");
        goto error;
    }

    if (!format_fpu64_trimmed(params->printable_amount,
                              sizeof(params->printable_amount), amount,
                              decimals)) {
        PRINTF("[ERROR] Fail to print amount\n");
        goto error;
    }

    strlcat(params->printable_amount, " ", sizeof(params->printable_amount));
    strlcat(params->printable_amount, ticker,
            sizeof(params->printable_amount));

    FUNC_LEAVE();
    return;

error:
    memset(params->printable_amount, '\0', sizeof(params->printable_amount));
    FUNC_LEAVE();
}

typedef struct {
    uint64_t amount;
    uint64_t fee;  /// Contains transaction fees plus reveal fees, if any.
    char     destination_address[ADDRESS_MAX_SIZE];
    /// Set when the Exchange application gave us a coin configuration, i.e.
    /// when the currency being sent is an FA2 token rather than tez.
    bool    is_token;
    char    ticker[TICKER_MAX_SIZE];  /// ticker of that token
    uint8_t decimals;                 /// its number of decimals
} swap_transaction_parameters_t;

static swap_transaction_parameters_t G_swap_params;

static uint8_t *G_swap_transaction_result;

/* Backup up transaction parameters and wipe BSS to avoid collusion with
 * app-exchange BSS data.
 *
 * return false on error, true otherwise */
bool
swap_copy_transaction_parameters(create_transaction_parameters_t *params)
{
    FUNC_ENTER(("params=%p", params));

    swap_transaction_parameters_t params_copy;
    memset(&params_copy, 0, sizeof(params_copy));

    if (!swap_str_to_u64(params->amount, params->amount_length,
                         &params_copy.amount)) {
        PRINTF("[ERROR] Fail to parse amount\n");
        goto error;
    }

    if (!swap_str_to_u64(params->fee_amount, params->fee_amount_length,
                         &params_copy.fee)) {
        PRINTF("[ERROR] Fail to parse fee\n");
        goto error;
    }

    /* A coin configuration means the swap sends a token, not tez. Without one
     * we have no way to tell which token an FA2 transfer moves, so token
     * swaps are only accepted when the Exchange application provides it. */
    if ((params->coin_configuration != NULL)
        && (params->coin_configuration_length > 0)) {
        if (!swap_parse_config(params->coin_configuration,
                               params->coin_configuration_length,
                               params_copy.ticker, sizeof(params_copy.ticker),
                               &params_copy.decimals)) {
            PRINTF("[ERROR] Fail to parse coin configuration\n");
            goto error;
        }
        params_copy.is_token = true;
    }

    if (params->destination_address == NULL) {
        PRINTF("[ERROR] Destination address is null\n");
        goto error;
    }

    strlcpy(params_copy.destination_address, params->destination_address,
            sizeof(params_copy.destination_address));
    if (params_copy
            .destination_address[sizeof(params_copy.destination_address) - 1]
        != '\0') {
        PRINTF("[ERROR] Fail to copy destination address\n");
        goto error;
    }

    os_explicit_zero_BSS_segment();

    G_swap_transaction_result = &params->result;

    memcpy(&G_swap_params, &params_copy, sizeof(params_copy));

    FUNC_LEAVE();
    return true;

error:
    FUNC_LEAVE();
    return false;
}

void
swap_check_validity(void)
{
    tz_operation_state *op
        = &global.keys.apdu.sign.u.clear.parser_state.operation;
    char dstaddr[ADDRESS_MAX_SIZE];
    TZ_PREAMBLE((""));

    if (!G_called_from_swap) {
        TZ_SUCCEED();
    }

    if (G_swap_response_ready) {
        os_sched_exit(-1);
    }
    G_swap_response_ready = true;

    PRINTF("[DEBUG] batch_index = %u, nb_reveal=%d, tag=%d\n",
           op->batch_index, op->nb_reveal, op->last_tag);
    TZ_ASSERT(EXC_REJECT, op->nb_reveal <= 1);
    TZ_ASSERT(EXC_REJECT, (op->batch_index - op->nb_reveal) == 1);
    TZ_ASSERT(EXC_REJECT, op->last_tag == TZ_OPERATION_TAG_TRANSACTION);
    TZ_ASSERT(EXC_REJECT, op->total_fee == G_swap_params.fee);

    if (G_swap_params.is_token) {
        /* A token swap is an FA2 `transfer` call on the token contract: the
         * operation carries no tez, its destination is the contract, and the
         * recipient and the amount live in the Michelson parameters. */
        const fa2_token_metadata_t *token;

        TZ_ASSERT(EXC_REJECT, op->total_amount == 0);

        /* Set only when the parser decoded a single, complete transfer.
         * Anything else - several transfers, an unsupported encoding, an
         * amount we cannot represent - leaves it clear. */
        TZ_ASSERT(EXC_REJECT, op->fa2_swap_ok);

        /* The token being moved must be the one the swap was quoted for. The
         * ticker comes from the Ledger-signed coin configuration, so matching
         * it against the registry entry of the contract actually called ties
         * the two together. */
        token = fa2_find_token(op->destination, op->fa2_token_id);
        TZ_ASSERT(EXC_REJECT, token != NULL);
        PRINTF("[DEBUG] token=\"%s\" ticker=\"%s\"\n", token->symbol,
               G_swap_params.ticker);
        TZ_ASSERT(EXC_REJECT, !strcmp(token->symbol, G_swap_params.ticker));
        TZ_ASSERT(EXC_REJECT, token->decimals == G_swap_params.decimals);

        PRINTF("[DEBUG] fa2 dstaddr=\"%s\"\n", op->fa2_destination);
        PRINTF("[DEBUG] G...dstaddr=\"%s\"\n",
               G_swap_params.destination_address);
        TZ_ASSERT(EXC_REJECT, !strcmp(op->fa2_destination,
                                      G_swap_params.destination_address));
        TZ_ASSERT(EXC_REJECT, op->fa2_amount == G_swap_params.amount);
    } else {
        TZ_ASSERT(EXC_REJECT, op->total_amount == G_swap_params.amount);

        tz_format_address(op->destination, 22, dstaddr, sizeof(dstaddr));

        PRINTF("[DEBUG] dstaddr=\"%s\"\n", dstaddr);
        PRINTF("[DEBUG] G...dstaddr=\"%s\"\n",
               G_swap_params.destination_address);
        TZ_ASSERT(EXC_REJECT,
                  !strcmp(dstaddr, G_swap_params.destination_address));
    }

    TZ_POSTAMBLE;
}

/* Set create_transaction.result and call os_lib_end().
 *
 * Doesn't return */
void __attribute__((noreturn))
swap_finalize_exchange_sign_transaction(bool is_success)
{
    *G_swap_transaction_result = is_success;
    os_lib_end();
}

#else   // HAVE_SWAP
void
swap_check_validity(void)
{
}
#endif  // HAVE_SWAP
