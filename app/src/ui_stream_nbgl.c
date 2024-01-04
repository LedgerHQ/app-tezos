/* Tezos Ledger application - Generic stream display

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 Functori <contact@functori.com>
   Copyright 2023 TriliTech <contact@trili.tech>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#ifdef HAVE_NBGL

#include <nbgl_use_case.h>
#include <ux.h>

#include "globals.h"
#include "ui_stream.h"

bool tz_ui_nav_cb(uint8_t, nbgl_pageContent_t *);
bool has_final_screen(void);
void tz_ui_stream_start(void);

void drop_last_screen(void);
void push_str(const char *, size_t, char **);
void switch_to_blindsigning(const char *, const char *);

void
tz_cancel_ui(void)
{
    tz_ui_stream_t *s = &global.stream;
    FUNC_ENTER(("void"));

    size_t bucket = s->current % TZ_UI_STREAM_HISTORY_SCREENS;
    switch_to_blindsigning(s->screens[bucket].pairs[0].item,
                           s->screens[bucket].pairs[0].value);
    FUNC_LEAVE();
}

void
tz_reject(void)
{
    tz_ui_stream_t *s = &global.stream;

    FUNC_ENTER(("void"));

    // Stax can reject early
    global.keys.apdu.sign.step              = SIGN_ST_WAIT_USER_INPUT;
    global.keys.apdu.sign.received_last_msg = true;

    if (global.step == ST_BLIND_SIGN) {
        s->cb(TZ_UI_STREAM_CB_BLINDSIGN_REJECT);
    } else {
        s->cb(TZ_UI_STREAM_CB_REJECT);
    }

    global.step = ST_IDLE;
    nbgl_useCaseStatus("Transaction rejected", false, ui_home_init);

    FUNC_LEAVE();
}

void
tz_reject_ui(void)
{
    FUNC_ENTER(("void"));

    nbgl_useCaseConfirm("Reject transaction?", NULL, "Yes, Reject",
                        "Go back to transaction", tz_reject);

    FUNC_LEAVE();
}

static void
start_blindsign(void)
{
    FUNC_ENTER(("void"));

    tz_ui_stream_t *s = &global.stream;
    s->cb(TZ_UI_STREAM_CB_BLINDSIGN);

    FUNC_LEAVE();
}

static void
blindsign_splash(void)
{
    TZ_PREAMBLE(("void"));
    nbgl_useCaseReviewStart(
        &C_round_warning_64px, "Blind signing",
        "This transaction can not be securely interpreted by Ledger Stax. It "
        "might put your assets at risk.",
        "Reject transaction", start_blindsign, tz_reject_ui);

    TZ_POSTAMBLE;
}

static void
handle_blindsigning(bool confirm)
{
    TZ_PREAMBLE(("void"));
    if (confirm) {
        if (!N_settings.blindsigning)
            toggle_blindsigning();
        nbgl_useCaseReviewStart(&C_round_check_64px, "Blind signing enabled",
                                NULL, "Reject transaction", blindsign_splash,
                                tz_reject_ui);

    } else {
        tz_reject_ui();
    }
    TZ_POSTAMBLE;
}

void
switch_to_blindsigning(__attribute__((unused)) const char *err_type,
                       const char                         *err_code)
{
    TZ_PREAMBLE(("void"));
    PRINTF("[DEBUG] refill_error: global.step = %d\n", global.step);
    TZ_ASSERT(EXC_UNEXPECTED_STATE, global.step == ST_CLEAR_SIGN);
    global.keys.apdu.sign.step = SIGN_ST_WAIT_USER_INPUT;
    global.step                = ST_BLIND_SIGN;
    if (N_settings.blindsigning) {
        nbgl_useCaseReviewStart(
            &C_round_warning_64px, "Blind signing required:\nParsing Error",
            err_code, "Reject transaction", blindsign_splash, tz_reject_ui);
    } else {
        nbgl_useCaseChoice(&C_round_warning_64px,
                           "Enable blind signing to authorize this "
                           "transaction:\nParsing Error",
                           err_code, "Enable blind signing",
                           "Reject transaction", handle_blindsigning);
    }

    TZ_POSTAMBLE;
}
void
expert_mode_splash(void)
{
    TZ_PREAMBLE(("void"));

    nbgl_useCaseReviewStart(&C_round_warning_64px, "Expert mode",
                            "Next screen requires careful review",
                            "Reject transaction", tz_ui_stream_start,
                            tz_reject_ui);

    TZ_POSTAMBLE;
}

void
handle_expert_mode(bool confirm)
{
    TZ_PREAMBLE(("void"));
    if (confirm) {
        if (!N_settings.expert_mode)
            toggle_expert_mode();

        nbgl_useCaseReviewStart(&C_round_check_64px, "Expert mode enabled",
                                NULL, "Reject transaction",
                                tz_ui_stream_start, tz_reject_ui);

    } else {
        tz_reject_ui();
    }
    TZ_POSTAMBLE;
}

void
tz_enable_expert_mode_ui(void)
{
    FUNC_LEAVE();

    nbgl_useCaseChoice(&C_round_warning_64px,
                       "Enable expert mode to authorize this "
                       "transaction",
                       "", "Enable expert mode", "Reject transaction",
                       handle_expert_mode);

    FUNC_LEAVE();
}

void
tz_accept_ui(void)
{
    tz_ui_stream_t *s = &global.stream;

    FUNC_ENTER(("void"));

    global.keys.apdu.sign.step = SIGN_ST_WAIT_USER_INPUT;
    s->cb(TZ_UI_STREAM_CB_ACCEPT);

    nbgl_useCaseStatus("TRANSACTION\nSIGNED", true, ui_home_init);

    FUNC_LEAVE();
}

void
tz_choice_ui(bool accept)
{
    FUNC_ENTER(("accept=%d", accept));

    if (accept) {
        tz_accept_ui();
    } else {
        tz_reject_ui();
    }

    FUNC_LEAVE();
}

void
tz_ui_continue(void)
{
    tz_ui_stream_t *s = &global.stream;

    TZ_PREAMBLE(("void"));

    if (!s->full) {
        TZ_CHECK(s->cb(TZ_UI_STREAM_CB_REFILL));
        PRINTF("[DEBUG] total=%d\n", s->total);
    }

    TZ_POSTAMBLE;
}

void
tz_ui_stream_cb(void)
{
    FUNC_ENTER(("void"));

    nbgl_useCaseForwardOnlyReviewNoSkip("Reject transaction", NULL,
                                        tz_ui_nav_cb, tz_choice_ui);

    FUNC_LEAVE();
}

void
tz_ui_stream(void)
{
    tz_ui_stream_t *s = &global.stream;
    FUNC_ENTER(("void"));

    if (s->stream_cb)
        s->stream_cb();

    FUNC_LEAVE();
    return;
}

void
tz_ui_review_start()
{
    tz_ui_stream_t *s = &global.stream;

    FUNC_ENTER(("void"));

    s->stream_cb = &tz_ui_stream_cb;
    tz_ui_stream();

    FUNC_LEAVE();
}

void
tz_ui_stream_init(void (*cb)(uint8_t))
{
    tz_ui_stream_t *s = &global.stream;

    FUNC_ENTER(("cb=%p", cb));
    memset(s, 0x0, sizeof(*s));
    s->cb      = cb;
    s->full    = false;
    s->last    = 0;
    s->current = -1;
    s->total   = -1;

    ui_strings_init();

    nbgl_useCaseReviewStart(&C_tezos, "Review request to sign operation",
                            NULL, "Reject request", tz_ui_review_start,
                            tz_reject_ui);

    FUNC_LEAVE();
}

void
tz_ui_stream_start(void)
{
    FUNC_ENTER(("void"));
    tz_ui_stream_cb();
    FUNC_LEAVE();
}

void
tz_ui_stream_close(void)
{
    tz_ui_stream_t *s = &global.stream;

    FUNC_ENTER(("full=%d", s->full));
    if (s->full) {
        PRINTF("trying to close already closed stream display");
        THROW(EXC_UNKNOWN);
    }
    s->full = true;

    FUNC_LEAVE();
}

bool
tz_ui_nav_cb(uint8_t page, nbgl_pageContent_t *content)
{
    FUNC_ENTER(("page=%d, content=%p", page, content));

    tz_ui_stream_t         *s      = &global.stream;
    tz_ui_stream_display_t *c      = &s->current_screen;
    bool                    result = true;
    tz_parser_state        *st = &global.keys.apdu.sign.u.clear.parser_state;

    PRINTF(
        "[DEBUG] pressed_right=%d, current=%d, total=%d, full=%d, "
        "global.step= %d\n",
        s->pressed_right, s->current, s->total, s->full, global.step);

    while ((s->total < 0 || (s->current == s->total && !s->full))
           && (st->errno < TZ_ERR_INVALID_TAG)) {
        PRINTF("tz_ui_nav_cb: Looping...\n");
        tz_ui_continue();
        if (global.step == ST_ERROR) {
            break;
        } else if (global.keys.apdu.sign.step == SIGN_ST_WAIT_DATA) {
            result = false;
            break;
        }
    }

    if (s->full && has_final_screen()) {
        s->total++;
    }

    PRINTF(
        "[DEBUG] pressed_right=%d, current=%d, total=%d, full=%d, "
        "global.step=%d\n",
        s->pressed_right, s->current, s->total, s->full, global.step);

    if (global.step == ST_ERROR) {
        // TODO: this is handled by change_screen_right except we disable it
        // to use it here. We should make ui_stream fully compatible with
        // exception.h
        global.step = ST_IDLE;
        ui_home_init();
        result = false;
    } else if (global.step != ST_CLEAR_SIGN) {
        result = false;
    } else if ((s->current == s->total) && s->full) {
        PRINTF("[DEBUG] signing...\n");
        content->type                        = INFO_LONG_PRESS;
        content->infoLongPress.icon          = &C_tezos;
        content->infoLongPress.text          = "Sign transaction?";
        content->infoLongPress.longPressText = "Hold to sign";
    } else if (page == LAST_PAGE_FOR_REVIEW) {
        s->current = s->total;
        result     = false;

    } else if (s->total >= 0) {
        PRINTF("[DEBUG] Display: curr=%d total=%d pr=%d\n", s->current,
               s->total, s->pressed_right);

        if (s->current < s->total) {
            s->current++;
        }

        size_t bucket = s->current % TZ_UI_STREAM_HISTORY_SCREENS;

        if (tz_ui_stream_get_cb_type() == TZ_UI_STREAM_CB_CANCEL) {
            // We hit an error in the parsing workflow...
            tz_cancel_ui();
            result = false;
        } else if (tz_ui_stream_get_cb_type()
                   == TZ_UI_STREAM_CB_EXPERT_MODE_ENABLE) {
            tz_enable_expert_mode_ui();
            result = false;
        } else if (tz_ui_stream_get_cb_type()
                   == TZ_UI_STREAM_CB_EXPERT_MODE_FIELD) {
            expert_mode_splash();
            s->current--;
            s->screens[bucket].cb_type = TZ_UI_STREAM_CB_NOCB;
            result                     = false;
        } else {
            c->list.pairs             = s->screens[bucket].pairs;
            c->list.callback          = NULL;
            c->list.startIndex        = 0;
            c->list.nbPairs           = s->screens[bucket].nb_pairs;
            c->list.smallCaseForValue = false;
            c->list.wrapping          = true;

            content->type         = TAG_VALUE_LIST;
            content->tagValueList = c->list;
            result                = true;
        }
    }

    FUNC_LEAVE();
    return result;
}

bool
has_final_screen(void)
{
    tz_ui_stream_t *s  = &global.stream;
    size_t last_bucket = (s->total + 1) % TZ_UI_STREAM_HISTORY_SCREENS;
    return s->screens[last_bucket].nb_pairs > 0;
}

/* pushl mechanism
 * We use s->total differently to other devices
 * s->total     points to the last completed screen
 * s->total + 1 points to the screen under construction
 */
