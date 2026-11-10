/* Reference helpers for the fuzz targets that check their output.
 *
 * They are written from the definitions (digits of a number in a base), not
 * from the code under test, so that a bug there does not hide in both.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FUZZ_B58_ALPHABET \
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
#define FUZZ_DEC_ALPHABET "0123456789"

/* Value of the digits of `str` in `alphabet`, as a big-endian number without
 * leading zero byte, written to `out`. Returns its length (0 for zero), or -1
 * on a character outside of the alphabet or a number larger than `cap`. */
static long
ref_decode(const char *str, const char *alphabet, uint8_t *out, size_t cap)
{
    size_t base = strlen(alphabet);
    size_t n    = 0;  // little-endian while accumulating

    for (; *str != '\0'; str++) {
        const char *p = memchr(alphabet, *str, base);
        if (p == NULL) {
            return -1;
        }
        unsigned carry = (unsigned)(p - alphabet);
        for (size_t i = 0; i < n; i++) {
            carry += (unsigned)out[i] * (unsigned)base;
            out[i] = (uint8_t)carry;
            carry >>= 8;
        }
        while (carry != 0) {
            if (n == cap) {
                return -1;
            }
            out[n++] = (uint8_t)carry;
            carry >>= 8;
        }
    }

    for (size_t i = 0; i < n / 2; i++) {
        uint8_t tmp    = out[i];
        out[i]         = out[n - 1 - i];
        out[n - 1 - i] = tmp;
    }
    return (long)n;
}
