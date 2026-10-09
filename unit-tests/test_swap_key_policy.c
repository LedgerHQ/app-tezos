/* Tezos Embedded C parser for Ledger - swap key policy unit tests

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "swap_key_policy.h"

#define H(n) (0x80000000u | (n))

#define ALLOWED(is_ed25519, path)                  \
    swap_signing_key_allowed((is_ed25519), (path), \
                             sizeof(path) / sizeof(path[0]))

/* The paths the swap flow really uses: Ledger Live signs with the four
   component form, Exchange and its tests use the three component one. */
static void
test_swap_key_accepts_the_paths_of_the_swap_flow(void **state)
{
    const uint32_t live[]     = {H(44), H(1729), H(0), H(0)};
    const uint32_t exchange[] = {H(44), H(1729), H(0)};
    const uint32_t account[]  = {H(44), H(1729), H(7), H(3)};

    assert_true(ALLOWED(true, live));
    assert_true(ALLOWED(true, exchange));
    assert_true(ALLOWED(true, account));
}

/* Without the review screen nothing tells the user which curve signs. */
static void
test_swap_key_rejects_other_derivation_types(void **state)
{
    const uint32_t live[] = {H(44), H(1729), H(0), H(0)};

    assert_false(ALLOWED(false, live));
}

/* The host cannot point the swap at a key from another purpose or coin. */
static void
test_swap_key_rejects_paths_outside_a_tezos_account(void **state)
{
    const uint32_t other_purpose[] = {H(49), H(1729), H(0), H(0)};
    const uint32_t other_coin[]    = {H(44), H(60), H(0), H(0)};
    const uint32_t bitcoin[]       = {H(44), H(0), H(0), H(0)};
    const uint32_t swapped[]       = {H(1729), H(44), H(0), H(0)};

    assert_false(ALLOWED(true, other_purpose));
    assert_false(ALLOWED(true, other_coin));
    assert_false(ALLOWED(true, bitcoin));
    assert_false(ALLOWED(true, swapped));
}

static void
test_swap_key_rejects_unhardened_components(void **state)
{
    const uint32_t soft_purpose[]  = {44, H(1729), H(0), H(0)};
    const uint32_t soft_coin[]     = {H(44), 1729, H(0), H(0)};
    const uint32_t soft_account[]  = {H(44), H(1729), 0, H(0)};
    const uint32_t soft_change[]   = {H(44), H(1729), H(0), 0};
    const uint32_t soft_account3[] = {H(44), H(1729), 0};

    assert_false(ALLOWED(true, soft_purpose));
    assert_false(ALLOWED(true, soft_coin));
    assert_false(ALLOWED(true, soft_account));
    assert_false(ALLOWED(true, soft_change));
    assert_false(ALLOWED(true, soft_account3));
}

static void
test_swap_key_rejects_paths_of_the_wrong_length(void **state)
{
    const uint32_t five[] = {H(44), H(1729), H(0), H(0), H(0)};
    const uint32_t two[]  = {H(44), H(1729)};
    const uint32_t one[]  = {H(44)};

    assert_false(ALLOWED(true, five));
    assert_false(ALLOWED(true, two));
    assert_false(ALLOWED(true, one));
    assert_false(swap_signing_key_allowed(true, one, 0));
}

static void
test_swap_key_rejects_a_missing_path(void **state)
{
    assert_false(swap_signing_key_allowed(true, NULL, 4));
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_swap_key_accepts_the_paths_of_the_swap_flow),
        cmocka_unit_test(test_swap_key_rejects_other_derivation_types),
        cmocka_unit_test(test_swap_key_rejects_paths_outside_a_tezos_account),
        cmocka_unit_test(test_swap_key_rejects_unhardened_components),
        cmocka_unit_test(test_swap_key_rejects_paths_of_the_wrong_length),
        cmocka_unit_test(test_swap_key_rejects_a_missing_path),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
