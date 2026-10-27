#pragma once
/* Checks of the formatting helpers, for the fuzz targets that call
 * run_formatting(): decimal, base58, base58check, and the Tezos formats built
 * on them.
 *
 * Input: [function | canonical][output size][aux][payload]
 *
 *   function:    which helper to call
 *   canonical:   fix the payload up to a length and a tag the helper accepts,
 *                so that the formatting itself is reached, not only its
 *                refusals
 *   output size: around what the helper needs, so that the boundary is
 *                crossed in both directions
 *   aux:         tag, curve or prefix, depending on the helper
 *
 * The input and output buffers are exactly the size the helper is told, so
 * sanitizers catch any access past them. What is formatted is decoded back
 * and compared with what went in.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "formatting.h"
#include "fuzz_check.h"
#include "fuzz_ref.h"

#define MAX_PAYLOAD 256
#define CANONICAL   0x80
#define OUT_ENOUGH  160  // the longest output is 142 characters

typedef enum {
    FN_DECIMAL,
    FN_BASE58,
    FN_PKH,
    FN_PK,
    FN_SIG,
    FN_OPH,
    FN_BH,
    FN_ADDRESS,
    FN_BASE58CHECK,
    FN_OP_NAME,
    FN_COUNT,
} fn_t;

/* Prefixes tz_format_base58check knows, with the length of what they prefix
 */
static const struct {
    const char *name;
    size_t      len;
} prefixes[] = {
    {"B",      32},
    {"o",      32},
    {"expr",   32},
    {"proto",  32},
    {"tz1",    20},
    {"tz2",    20},
    {"tz3",    20},
    {"tz4",    20},
    {"edpk",   32},
    {"sppk",   33},
    {"p2pk",   33},
    {"BLpk",   48},
    {"sig",    64},
    {"edsig",  64},
    {"spsig1", 64},
    {"p2sig",  64},
    {"BLsig",  96},
    {"KT1",    20},
    {"txr1",   20},
    {"zkr1",   20},
    {"sr1",    20},
    {"src1",   32},
};

#define N_PREFIXES (sizeof(prefixes) / sizeof(prefixes[0]))

static const char *const unknown_prefixes[] = {"", "x", "tz5"};

#define N_UNKNOWN (sizeof(unknown_prefixes) / sizeof(unknown_prefixes[0]))

/* Size of a public key by tag: edpk, sppk, p2pk, BLpk, tag byte included */
static const size_t pk_sizes[] = {33, 34, 34, 49};

/* A sha256 of its own, from FIPS 180-4, for the checksum of base58check: it
 * does not share anything with the one the app uses. Its additions are modulo
 * 2^32 by definition, and its rotations drop bits, which the sanitizers for
 * unsigned overflow, shifts and implicit conversions of the fuzzers would
 * report on every block. */
#define REF_SHA256_MODULAR                                  \
    __attribute__((no_sanitize("unsigned-integer-overflow", \
                               "unsigned-shift-base",       \
                               "implicit-conversion")))

static const uint32_t ref_sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

REF_SHA256_MODULAR static uint32_t
ref_ror32(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32 - n));
}

REF_SHA256_MODULAR static void
ref_sha256_block(uint32_t h[8], const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t v[8];

    for (size_t t = 0; t < 16; t++) {
        w[t] = ((uint32_t)block[4 * t] << 24)
               | ((uint32_t)block[4 * t + 1] << 16)
               | ((uint32_t)block[4 * t + 2] << 8)
               | (uint32_t)block[4 * t + 3];
    }
    for (size_t t = 16; t < 64; t++) {
        uint32_t s0 = ref_ror32(w[t - 15], 7) ^ ref_ror32(w[t - 15], 18)
                      ^ (w[t - 15] >> 3);
        uint32_t s1 = ref_ror32(w[t - 2], 17) ^ ref_ror32(w[t - 2], 19)
                      ^ (w[t - 2] >> 10);

        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }

    memcpy(v, h, sizeof(v));
    for (size_t t = 0; t < 64; t++) {
        uint32_t big1
            = ref_ror32(v[4], 6) ^ ref_ror32(v[4], 11) ^ ref_ror32(v[4], 25);
        uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
        uint32_t t1 = v[7] + big1 + ch + ref_sha256_k[t] + w[t];
        uint32_t big0
            = ref_ror32(v[0], 2) ^ ref_ror32(v[0], 13) ^ ref_ror32(v[0], 22);
        uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        uint32_t t2  = big0 + maj;

        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + t2;
    }
    for (size_t i = 0; i < 8; i++) {
        h[i] += v[i];
    }
}

