#pragma once
/* Checks of the key policy of swaps, for the fuzz targets that call
 * run_swap_key_policy().
 *
 * A swap is signed with no review, with a key the host picks, so which keys
 * are allowed must be exactly what the header of the policy says: an ed25519
 * key on 44'/1729'/account' or 44'/1729'/account'/0', every component
 * hardened. The policy is restated here from that sentence.
 *
 * Input: [flags][length][components, four bytes each]
 *
 * The flags, by bit: 0 ed25519, 1 start from a valid path, 2 and 3 keep the
 * account and the change of that path hardened and zero, 4 no components.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "fuzz_check.h"
#include "swap_key_policy.h"

#define HARDENED 0x80000000u

static void
run_swap_key_policy(const uint8_t *data, size_t size)
{
    if (size < 2) {
        return;
    }

    uint8_t   flags      = data[0];
    bool      is_ed25519 = (flags & 0x01) != 0;
    bool      no_comps   = (flags & 0x10) != 0;
    size_t    length     = data[1] % 12;
    uint32_t *components = malloc(length * sizeof(*components));
    bool      expected;

    if (length > 0 && components == NULL) {
        abort();
    }
    for (size_t i = 0; i < length; i++) {
        uint32_t v = 0;

        for (size_t k = 0; k < 4; k++) {
            size_t ofs = 2 + (4 * i) + k;

            v = (v << 8) | ((ofs < size) ? data[ofs] : 0);
        }
        components[i] = v;
    }

    if (flags & 0x02) {
        // Start from a valid path and let the other flags break it
        if (length > 0) {
            components[0] = HARDENED | 44;
        }
        if (length > 1) {
            components[1] = HARDENED | 1729;
        }
        if ((length > 2) && (flags & 0x04)) {
            components[2] |= HARDENED;
        }
        if ((length > 3) && (flags & 0x08)) {
            components[3] = HARDENED | 0;
        }
    }

    expected = is_ed25519 && !no_comps && (length >= 3) && (length <= 4)
               && (components[0] == (HARDENED | 44))
               && (components[1] == (HARDENED | 1729))
               && ((components[2] & HARDENED) != 0)
               && ((length == 3) || (components[3] == (HARDENED | 0)));

    CHECK(swap_signing_key_allowed(is_ed25519, no_comps ? NULL : components,
                                   length)
          == expected);
    free(components);
}
