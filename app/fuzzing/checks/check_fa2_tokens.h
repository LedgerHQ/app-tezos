#pragma once
/* Checks of the FA2 token registry, for the fuzz targets that call
 * run_fa2_tokens().
 *
 * Swap validation and the display of token transfers trust what these lookups
 * answer, so they are held to what a registry must satisfy: an index is valid
 * exactly when it gives a token, a token found is the one asked for, and the
 * ways to ask agree with each other.
 *
 * Input: [lookup][bits][index or token id]
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fa2_tokens.h"
#include "fuzz_check.h"

#define DESTINATION_LEN 21  // the tag, then the 20 bytes of the contract hash

static size_t
registry_size(void)
{
    size_t n = 0;

    while (fa2_token_by_index((int16_t)n) != NULL) {
        n++;
    }
    return n;
}

/* No two tokens are the same, and none is blank */
static void
check_registry(void)
{
    size_t n = registry_size();

    for (size_t i = 0; i < n; i++) {
        const fa2_token_metadata_t *a = fa2_token_by_index((int16_t)i);

        CHECK(a->name[0] != '\0');
        CHECK(a->symbol[0] != '\0');
        for (size_t j = i + 1; j < n; j++) {
            const fa2_token_metadata_t *b = fa2_token_by_index((int16_t)j);

            CHECK(!((a->token_id == b->token_id)
                    && (memcmp(a->contract_hash, b->contract_hash,
                               FA2_TOKEN_CONTRACT_HASH_LENGTH)
                        == 0)));
        }
    }
}

/* Whether some token of the registry has this contract, and this token id if
 * `any_id` is false */
static bool
registry_has(const uint8_t *destination, bool any_id, uint64_t token_id)
{
    for (size_t i = 0; fa2_token_by_index((int16_t)i) != NULL; i++) {
        const fa2_token_metadata_t *t = fa2_token_by_index((int16_t)i);

        if (memcmp(t->contract_hash, destination + 1,
                   FA2_TOKEN_CONTRACT_HASH_LENGTH)
                == 0
            && (any_id || t->token_id == token_id)) {
            return true;
        }
    }
    return false;
}

static void
check_index(int16_t idx)
{
    const fa2_token_metadata_t *t = fa2_token_by_index(idx);

    CHECK(fa2_token_idx_valid(idx) == (t != NULL));
    CHECK(fa2_token_idx_valid(idx)
          == ((idx >= 0) && ((size_t)idx < registry_size())));
    if (t != NULL) {
        CHECK(fa2_token_index(t) == idx);
    }
}

static void
check_destination(const uint8_t *destination, uint64_t token_id)
{
    const fa2_token_metadata_t *t = fa2_find_token(destination, token_id);
    bool registered               = fa2_contract_is_registered(destination);
    bool has_contract
        = (destination[0] == 1) && registry_has(destination, true, 0);
    bool has_token
        = (destination[0] == 1) && registry_has(destination, false, token_id);

    // Only a KT1 can be a token contract
    CHECK(registered == has_contract);
    CHECK((t != NULL) == has_token);
    if (t != NULL) {
        CHECK(destination[0] == 1);
        CHECK(t->token_id == token_id);
        CHECK(memcmp(t->contract_hash, destination + 1,
                     FA2_TOKEN_CONTRACT_HASH_LENGTH)
              == 0);
        CHECK(registered);
        CHECK(fa2_token_index(t) >= 0);
    }
}

static void
run_fa2_tokens(const uint8_t *data, size_t size)
{
    static bool checked;

    if (!checked) {
        check_registry();
        checked = true;
    }
    if (size < 3) {
        return;
    }

    uint8_t lookup = data[0] % 4;
    uint8_t bits   = data[1];

    switch (lookup) {
    case 0:
        check_index((int16_t)((data[1] << 8) | data[2]));
        break;
    case 1: {
        // A destination, from the registry when the top bit is set, so that
        // the lookups that succeed are reached as well
        size_t   n           = registry_size();
        uint8_t *destination = malloc(DESTINATION_LEN);
        uint64_t token_id    = 0;

        if (destination == NULL) {
            abort();
        }
        memset(destination, 0, DESTINATION_LEN);
        for (size_t i = 3; i < size && i < 3 + DESTINATION_LEN; i++) {
            destination[i - 3] = data[i];
        }
        for (size_t i = 3 + DESTINATION_LEN;
             i < size && i < 11 + DESTINATION_LEN; i++) {
            token_id = (token_id << 8) | data[i];
        }
        if ((bits & 0x80) && n > 0) {
            const fa2_token_metadata_t *t
                = fa2_token_by_index((int16_t)(data[2] % n));

            destination[0] = 1;
            memcpy(destination + 1, t->contract_hash,
                   FA2_TOKEN_CONTRACT_HASH_LENGTH);
            if (bits & 0x40) {
                token_id = t->token_id;
            }
        }
        check_destination(destination, token_id);
        free(destination);
        break;
    }
    case 2:
        // Nothing to look up
        CHECK(fa2_find_token(NULL, 0) == NULL);
        CHECK(!fa2_contract_is_registered(NULL));
        CHECK(fa2_token_index(NULL) == -1);
        break;
    default: {
        // A token that is not in the registry has no index
        static const fa2_token_metadata_t foreign;

        CHECK(fa2_token_index(&foreign) == -1);
        break;
    }
    }
}
