/* Fuzz target for the operation parser.
 *
 * The first byte of the input is the magic byte, so micheline expressions are
 * fuzzed as well, the way the app parses them.
 */

#include "fuzz_parser_driver.h"
#include "operation_parser.h"

static void
init(tz_parser_state *st)
{
    tz_operation_parser_init(st, TZ_UNKNOWN_SIZE, false);
}

static const parser_driver_t driver = {
    .init     = init,
    .step     = tz_operation_parser_step,
    .set_size = tz_operation_parser_set_size,
};

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_run_parser(&driver, data, size);
    return 0;
}
