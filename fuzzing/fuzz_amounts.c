/* Fuzz target for the number and amount helpers, see check_amounts.h */

#include "operation_parser.c"

#include "check_amounts.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    run_amounts(data, size);
    return 0;
}
