/* Host stand-in for app/src/globals.h, for the NBGL stream test only.

   It keeps the layout app/src/ui/ui_stream_nbgl.c and ui_stream_common.c
   reach through, and what the TZ_* error macros write, so the unmodified
   files can be linked into a test. The test defines `global`, N_settings and
   the functions of the SDK. */

#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "exception.h"
#include "parser_state.h"
#include "ui_stream.h"
#include "ui_strings.h"

#define ERROR_CODE_SIZE 15

#define REJECT_CONFIRM_BUTTON "Yes, reject"
#define RESUME(msg)           "Go back to " msg
#define SIGN(msg)             "Sign " msg "?"

typedef enum {
    ST_IDLE = 0,
    ST_CLEAR_SIGN,
    ST_BLIND_SIGN,
    ST_SUMMARY_SIGN,
    ST_ERROR,
} tz_step_t;

typedef enum {
    REASON_NONE          = 0,
    REASON_PARSING_ERROR = 1,
} blindsign_reason_t;

typedef enum {
    SIGN_ST_IDLE = 0,
    SIGN_ST_WAIT_USER_INPUT,
    SIGN_ST_WAIT_DATA,
} sign_step_t;

typedef struct {
    tz_step_t          step;
    blindsign_reason_t blindsign_reason;
    char               error_code[ERROR_CODE_SIZE];
    struct {
        struct {
            struct {
                sign_step_t step;
                bool        received_last_msg;
                struct {
                    struct {
                        tz_parser_state parser_state;
                    } clear;
                } u;
            } sign;
        } apdu;
    } keys;
    struct {
        tz_ui_stream_t stream;
    } ui;
} globals_t;

extern globals_t global;

typedef struct {
    bool blindsigning;
    bool expert_mode;
} settings_t;

extern settings_t N_settings;

/* The SDK throws with longjmp; the test catches it with g_throw_jmp. */
extern jmp_buf  g_throw_jmp;
extern unsigned g_thrown;
#define THROW(e)                  \
    do {                          \
        g_thrown = (unsigned)(e); \
        longjmp(g_throw_jmp, 1);  \
    } while (0)

/* The build may define PRINTF empty for every test: this one needs it to take
   arguments, and to print them when TEST_TRACE is set. */
#ifdef PRINTF
#undef PRINTF
#endif
#ifdef TEST_TRACE
#include <stdio.h>
#define PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINTF(...) \
    do {            \
    } while (0)
#endif

extern const nbgl_icon_details_t IMPORTANT_CIRCLE_ICON;
extern const nbgl_icon_details_t C_TZ_APP_ICON;

void ui_home_init(void);
void toggle_expert_mode(void);