size_t
tz_ui_stream_pushl(tz_ui_cb_type_t cb_type, const char *title,
                   const char *value, ssize_t max,
                   __attribute__((unused)) tz_ui_layout_type_t layout_type,
                   __attribute__((unused)) tz_ui_icon_t        icon)
{
    tz_ui_stream_t *s = &global.stream;
    bool            push_to_next;
    bool            append    = false;
    size_t          max_pairs = (cb_type == TZ_UI_STREAM_CB_CANCEL)
                                    ? 1
                                    : NB_MAX_DISPLAYED_PAIRS_IN_REVIEW;

    FUNC_ENTER(("title=%s, value=%s", title, value));
    if (s->full) {
        PRINTF("trying to push in already closed stream display");
        THROW(EXC_UNKNOWN);
    }

#ifdef TEZOS_DEBUG
    int    prev_total   = s->total;
    int    prev_current = s->current;
    int    prev_last    = s->last;
    size_t i;
#endif

    int    bucket = (s->total + 1) % TZ_UI_STREAM_HISTORY_SCREENS;
    size_t idx    = s->screens[bucket].nb_pairs;
    size_t offset = 0;
    size_t length = strlen(value);

    if (s->total >= 0
        && (s->current % TZ_UI_STREAM_HISTORY_SCREENS) == bucket) {
        PRINTF(
            "[ERROR] PANIC!!!! Overwriting current screen, some bad things "
            "are happening\n");
    }

    if ((cb_type == TZ_UI_STREAM_CB_CANCEL
         || cb_type == TZ_UI_STREAM_CB_EXPERT_MODE_ENABLE
         || cb_type == TZ_UI_STREAM_CB_EXPERT_MODE_FIELD)
        && idx > 0) {
        PRINTF("[DEBUG] PUSH_TO_NEXT: %x\n", cb_type);
        push_to_next = true;
    } else {
        /* Are we continuing to construct or starting from scratch? */
        if (idx == NB_MAX_DISPLAYED_PAIRS_IN_REVIEW) {
            PRINTF("[ERROR] PANIC!!! we pushing to a screen that's full");
        }

        if (idx > 0
            && 0 == strcmp(title, s->screens[bucket].pairs[idx - 1].item))
            append = true;

        if (!append)
            push_str(title, strlen(title),
                     (char **)&s->screens[bucket].pairs[idx].item);

        if (max != -1)
            length = MIN(length, (size_t)max);

        if (idx == 0) {
            s->screens[bucket].cb_type = cb_type;
        }

        char *out = NULL;

        if (append) {
            bool can_fit = false;
            ui_strings_can_fit(length, &can_fit);
            if (!can_fit)
                drop_last_screen();

            offset = ui_strings_append_last(&value[offset], length, &out);
            idx--;
        } else {
            push_str(&value[offset], length,
                     (char **)&s->screens[bucket].pairs[idx].value);
            offset = length;
        }

        /* Check that the whole screen fits on the page
         * if it doesn't, we need to pop this pair, and move
         * to the next screen.
         */
        nbgl_layoutTagValueList_t l = {.nbPairs = idx,
                                       .pairs   = s->screens[bucket].pairs,
                                       .smallCaseForValue = false,
                                       .wrapping          = true};

        /* This returns the number that can fit. Potentially we could
         * optimistically push all 4 rows, and then try?
         */
        uint8_t fit = nbgl_useCaseGetNbTagValuesInPage((uint8_t)(idx + 1), &l,
                                                       0, &push_to_next);
        PRINTF("[DEBUG] idx=%d fit=%d push_to_next=%d\n", idx, fit,
               push_to_next);
        push_to_next |= fit <= (uint8_t)idx;

        if (push_to_next) {
            /* We need to move to the next screen, retry */
            if (append && offset > 0) {
                ui_strings_drop_last(&out);
            } else {
                ui_strings_drop_last(
                    (char **)&s->screens[bucket].pairs[idx].value);
                ui_strings_drop_last(
                    (char **)&s->screens[bucket].pairs[idx].item);
            }

            offset = 0;
        }
    }

    if (push_to_next
        || (!append && ++(s->screens[bucket].nb_pairs) == max_pairs)
        || (append && offset == 0)) {
        s->total++;
        if (s->total > 0
            && s->total % TZ_UI_STREAM_HISTORY_SCREENS
                   == s->last % TZ_UI_STREAM_HISTORY_SCREENS) {
            drop_last_screen();
        }
    }

#ifdef TEZOS_DEBUG
    PRINTF("[DEBUG] tz_ui_stream_pushl(%s, %s, %u)\n", title, value, max);
    PRINTF("[DEBUG]        bucket     %d\n", bucket);
    PRINTF("[DEBUG]        nb_pairs   %d\n", s->screens[bucket].nb_pairs);

    for (i = 0; i < s->screens[bucket].nb_pairs; i++) {
        PRINTF("[DEBUG]        item[%d]:   \"%s\"\n", i,
               s->screens[bucket].pairs[i].item);
        PRINTF("[DEBUG]        value[%d]:  \"%s\"\n", i,
               s->screens[bucket].pairs[i].value);
    }
    PRINTF("[DEBUG]        total:     %d -> %d\n", prev_total, s->total);
    PRINTF("[DEBUG]        current:   %d -> %d\n", prev_current, s->current);
    PRINTF("[DEBUG]        last:      %d -> %d\n", prev_last, s->last);
    PRINTF("[DEBUG]        offset:    %d\n", offset);
    PRINTF("[DEBUG]        cb:        %x\n", s->screens[bucket].cb_type);
#endif  // TEZOS_DEBUG

    FUNC_LEAVE();

    return offset;
}

void
drop_last_screen(void)
{
    tz_ui_stream_t *s      = &global.stream;
    size_t          bucket = s->last % TZ_UI_STREAM_HISTORY_SCREENS;
    size_t          i;

    TZ_PREAMBLE(("last: %d", s->last));

    for (i = 0; i < s->screens[bucket].nb_pairs; i++) {
        if (s->screens[bucket].pairs[i].item)
            TZ_CHECK(
                ui_strings_drop((char **)&s->screens[bucket].pairs[i].item));
        if (s->screens[bucket].pairs[i].value)
            TZ_CHECK(
                ui_strings_drop((char **)&s->screens[bucket].pairs[i].value));
    }
    s->screens[bucket].nb_pairs = 0;

    s->last++;

    TZ_POSTAMBLE;
}

#endif
