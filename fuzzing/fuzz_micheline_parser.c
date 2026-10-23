/* Fuzz target for the micheline parser, on its own.
 *
 * The operation parser reaches it through the magic byte and the parameters
 * of a transaction, but behind other bytes. Here the input is the micheline
 * expression itself, so every byte goes to its stack, sizes and tags.
 */

#include "fuzz_parser_driver.h"
#include "micheline_parser.h"

static void
init(tz_parser_state *st)
{
    tz_parser_init(st);
    tz_micheline_parser_init(st);
}

static const parser_driver_t driver = {
    .init     = init,
    .step     = tz_micheline_parser_step,
    .set_size = NULL,
};

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_run_parser(&driver, data, size);
    return 0;
}
