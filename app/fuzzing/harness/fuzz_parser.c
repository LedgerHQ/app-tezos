/*
 * fuzz_parser: the parsers of what is signed, called at their own entry
 * points.
 *
 * Through fuzz_app they are behind the path of a key and the packets of a
 * signing, with the state of the operation parser in `global`, which the
 * invariant cannot restore as it holds pointers into itself. This target
 * skips the APDU layer and hands each parser the data directly, in the slices
 * the app gets it in (see fuzz_parser_driver.h).
 *
 * Control byte 1 picks the parser; everything after the control bytes is the
 * data.
 */

#include <stdint.h>

#include "micheline_parser.h"
#include "operation_parser.h"

#include "fuzz_harness.h"

#include "fuzz_parser_driver.h"

enum {
    PARSER_OPERATION,
    PARSER_MICHELINE,
    PARSER_COUNT,
};

#define PARSER_ROW(idx) {.ins = (idx), .flags = FUZZ_CMD_HAS_DATA}
const fuzz_command_spec_t fuzz_commands[] = {
    PARSER_ROW(PARSER_OPERATION),
    PARSER_ROW(PARSER_MICHELINE),
};
FUZZ_COMMAND_COUNT();
_Static_assert(sizeof(fuzz_commands) / sizeof(fuzz_commands[0])
                   == PARSER_COUNT,
               "parser tables out of sync");

/* The first byte is the magic byte, so micheline expressions are reached
 * through the operation parser as well, the way the app parses them */
static void
operation_init(tz_parser_state *st)
{
    tz_operation_parser_init(st, TZ_UNKNOWN_SIZE, false);
}

static void
micheline_init(tz_parser_state *st)
{
    tz_parser_init(st);
    tz_micheline_parser_init(st);
}

static const parser_driver_t drivers[PARSER_COUNT] = {
    [PARSER_OPERATION] = {.init     = operation_init,
                          .step     = tz_operation_parser_step,
                          .set_size = tz_operation_parser_set_size},
    [PARSER_MICHELINE] = {.init     = micheline_init,
                          .step     = tz_micheline_parser_step,
                          .set_size = NULL                        },
};

void
fuzz_app_dispatch(void *cmd_v)
{
    const command_t *cmd = (const command_t *)cmd_v;

    fuzz_run_parser(&drivers[cmd->ins % PARSER_COUNT], fuzz_tail_ptr,
                    fuzz_tail_len);
}
