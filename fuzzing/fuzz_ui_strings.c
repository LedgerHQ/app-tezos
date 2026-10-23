/* Fuzz target for the ring buffer of the strings the UI displays, see
 * check_ui_strings.h. The shims stand in for the SDK headers, and the target
 * is built for the size of each kind of device. */

#include "globals.h"

globals_t global;

void
io_send_sw(uint16_t sw)
{
    (void)sw;
}

#include "check_ui_strings.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    run_ui_strings(data, size);
    return 0;
}
