/* Tests of the history of screens of app/src/ui/ui_stream_nbgl.c.

   The unmodified application files are linked against host stand-ins of the
   SDK (see shims/nbgl). The sizing of a page is modelled on the rules of
   nbgl_useCaseGetNbTagValuesInPage(): the pairs that fit a page, and a first
   pair that is too tall for it fits alone and is reported as such.

   The stream is driven as the application does: the parser output goes in
   chunks of at most TZ_UI_STREAM_CONTENTS_SIZE bytes, what a push consumed is
   dropped and the rest is pushed again, and the user steps through each page
   that completes. */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "globals.h"
#include "ui_stream.h"

/* What the application files expect to find */
globals_t                 global;
settings_t                N_settings;
jmp_buf                   g_throw_jmp;
unsigned                  g_thrown;
const nbgl_icon_details_t IMPORTANT_CIRCLE_ICON = {0};
const nbgl_icon_details_t C_TZ_APP_ICON         = {0};

static unsigned g_sw_sent;

void
io_send_sw(uint16_t sw)
{
    (void)sw;
    g_sw_sent++;
}

const char *
tz_parser_result_name(tz_parser_result code)
{
    (void)code;
    return "result";
}

void
ui_home_init(void)
{
}

void
toggle_expert_mode(void)
{
}

bool has_final_screen(void);

/* The part of the SDK the stream calls while it builds pages */

#define CHARS_PER_LINE  20
#define PAGE_LINES      16
#define MAX_VALUE_LINES 10

static unsigned
value_lines(const char *text)
{
    size_t len = strlen(text);

    return (len == 0)
               ? 1U
               : (unsigned)((len + CHARS_PER_LINE - 1) / CHARS_PER_LINE);
}

uint8_t
nbgl_useCaseGetNbTagValuesInPage(
    uint8_t nbPairs, const nbgl_contentTagValueList_t *tagValueList,
    uint8_t startIndex, bool *requireSpecificDisplay)
{
    uint8_t  in_page = 0;
    unsigned height  = 0;

    *requireSpecificDisplay = false;
    while (in_page < nbPairs) {
        const nbgl_layoutTagValue_t *pair
            = &tagValueList->pairs[startIndex + in_page];
        unsigned lines = value_lines(pair->value);

        height += (in_page > 0 ? 1U : 0U) + 1U + lines;
        if ((height >= PAGE_LINES) || (lines > MAX_VALUE_LINES)) {
            if (in_page == 0) {
                in_page                 = 1;
                *requireSpecificDisplay = true;
            }
            break;
        }
        in_page++;
    }
    return in_page;
}

void
nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t t, nbgl_callback_t cb)
{
    (void)t;
    (void)cb;
}

void
nbgl_useCaseChoice(const nbgl_icon_details_t *icon, const char *m,
                   const char *sm, const char *ct, const char *cat,
                   nbgl_choiceCallback_t cb)
{
    (void)icon;
    (void)m;
    (void)sm;
    (void)ct;
    (void)cat;
    (void)cb;
}

void
nbgl_useCaseConfirm(const char *m, const char *sm, const char *ct,
                    const char *cat, nbgl_callback_t cb)
{
    (void)m;
    (void)sm;
    (void)ct;
    (void)cat;
    (void)cb;
}

void
nbgl_useCaseStatus(const char *m, bool ok, nbgl_callback_t cb)
{
    (void)m;
    (void)ok;
    (void)cb;
}

void
nbgl_useCaseReviewStart(const nbgl_icon_details_t *icon, const char *t,
                        const char *st, const char *rt, nbgl_callback_t cont,
                        nbgl_callback_t rej)
{
    (void)icon;
    (void)t;
    (void)st;
    (void)rt;
    (void)cont;
    (void)rej;
}

void
nbgl_useCaseReviewStreamingStart(nbgl_operationType_t       op,
                                 const nbgl_icon_details_t *icon,
                                 const char *t, const char *st,
                                 nbgl_choiceCallback_t cb)
{
    (void)op;
    (void)icon;
    (void)t;
    (void)st;
    (void)cb;
}

void
nbgl_useCaseReviewStreamingContinue(const nbgl_contentTagValueList_t *l,
                                    nbgl_choiceCallback_t             cb)
{
    (void)l;
    (void)cb;
}

