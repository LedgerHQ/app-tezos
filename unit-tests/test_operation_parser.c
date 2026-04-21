/* Copyright 2023 Functori <contact@functori.com>

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
#include "operation_parser.h"

typedef struct {
    tz_parser_state *state;
    char            *obuf;
    size_t           olen;
    uint8_t         *ibuf;
    size_t           ilen;
    size_t           max_ilen;
    char            *str;
    size_t           str_len;
    size_t           str_ofs;
} operation_parser_data;

static int
operation_parser_setup(void **state)
{
    operation_parser_data *data = malloc(sizeof(*data));

    if (data == NULL) {
        return -1;
    }

    data->state = malloc(sizeof(tz_parser_state));
    if (data->state == NULL) {
        free(data);
        return -1;
    }
    memset(data->state, 0, sizeof(tz_parser_state));
    data->olen = 50;
    data->obuf = malloc(data->olen + 1);
    if (data->obuf == NULL) {
        free(data->state);
        free(data);
        return -1;
    }
    memset(data->obuf, 0, data->olen + 1);
    data->max_ilen = 235;
    data->ilen     = data->max_ilen;
    data->ibuf     = malloc(data->max_ilen);
    if (data->ibuf == NULL) {
        free(data->obuf);
        free(data->state);
        free(data);
        return -1;
    }
    memset(data->ibuf, 0, data->max_ilen);
    tz_operation_parser_init(data->state, TZ_UNKNOWN_SIZE, false);
    tz_parser_refill(data->state, NULL, 0);
    tz_parser_flush(data->state, data->obuf, data->olen);
    data->str = NULL;

    *state = data;
    return 0;
}

static int
operation_parser_teardown(void **state)
{
    operation_parser_data *data = *state;

    free(data->state);
    free(data->obuf);
    free(data->ibuf);
    free(data);
    return 0;
}

void
refill(operation_parser_data *data)
{
    data->ilen = 0;
    while (
        ((data->str_ofs < data->str_len) || (data->ilen < data->max_ilen))
        && sscanf(data->str + data->str_ofs, "%2hhx", &data->ibuf[data->ilen])
               == 1) {
        data->str_ofs += 2;
        data->ilen++;
    }
}

typedef struct {
    const char *name;
    uint8_t     complex : 1;
    int         field_index;
} tz_fields_check;

static void
fill_data_str(operation_parser_data *data, char *str)
{
    size_t str_len = strlen(str) / 2;
    data->str      = str;
    data->str_len  = str_len;
    data->str_ofs  = 0;
}

static void
check_field_complexity(operation_parser_data *data, char *str,
                       const tz_fields_check *fields_check,
                       size_t                 fields_check_size)
{
    fill_data_str(data, str);
    size_t fields_check_len = fields_check_size / sizeof(tz_fields_check);

    tz_operation_parser_set_size(data->state, (uint16_t)data->str_len);

    tz_parser_state *st = data->state;

    bool   result       = true;
    size_t idx          = 0;
    bool   already_seen = false;

    while (true) {
        while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {
            // Loop while the result is successful and not blocking
        }

        switch (st->errno) {
        case TZ_BLO_FEED_ME:
            refill(data);
            tz_parser_refill(data->state, data->ibuf, data->ilen);
            continue;

        case TZ_BLO_IM_FULL:
            if (already_seen
                && strcmp(st->field_info.field_name, fields_check[idx].name)
                       != 0) {
                idx++;
                assert_true(((intmax_t)idx) < ((intmax_t)fields_check_len));
                already_seen = false;
            }
            if (strcmp(st->field_info.field_name, fields_check[idx].name)
                == 0) {
                if ((fields_check[idx].complex
                     != st->field_info.is_field_complex)
                    || (fields_check[idx].field_index
                        != st->field_info.field_index)) {
                    print_message(
                        "%s:%d '%s' field expected to have complex: %s "
                        "index: %d but "
                        "got complex: %s index: %d",
                        __FILE__, __LINE__, st->field_info.field_name,
                        fields_check[idx].complex ? "true" : "false",
                        fields_check[idx].field_index,
                        st->field_info.is_field_complex ? "true" : "false",
                        st->field_info.field_index);
                    result = false;
                }
                already_seen = true;
            } else if (st->field_info.is_field_complex) {
                print_message(
                    "%s:%d '%s' has not been defined as an operation field "
                    "and therefore must not be complex",
                    __FILE__, __LINE__, st->field_info.field_name);
                result = false;
            }
            tz_parser_flush(st, data->obuf, data->olen);
            continue;

        case TZ_BLO_DONE:
            if (fields_check_len != (idx + 1)) {
                fail_msg(
                    "%s:%d all the field have not been seen, %d fields "
                    "expected but got %d seen",
                    __FILE__, __LINE__, (int)fields_check_len, (int)idx);
            }
            assert_true(result);
            break;

        default:
            fail_msg("%s:%d parsing error: %s", __FILE__, __LINE__,
                      tz_parser_result_name(st->errno));
        }
        break;
    }
}

static void
check_parser_error(operation_parser_data *data, char *str,
                   tz_parser_result expected)
{
    fill_data_str(data, str);
    tz_operation_parser_set_size(data->state, (uint16_t)data->str_len);
    tz_parser_state *st = data->state;
    while (true) {
        while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {}
        switch (st->errno) {
        case TZ_BLO_FEED_ME:
            refill(data);
            tz_parser_refill(st, data->ibuf, data->ilen);
            continue;
        case TZ_BLO_IM_FULL:
            tz_parser_flush(st, data->obuf, data->olen);
            continue;
        default:
            assert_int_equal(st->errno, expected);
            return;
        }
    }
}

static void
test_check_proposals_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "0500ffdd6102321bc251e4a5190ad5b12b251069d9b400000020000000400bcd7b"
          "2cadcd87ecb0d5c50330fb59feed7432bffecede8a09a2b86cfb33847b0bcd7b2c"
          "adcd87ecb0d5c50330fb59feed7432bffecede8a09a2b86dac301a2d";
    const tz_fields_check fields_check[] = {
        {"Source",   false, 1},
        {"Period",   false, 2},
        {"Proposal", false, 3},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_ballot_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "0600ffdd6102321bc251e4a5190ad5b12b251069d9b4000000200bcd7b2cadcd87"
          "ecb0d5c50330fb59feed7432bffecede8a09a2b86cfb33847b00";
    const tz_fields_check fields_check[] = {
        {"Source",   false, 1},
        {"Period",   false, 2},
        {"Proposal", false, 3},
        {"Ballot",   false, 4},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_failing_noop_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "11000000c639663039663239353264333435323863373333663934363135636663"
          "333962633535353631396663353530646434613637626132323038636538653836"
          "376161336431336136656639396466626533326336393734616139613231353064"
          "323165636132396333333439653539633133623930383166316331316234343061"
          "633464333435356465646265346565306465313561386166363230643463383632"
          "343764396431333264653162623664613233643566663964386466666461323262"
          "6139613834";
    const tz_fields_check fields_check[] = {
        {"Message", false, 1},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_reveal_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6b00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400747884d9ab"
          "df16b3ab745158925f567e222f71225501826fa83347f6cbe9c393ff0000006097"
          "df7f48a17994a2dd1e4cb3c15104a36f3b9298ec5210f40b5ff23f4cbb78c543b2"
          "4b4e7e60cfa13f01568da45018ff156118c15592609d6f2c38972c336cd104cd26"
          "ece288c06a3b3ab4ba5b5542625bee94a4960f45a2c50425e88d271c58";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Public key",    false, 4},
        //     {"Option",        _,     5},
        {"Proof",         false, 6},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_simple_transaction_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4a0c21e020304904e010000"
          "0000000000000000000000000000000000000000";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_transaction_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c016e8874874d31c3fbd636e924d5a036a43ec8faa7d0860308362d80d30e0100"
          "0000000000000000000000000000000000000000ff02000000020316";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
        //    {"Tuple",         _,     7},
        {"Entrypoint",    false, 8},
        {"Parameter",     true,  9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_double_transaction_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4a0c21e020304904e010000"
          "0000000000000000000000000000000000000000"
          "6c016e8874874d31c3fbd636e924d5a036a43ec8faa7d0860308362d80d30e0100"
          "0000000000000000000000000000000000000000ff02000000020316";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1 },
        {"Fee",           false, 2 },
        {"Storage limit", false, 3 },
        {"Amount",        false, 4 },
        {"Destination",   false, 5 },
        //     {"Option",        _,     6 },
        {"Source",        false, 7 },
        {"Fee",           false, 8 },
        {"Storage limit", false, 9 },
        {"Amount",        false, 10},
        {"Destination",   false, 11},
        //     {"Option",        _,     12},
        //    {"Tuple",         _,     13},
        {"Entrypoint",    false, 14},
        {"Parameter",     true,  15},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* Builtin entrypoints 0x00–0x05 not covered by other tests.
 * Base hex = finalize_unstake with the entrypoint byte swapped. */
