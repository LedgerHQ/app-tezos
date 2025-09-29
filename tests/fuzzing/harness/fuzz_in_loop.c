#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include "globals.h"
#include "dispatcher.h"
#include "mocks.h"

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#warning "Define FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION for fuzz builds"
#endif

void print_apdu(const command_t *cmd) {
    PRINTF("=> CLA=%02x, INS=%02x, P1=%02x, P2=%02x, LC=%02x, DATA=",
           cmd->cla, cmd->ins, cmd->p1, cmd->p2, cmd->lc);

    for (int i = 0; i < cmd->lc; i++) {
        PRINTF("%02X", cmd->data[i]);
    }
    PRINTF("\n");
}
void app_exit(void)
{
    PRINTF("[DEBUG] Trying to exit the app. \n");
    os_sched_exit(-1);
}
const uint8_t *fuzz_data;
size_t fuzz_size;
bool finished = false;


ux_state_t        G_ux;
bolos_ux_params_t G_ux_params;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;

    int used_size = 0;

    while (used_size + 1 < size) {
        uint8_t cmd_size = data[used_size];

        PRINTF("Total size = %zu, Next command size = %u, Used size = %d\n",
               size, cmd_size, used_size);

        // Sanity check
        if (cmd_size < 5 || cmd_size > 255) {
            PRINTF("Invalid cmd_size %u, must be at least 5 (header)\n", cmd_size);
            break;
        }

        if (used_size + 1 + cmd_size > size) {
            PRINTF("Not enough bytes for full command, stopping.\n");
            break;
        }

        const uint8_t *cmd_buf = &data[used_size + 1];

        command_t cmd;
        cmd.cla = cmd_buf[0];
        cmd.ins = cmd_buf[1];
        cmd.p1  = cmd_buf[2];
        cmd.p2  = cmd_buf[3];
        cmd.lc  = cmd_buf[4];

        // Remaining length for data field
        size_t remaining = cmd_size - 5;
        if (cmd.lc > remaining) {
            PRINTF("Declared LC (%u) > available data (%zu), trimming.\n", cmd.lc, remaining);
            cmd.lc = (uint8_t)remaining;
        }

        cmd.data = (uint8_t *)&cmd_buf[5]; // direct pointer to fuzz buffer

        // print_apdu(&cmd);

        apdu_parser(&cmd, G_io_apdu_buffer, size);
        
        dispatch(&cmd);

        used_size += cmd_size + 1;
    }

    return 0;
}
