/* Host stand-in for app/src/globals.h, for unit tests only.

   The real header pulls in the whole application and the SDK. This one keeps
   the layout ui_strings.c reaches through (global.ui.stream.strings) and the
   `step` field the TZ_* error macros write, so the unmodified ui_strings.c
   can be linked into a test. The test defines `global`. */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "exception.h"
#include "ui_strings.h"

typedef enum {
    ST_IDLE = 0,
    ST_ERROR,
} tz_step_t;

typedef struct {
    tz_step_t step;
    struct {
        struct {
            tz_ui_strings_t strings;
        } stream;
    } ui;
} globals_t;

extern globals_t global;
