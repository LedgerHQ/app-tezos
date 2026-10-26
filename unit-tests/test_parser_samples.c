/* Tezos Embedded C parser for Ledger - parser tests against committed samples

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

/* Replays the samples of tests/samples/<kind>/nano/ through the C parser.

   `samples.hex` holds the binary encoding of one Micheline expression or one
   operation batch per line, and `samples.expected` the hex of what the parser
   must render for the same line. Both are generated once, from Octez, by
   tests/generate (see tests/samples/README.md): running this test needs
   neither Octez nor OCaml.

   Each sample is fed in pseudo-random chunks of input, and drained through
   pseudo-random chunks of output, to exercise the FEED_ME / IM_FULL
   resumption of the parser. The PRNG seed is fixed, so a failure can be
   reproduced. */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "micheline_parser.h"
#include "operation_parser.h"

#ifndef SAMPLES_DIR
#error "SAMPLES_DIR must be defined as the path of tests/samples"
#endif

#define MAX_INPUT_CHUNK  234
#define MAX_OUTPUT_CHUNK 100
#define MAX_REPORTED     5
/* A parser that neither progresses nor blocks must not hang the test. */
#define MAX_STEPS        1000000

typedef enum {
    SAMPLES_MICHELINE,
    SAMPLES_OPERATIONS,
} samples_kind;

typedef enum {
    RUN_OK,
    RUN_TOO_DEEP,   // data the parser deliberately refuses, see below
    RUN_TOO_LARGE,  // data the parser deliberately refuses, see below
    RUN_ERROR,
} run_status;

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} string_buffer;

static uint32_t
prng_next(uint32_t *seed)
{
    uint32_t x = *seed;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;
    return x;
}

