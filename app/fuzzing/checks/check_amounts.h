#pragma once
/* Checks of the number and amount helpers of the parsers, for the fuzz
 * targets that call run_amounts().
 *
 * Input: [function][slack][decimals][symbol length][payload]
 *
 * The helpers of the operation parser that format amounts are static, so its
 * source has to be included before this file. Each one is called on a buffer
 * of exactly the size it is told, so sanitizers catch any access past it, and
 * its result is compared with a reference written the other way around: from
 * the specification, building a new string instead of shifting the digits in
 * place.
 */

#include <stdlib.h>

#include "fuzz_check.h"
#include "fuzz_ref.h"
#include "num_parser.h"

#define MAX_DIGITS 120
#define SENTINEL   0xDEADBEEFCAFEF00DULL

typedef enum {
    AFN_AMOUNT,
    AFN_PREPEND_MINUS,
    AFN_TOKEN_AMOUNT,
    AFN_U64_TO_STRING,
    AFN_FA2_DECIMAL,
    AFN_MUTEZ,
    AFN_NUM,
    AFN_COUNT,
} amounts_fn_t;

/* `n` decimal digits made of `src`, as a string in a buffer of `n + 1 +
 * slack` bytes, whose spare bytes are garbage */
static char *
make_digits(const uint8_t *src, size_t n, size_t slack)
{
    char *s = malloc(n + 1 + slack);

    if (s == NULL) {
        abort();
    }
    for (size_t i = 0; i < n; i++) {
        s[i] = (char)('0' + (src[i] % 10));
    }
    s[n] = '\0';
    memset(s + n + 1, 'Z', slack);
    return s;
}

/* The bytes of `src` as a string, in a buffer of exactly its size. With
 * `digits`, they are made decimal digits first. */
static char *
make_string(const uint8_t *src, size_t n, bool digits)
{
    char *s = malloc(n + 1);

    if (s == NULL) {
        abort();
    }
    for (size_t i = 0; i < n; i++) {
        s[i] = digits ? (char)('0' + (src[i] % 10)) : (char)src[i];
    }
    s[n] = '\0';
    return s;
}

static char *
copy_string(const char *s)
{
    char *c = strdup(s);

    if (c == NULL) {
        abort();
    }
    return c;
}

/* The amount in tez of `n` digits of mutez: at least 7 digits, six of them
 * the fraction, which loses its trailing zeros */
static void
ref_amount(const char *digits, size_t n, char *out)
{
    char   padded[MAX_DIGITS + 8];
    size_t m = n;
    size_t int_len, frac_len, o;

    if (n < 7) {
        memset(padded, '0', 7 - n);
        memcpy(padded + 7 - n, digits, n);
        m = 7;
    } else {
        memcpy(padded, digits, n);
    }

    int_len  = m - 6;
    frac_len = 6;
    while (frac_len > 0 && padded[int_len + frac_len - 1] == '0') {
        frac_len--;
    }

    memcpy(out, padded, int_len);
    o = int_len;
    if (frac_len > 0) {
        out[o++] = '.';
        memcpy(out + o, padded + int_len, frac_len);
        o += frac_len;
    }
    memcpy(out + o, " XTZ", sizeof(" XTZ"));
}

/* The amount of a token, with the same steps and the same refusals as
 * tz_format_token_amount, on a copy: what does not fit leaves the digits as
 * they are, and without the symbol */
static void
ref_token_amount(const char *digits, size_t bufsize, unsigned decimals,
                 const char *symbol, char *out)
{
    char   d[512];
    size_t n = strlen(digits);

    if (n == 0) {
        if (bufsize < 2) {
            out[0] = '\0';
            return;
        }
        strcpy(d, "0");
        n = 1;
    } else {
        memcpy(d, digits, n + 1);
    }

    if (decimals > 0) {
        bool zero_frac = true;

        // At least one digit for the integer part
        if (n <= decimals) {
            size_t pad = decimals + 1 - n;

            if (n + pad + 1 > bufsize) {
                strcpy(out, d);
                return;
            }
            memmove(d + pad, d, n + 1);
            memset(d, '0', pad);
            n += pad;
        }

        for (size_t i = 0; i < decimals; i++) {
            zero_frac = zero_frac && (d[n - 1 - i] == '0');
        }
        if (zero_frac) {
            n -= decimals;
            d[n] = '\0';
        } else {
            // Room for the '.' and the terminator
            if (n + 1 >= bufsize) {
                strcpy(out, d);
                return;
            }
            memmove(d + n - decimals + 1, d + n - decimals, decimals + 1);
            d[n - decimals] = '.';
            n++;
            while (n > 0 && d[n - 1] == '0') {
                d[--n] = '\0';
            }
            if (n > 0 && d[n - 1] == '.') {
                d[--n] = '\0';
            }
        }
    }

    if (symbol != NULL && symbol[0] != '\0') {
        size_t symbol_len = strlen(symbol);

        if (n + 1 + symbol_len + 1 <= bufsize) {
            d[n] = ' ';
            memcpy(d + n + 1, symbol, symbol_len + 1);
        }
    }
    strcpy(out, d);
}

