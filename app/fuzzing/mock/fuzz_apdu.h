#pragma once
/*
 * The APDUs of a sequence, dispatched as app_main() dispatches them.
 *
 * To be included after fuzz_harness.h, which it needs for the lane and the
 * clamping of P1 and P2.
 */

#include <parser.h>
#include <stdint.h>
#include <string.h>
#ifdef HAVE_SWAP
#include <swap.h>
#endif

#include "dispatcher.h"
#include "fuzz_input.h"
#include "globals.h"
#include "ui_home.h"

/* The class and the instructions, as in apdu/dispatcher.c */
#define CLA                   0x80
#define INS_VERSION           0x00
#define INS_GET_PUBLIC_KEY    0x02
#define INS_PROMPT_PUBLIC_KEY 0x03
#define INS_SIGN              0x04
#define INS_GIT               0x09
#define INS_SIGN_WITH_HASH    0x0F

/* P2 of a command that takes a key is its derivation type: the values 0 to 3
 * are the valid ones */
#define DERIVATION_TYPE_P2_MAX 3

/* P1 of a signing command is the index of the packet and a flag for the last
 * one: first, next, and each of them as the last. The clamped P1 is an index
 * in these four. */
#define P1_SIGN_MAX 3

/* The commands of the app, one row per fuzzable APDU. Signing is where most
 * of the logic is, so it gets most of the rows. A harness of its own uses
 * them as its table, or as the steps of its sequence. */
// clang-format off
#define FUZZ_APDU_COMMANDS                                                    \
    {.cla = CLA, .ins = INS_VERSION},                                         \
    {.cla = CLA, .ins = INS_GIT},                                             \
    {.cla = CLA, .ins = INS_GET_PUBLIC_KEY,                                   \
     .p2_max = DERIVATION_TYPE_P2_MAX, .flags = FUZZ_CMD_HAS_DATA},           \
    {.cla = CLA, .ins = INS_PROMPT_PUBLIC_KEY,                                \
     .p2_max = DERIVATION_TYPE_P2_MAX, .flags = FUZZ_CMD_HAS_DATA},           \
    {.cla = CLA, .ins = INS_SIGN, .p1_max = P1_SIGN_MAX,                      \
     .p2_max = DERIVATION_TYPE_P2_MAX, .flags = FUZZ_CMD_HAS_DATA},           \
    {.cla = CLA, .ins = INS_SIGN, .p1_max = P1_SIGN_MAX,                      \
     .p2_max = DERIVATION_TYPE_P2_MAX, .flags = FUZZ_CMD_HAS_DATA},           \
    {.cla = CLA, .ins = INS_SIGN, .p1_max = P1_SIGN_MAX,                      \
     .p2_max = DERIVATION_TYPE_P2_MAX, .flags = FUZZ_CMD_HAS_DATA},           \
    {.cla = CLA, .ins = INS_SIGN_WITH_HASH, .p1_max = P1_SIGN_MAX,            \
     .p2_max = DERIVATION_TYPE_P2_MAX, .flags = FUZZ_CMD_HAS_DATA}
// clang-format on

/* The structured lane builds the APDUs the dispatcher accepts, from the
 * clamped P1: a maximum cannot express that the public key commands take
 * exactly 0, nor which four values of P1 a signing command knows. The raw
 * lane leaves it as the input made it, to reach the refusals. */
static uint8_t
fuzz_fix_p1(uint8_t ins, uint8_t p1)
{
    static const uint8_t sign_p1[P1_SIGN_MAX + 1] = {0x00, 0x01, 0x80, 0x81};

    if (!fuzz_use_structured_lane()) {
        return p1;
    }
    switch (ins) {
    case INS_GET_PUBLIC_KEY:
    case INS_PROMPT_PUBLIC_KEY:
        return 0;
    case INS_SIGN:
    case INS_SIGN_WITH_HASH:
        return sign_p1[p1 % (P1_SIGN_MAX + 1)];
    default:
        return p1;
    }
}

/* Re-encodes one command as a real APDU, and does with it what the loop of
 * app_main() does: parse it, and dispatch it. */
static void
fuzz_dispatch_one(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                  const uint8_t *payload, uint8_t lc)
{
    // Like G_io_apdu_buffer, it outlives the call: the parser of the
    // operation keeps pointing into it until the next APDU
    static uint8_t apdu[5 + UINT8_MAX];
    command_t      cmd;

    memset(apdu, 0, sizeof(apdu));
    apdu[0] = cla;
    apdu[1] = ins;
    apdu[2] = p1;
    apdu[3] = p2;
    apdu[4] = lc;
    if (lc > 0 && payload != NULL) {
        memcpy(apdu + 5, payload, lc);
    }

    if (!apdu_parser(&cmd, apdu, 5u + lc)) {
        return;
    }

#ifdef HAVE_SWAP
    if (G_called_from_swap) {
        global.step = ST_SWAP_SIGN;
    }
#endif
    // ST_ERROR implies that we are completely unknown and need to reset
    if (global.step == ST_ERROR) {
        global.step = ST_IDLE;
        ui_home_init();
    }

    dispatch(&cmd);
}

/*
 * One input drives a sequence of APDUs, not a single one.
 *
 * A signing is the path of the key in a first APDU, then the operation in
 * several: the state that the packets after the first depend on, the parser
 * of the operation and the hash, is built by the ones before, and cannot be
 * put in the prefix the invariant restores, as the parser holds pointers into
 * its own state. So a single dispatch can never get past the first packet.
 *
 * The fuzzer picks every field of every step, including how many steps there
 * are: nothing here supplies content, it only stops discarding the rest of
 * the input after the first command.
 */
static void
fuzz_run_sequence(const fuzz_command_spec_t *commands, size_t n_commands,
                  const fuzz_command_spec_t *first, uint8_t p1, uint8_t p2,
                  fuzz_cursor_t *cur)
{
    const fuzz_command_spec_t *spec = first;

    for (;;) {
        uint8_t        lc;
        const uint8_t *payload = fuzz_take_slice(cur, &lc);

        fuzz_dispatch_one(spec->cla, spec->ins, fuzz_fix_p1(spec->ins, p1),
                          p2, payload, lc);

        // A zero byte ends the sequence; so does running out of input.
        if (cur->left == 0 || fuzz_take_u8(cur) == 0) {
            break;
        }
        spec = &commands[fuzz_take_u8(cur) % n_commands];
        p1   = fuzz_clamp_p(fuzz_take_u8(cur), spec->p1_max);
        p2   = fuzz_clamp_p(fuzz_take_u8(cur), spec->p2_max);
    }
}
