/* Tezos Embedded C parser for Ledger - formatting unit tests

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

#include "formatting.h"

#define HASH_LEN 32
#define OBUF_LEN TZ_BASE58_BUFFER_SIZE(HASH_LEN)

/* The signing UI declares its base58 buffers at exactly
   TZ_BASE58_BUFFER_SIZE(). CANARY sits immediately after that buffer so a
   write past its last byte is observable. */
#define CANARY 0x7e

typedef struct {
    char obuf[OBUF_LEN];
    char canary;
} guarded_buffer;

static void
format_and_check(const uint8_t *in, size_t in_len, const char *expected)
{
    guarded_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.canary = CANARY;

    assert_int_equal(tz_format_base58(in, in_len, buf.obuf, sizeof(buf.obuf)),
                     0);
    /* Memory safety first: a one-byte overrun is the more severe defect and
       must be reported even when the rendered string is also wrong. */
    assert_int_equal((unsigned char)buf.canary, CANARY);
    assert_string_equal(buf.obuf, expected);
}

/* A hash whose first byte is non-zero exercises the ordinary path. */
static void
test_base58_no_leading_zero(void **state)
{
    uint8_t in[HASH_LEN];

    memset(in, 0x1a, sizeof(in));
    in[0] = 0x9d;
    format_and_check(in, sizeof(in),
                     "BaG6QiAKTHaR8yFpysNNr9hnFQVD17C62EjPTRowr3zZ");
}

/* A host can grind the operation until the signing hash starts with 0x00.
   The leading zero byte must render as '1' and must not push the NUL
   terminator past the caller's buffer. */
static void
test_base58_one_leading_zero(void **state)
{
    uint8_t in[HASH_LEN];

    memset(in, 0x11, sizeof(in));
    in[0] = 0x00;
    format_and_check(in, sizeof(in),
                     "1G6ShajrrdiRnD4mW22j8T5kXyKSvwXaC64S9VGSzFA");
}

static void
test_base58_two_leading_zeros(void **state)
{
    uint8_t in[HASH_LEN];

    memset(in, 0xff, sizeof(in));
    in[0] = 0x00;
    in[1] = 0x00;
    format_and_check(in, sizeof(in),
                     "11tJ93RwaVfE1PEMxd5rpZZuPtLCwbEaDCrNBhAy8Cv");
}

/* Every byte zero: the value has no significant digit at all. */
static void
test_base58_all_zero(void **state)
{
    uint8_t in[HASH_LEN];

    memset(in, 0, sizeof(in));
    format_and_check(in, sizeof(in), "11111111111111111111111111111111");
}

/* An undersized destination is refused, not truncated. */
static void
test_base58_rejects_short_buffer(void **state)
{
    uint8_t in[HASH_LEN];
    char    obuf[OBUF_LEN - 1];

    memset(in, 0x11, sizeof(in));
    in[0] = 0x00;
    assert_int_equal(tz_format_base58(in, sizeof(in), obuf, sizeof(obuf)), 1);
}

/* A contract of tag 3 is a smart rollup: its hash is shown as an sr1 address,
   without the padding byte that follows it. */
static void
test_address_smart_rollup(void **state)
{
    uint8_t in[22];
    char    obuf[64];
    char    expected[64];

    memset(in, 0x2b, sizeof(in));
    in[0]  = 3;
    in[21] = 0;

    assert_int_equal(tz_format_address(in, sizeof(in), obuf, sizeof(obuf)),
                     0);
    assert_memory_equal(obuf, "sr1", 3);
    assert_int_equal(
        tz_format_base58check("sr1", in + 1, 20, expected, sizeof(expected)),
        0);
    assert_string_equal(obuf, expected);
}

/* The tag and the padding byte do not make an address: too few bytes for the
   hash that goes between them are refused. */
static void
test_address_rejects_short_input(void **state)
{
    uint8_t in[3] = {0, 0, 0};
    char    obuf[64];

    for (size_t tag = 1; tag <= 4; tag++) {
        in[0] = (uint8_t)tag;
        assert_int_equal(tz_format_address(in, 0, obuf, sizeof(obuf)), 1);
        assert_int_equal(tz_format_address(in, 1, obuf, sizeof(obuf)), 1);
        assert_int_equal(tz_format_address(in, 2, obuf, sizeof(obuf)), 1);
        assert_int_equal(tz_format_address(in, 3, obuf, sizeof(obuf)), 1);
    }
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_base58_no_leading_zero),
        cmocka_unit_test(test_base58_one_leading_zero),
        cmocka_unit_test(test_base58_two_leading_zeros),
        cmocka_unit_test(test_base58_all_zero),
        cmocka_unit_test(test_base58_rejects_short_buffer),
        cmocka_unit_test(test_address_smart_rollup),
        cmocka_unit_test(test_address_rejects_short_input),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