#define _BUILTIN_EP_HEX_PREFIX \
    "030000000000000000000000000000000000000000000000000000000000000000" \
    "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031020000012bad" \
    "922d045c068660fabe19576f8506a1fa8fa3"

#define _BUILTIN_EP_HEX_SUFFIX  "00000002030b"

#define _BUILTIN_EP_FIELDS \
    {"Source",        false, 1}, \
    {"Fee",           false, 2}, \
    {"Storage limit", false, 3}, \
    {"Amount",        false, 4}, \
    {"Destination",   false, 5}, \
    {"Entrypoint",    false, 8}, \
    {"Parameter",     false, 9}

static void
test_check_builtin_entrypoint_default(void **state)
{
    operation_parser_data    *data = *state;
    char                      str[] = _BUILTIN_EP_HEX_PREFIX "ff00" _BUILTIN_EP_HEX_SUFFIX;
    const tz_fields_check     fields_check[] = { _BUILTIN_EP_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_builtin_entrypoint_root(void **state)
{
    operation_parser_data    *data = *state;
    char                      str[] = _BUILTIN_EP_HEX_PREFIX "ff01" _BUILTIN_EP_HEX_SUFFIX;
    const tz_fields_check     fields_check[] = { _BUILTIN_EP_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_builtin_entrypoint_set_delegate(void **state)
{
    operation_parser_data    *data = *state;
    char                      str[] = _BUILTIN_EP_HEX_PREFIX "ff03" _BUILTIN_EP_HEX_SUFFIX;
    const tz_fields_check     fields_check[] = { _BUILTIN_EP_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_builtin_entrypoint_remove_delegate(void **state)
{
    operation_parser_data    *data = *state;
    char                      str[] = _BUILTIN_EP_HEX_PREFIX "ff04" _BUILTIN_EP_HEX_SUFFIX;
    const tz_fields_check     fields_check[] = { _BUILTIN_EP_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_builtin_entrypoint_deposit(void **state)
{
    operation_parser_data    *data = *state;
    char                      str[] = _BUILTIN_EP_HEX_PREFIX "ff05" _BUILTIN_EP_HEX_SUFFIX;
    const tz_fields_check     fields_check[] = { _BUILTIN_EP_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_builtin_entrypoint_invalid(void **state)
{
    operation_parser_data *data = *state;
    /* Entrypoint byte 0x0a (10) is not a valid builtin: triggers default branch. */
    char str[] = _BUILTIN_EP_HEX_PREFIX "ff0a" _BUILTIN_EP_HEX_SUFFIX;
    check_parser_error(data, str, TZ_ERR_INVALID_TAG);
}

#undef _BUILTIN_EP_HEX_PREFIX
#undef _BUILTIN_EP_HEX_SUFFIX
#undef _BUILTIN_EP_FIELDS

static void
test_check_stake_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031028094ebdc03"
          "00012bad922d045c068660fabe19576f8506a1fa8fa3ff0600000002030b";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
        //    {"Tuple",         _,     7},
        {"Entrypoint",    false, 8},
        {"Parameter",     false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_unstake_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b80200310280cab5ee01"
          "00012bad922d045c068660fabe19576f8506a1fa8fa3ff0700000002030b";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
        //    {"Tuple",         _,     7},
        {"Entrypoint",    false, 8},
        {"Parameter",     false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* finalize_unstake: source == destination (Paris self-call, no sponsored note). */
static void
test_check_finalize_unstake_complexity(void **state)
{
    operation_parser_data *data = *state;
    /* Source and destination are the same tz2 address: no Note emitted. */
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b802003102000001f6"
          "552df4f5ff51c3d13347cab045cfdb8b9bd803ff0800000002030b";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
        //    {"Tuple",         _,     7},
        {"Entrypoint",    false, 8},
        {"Parameter",     false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* finalize_unstake: source != destination (Seoul sponsored fee payer). */
static void
test_check_sponsored_finalize_unstake(void **state)
{
    operation_parser_data *data = *state;
    /* Source (fee payer) differs from destination (staker): Note emitted. */
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031020000012bad"
          "922d045c068660fabe19576f8506a1fa8fa3ff0800000002030b";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
        //    {"Tuple",         _,     7},
        {"Entrypoint",    false, 8},
        {"Note",          false, 9},
        {"Parameter",     false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* finalize_unstake: KT1 (originated) destination.
 * destination[0] = 0x01 ≠ 0x00 → tz_implicit_fee_payer_differs_from_dest
 * returns true (line 409), Note field shown. */
static void
test_check_finalize_unstake_kt1_dest(void **state)
{
    operation_parser_data *data = *state;
    /* Same source/fee/NATs as sponsored test; destination replaced with
     * KT1 (type=0x01, 20-byte zeroed hash, trailing 0x00). */
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd8c0b80200310200"
          "01000000000000000000000000000000000000000000ff0800000002030b";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        {"Entrypoint",    false, 8},
        {"Note",          false, 9},
        {"Parameter",     false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* Ballot operation: intercept at TZ_OPERATION_STEP_READ_BALLOT FEED_ME,
 * set skip=1 to exercise tz_print_string skip path (lines 419-421). */
static void
test_check_print_string_skip(void **state)
{
    operation_parser_data *data = *state;
    /* All ballot bytes except the final ballot byte (fed separately). */
    static const uint8_t ballot_body[] = {
        0x03,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x06,
        0x00,
        0xff,0xdd,0x61,0x02,0x32,0x1b,0xc2,0x51,
        0xe4,0xa5,0x19,0x0a,0xd5,0xb1,0x2b,0x25,
        0x10,0x69,0xd9,0xb4,
        0x00,0x00,0x00,0x20,
        0x0b,0xcd,0x7b,0x2c,0xad,0xcd,0x87,0xec,
        0xb0,0xd5,0xc5,0x03,0x30,0xfb,0x59,0xfe,
        0xed,0x74,0x32,0xbf,0xfe,0xce,0xde,0x8a,
        0x09,0xa2,0xb8,0x6c,0xfb,0x33,0x84,0x7b,
    };
    static const uint8_t ballot_vote = 0x00;

    tz_operation_parser_set_size(data->state,
                                 (uint16_t)(sizeof(ballot_body) + 1));
    tz_parser_state *st = data->state;
    tz_parser_refill(st, ballot_body, sizeof(ballot_body));

    bool skipped = false;
    while (true) {
        while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {}
        switch (st->errno) {
        case TZ_BLO_FEED_ME:
            if (!skipped
                && st->operation.frame != NULL
                && st->operation.frame->step
                       == TZ_OPERATION_STEP_READ_BALLOT) {
                st->operation.frame->step_read_string.skip = 1;
                skipped = true;
            }
            tz_parser_refill(st, &ballot_vote, 1);
            continue;
        case TZ_BLO_IM_FULL:
            tz_parser_flush(st, data->obuf, data->olen);
            continue;
        case TZ_BLO_DONE:
            assert_true(skipped);
            return;
        default:
            fail_msg("unexpected error: %s",
                     tz_parser_result_name(st->errno));
        }
    }
}

static void
test_check_set_delegate_parameters_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031020000012bad"
          "922d045c068660fabe19576f8506a1fa8fa3ff090000001007070080a4e8030707"
          "0080b48913030b";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        //     {"Option",        _,     6},
        //    {"Tuple",         _,     7},
        {"Entrypoint",         false, 8},
        {"Limit (stake/bake)", false, 9},
        {"Edge (bake/stake)",  false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP with non-Pair parameter falls back to raw binary display. */
static void
test_check_set_delegate_parameters_sdp_fallback(void **state)
{
    operation_parser_data *data = *state;
    /* set_delegate_parameters with {int: 0} → sdp fallback to binary.
     * Reuses same source/dest/fee bytes as finalize_unstake test. */
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031020000012bad"
          "922d045c068660fabe19576f8506a1fa8fa3ff09000000020000";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        {"Entrypoint",    false, 8},
        {"Parameter",     true,  9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP fallback triggered deep in the state machine (SDP_STEP_UNIT_OP).
 * Same structure as the working SDP test but final Unit opcode (0x0b) replaced
 * with 0x12.  Verifies the buffer rewind is correct after 15 bytes consumed. */
static void
test_check_set_delegate_parameters_sdp_fallback_late(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031020000012bad"
          "922d045c068660fabe19576f8506a1fa8fa3ff090000001007070080a4e8030707"
          "0080b489130312";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
        {"Entrypoint",    false, 8},
        {"Parameter",     true,  9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/*
 * SDP fallback at each SDP sub-step.  Each parameter starts with bytes that
 * pass all earlier SDP checks then intentionally fails one, triggering
 * sdp_fail_to_micheline().  After the rewind the Micheline parser parses
 * the bytes from the start — so each parameter must also be valid Micheline.
 *
 * Prefixes common to all six tests:
 *   header(32) + tag(1) + source(21) + fee+counter+gas+storage+amount(7)
 *   + destination(22)  →  same as the other SDP tests.
 */
#define _SDP_FALLBACK_HEX_PREFIX \
    "030000000000000000000000000000000000000000000000000000000000000000" \
    "6c01f6552df4f5ff51c3d13347cab045cfdb8b9bd803c0b8020031020000012bad" \
    "922d045c068660fabe19576f8506a1fa8fa3"

#define _SDP_FALLBACK_FIELDS \
    {"Source",        false, 1}, \
    {"Fee",           false, 2}, \
    {"Storage limit", false, 3}, \
    {"Amount",        false, 4}, \
    {"Destination",   false, 5}, \
    {"Entrypoint",    false, 8}, \
    {"Parameter",     true,  9}

/* SDP_STEP_OUTER_PAIR_OP: tag=0x07 OK, opcode=0x08 (≠Pair 0x07) → fallback.
 * Micheline: PRIM_2_NOANNOTS(Right, Unit, Unit) = 6 bytes. */
static void
test_check_set_delegate_parameters_sdp_fallback_outer_pair_op(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff0900000006" "0708030b030b";
    const tz_fields_check fields_check[] = { _SDP_FALLBACK_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP_STEP_FIRST_INT_TAG: tag=0x07 OK, opcode=0x07 OK, byte=0x03 (≠INT 0x00) → fallback.
 * Micheline: PRIM_2_NOANNOTS(Pair, Unit, Unit) = 6 bytes. */
static void
test_check_set_delegate_parameters_sdp_fallback_first_int_tag(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff0900000006" "0707030b030b";
    const tz_fields_check fields_check[] = { _SDP_FALLBACK_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP_STEP_INNER_PAIR_TAG: outer Pair+INT(1) OK, next byte=0x03 (≠0x07) → fallback.
 * Micheline: PRIM_2_NOANNOTS(Pair, INT(1), Unit) = 6 bytes. */
static void
test_check_set_delegate_parameters_sdp_fallback_inner_pair_tag(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff0900000006" "07070001030b";
    const tz_fields_check fields_check[] = { _SDP_FALLBACK_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP_STEP_INNER_PAIR_OP: inner tag=0x07 OK, opcode=0x03 (≠0x07) → fallback.
 * Micheline: PRIM_2_NOANNOTS(Pair, INT(1), PRIM_2_NOANNOTS(False, Unit, Unit))
 * = 10 bytes. */
static void
test_check_set_delegate_parameters_sdp_fallback_inner_pair_op(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff090000000a" "070700010703030b030b";
    const tz_fields_check fields_check[] = { _SDP_FALLBACK_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP_STEP_EDGE_INT_TAG: inner Pair OK, next byte=0x03 (≠INT 0x00) → fallback.
 * Micheline: PRIM_2_NOANNOTS(Pair, INT(1), PRIM_2_NOANNOTS(Pair, Unit, Unit))
 * = 10 bytes. */
static void
test_check_set_delegate_parameters_sdp_fallback_edge_int_tag(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff090000000a" "070700010707030b030b";
    const tz_fields_check fields_check[] = { _SDP_FALLBACK_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP_STEP_UNIT_PRIM0: both ints OK, next byte=0x00 (≠PRIM_0 0x03) → fallback.
 * Micheline: PRIM_2_NOANNOTS(Pair, INT(1), PRIM_2_NOANNOTS(Pair, INT(1), INT(1)))
 * = 10 bytes. */
static void
test_check_set_delegate_parameters_sdp_fallback_unit_prim0(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff090000000a" "07070001070700010001";
    const tz_fields_check fields_check[] = { _SDP_FALLBACK_FIELDS };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* SDP_STEP_UNIT_OP: declared param_size=17 but only 16 bytes are valid SDP
 * (Pair int (Pair int Unit)).  After reading Unit the stop check fires
 * (lines 1260-1263), sdp_fail_to_micheline rewinds, micheline then sees
 * 16 consumed < 17 declared → tz_raise(TOO_LARGE). */
static void
test_check_sdp_unit_op_trailing_data(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff090000001107070080a4e80307070080b48913030b00";
    check_parser_error(data, str, TZ_ERR_TOO_LARGE);
}

/* SDP_STEP_DONE: call parser once without flush after "Edge (bake/stake)"
 * PRINT completes, so oofs>0 when SDP_DONE runs → tz_stop(IM_FULL) line 1285. */
static void
test_check_sdp_done_oofs(void **state)
{
    operation_parser_data *data = *state;
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff090000001007070080a4e8030707"
                 "0080b48913030b";
    fill_data_str(data, str);
    tz_operation_parser_set_size(data->state, (uint16_t)data->str_len);
    tz_parser_state *st = data->state;
    bool sdp_done_tested = false;
    while (true) {
        while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {}
        switch (st->errno) {
        case TZ_BLO_FEED_ME:
            refill(data);
            tz_parser_refill(st, data->ibuf, data->ilen);
            continue;
        case TZ_BLO_IM_FULL:
            if (!sdp_done_tested
                && strcmp(st->field_info.field_name,
                          "Edge (bake/stake)") == 0
                && st->operation.frame != NULL
                && st->operation.frame->step
                       == TZ_OPERATION_STEP_READ_SET_DELEGATE_PARAMS) {
                sdp_done_tested = true;
                while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {}
                assert_int_equal(st->errno, TZ_BLO_IM_FULL);
            }
            tz_parser_flush(st, data->obuf, data->olen);
            continue;
        case TZ_BLO_DONE:
            assert_true(sdp_done_tested);
            return;
        default:
            fail_msg("unexpected: %s", tz_parser_result_name(st->errno));
        }
    }
}

/* SDP default branch (lines 1288-1289): intercept at FEED_ME when the SDP
 * frame is active, set sub_step=255, provide no more bytes → switch(255)
 * → default → tz_raise(INVALID_STATE). */
static void
test_check_sdp_invalid_sub_step(void **state)
{
    operation_parser_data *data = *state;
    /* One-byte SDP payload (0x07 = PRIM_2_NOANNOTS).  Parser reaches
     * SDP_STEP_OUTER_PAIR_OP and needs another byte → FEED_ME. */
    char str[] = _SDP_FALLBACK_HEX_PREFIX
                 "ff090000000107";
    fill_data_str(data, str);
    tz_operation_parser_set_size(data->state, (uint16_t)data->str_len);
    tz_parser_state *st = data->state;
    bool invalid_tested = false;
    while (true) {
        while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {}
        switch (st->errno) {
        case TZ_BLO_FEED_ME:
            if (!invalid_tested
                && st->operation.frame != NULL
                && st->operation.frame->step
                       == TZ_OPERATION_STEP_READ_SET_DELEGATE_PARAMS) {
                st->operation.frame->step_read_sdp.sub_step = 255;
                invalid_tested = true;
            }
            refill(data);
            tz_parser_refill(st, data->ibuf, data->ilen);
            continue;
        case TZ_BLO_IM_FULL:
            tz_parser_flush(st, data->obuf, data->olen);
            continue;
        default:
            assert_true(invalid_tested);
            assert_int_equal(st->errno, TZ_ERR_INVALID_STATE);
            return;
        }
    }
}

#undef _SDP_FALLBACK_HEX_PREFIX
#undef _SDP_FALLBACK_FIELDS

static void
test_check_origination_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6d00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e020304a0c21e000000"
          "0002037a0000000a07650100000001310002";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Balance",       false, 4},
        {"Delegate",      false, 5}, // None
        {"Code",          true,  6},
        {"Storage",       true,  7},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_delegation_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6e01774d99da021b92d8c3dfc2e814c7658440319be2c09a0cf40509f906ff0059"
          "1e842444265757d6a65e3670ca18b5e662f9c0";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        //     {"Option",        _,     4},
        {"Delegate",      false, 5},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_register_global_constant_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6f00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e0203040000000a0707"
          "0100000001310002";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Value",         true,  4},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_set_deposit_limit_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "70027c252d3806e6519ed064026bdb98edf866117331e0d40304f80204ffa09c0"
          "1";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        //     {"Option",        _,     4},
        {"Staking limit", false, 5},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_increase_paid_storage_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "7100ffdd6102321bc251e4a5190ad5b12b251069d9b4904e020304050100000000"
          "0000000000000000000000000000000000";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Destination",   false, 5},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_set_consensus_key_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "7200c921d4487c90b4472da6cc566a58d79f0d991dbf904e02030400747884d9ab"
          "df16b3ab745158925f567e222f71225501826fa83347f6cbe9c393ff0000006086"
          "24f9a019acb6ce38a73cc3f6f09972d873c28affe25e6f02af57650fad1f6017fa"
          "0b5dad6b9b3a06a6f329bd44f84213c215157d2b5f71d0c1cacaf632ec42165f3a"
          "bc6637c69d073fc6bb84893c06a5cb6691c8b0876f76430e26800c3192";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Public key",    false, 4},
        //     {"Option",        _,     5},
        {"Proof",         false, 6},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_set_companion_key_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "7300c921d4487c90b4472da6cc566a58d79f0d991dbf904e02030400747884d9ab"
          "df16b3ab745158925f567e222f71225501826fa83347f6cbe9c393ff0000006086"
          "24f9a019acb6ce38a73cc3f6f09972d873c28affe25e6f02af57650fad1f6017fa"
          "0b5dad6b9b3a06a6f329bd44f84213c215157d2b5f71d0c1cacaf632ec42165f3a"
          "bc6637c69d073fc6bb84893c06a5cb6691c8b0876f76430e26800c3192";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Public key",    false, 4},
        //     {"Option",        _,     5},
        {"Proof",         false, 6},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_transfer_ticket_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "9e00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000002037a"
          "0000000a076501000000013100020000ffdd6102321bc251e4a5190ad5b12b2510"
          "69d9b4010100000000000000000000000000000000000000000000000007646566"
          "61756c74";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Contents",      true,  4},
        {"Type",          true,  5},
        {"Ticketer",      false, 6},
        {"Amount",        false, 7},
        {"Destination",   false, 8},
        {"Entrypoint",    false, 9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_sc_rollup_add_messages_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "c900ffdd6102321bc251e4a5190ad5b12b251069d9b4904e020304000000140000"
          "000301234500000001670000000489abcdef";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Message",       false, 4},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_sc_rollup_execute_outbox_message_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "ce00ffdd6102321bc251e4a5190ad5b12b251069d9b4904e020304000000000000"
          "000000000000000000000000000000000000000000000000000000000000000000"
          "00000000000000000000000000000000c639663039663239353264333435323863"
          "373333663934363135636663333962633535353631396663353530646434613637"
          "626132323038636538653836376161336431336136656639396466626533326336"
          "393734616139613231353064323165636132396333333439653539633133623930"
          "383166316331316234343061633464333435356465646265346565306465313561"
          "386166363230643463383632343764396431333264653162623664613233643566"
          "6639643864666664613232626139613834";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Rollup",        false, 4},
        {"Commitment",    false, 5},
        {"Output proof",  true,  6},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_sc_rollup_originate_complexity(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "c800ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000000c639"
          "663039663239353264333435323863373333663934363135636663333962633535"
          "353631396663353530646434613637626132323038636538653836376161336431"
          "336136656639396466626533326336393734616139613231353064323165636132"
          "396333333439653539633133623930383166316331316234343061633464333435"
          "356465646265346565306465313561386166363230643463383632343764396431"
          "333264653162623664613233643566663964386466666461323262613961383400"
          "00000a07070100000001310002ff0000003f00ffdd6102321bc251e4a5190ad5b1"
          "2b251069d9b401f6552df4f5ff51c3d13347cab045cfdb8b9bd8030278eb8b6ab9"
          "a768579cd5146b480789650c83f28e";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Kind",          false, 4},
        {"Kernel",        true,  5},
        {"Parameters",    true,  6},
        //     {"Option",        _,     7},
        {"Whitelist",     false, 8},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* Hex strings generated by scripts/gen_fa2_ctest_hex.py (see script
 * docstring). */
static void
test_check_fa2_transfer_clear_signing_fields(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4e807016464000105001a8a"
          "f813094ee8bf162ac2093a8937e8ba830001ff087472616e7366657200000068"
          "0200000063070701000000244b543151576462415376615458573847576668664e"
          "68334a4d6a6758766e5a4141544a570200000033070701000000247372314d7943"
          "77523833685a70684353716159535141705078504d65796b734a57576e68070700"
          "000080b518";
    const tz_fields_check fields_check[] = {
        {"Source",             false, 1 },
        {"Fee",                false, 2 },
        {"Storage limit",      false, 3 },
        {"Amount",             false, 4 },
        // Destination omitted from review for registered FA2 contracts
        //     {"Option",        _,     5},
        //    {"Tuple",         _,     6},
        {"Entrypoint",         false, 8 },
        {"Transfer tokens to", false, 10},
        {"Token Amount",       false, 11},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_fa2_transfer_fallback_uses_complex_parameter(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4e807016464000105001a8a"
          "f813094ee8bf162ac2093a8937e8ba830001ff087472616e736665720000006702"
          "00000062070701000000244b543151576462415376615458573847576668664e68"
          "33"
          "4a4d6a6758766e5a4141544a570200000032070701000000247372314d79437752"
          "38"
          "33685a70684353716159535141705078504d65796b734a57576e680707000100a4"
          "01";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Entrypoint",    false, 8},
        {"Parameter",     true,  9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_fa2_transfer_multi_item_fallback(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4e807016464000105001a8a"
          "f813094ee8bf162ac2093a8937e8ba830001ff087472616e736665720000009902"
          "00000094070701000000244b543151576462415376615458573847576668664e68"
          "33"
          "4a4d6a6758766e5a4141544a570200000064070701000000247372314d79437752"
          "38"
          "33685a70684353716159535141705078504d65796b734a57576e680707000000a4"
          "01070701000000247372314d794377523833685a70684353716159535141705078"
          "50"
          "4d65796b734a57576e6807070000008803";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Entrypoint",    false, 8},
        {"Parameter",     true,  9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

static void
test_check_fa2_transfer_negative_amount_fallback(void **state)
{
    operation_parser_data *data = *state;
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4e807016464000105001a8a"
          "f813094ee8bf162ac2093a8937e8ba830001ff087472616e736665720000006602"
          "00000061070701000000244b543151576462415376615458573847576668664e68"
          "33"
          "4a4d6a6758766e5a4141544a570200000031070701000000247372314d79437752"
          "38"
          "33685a70684353716159535141705078504d65796b734a57576e6807070000004"
          "1";
    const tz_fields_check fields_check[] = {
        {"Source",        false, 1},
        {"Fee",           false, 2},
        {"Storage limit", false, 3},
        {"Amount",        false, 4},
        {"Entrypoint",    false, 8},
        {"Parameter",     true,  9},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}

/* FA2 transfer where amount is a whole number (no fractional part). */
static void
test_check_fa2_transfer_whole_number_amount(void **state)
{
    operation_parser_data *data = *state;
    /* amount = 2000000, QUIPU 6 decimals → "2 QUIPU" (no_decimals=1 path) */
    char str[]
        = "030000000000000000000000000000000000000000000000000000000000000000"
          "6c00ffdd6102321bc251e4a5190ad5b12b251069d9b4e807016464000105001a8a"
          "f813094ee8bf162ac2093a8937e8ba8300ffff087472616e736665720000006902"
          "00000064070701000000244b543151576462415376615458573847576668664e68"
          "334a4d6a6758766e5a4141544a570200000034070701000000247372314d794377"
          "523833685a70684353716159535141705078504d65796b734a57576e6807070000"
          "008092f401";
    const tz_fields_check fields_check[] = {
        {"Source",             false, 1 },
        {"Fee",                false, 2 },
        {"Storage limit",      false, 3 },
        {"Amount",             false, 4 },
        {"Entrypoint",         false, 8 },
        {"Transfer tokens to", false, 10},
        {"Token Amount",       false, 11},
    };
    check_field_complexity(data, str, fields_check, sizeof(fields_check));
}


int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_check_proposals_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_ballot_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_failing_noop_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_reveal_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_simple_transaction_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_transaction_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_double_transaction_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_builtin_entrypoint_default, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_builtin_entrypoint_root, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_builtin_entrypoint_set_delegate, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_builtin_entrypoint_remove_delegate, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_builtin_entrypoint_deposit, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_builtin_entrypoint_invalid, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_stake_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_unstake_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_finalize_unstake_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sponsored_finalize_unstake, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_finalize_unstake_kt1_dest, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_print_string_skip, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_late, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_outer_pair_op, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_first_int_tag, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_inner_pair_tag, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_inner_pair_op, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_edge_int_tag, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_delegate_parameters_sdp_fallback_unit_prim0, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sdp_unit_op_trailing_data, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sdp_done_oofs, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sdp_invalid_sub_step, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_origination_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_delegation_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_register_global_constant_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_deposit_limit_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_increase_paid_storage_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_consensus_key_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_set_companion_key_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_transfer_ticket_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sc_rollup_add_messages_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sc_rollup_execute_outbox_message_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_sc_rollup_originate_complexity, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_fa2_transfer_clear_signing_fields, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_fa2_transfer_fallback_uses_complex_parameter, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_fa2_transfer_multi_item_fallback, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_fa2_transfer_negative_amount_fallback, operation_parser_setup, operation_parser_teardown),
        cmocka_unit_test_setup_teardown(test_check_fa2_transfer_whole_number_amount, operation_parser_setup, operation_parser_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
