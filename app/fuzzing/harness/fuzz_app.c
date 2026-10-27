/*
 * fuzz_app: the full-app target. One fuzzer input becomes a sequence of
 * APDUs, each parsed by the real apdu_parser() and dispatched through
 * dispatch().
 *
 * See fuzz_run_sequence() for why it is a sequence. Adding a production INS
 * means one row in FUZZ_APDU_COMMANDS (mock/fuzz_apdu.h) and a seed for it in
 * fuzz-manifest.toml.
 */

#include <stdint.h>

#include "fuzz_harness.h"

#include "fuzz_apdu.h"

const fuzz_command_spec_t fuzz_commands[] = {FUZZ_APDU_COMMANDS};

FUZZ_COMMAND_COUNT();

/* The swap mode is reached by fuzz_swap, which sets what the app is given by
 * Exchange itself. Here the invariant keeps G_called_from_swap clear. */
void
fuzz_app_dispatch(void *cmd_v)
{
    const command_t          *cmd   = (const command_t *)cmd_v;
    const fuzz_command_spec_t first = {.cla = cmd->cla, .ins = cmd->ins};
    fuzz_cursor_t cur = {.ptr = fuzz_tail_ptr, .left = fuzz_tail_len};

    fuzz_run_sequence(fuzz_commands, fuzz_n_commands, &first, cmd->p1,
                      cmd->p2, &cur);
}
