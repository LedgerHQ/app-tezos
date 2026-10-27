/*
 * fuzz_helpers: the helpers that the parsers and the screens rely on, called
 * at their own entry points and held to what they must satisfy: what is
 * formatted decodes back to what went in, the amounts match a reference built
 * the other way around, the lookups of the token registry agree, the key
 * policy of swaps is what its header says, and the ring buffer of the strings
 * keeps each of them whole.
 *
 * Control byte 1 picks the helper; everything after the control bytes is the
 * input of its check (see mock/../checks/check_*.h for the layout).
 */

#include <stdint.h>

#include "fuzz_harness.h"

/* The helpers of the operation parser that format amounts are static, so its
 * source is part of this target, instead of the copy of the app. */
#include "operation_parser.c"

#include "check_amounts.h"
#include "check_fa2_tokens.h"
#include "check_formatting.h"
#include "check_swap_key_policy.h"
#include "check_ui_strings.h"

enum {
    HELPER_FORMATTING,
    HELPER_AMOUNTS,
    HELPER_FA2_TOKENS,
    HELPER_SWAP_KEY_POLICY,
    HELPER_UI_STRINGS,
    HELPER_COUNT,
};

#define HELPER_ROW(idx) {.ins = (idx), .flags = FUZZ_CMD_HAS_DATA}
const fuzz_command_spec_t fuzz_commands[] = {
    HELPER_ROW(HELPER_FORMATTING), HELPER_ROW(HELPER_AMOUNTS),
    HELPER_ROW(HELPER_FA2_TOKENS), HELPER_ROW(HELPER_SWAP_KEY_POLICY),
    HELPER_ROW(HELPER_UI_STRINGS),
};
FUZZ_COMMAND_COUNT();
_Static_assert(sizeof(fuzz_commands) / sizeof(fuzz_commands[0])
                   == HELPER_COUNT,
               "helper tables out of sync");

void
fuzz_app_dispatch(void *cmd_v)
{
    const command_t *cmd = (const command_t *)cmd_v;

    switch (cmd->ins % HELPER_COUNT) {
    case HELPER_FORMATTING:
        run_formatting(fuzz_tail_ptr, fuzz_tail_len);
        break;
    case HELPER_AMOUNTS:
        run_amounts(fuzz_tail_ptr, fuzz_tail_len);
        break;
    case HELPER_FA2_TOKENS:
        run_fa2_tokens(fuzz_tail_ptr, fuzz_tail_len);
        break;
    case HELPER_SWAP_KEY_POLICY:
        run_swap_key_policy(fuzz_tail_ptr, fuzz_tail_len);
        break;
    default:
        run_ui_strings(fuzz_tail_ptr, fuzz_tail_len);
        break;
    }
}