/* The digits of `value`, as the string of a decimal number */
static void
ref_u64_to_string(uint64_t value, char *out)
{
    char   reversed[21];
    size_t n = 0;

    do {
        reversed[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);
    for (size_t i = 0; i < n; i++) {
        out[i] = reversed[n - 1 - i];
    }
    out[n] = '\0';
}

/* The value of a string of digits, false if there is a character that is not
 * one or if it does not fit in 64 bits */
static bool
ref_u64(const char *s, uint64_t *value)
{
    unsigned __int128 acc = 0;

    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        acc = (acc * 10) + (unsigned)(*s - '0');
        if (acc > UINT64_MAX) {
            return false;
        }
    }
    *value = (uint64_t)acc;
    return true;
}

/* The number that the bytes of `in` encode in Zarith: seven bits per byte,
 * low bits first, and for a signed number the first byte has six, and the
 * sign in the seventh. The magnitude comes out big-endian, without leading
 * zero byte. */
static bool
ref_zarith(const uint8_t *in, size_t n, bool natural, uint8_t *magnitude,
           size_t *magnitude_len, bool *sign)
{
    uint8_t le[80] = {0};
    size_t  pos    = 0;
    size_t  top    = sizeof(le);

    *sign = false;
    for (size_t i = 0; i < n; i++) {
        bool     first = (i == 0) && !natural;
        unsigned bits  = first ? 6 : 7;
        unsigned v     = in[i] & ((1u << bits) - 1);

        if (first) {
            *sign = ((in[i] >> 6) & 1) != 0;
        }
        if (pos + bits > sizeof(le) * 8) {
            return false;
        }
        for (unsigned k = 0; k < bits; k++) {
            if ((v >> k) & 1) {
                le[(pos + k) / 8] |= (uint8_t)(1u << ((pos + k) % 8));
            }
        }
        pos += bits;
    }

    while (top > 0 && le[top - 1] == 0) {
        top--;
    }
    for (size_t i = 0; i < top; i++) {
        magnitude[i] = le[top - 1 - i];
    }
    *magnitude_len = top;
    return true;
}

static void
fuzz_amount(const uint8_t *src, size_t len, size_t slack)
{
    size_t n       = (len > MAX_DIGITS) ? MAX_DIGITS : len;
    size_t bufsize = n + 1 + slack;
    char  *s       = make_digits(src, n, slack);
    char  *orig    = copy_string(s);
    char   ref[2 * MAX_DIGITS];
    size_t needed = ((n < 7) ? 7 : n) + 6;
    bool   rc     = tz_format_amount(s, bufsize);

    // It formats what fits, and refuses the rest without touching it
    CHECK(rc == (needed <= bufsize));
    if (rc) {
        ref_amount(orig, n, ref);
        CHECK(strcmp(s, ref) == 0);
    } else {
        CHECK(strcmp(s, orig) == 0);
    }
    free(s);
    free(orig);
}

static void
fuzz_prepend_minus(const uint8_t *src, size_t len, size_t slack)
{
    size_t n       = (len > MAX_DIGITS) ? MAX_DIGITS : len;
    size_t bufsize = n + 1 + slack;
    char  *s       = make_digits(src, n, slack);
    char  *orig    = copy_string(s);
    bool   rc      = tz_prepend_minus(s, bufsize);

    CHECK(rc == ((n + 2) <= bufsize));
    if (rc) {
        CHECK(s[0] == '-');
        CHECK(strcmp(s + 1, orig) == 0);
    } else {
        CHECK(strcmp(s, orig) == 0);
    }
    free(s);
    free(orig);
}

static void
fuzz_token_amount(const uint8_t *src, size_t len, size_t slack,
                  uint8_t decimals, size_t symbol_len)
{
    char        symbol[13];
    const char *symbol_arg;
    size_t      sl      = (symbol_len > len) ? len : symbol_len;
    size_t      n       = (len - sl > MAX_DIGITS) ? MAX_DIGITS : (len - sl);
    size_t      bufsize = n + 1 + slack;
    char       *s       = make_digits(src + sl, n, slack);
    char       *orig    = copy_string(s);
    char        ref[512];

    for (size_t i = 0; i < sl; i++) {
        symbol[i] = (char)('A' + (src[i] % 26));
    }
    symbol[sl] = '\0';
    symbol_arg = ((sl == 0) && (slack % 7 == 3)) ? NULL : symbol;

    tz_format_token_amount(s, bufsize, decimals, symbol_arg);

    CHECK(memchr(s, '\0', bufsize) != NULL);
    ref_token_amount(orig, bufsize, decimals, symbol_arg, ref);
    CHECK(strcmp(s, ref) == 0);
    free(s);
    free(orig);
}

