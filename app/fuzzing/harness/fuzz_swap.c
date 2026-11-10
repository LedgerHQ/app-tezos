/*
 * fuzz_swap: the app as Exchange calls it. The callbacks of the swap library
 * are entered from Exchange, never through an APDU, and the signing flow of a
 * swap runs differently, with no review: it validates the operation against
 * the parameters Exchange gave (swap_check_validity()).
 *
 * Control byte 1 picks what Exchange does: a swap of tez, a swap of a token
 * (a coin configuration comes with it), or the other two callbacks. A swap
 * then continues with a sequence of APDUs, as a host would send them.
 */

#include <stdbool.h>
#include <stdint.h>

#include "swap.h"
#include "swap_utils.h"

#include "fuzz_harness.h"

#include "fuzz_apdu.h"

#ifndef HAVE_SWAP
#error "fuzz_swap needs the swap build of the app"
#endif

#define SWAP_AMOUNT_MAX  16
#define SWAP_CONFIG_MAX  32
#define SWAP_ADDRESS_MAX 63
#define SWAP_PATH_MAX    255

enum {
    SWAP_TEZ,
    SWAP_TOKEN,
    SWAP_CALLBACKS,
    SWAP_COUNT,
};

#define SWAP_ROW(idx) {.ins = (idx), .flags = FUZZ_CMD_HAS_DATA}
const fuzz_command_spec_t fuzz_commands[] = {
    SWAP_ROW(SWAP_TEZ),
    SWAP_ROW(SWAP_TOKEN),
    SWAP_ROW(SWAP_CALLBACKS),
};
FUZZ_COMMAND_COUNT();
_Static_assert(sizeof(fuzz_commands) / sizeof(fuzz_commands[0]) == SWAP_COUNT,
               "swap tables out of sync");

/* What a swap does with the commands of the app */
static const fuzz_command_spec_t apdu_commands[] = {FUZZ_APDU_COMMANDS};

static volatile uint8_t swap_return_dummy;

/* The app is started by Exchange, which sets these. The invariant keeps them
 * clear for the other targets, as the end of a swap hands back to the OS, and
 * the OS is not there. */
void
fuzz_app_reset(void)
{
    G_swap_signing_return_value_address = &swap_return_dummy;
    G_called_from_swap                  = true;
    G_swap_response_ready               = false;
}

/* What Exchange tells the app of a swap, before the host signs it */
static void
swap_copy_params(fuzz_cursor_t *cur, bool token)
{
    uint8_t                         amount[SWAP_AMOUNT_MAX];
    uint8_t                         fee[SWAP_AMOUNT_MAX];
    uint8_t                         config[SWAP_CONFIG_MAX];
    char                            destination[SWAP_ADDRESS_MAX + 1];
    create_transaction_parameters_t p = {0};

    p.amount_length     = fuzz_take_sized(cur, amount, sizeof(amount));
    p.fee_amount_length = fuzz_take_sized(cur, fee, sizeof(fee));
    if (token) {
        p.coin_configuration_length
            = fuzz_take_sized(cur, config, sizeof(config));
        p.coin_configuration = config;
    }
    destination[fuzz_take_sized(cur, (uint8_t *)destination,
                                SWAP_ADDRESS_MAX)]
        = '\0';

    p.amount              = amount;
    p.fee_amount          = fee;
    p.destination_address = destination;
    swap_copy_transaction_parameters(&p);
}

/* The two callbacks that are not the swap itself: is this address one of the
 * device, and how is this amount shown */
static void
swap_callbacks(fuzz_cursor_t *cur)
{
    uint8_t path[SWAP_PATH_MAX];
    uint8_t config[SWAP_CONFIG_MAX];
    uint8_t amount[SWAP_AMOUNT_MAX];
    char    address[SWAP_ADDRESS_MAX + 1];
    uint8_t selector = fuzz_take_u8(cur);

    if (selector & 1) {
        check_address_parameters_t p = {0};

        p.address_parameters_length
            = fuzz_take_sized(cur, path, sizeof(path));
        address[fuzz_take_sized(cur, (uint8_t *)address, SWAP_ADDRESS_MAX)]
            = '\0';
        p.address_parameters = path;
        p.address_to_check   = address;
        p.extra_id_to_check  = (char *)"";
        swap_handle_check_address(&p);
    } else {
        get_printable_amount_parameters_t p = {0};

        p.is_fee = (selector & 2) != 0;
        if (selector & 4) {
            p.coin_configuration_length
                = fuzz_take_sized(cur, config, sizeof(config));
            p.coin_configuration = config;
        }
        p.amount_length = fuzz_take_sized(cur, amount, sizeof(amount));
        p.amount        = amount;
        swap_handle_get_printable_amount(&p);
    }
}

void
fuzz_app_dispatch(void *cmd_v)
{
    const command_t *cmd = (const command_t *)cmd_v;
    fuzz_cursor_t    cur = {.ptr = fuzz_tail_ptr, .left = fuzz_tail_len};
    const fuzz_command_spec_t *first;
    uint8_t                    row = cmd->ins % SWAP_COUNT;

    if (row == SWAP_CALLBACKS) {
        swap_callbacks(&cur);
        return;
    }
    swap_copy_params(&cur, row == SWAP_TOKEN);

    first = &apdu_commands[fuzz_take_u8(&cur)
                           % (sizeof(apdu_commands)
                              / sizeof(apdu_commands[0]))];
    fuzz_run_sequence(apdu_commands,
                      sizeof(apdu_commands) / sizeof(apdu_commands[0]), first,
                      fuzz_clamp_p(fuzz_take_u8(&cur), first->p1_max),
                      fuzz_clamp_p(fuzz_take_u8(&cur), first->p2_max), &cur);
}
