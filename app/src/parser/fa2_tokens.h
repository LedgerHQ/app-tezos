/* Tezos Embedded C parser for Ledger - FA2 token registry

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 Functori <contact@functori.com>
   Copyright 2023 TriliTech <contact@trili.tech>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */
#ifndef FA2_TOKENS_H
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FA2_TOKEN_NAME_LENGTH          32
#define FA2_TOKEN_SYMBOL_LENGTH        12
#define FA2_TOKEN_CONTRACT_HASH_LENGTH 20

typedef struct {
    const char    name[FA2_TOKEN_NAME_LENGTH];
    const char    symbol[FA2_TOKEN_SYMBOL_LENGTH];
    uint8_t       decimals;
    uint64_t      token_id;
    const uint8_t contract_hash[FA2_TOKEN_CONTRACT_HASH_LENGTH];
} fa2_token_metadata_t;

bool fa2_token_idx_valid(int16_t idx);

const fa2_token_metadata_t *fa2_token_by_index(int16_t idx);

int16_t fa2_token_index(const fa2_token_metadata_t *token);

const fa2_token_metadata_t *fa2_find_token(const uint8_t *destination,
                                           uint64_t       token_id);
#endif  // FA_TOKENS_H
