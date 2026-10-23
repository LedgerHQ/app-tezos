/* Fuzz target for the formatting helpers, see check_formatting.h */

#include "check_formatting.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    run_formatting(data, size);
    return 0;
}