void
nbgl_useCaseReviewStreamingContinueExt(const nbgl_contentTagValueList_t *l,
                                       nbgl_choiceCallback_t             cb,
                                       nbgl_callback_t                   skip)
{
    (void)l;
    (void)cb;
    (void)skip;
}

void
nbgl_useCaseReviewStreamingFinish(const char *t, nbgl_choiceCallback_t cb)
{
    (void)t;
    (void)cb;
}

/* Driving the stream */

#define H TZ_UI_STREAM_HISTORY_SCREENS

#define MAX_FIELDS    64
#define MAX_FIELD_LEN 1024

/* The text the user went through, by field (the title of a field is F<n>) */
static char g_shown[MAX_FIELDS][MAX_FIELD_LEN];
static int  g_empty_pages;

static int
setup(void **state)
{
    (void)state;
    memset(&global, 0, sizeof(global));
    memset(g_shown, 0, sizeof(g_shown));
    g_empty_pages = 0;
    g_sw_sent     = 0;
    g_thrown      = 0;
    tz_ui_stream_init(NULL);
    return 0;
}

/* The user steps through each page that has completed */
static void
step_through_pages(void)
{
    tz_ui_stream_t *s = &global.ui.stream;

    while (s->current < s->total) {
        const tz_ui_stream_screen_t *page;

        s->current++;
        page = &s->screens[s->current % H];
        if (page->nb_pairs == 0) {
            g_empty_pages++;
        }
        for (uint8_t i = 0; i < page->nb_pairs; i++) {
            int n = atoi(page->pairs[i].item + 1);

            assert_true((n >= 0) && (n < MAX_FIELDS));
            strncat(g_shown[n], page->pairs[i].value,
                    MAX_FIELD_LEN - strlen(g_shown[n]) - 1);
        }
    }
}

/* Push a field the way the application does, 20 bytes at a time, pushing
   again what a push did not consume. Returns false if the stream threw. */
static bool
push_field(int n, tz_ui_cb_type_t cb_type, const char *value)
{
    char   title[16];
    size_t len    = strlen(value);
    size_t done   = 0;
    int    spins  = 0;
    bool   thrown = false;

    snprintf(title, sizeof(title), "F%d", n);
    if (setjmp(g_throw_jmp) != 0) {
        return false;
    }
    while (done < len) {
        char   chunk[TZ_UI_STREAM_CONTENTS_SIZE + 1];
        size_t take = MIN(len - done, (size_t)TZ_UI_STREAM_CONTENTS_SIZE);

        memcpy(chunk, value + done, take);
        chunk[take] = '\0';
        step_through_pages();
        done += tz_ui_stream_pushl(cb_type, title, chunk, -1, 0, 0);
        assert_true(++spins < 1000);
    }
    (void)thrown;
    return true;
}

/* Close the stream and step through what is left, as the review does */
static void
finish(void)
{
    tz_ui_stream_t *s = &global.ui.stream;

    tz_ui_stream_close();
    if (has_final_screen()) {
        s->total++;
    }
    step_through_pages();
}

/* A page that holds `n` pairs and is completed because the next one does not
   fit. The values are long enough for that, the fields are F<first>.. */
static void
push_page_of(int first, int n)
{
    char value[40];

    for (int i = 0; i < n; i++) {
        snprintf(value, sizeof(value), "page%d-pair%d", first, i);
        assert_true(push_field(first + i, TZ_UI_STREAM_CB_NOCB, value));
    }
}

static void
push_compact_page(int n)
{
    char value[16];

    snprintf(value, sizeof(value), "compact%d", n);
    assert_true(push_field(n, TZ_UI_STREAM_CB_CANCEL, value));
}

/* The history holds eight pages: the first one has `n0` pairs, completed by
   a boundary when it has less than the most a page holds, and the others
   hold one pair each. Returns the first field number that is free. */
static int
fill_history(int n0)
{
    int next = n0;

    push_page_of(0, n0);
    while (global.ui.stream.total < (int16_t)(H - 1)) {
        push_compact_page(next);
        next++;
    }
    return next;
}

/* The ninth page takes the place of the first one: it has to start empty,
   with the field that starts it and nothing of the page it replaced, and no
   page is completed by the push. */
