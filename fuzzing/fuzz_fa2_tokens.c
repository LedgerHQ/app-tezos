/* Fuzz target for the FA2 token registry, see check_fa2_tokens.h */

#include "check_fa2_tokens.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    run_fa2_tokens(data, size);
    return 0;
}
