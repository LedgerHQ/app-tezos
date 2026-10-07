/* Tezos Embedded C parser for Ledger - UI string ring buffer unit tests

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "globals.h"
#include "ui_strings.h"

globals_t global;

static unsigned io_sw_count;
static uint16_t io_sw_last;

void
io_send_sw(uint16_t sw)
{
    io_sw_count++;
    io_sw_last = sw;
}

static int
setup(void **state)
{
    memset(&global, 0, sizeof(global));
    io_sw_count = 0;
    io_sw_last  = 0;
    ui_strings_init();
    return 0;
}

static tz_ui_strings_t *
ring(void)
{
    return &global.ui.stream.strings;
}

/* Push a string of `len` copies of `c` and return where it landed. */
static char *
push_filled(char c, size_t len)
{
    char  src[BUFF_LEN];
    char *out = NULL;

    assert_true(len < sizeof(src));
    memset(src, c, len);
    src[len] = '\0';
    ui_strings_push(src, len, &out);
    assert_non_null(out);
    return out;
}

/* A string that fills the whole ring leaves `end` on BUFF_END: there is no
   contiguous room after it. ui_strings_append_last() used to derive its
   length as `max - 1` from that empty window, wrapped it to SIZE_MAX and
   moved `end` out of the ring. It must report nothing appended instead. */
static void
test_append_last_without_contiguous_room(void **state)
{
    tz_ui_strings_t *s = ring();
    char            *out = NULL;
    char            *end_before;
    size_t           appended;

    push_filled('a', BUFF_LEN - 1);
    assert_ptr_equal(s->end, s->buffer + BUFF_LEN);
    end_before = s->end;

    appended = ui_strings_append_last("xyz", 3, &out);

    assert_int_equal(appended, 0);
    assert_null(out);
    assert_ptr_equal(s->end, end_before);
    assert_ptr_equal(s->start, s->buffer);
    assert_int_equal(io_sw_count, 0);
}

static void
test_append_last_with_room(void **state)
{
    tz_ui_strings_t *s   = ring();
    char            *out = NULL;
    size_t           appended;

    push_filled('a', 2);
    appended = ui_strings_append_last("cdef", 4, &out);

    assert_int_equal(appended, 4);
    assert_ptr_equal(out, s->buffer + 2);
    assert_string_equal(s->start, "aacdef");
    assert_ptr_equal(s->end, s->buffer + 7);
    assert_int_equal(io_sw_count, 0);
}

/* Two contiguous bytes remain after `end`: the append is clipped to one
   character. `end` stays one past the terminator, so a byte is left over and
   a further append writes nothing. It must stay inside the ring throughout. */
static void
test_append_last_is_clipped_to_the_remaining_room(void **state)
{
    tz_ui_strings_t *s   = ring();
    char            *out = NULL;
    size_t           appended;

    push_filled('a', BUFF_LEN - 3);
    assert_ptr_equal(s->end, s->buffer + BUFF_LEN - 2);

    appended = ui_strings_append_last("wxyz", 4, &out);

    assert_int_equal(appended, 1);
    assert_ptr_equal(s->end, s->buffer + BUFF_LEN - 1);
    assert_ptr_equal(s->start, s->buffer);
    assert_int_equal(s->start[BUFF_LEN - 3], 'w');
    assert_int_equal(s->start[BUFF_LEN - 2], '\0');

    out = NULL;
    assert_int_equal(ui_strings_append_last("more", 4, &out), 0);
    assert_ptr_equal(s->end, s->buffer + BUFF_LEN - 1);
    assert_ptr_equal(s->start, s->buffer);
    assert_int_equal(s->start[BUFF_LEN - 2], '\0');
    assert_int_equal(io_sw_count, 0);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_append_last_without_contiguous_room,
                               setup),
        cmocka_unit_test_setup(test_append_last_with_room, setup),
        cmocka_unit_test_setup(
            test_append_last_is_clipped_to_the_remaining_room, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
