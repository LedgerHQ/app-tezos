/* Portable strlcpy/strlcat for Linux glibc (ctest build only).
 *
 * Copyright 2023 Functori <contact@functori.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 */

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) \
    && !defined(__NetBSD__)

#include <stddef.h>

size_t
strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = 0;

    if (size > 0) {
        while (len < size - 1 && src[len]) {
            dst[len] = src[len];
            len++;
        }
        dst[len] = '\0';
    }
    while (src[len]) {
        len++;
    }
    return len;
}

size_t
strlcat(char *dst, const char *src, size_t size)
{
    size_t dlen = 0;

    while (dlen < size && dst[dlen]) {
        dlen++;
    }
    return dlen + strlcpy(dst + dlen, src, size > dlen ? size - dlen : 0);
}

#else

typedef int strlcpy_stub_libc_provided;

#endif
