#pragma once
/* Checks of the ring buffer of the strings the UI displays, for the fuzz
 * targets that call run_ui_strings(). A host build defines `global` and
 * io_send_sw() itself, with the shims of the unit tests, and the build of the
 * app has them.
 *
 * The input is a list of operations, made the way the stream of the UI makes
 * them: make room for a string by dropping the oldest ones, push it, append
 * to the last one, undo either. The ring is mirrored in a list of what it
 * must hold, and after every operation each string must be where it was put,
 * whole, and apart from the others, whatever was dropped or wrapped around
 * since. The same code is built for each size of buffer of the devices.
 *
 * Input: operations of [opcode][arg][arg][arg], at most MAX_OPS of them
 *
 * Appending to an empty string is not modelled: undoing it would drop the
 * string altogether instead of the fragment.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_check.h"
#include "globals.h"
#include "ui_strings.h"

#define MAX_ITEMS 64
#define MAX_OPS   256

bool ui_strings_is_empty(void);

/* A string the ring must hold: where it is, and what it says */
typedef struct {
    char  *ptr;
    size_t len;
    char   text[BUFF_LEN];
} item_t;

typedef enum {
    LAST_NONE,
    LAST_PUSH,
    LAST_APPEND,
} last_op_t;

static item_t    items[MAX_ITEMS];  // oldest first
static size_t    n_items;
static last_op_t last_op;
static char     *last_append_ptr;
static size_t    last_append_len;

static void
fill_text(char *dst, size_t len, uint8_t seed)
{
    for (size_t i = 0; i < len; i++) {
        dst[i] = (char)('a' + ((seed + i) % 26));
    }
    dst[len] = '\0';
}

static void
check_ring(void)
{
    tz_ui_strings_t *s     = &global.ui.stream.strings;
    char            *begin = s->buffer;
    char            *end   = s->buffer + BUFF_LEN;

    // The calls made here follow the contract, so none may report an error,
    // which sets the step of the app
    CHECK(global.step != ST_ERROR);

    CHECK((s->start >= begin) && (s->start <= end));
    CHECK((s->end >= begin) && (s->end <= end));
    CHECK((s->internal_end >= begin) && (s->internal_end <= end));
    CHECK(s->end <= s->internal_end);
    CHECK(s->count == n_items);
    CHECK(ui_strings_is_empty() == (n_items == 0));

    for (size_t i = 0; i < n_items; i++) {
        const item_t *a = &items[i];

        // Inside the ring, whole and unchanged, terminator included
        CHECK((a->ptr >= begin) && (a->ptr + a->len < end));
        CHECK(memcmp(a->ptr, a->text, a->len + 1) == 0);

        for (size_t j = i + 1; j < n_items; j++) {
            const item_t *b = &items[j];

            CHECK((a->ptr + a->len < b->ptr) || (b->ptr + b->len < a->ptr));
        }
    }
}

static void
evict_oldest(void)
{
    tz_ui_strings_t *s = &global.ui.stream.strings;
    char            *p;

    CHECK(n_items > 0);
    p = items[0].ptr;
    // The oldest string is the one `start` points to
    CHECK(p == s->start);
    ui_strings_drop(&p);
    CHECK(p == NULL);

    memmove(&items[0], &items[1], (n_items - 1) * sizeof(items[0]));
    n_items--;
    last_op = LAST_NONE;
}

static void
op_push(size_t len, uint8_t seed)
{
    char  text[BUFF_LEN];
    char *out     = NULL;
    bool  can_fit = false;

    fill_text(text, len, seed);

    if (n_items == MAX_ITEMS) {
        evict_oldest();
    }
    ui_strings_can_fit(len, &can_fit);
    while (!can_fit && n_items > 0) {
        evict_oldest();
        ui_strings_can_fit(len, &can_fit);
    }
    // Once empty, the ring holds any string shorter than itself
    CHECK(can_fit);

    ui_strings_push(text, len, &out);
    CHECK(out != NULL);

    items[n_items].ptr = out;
    items[n_items].len = len;
    memcpy(items[n_items].text, text, len + 1);
    n_items++;
    last_op = LAST_PUSH;
}

static void
op_undo_push(void)
{
    char *p;

    if (last_op != LAST_PUSH || n_items == 0) {
        return;
    }
    p = items[n_items - 1].ptr;
    ui_strings_drop_last(&p);
    CHECK(p == NULL);
    n_items--;
    last_op = LAST_NONE;
}

static void
op_append(size_t src_len, size_t max, uint8_t seed)
{
    char    src[64];
    char   *out     = NULL;
    bool    can_fit = false;
    item_t *last;
    size_t  before, appended;

    // An append continues a value, which is not empty
    if (n_items == 0 || items[n_items - 1].len == 0) {
        return;
    }
    fill_text(src, src_len, seed);

    // Make room, but never at the cost of the string that is appended to
    ui_strings_can_fit(src_len, &can_fit);
    while (!can_fit && n_items > 1) {
        evict_oldest();
        ui_strings_can_fit(src_len, &can_fit);
    }

    last     = &items[n_items - 1];
    before   = last->len;
    appended = ui_strings_append_last(src, max, &out);

    // What was asked for, and no more than there is
    CHECK(appended <= max);
    if (appended > 0) {
        CHECK(out == last->ptr + before);
        memcpy(last->text + before, src, appended);
        last->len += appended;
        last->text[last->len] = '\0';
        last_op               = LAST_APPEND;
        last_append_ptr       = out;
        last_append_len       = appended;
    } else {
        last_op = LAST_NONE;
    }
}

static void
op_undo_append(void)
{
    item_t *last;
    char   *p;

    if (last_op != LAST_APPEND) {
        return;
    }
    p = last_append_ptr;
    ui_strings_drop_last(&p);
    CHECK(p == NULL);

    last = &items[n_items - 1];
    last->len -= last_append_len;
    last->text[last->len] = '\0';
    last_op               = LAST_NONE;
}

static void
run_ui_strings(const uint8_t *data, size_t size)
{
    size_t pos = 0;

    global.step = ST_IDLE;
    ui_strings_init();
    n_items = 0;
    last_op = LAST_NONE;

    for (size_t ops = 0; pos < size && ops < MAX_OPS; ops++) {
        uint8_t arg[4] = {0};

        for (size_t i = 0; i < sizeof(arg) && pos < size; i++) {
            arg[i] = data[pos++];
        }
        size_t value = ((size_t)arg[1] << 8) | arg[2];

        switch (arg[0] % 8) {
        case 0:
        case 1:
            op_push(value % BUFF_LEN, arg[3]);
            break;
        case 2:
            if (n_items > 0) {
                evict_oldest();
            }
            break;
        case 3:
            op_undo_push();
            break;
        case 4:
        case 5: {
            size_t src_len = arg[1] % 64;

            op_append(src_len, arg[2] % (src_len + 1), arg[3]);
            break;
        }
        case 6:
            op_undo_append();
            break;
        default:
            if (arg[1] % 4 == 0) {
                ui_strings_init();
                n_items = 0;
                last_op = LAST_NONE;
            } else {
                bool can_fit = false;

                ui_strings_can_fit(value % BUFF_LEN, &can_fit);
            }
            break;
        }
        check_ring();
    }
}
