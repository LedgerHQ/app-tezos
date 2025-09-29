#include "mocks.h"

// APPs expect a specific length
cx_err_t cx_ecdomain_parameters_length(cx_curve_t cv, size_t *length) {
    (void) cv;
    *length = (size_t) 32;
    return 0x00000000;
}

// Simulates writing to NVM
void nvm_write(void *dst_adr, void *src_adr, unsigned int src_len) {
    if (!dst_adr || !src_adr || src_len == 0) {
        return;
    }
    memcpy(dst_adr, src_adr, src_len);
}

try_context_t fuzz_exit_jump_ctx = {0};
try_context_t *G_exception_context = &fuzz_exit_jump_ctx;

try_context_t *try_context_get(void) {
    return G_exception_context;
}

try_context_t *try_context_set(try_context_t *context) {
    try_context_t *previous = G_exception_context;
    G_exception_context = context;
    return previous;
}

void __attribute__((noreturn))
os_sched_exit(bolos_task_status_t exit_code __attribute__((unused))) {
    longjmp(fuzz_exit_jump_ctx.jmp_buf, 1);
}

void __attribute__((noreturn)) os_lib_end(void) {
    longjmp(fuzz_exit_jump_ctx.jmp_buf, 1);
}

/* If strnlen isn't available, provide a tiny fallback */
static size_t _mini_strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') n++;
    return n;
}

/* BSD-compatible strlcat */
size_t strlcat(char *dst, const char *src, size_t size) {
    const size_t dlen = _mini_strnlen(dst, size);  /* length of dst within bound */
    const size_t slen = strlen(src);

    /* If no NUL found in the first `size` bytes of dst, treat dst as length `size`
       and report the length we *tried* to build (no NUL written). */
    if (dlen == size) {
        return size + slen;
    }

    /* We can append at most `size - dlen - 1` bytes, keeping space for the NUL. */
    size_t tocopy = size - dlen - 1;
    if (tocopy > 0) {
        if (tocopy > slen) tocopy = slen;
        memcpy(dst + dlen, src, tocopy);
        dst[dlen + tocopy] = '\0';
    }

    /* Return the total length we tried to create: initial dst len + src len. */
    return dlen + slen;
}