static void
fuzz_u64_to_string(const uint8_t *src, size_t len, size_t slack)
{
    uint64_t value    = 0;
    size_t   out_size = slack % 24;
    char    *out      = malloc(out_size);
    char     ref[32];
    bool     rc;

    for (size_t i = 0; i < 8 && i < len; i++) {
        value = (value << 8) | src[i];
    }
    ref_u64_to_string(value, ref);

    rc = tz_u64_to_string(out, out_size, value);

    // It writes every digit and the terminator, or refuses
    CHECK(rc == ((out_size > 0) && (strlen(ref) + 1 <= out_size)));
    if (rc) {
        CHECK(strcmp(out, ref) == 0);
    } else if (out_size > 0) {
        CHECK(out[0] == '\0');
    }
    free(out);
}

#ifdef HAVE_SWAP
static void
fuzz_fa2_decimal(const uint8_t *src, size_t len, bool digits)
{
    size_t   n       = (len > 40) ? 40 : len;
    char    *s       = make_string(src, n, digits);
    uint64_t value   = SENTINEL;
    uint64_t want    = 0;
    bool     rc      = fa2_decimal_to_u64(s, &value);
    bool     want_rc = (s[0] != '\0') && ref_u64(s, &want);

    CHECK(rc == want_rc);
    // The value is left alone on failure
    CHECK(value == (rc ? want : SENTINEL));
    free(s);
}
#endif  // HAVE_SWAP

static void
fuzz_mutez(const uint8_t *src, size_t len, bool digits)
{
    size_t   n     = (len > 40) ? 40 : len;
    char    *s     = make_string(src, n, digits);
    uint64_t value = SENTINEL;
    uint64_t want  = 0;
    bool     rc    = tz_string_to_mutez(s, &value);

    // An empty string is accepted as zero, which is not checked here
    if (s[0] != '\0') {
        CHECK(rc == ref_u64(s, &want));
        if (rc) {
            CHECK(value == want);
        }
    }
    free(s);
}

/* The number that the parser read from `in`, as it formats it */
static void
check_num(const uint8_t *in, size_t n, bool natural,
          const tz_num_parser_buffer *buf, const tz_num_parser_regs *regs)
{
    uint8_t want[80];
    uint8_t got[400];
    size_t  want_len;
    bool    sign;
    long    got_len;

    if (!ref_zarith(in, n, natural, want, &want_len, &sign)) {
        return;
    }
    CHECK(memchr(buf->decimal, '\0', sizeof(buf->decimal)) != NULL);
    got_len = ref_decode((const char *)buf->decimal, FUZZ_DEC_ALPHABET, got,
                         sizeof(got));
    CHECK(got_len >= 0);
    CHECK((size_t)got_len == want_len);
    CHECK(memcmp(got, want, want_len) == 0);
    CHECK((regs->sign != 0) == sign);
}

static void
fuzz_num(const uint8_t *src, size_t len, bool natural)
{
    tz_num_parser_buffer *buf  = calloc(1, sizeof(*buf));
    tz_num_parser_regs   *regs = calloc(1, sizeof(*regs));

    if (buf == NULL || regs == NULL) {
        abort();
    }
    tz_parse_num_state_init(buf, regs);

    if (len > 64) {
        len = 64;
    }
    for (size_t i = 0; i < len; i++) {
        tz_parser_result res = tz_parse_num_step(buf, regs, src[i], natural);

        if (res != TZ_CONTINUE) {
            // Too large
            break;
        }
        if (regs->stop) {
            check_num(src, i + 1, natural, buf, regs);
            break;
        }
    }
    free(buf);
    free(regs);
}

static void
run_amounts(const uint8_t *data, size_t size)
{
    if (size < 4) {
        return;
    }

    amounts_fn_t   fn       = (amounts_fn_t)(data[0] % AFN_COUNT);
    size_t         slack    = data[1] % 90;
    uint8_t        decimals = data[2];
    size_t         symbol   = data[3] % 13;
    const uint8_t *rest     = data + 4;
    size_t         rest_len = size - 4;
    bool           flag     = (data[1] & 0x80) != 0;

    switch (fn) {
    case AFN_AMOUNT:
        fuzz_amount(rest, rest_len, slack);
        break;
    case AFN_PREPEND_MINUS:
        fuzz_prepend_minus(rest, rest_len, slack);
        break;
    case AFN_TOKEN_AMOUNT:
        fuzz_token_amount(rest, rest_len, slack, decimals, symbol);
        break;
    case AFN_U64_TO_STRING:
        fuzz_u64_to_string(rest, rest_len, slack);
        break;
    case AFN_FA2_DECIMAL:
#ifdef HAVE_SWAP
        fuzz_fa2_decimal(rest, rest_len, flag);
#endif
        break;
    case AFN_MUTEZ:
        fuzz_mutez(rest, rest_len, flag);
        break;
    case AFN_NUM:
        fuzz_num(rest, rest_len, flag);
        break;
    default:
        break;
    }
}
