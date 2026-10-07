/* Host stand-in for the Ledger SDK <io.h>, for unit tests only.

   Just enough for app/src/exception.h and app/src/ui/ui_strings.c to compile
   on a development machine. The test that links against it provides
   io_send_sw() so it can observe whether an error status was sent. */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void io_send_sw(uint16_t sw);