REF_SHA256_MODULAR static void
ref_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t  tail[128];
    size_t   full     = (len / 64) * 64;
    size_t   rem      = len - full;
    size_t   tail_len = (rem < 56) ? 64 : 128;
    uint64_t bits     = (uint64_t)len * 8;

    for (size_t off = 0; off < full; off += 64) {
        ref_sha256_block(h, data + off);
    }

    memset(tail, 0, sizeof(tail));
    if (rem > 0) {
        memcpy(tail, data + full, rem);
    }
    tail[rem] = 0x80;
    for (size_t i = 0; i < 8; i++) {
        tail[tail_len - 1 - i] = (uint8_t)(bits >> (8 * i));
    }
    for (size_t off = 0; off < tail_len; off += 64) {
        ref_sha256_block(h, tail + off);
    }

    for (size_t i = 0; i < 8; i++) {
        out[4 * i]     = (uint8_t)(h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(h[i] >> 8);
        out[4 * i + 3] = (uint8_t)h[i];
    }
}

static void
ref_sha256d(const uint8_t *data, size_t len, uint8_t out[32])
{
    uint8_t first[32];

    ref_sha256(data, len, first);
    ref_sha256(first, sizeof(first), out);
}

/* `s` is the decimal of the little-endian number `n` of `l` bytes */
static void
check_decimal(const uint8_t *n, size_t l, const char *s, size_t olen)
{
    uint8_t num[1024];
    size_t  top = l;
    long    dl;

    CHECK(memchr(s, '\0', olen) != NULL);
    dl = ref_decode(s, FUZZ_DEC_ALPHABET, num, sizeof(num));
    CHECK(dl >= 0);

    while (top > 0 && n[top - 1] == 0) {
        top--;
    }
    CHECK((size_t)dl == top);
    for (size_t i = 0; i < top; i++) {
        CHECK(num[i] == n[top - 1 - i]);
    }
    // No leading zero, except for zero itself
    CHECK((s[0] != '0') || (s[1] == '\0'));
}

/* `s` is the base58 of the `l` bytes of `n`: a leading zero byte is a leading
 * '1', and the rest is the number */
static void
check_base58(const uint8_t *n, size_t l, const char *s, size_t olen)
{
    uint8_t num[1024];
    size_t  zeros = 0;
    size_t  lead  = 0;
    long    dl;

    CHECK(memchr(s, '\0', olen) != NULL);
    while (zeros < l && n[zeros] == 0) {
        zeros++;
    }
    while (s[lead] == FUZZ_B58_ALPHABET[0]) {
        lead++;
    }
    CHECK(lead == zeros);

    dl = ref_decode(s + lead, FUZZ_B58_ALPHABET, num, sizeof(num));
    CHECK(dl >= 0);
    CHECK((size_t)dl == l - zeros);
    CHECK(memcmp(num, n + zeros, l - zeros) == 0);
}

/* `s` is a base58check string: some prefix, `payload`, and the first four
 * bytes of the double sha256 of both */
static void
check_base58check(const char *s, size_t olen, const uint8_t *payload,
                  size_t plen)
{
    uint8_t full[1024];
    uint8_t digest[32];
    size_t  lead = 0;
    size_t  total;
    long    dl;

    CHECK(memchr(s, '\0', olen) != NULL);
    while (s[lead] == FUZZ_B58_ALPHABET[0]) {
        lead++;
    }
    CHECK(lead < sizeof(full) / 2);

    memset(full, 0, lead);
    dl = ref_decode(s + lead, FUZZ_B58_ALPHABET, full + lead,
                    sizeof(full) - lead);
    CHECK(dl >= 0);
    total = lead + (size_t)dl;

    CHECK(total >= plen + 4);
    ref_sha256d(full, total - 4, digest);
    CHECK(memcmp(digest, full + total - 4, 4) == 0);
    CHECK(memcmp(full + total - 4 - plen, payload, plen) == 0);
}

