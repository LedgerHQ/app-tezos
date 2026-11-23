/* Assertion of the fuzz targets that check what the code under test returns:
 * a failure aborts, which libFuzzer records as a crash with its input.
 *
 * It does not use <stdio.h>, which the framework disables in the fuzzers it
 * generates. */

#pragma once

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void
fuzz_check_say(const char *s)
{
    ssize_t written = write(STDERR_FILENO, s, strlen(s));

    (void)written;
}

static inline void
fuzz_check_failed(const char *expr, const char *file, int line)
{
    char     digits[12];
    char     number[12];
    size_t   n = 0;
    unsigned v = (unsigned)line;

    do {
        digits[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);
    for (size_t i = 0; i < n; i++) {
        number[i] = digits[n - 1 - i];
    }
    number[n] = '\0';

    fuzz_check_say("check failed: ");
    fuzz_check_say(expr);
    fuzz_check_say(" (");
    fuzz_check_say(file);
    fuzz_check_say(":");
    fuzz_check_say(number);
    fuzz_check_say(")\n");
    abort();
}

#define CHECK(cond)                                       \
    do {                                                  \
        if (!(cond)) {                                    \
            fuzz_check_failed(#cond, __FILE__, __LINE__); \
        }                                                 \
    } while (0)