static void
check_ninth_page_is_clean(void)
{
    tz_ui_stream_t              *s    = &global.ui.stream;
    const tz_ui_stream_screen_t *page = &s->screens[(s->total + 1) % H];

    assert_int_equal(g_sw_sent, 0);
    assert_int_equal(s->total, H - 1);
    assert_int_equal(s->last, 1);
    assert_int_equal(page->nb_pairs, 1);
    assert_string_equal(page->pairs[0].value, "ninth-page");
    assert_int_equal(g_empty_pages, 0);
}

static void
test_ninth_page_over_an_underfilled_page(void **state)
{
    (void)state;
    int next = fill_history(2);

    assert_true(push_field(next, TZ_UI_STREAM_CB_NOCB, "ninth-page"));
    check_ninth_page_is_clean();
}

static void
test_ninth_page_over_a_full_page(void **state)
{
    (void)state;
    int next = fill_history(NB_MAX_DISPLAYED_PAIRS_IN_REVIEW);

    assert_true(push_field(next, TZ_UI_STREAM_CB_NOCB, "ninth-page"));
    check_ninth_page_is_clean();
}

/* The final page, with nothing pending, is not a page: the one that is in its
   place is old. */
static void
test_no_final_page_over_a_held_one(void **state)
{
    (void)state;
    (void)fill_history(NB_MAX_DISPLAYED_PAIRS_IN_REVIEW);

    assert_false(has_final_screen());
}

/* A page completed by a boundary (here an expert mode field) before the first
   page is full is shown, and so are the pages after it. */
static void
test_first_page_completed_by_a_boundary(void **state)
{
    (void)state;
    char expected[MAX_FIELDS][MAX_FIELD_LEN];

    memset(expected, 0, sizeof(expected));
    for (int n = 0; n < 12; n++) {
        char value[16];

        snprintf(value, sizeof(value), "value%d", n);
        strcpy(expected[n], value);
        assert_true(push_field(n,
                               (n == 2) ? TZ_UI_STREAM_CB_EXPERT_MODE_FIELD
                                        : TZ_UI_STREAM_CB_NOCB,
                               value));
    }
    finish();

    for (int n = 0; n < 12; n++) {
        assert_string_equal(g_shown[n], expected[n]);
    }
    assert_int_equal(g_empty_pages, 0);
}

/* More fields than the history holds, of every length and kind: each one is
   shown once, whole, and in order, and there is no empty page. Many
   sequences, each from its own seed. */
static void
test_every_field_is_shown_once(void **state)
{
    for (unsigned seed0 = 1; seed0 <= 80U; seed0++) {
        unsigned seed = seed0 * 2654435761U;
        char     expected[MAX_FIELDS][MAX_FIELD_LEN];
        int      n = 0;

        setup(state);
        memset(expected, 0, sizeof(expected));
        while (n < 40) {
            char            value[160];
            unsigned        len;
            tz_ui_cb_type_t type = TZ_UI_STREAM_CB_NOCB;

            seed = seed * 1103515245U + 12345U;
            len  = 1U + (seed >> 8) % 90U;
            for (unsigned i = 0; i < len; i++) {
                seed     = seed * 1103515245U + 12345U;
                value[i] = (char)('a' + ((seed >> 16) % 26U));
            }
            value[len] = '\0';
            seed       = seed * 1103515245U + 12345U;
            if (((seed >> 16) % 7U) == 0U) {
                type = TZ_UI_STREAM_CB_CANCEL;
            } else if (((seed >> 16) % 7U) == 1U) {
                type = TZ_UI_STREAM_CB_EXPERT_MODE_FIELD;
            }
            strcpy(expected[n], value);
            assert_true(push_field(n, type, value));
            n++;
        }
        finish();

        for (int i = 0; i < n; i++) {
            assert_string_equal(g_shown[i], expected[i]);
        }
        assert_int_equal(g_empty_pages, 0);
        assert_int_equal(g_sw_sent, 0);
    }
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_ninth_page_over_an_underfilled_page,
                               setup),
        cmocka_unit_test_setup(test_ninth_page_over_a_full_page, setup),
        cmocka_unit_test_setup(test_no_final_page_over_a_held_one, setup),
        cmocka_unit_test_setup(test_first_page_completed_by_a_boundary,
                               setup),
        cmocka_unit_test_setup(test_every_field_is_shown_once, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
