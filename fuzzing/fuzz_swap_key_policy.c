/* Fuzz target for the key policy of swaps, see check_swap_key_policy.h */

#include "check_swap_key_policy.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    run_swap_key_policy(data, size);
    return 0;
}