static void
run_formatting(const uint8_t *data, size_t size)
{
    uint8_t     payload[MAX_PAYLOAD] = {0};
    const char *prefix               = NULL;
    size_t      prefix_idx           = 0;
    bool        known                = false;
    bool        valid                = false;  // an input the helper accepts
    size_t      len, need, olen;
    uint8_t    *ibuf;
    char       *obuf;
    int         rc = 0;

    if (size < 3) {
        return;
    }

    fn_t    fn        = (fn_t)((data[0] & ~CANONICAL) % FN_COUNT);
    bool    canonical = (data[0] & CANONICAL) != 0;
    uint8_t sel       = data[1];
    uint8_t aux       = data[2];

    len = size - 3;
    if (len > MAX_PAYLOAD) {
        len = MAX_PAYLOAD;
    }
    memcpy(payload, data + 3, len);

    switch (fn) {
    case FN_PKH:
        if (canonical) {
            payload[0] = aux % 4;
            len        = 21;
            valid      = true;
        }
        break;
    case FN_PK:
        if (canonical) {
            payload[0] = aux % 4;
            len        = pk_sizes[payload[0]];
            valid      = true;
        }
        break;
    case FN_SIG:
        if (canonical) {
            len   = (aux & 1) ? 96 : 64;
            valid = true;
        }
        break;
    case FN_OPH:
    case FN_BH:
        if (canonical) {
            len   = 32;
            valid = true;
        }
        break;
    case FN_ADDRESS:
        if (canonical) {
            payload[0] = aux % 5;
            len        = 22;
            if (payload[0] == 0) {
                // A public key hash, with its own tag
                payload[1] %= 4;
            }
            valid = true;
        }
        break;
    case FN_BASE58CHECK:
        prefix_idx = aux % (N_PREFIXES + N_UNKNOWN);
        known      = (prefix_idx < N_PREFIXES);
        if (known) {
            prefix = prefixes[prefix_idx].name;
            if (canonical) {
                len   = prefixes[prefix_idx].len;
                valid = true;
            }
        } else {
            prefix = unknown_prefixes[prefix_idx - N_PREFIXES];
        }
        break;
    default:
        break;
    }

    need = OUT_ENOUGH;
    if (fn == FN_DECIMAL) {
        need = TZ_DECIMAL_BUFFER_SIZE(len);
    } else if (fn == FN_BASE58) {
        need = TZ_BASE58_BUFFER_SIZE(len);
    }
    if (sel >= 250) {
        olen = sel - 250;
    } else {
        size_t delta = sel % 25;
        olen         = (need + delta > 12) ? (need + delta - 12) : 0;
    }

    // Buffers of exactly the size the helper is told
    ibuf = malloc(len);
    obuf = malloc(olen);
    if ((len > 0 && ibuf == NULL) || (olen > 0 && obuf == NULL)) {
        abort();
    }
    if (len > 0) {
        memcpy(ibuf, payload, len);
    }

    switch (fn) {
    case FN_DECIMAL:
        rc = tz_format_decimal(ibuf, len, obuf, olen);
        CHECK((olen < TZ_DECIMAL_BUFFER_SIZE(len)) == (rc != 0));
        if (rc == 0) {
            check_decimal(ibuf, len, obuf, olen);
        }
        break;
    case FN_BASE58:
        rc = tz_format_base58(ibuf, len, obuf, olen);
        if (olen < TZ_BASE58_BUFFER_SIZE(len)) {
            CHECK(rc != 0);
        }
        // One more byte than the digits of any `len` bytes need
        if (olen > TZ_BASE58_BUFFER_SIZE(len)) {
            CHECK(rc == 0);
        }
        if (rc == 0) {
            check_base58(ibuf, len, obuf, olen);
        }
        break;
    case FN_PKH:
        rc = tz_format_pkh(ibuf, len, obuf, olen);
        if (rc == 0) {
            check_base58check(obuf, olen, ibuf + 1, len - 1);
        }
        break;
    case FN_PK:
        rc = tz_format_pk(ibuf, len, obuf, olen);
        if (rc == 0) {
            check_base58check(obuf, olen, ibuf + 1, len - 1);
        }
        break;
    case FN_SIG:
        rc = tz_format_sig(ibuf, len, obuf, olen);
        if (rc == 0) {
            check_base58check(obuf, olen, ibuf, len);
        }
        break;
    case FN_OPH:
        rc = tz_format_oph(ibuf, len, obuf, olen);
        if (rc == 0) {
            check_base58check(obuf, olen, ibuf, len);
        }
        break;
    case FN_BH:
        rc = tz_format_bh(ibuf, len, obuf, olen);
        if (rc == 0) {
            check_base58check(obuf, olen, ibuf, len);
        }
        break;
    case FN_ADDRESS:
        rc = tz_format_address(ibuf, len, obuf, olen);
        if (rc == 0) {
            // Tag 0 is a public key hash, whose own tag comes next
            size_t skip = (ibuf[0] == 0) ? 2 : 1;
            check_base58check(obuf, olen, ibuf + skip, len - 2);
        }
        break;
    case FN_BASE58CHECK:
        rc = tz_format_base58check(prefix, ibuf, len, obuf, olen);
        if (!known || len != prefixes[prefix_idx].len) {
            CHECK(rc != 0);
        }
        if (rc == 0) {
            check_base58check(obuf, olen, ibuf, len);
        }
        break;
    case FN_OP_NAME: {
        const char *name = tz_michelson_op_name(aux);
        CHECK((aux > TZ_LAST_MICHELSON_OPCODE) == (name == NULL));
        CHECK((name == NULL) || (name[0] != '\0'));
        break;
    }
    default:
        break;
    }

    // What a helper is given enough room for, it must format
    if (valid && olen >= OUT_ENOUGH) {
        CHECK(rc == 0);
    }

    free(ibuf);
    free(obuf);
}