static bool
string_buffer_append(string_buffer *buf, const char *src, size_t len)
{
    if (buf->len + len + 1 > buf->cap) {
        size_t cap = buf->cap == 0 ? 256 : buf->cap;
        char  *data;

        while (buf->len + len + 1 > cap) {
            cap *= 2;
        }
        data = realloc(buf->data, cap);
        if (data == NULL) {
            return false;
        }
        buf->data = data;
        buf->cap  = cap;
    }
    memcpy(buf->data + buf->len, src, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return true;
}

static int
hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Decode a line of hex into a malloc'ed, NUL terminated buffer. The trailing
   newline, if any, is not part of the line. */
static uint8_t *
hex_decode(const char *line, size_t line_len, size_t *out_len)
{
    uint8_t *out;

    if (line_len > 0 && line[line_len - 1] == '\n') {
        line_len--;
    }
    if (line_len % 2 != 0) {
        return NULL;
    }
    out = malloc(line_len / 2 + 1);
    if (out == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < line_len / 2; i++) {
        int hi = hex_digit(line[2 * i]);
        int lo = hex_digit(line[2 * i + 1]);

        if (hi < 0 || lo < 0) {
            free(out);
            return NULL;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    out[line_len / 2] = 0;
    *out_len          = line_len / 2;
    return out;
}

/* Feed `in` to a fresh parser the way the Octez-based reference harness does:
   the parser is initialised for the whole input, then stepped on random input
   and output windows. `out` receives everything the parser wrote. */
static run_status
run_sample(samples_kind kind, const uint8_t *in, size_t in_len,
           uint32_t *seed, string_buffer *out, tz_parser_result *err)
{
    tz_parser_state *state;
    size_t           ofs   = 0;
    size_t           steps = 0;
    run_status       res   = RUN_ERROR;

    *err = TZ_CONTINUE;
    if (in_len >= TZ_UNKNOWN_SIZE) {
        return RUN_ERROR;
    }
    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return RUN_ERROR;
    }
    /* Operations are parsed with a known size and without magic byte. The
       reference harness also does it for bare Micheline expressions. */
    tz_micheline_parser_init(state);
    tz_operation_parser_init(state, (uint16_t)in_len, true);

    while (steps++ < MAX_STEPS) {
        size_t input_len  = 1 + prng_next(seed) % MAX_INPUT_CHUNK;
        size_t output_len = 1 + prng_next(seed) % MAX_OUTPUT_CHUNK;
        size_t chunk = in_len - ofs < input_len ? in_len - ofs : input_len;
        char   obuf[MAX_OUTPUT_CHUNK];
        size_t read;
        size_t written;

        memset(obuf, ' ', sizeof(obuf));
        state->regs.ibuf = in;
        state->regs.iofs = ofs;
        state->regs.ilen = chunk;
        state->regs.obuf = obuf;
        state->regs.oofs = 0;
        state->regs.olen = output_len;

        if (kind == SAMPLES_MICHELINE) {
            while (!TZ_IS_BLOCKED(tz_micheline_parser_step(state))) {
                // Loop while the result is successful and not blocking
            }
        } else {
            while (!TZ_IS_BLOCKED(tz_operation_parser_step(state))) {
                // Loop while the result is successful and not blocking
            }
        }

        read    = chunk - state->regs.ilen;
        written = output_len - state->regs.olen;
        if (!string_buffer_append(out, obuf, written)) {
            break;
        }

        *err = state->errno;
        if (state->errno == TZ_BLO_DONE) {
            res = RUN_OK;
            break;
        } else if (state->errno == TZ_BLO_FEED_ME) {
            if (in_len - ofs == 0) {
                // Out of input but the parser wants more
                break;
            }
            ofs += read;
        } else if (state->errno == TZ_BLO_IM_FULL) {
            ofs += read;
        } else {
            res = state->errno == TZ_ERR_TOO_DEEP    ? RUN_TOO_DEEP
                  : state->errno == TZ_ERR_TOO_LARGE ? RUN_TOO_LARGE
                                                     : RUN_ERROR;
            break;
        }
    }
    free(state);
    return res;
}

static void
report_mismatch(size_t index, const char *expected, const char *got,
                tz_parser_result err)
{
    print_message("sample %zu: ", index);
    if (err != TZ_BLO_DONE) {
        print_message("parser stopped on %s\n", tz_parser_result_name(err));
    }
    print_message("  expected: %s\n  got:      %s\n", expected, got);
}

static void
check_samples(samples_kind kind, const char *dir)
{
    char     hex_path[512];
    char     expected_path[512];
    FILE    *hex_fp;
    FILE    *expected_fp;
    char    *hex_line      = NULL;
    char    *expected_line = NULL;
    size_t   hex_cap       = 0;
    size_t   expected_cap  = 0;
    uint32_t seed          = 0x2545F491u;
    size_t   total = 0, ok = 0, too_deep = 0, too_large = 0, failed = 0;
    ssize_t  hex_len, expected_len;

    snprintf(hex_path, sizeof(hex_path), "%s/%s/samples.hex", SAMPLES_DIR,
             dir);
    snprintf(expected_path, sizeof(expected_path), "%s/%s/samples.expected",
             SAMPLES_DIR, dir);
    hex_fp      = fopen(hex_path, "r");
    expected_fp = fopen(expected_path, "r");
    if (hex_fp == NULL || expected_fp == NULL) {
        fail_msg(
            "cannot open %s or %s, the fixtures are generated by "
            "tests/generate, see tests/samples/README.md",
            hex_path, expected_path);
    }

    while ((hex_len = getline(&hex_line, &hex_cap, hex_fp)) > 0) {
        size_t           in_len, want_len;
        uint8_t         *in, *want;
        string_buffer    out = {0};
        tz_parser_result err;
        run_status       status;

        expected_len = getline(&expected_line, &expected_cap, expected_fp);
        assert_true(expected_len > 0);

        in   = hex_decode(hex_line, (size_t)hex_len, &in_len);
        want = hex_decode(expected_line, (size_t)expected_len, &want_len);
        assert_non_null(in);
        assert_non_null(want);

        status = run_sample(kind, in, in_len, &seed, &out, &err);
        if (status == RUN_OK
            && (out.len != want_len
                || (want_len > 0 && memcmp(out.data, want, want_len) != 0))) {
            // The parser went through, but not to the expected rendering
            status = RUN_ERROR;
        }
        // Too deep and too large data are refused by design, on devices that
        // cannot hold them. They are not a mismatch, but they are counted.
        switch (status) {
        case RUN_OK:
            ok++;
            break;
        case RUN_TOO_DEEP:
            too_deep++;
            break;
        case RUN_TOO_LARGE:
            too_large++;
            break;
        case RUN_ERROR:
            if (failed++ < MAX_REPORTED) {
                report_mismatch(total, (const char *)want,
                                out.data != NULL ? out.data : "", err);
            }
            break;
        }
        total++;
        free(out.data);
        free(in);
        free(want);
    }
    // Both files must have the same number of lines
    assert_true(getline(&expected_line, &expected_cap, expected_fp) < 0);

    free(hex_line);
    free(expected_line);
    fclose(hex_fp);
    fclose(expected_fp);

    print_message(
        "%s: %zu samples, %zu ok, %zu too deep, %zu too large, %zu failed\n",
        dir, total, ok, too_deep, too_large, failed);
    assert_true(total > 0);
    assert_int_equal(failed, 0);
    // The refused samples must stay the exception, or the test checks nothing
    assert_true(ok * 2 >= total);
}

static void
test_micheline_samples(void **state)
{
    (void)state;
    check_samples(SAMPLES_MICHELINE, "micheline/nano");
}

static void
test_operations_samples(void **state)
{
    (void)state;
    check_samples(SAMPLES_OPERATIONS, "operations/nano");
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_micheline_samples),
        cmocka_unit_test(test_operations_samples),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
