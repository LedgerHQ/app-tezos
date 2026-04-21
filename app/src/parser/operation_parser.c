/* Tezos Embedded C parser for Ledger - Operation parser

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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "operation_parser.h"
#include "fa2_tokens.h"
#include "formatting.h"
#include "micheline_parser.h"
#include "micheline_state.h"
#include "num_parser.h"

/* Prototypes */

static tz_parser_result push_frame(tz_parser_state              *state,
                                   tz_operation_parser_step_kind step);
static tz_parser_result pop_frame(tz_parser_state *state);
static tz_parser_result tz_step_read_set_delegate_params(
    tz_parser_state *state);
static void tz_manager_entrypoint_set(tz_operation_state *op,
                                      const char         *name);
static bool tz_implicit_fee_payer_differs_from_dest(
    const tz_operation_state *op);

#ifdef TEZOS_DEBUG
const char *const tz_operation_parser_step_name[]
    = {"OPTION",
       "TUPLE",
       "MAGIC",
       "READ_BINARY",
       "BRANCH",
       "BATCH",
       "TAG",
       "SIZE",
       "FIELD",
       "PRINT",
       "PARTIAL_PRINT",
       "READ_NUM",
       "READ_INT32",
       "READ_PK",
       "READ_BYTES",
       "READ_STRING",
       "READ_SMART_ENTRYPOINT",
       "READ_MICHELINE",
       "READ_SORU_MESSAGES",
       "READ_SORU_KIND",
       "READ_BALLOT",
       "READ_PROTOS",
       "READ_PKH_LIST",
       "READ_FA2_TRANSFER",
       "READ_SET_DELEGATE_PARAMS"};

/**
 * @brief Get the string format of an operations step
 *
 */
#define STRING_STEP(step) \
    (const char *)PIC(tz_operation_parser_step_name[step])

#endif

// clang-format off

// Default .skip=false, .complex=false

/**
 * @brief Helper to create an operation field descriptor
 *
 * @required name: name of the field
 * @required kind: kind of the field
 *
 *        By default .skip=false, .complex=false
 */
#define TZ_OPERATION_FIELD(name_v, kind_v, ...) \
  {.name=name_v, .kind=kind_v, __VA_ARGS__}

#define TZ_OPERATION_LAST_FIELD {0} /// Empty field with an END step

/**
 * @brief Helper to create an operation option field descriptor
 *
 * @required name: name of the option field
 * @required field: field of the option field
 * @required display_none: display if is none
 *
 *        By default .skip=false, .complex=false
 */
#define TZ_OPERATION_OPTION_FIELD(name_v, field_v, display_none, ...) \
  {.name=name_v, .kind=TZ_OPERATION_FIELD_OPTION,                     \
   .field_option={                                                    \
       .field=&(const tz_operation_field_descriptor)field_v,          \
       display_none                                                   \
   },                                                                 \
   __VA_ARGS__}

/**
 * @brief Helper to create an operation tuple field descriptor
 *
 * @required name: name of the tuple field
 * @required fields: fields of the tuple field
 */
#define TZ_OPERATION_TUPLE_FIELD(name_v, ...)           \
  {.name=name_v, .kind=TZ_OPERATION_FIELD_TUPLE,        \
   .field_tuple={                                       \
       .fields=(const tz_operation_field_descriptor[]){ \
           __VA_ARGS__,                                 \
           TZ_OPERATION_LAST_FIELD                      \
       }                                                \
   }                                                    \
  }

/**
 * @brief Helper to create an operation descriptor
 *
 * @required name: name of the operation
 * @required fields: fields of the operation
 */
#define TZ_OPERATION_FIELDS(name, ...) \
  const tz_operation_field_descriptor name[] = { __VA_ARGS__, TZ_OPERATION_LAST_FIELD}

TZ_OPERATION_FIELDS(proposals_fields,
    TZ_OPERATION_FIELD("Source",   TZ_OPERATION_FIELD_PKH),
    TZ_OPERATION_FIELD("Period",   TZ_OPERATION_FIELD_INT32),
    TZ_OPERATION_FIELD("Proposal", TZ_OPERATION_FIELD_PROTOS)
);

TZ_OPERATION_FIELDS(ballot_fields,
    TZ_OPERATION_FIELD("Source",   TZ_OPERATION_FIELD_PKH),
    TZ_OPERATION_FIELD("Period",   TZ_OPERATION_FIELD_INT32),
    TZ_OPERATION_FIELD("Proposal", TZ_OPERATION_FIELD_PROTO),
    TZ_OPERATION_FIELD("Ballot",   TZ_OPERATION_FIELD_BALLOT)
);

TZ_OPERATION_FIELDS(failing_noop_fields,
    TZ_OPERATION_FIELD("Message", TZ_OPERATION_FIELD_BINARY)
);

/**
 * @brief Set of fields for manager operations
 */
#define TZ_OPERATION_MANAGER_OPERATION_FIELDS                                \
    TZ_OPERATION_FIELD("Source",        TZ_OPERATION_FIELD_SOURCE),          \
    TZ_OPERATION_FIELD("Fee",           TZ_OPERATION_FIELD_FEE),             \
    TZ_OPERATION_FIELD("_Counter",      TZ_OPERATION_FIELD_NAT, .skip=true), \
    TZ_OPERATION_FIELD("_Gas",          TZ_OPERATION_FIELD_NAT, .skip=true), \
    TZ_OPERATION_FIELD("Storage limit", TZ_OPERATION_FIELD_NAT)

TZ_OPERATION_FIELDS(transaction_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Amount",      TZ_OPERATION_FIELD_AMOUNT),
    TZ_OPERATION_FIELD("Destination", TZ_OPERATION_FIELD_DESTINATION),
    TZ_OPERATION_OPTION_FIELD("_Parameters",
        TZ_OPERATION_TUPLE_FIELD("_Parameters",
            TZ_OPERATION_FIELD("Entrypoint", TZ_OPERATION_FIELD_SMART_ENTRYPOINT),
            TZ_OPERATION_FIELD("Parameter",  TZ_OPERATION_FIELD_EXPR, .complex=true)),
        .display_none=false)
);

TZ_OPERATION_FIELDS(reveal_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Public key", TZ_OPERATION_FIELD_PK),
    TZ_OPERATION_OPTION_FIELD("Proof",
        TZ_OPERATION_FIELD("Proof", TZ_OPERATION_FIELD_BLS_SIG),
        .display_none=false)
);

TZ_OPERATION_FIELDS(delegation_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_OPTION_FIELD("Delegate",
        TZ_OPERATION_FIELD("Delegate", TZ_OPERATION_FIELD_PKH),
        .display_none=true)
);

TZ_OPERATION_FIELDS(reg_glb_cst_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Value", TZ_OPERATION_FIELD_EXPR, .complex=true)
);

TZ_OPERATION_FIELDS(set_deposit_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_OPTION_FIELD("Staking limit",
        TZ_OPERATION_FIELD("Staking limit", TZ_OPERATION_FIELD_AMOUNT),
        .display_none=true)
);

TZ_OPERATION_FIELDS(inc_paid_stg_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Amount",      TZ_OPERATION_FIELD_INT),
    TZ_OPERATION_FIELD("Destination", TZ_OPERATION_FIELD_DESTINATION)
);

TZ_OPERATION_FIELDS(set_cons_key_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Public key", TZ_OPERATION_FIELD_PK),
    TZ_OPERATION_OPTION_FIELD("Proof",
        TZ_OPERATION_FIELD("Proof", TZ_OPERATION_FIELD_BLS_SIG),
        .display_none=false)
);

TZ_OPERATION_FIELDS(set_comp_key_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Public key", TZ_OPERATION_FIELD_PK),
    TZ_OPERATION_OPTION_FIELD("Proof",
        TZ_OPERATION_FIELD("Proof", TZ_OPERATION_FIELD_BLS_SIG),
        .display_none=false)
);

TZ_OPERATION_FIELDS(origination_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Balance", TZ_OPERATION_FIELD_AMOUNT),
    TZ_OPERATION_OPTION_FIELD("Delegate",
        TZ_OPERATION_FIELD("Delegate", TZ_OPERATION_FIELD_PKH),
        .display_none=true),
    TZ_OPERATION_FIELD("Code",    TZ_OPERATION_FIELD_EXPR, .complex=true),
    TZ_OPERATION_FIELD("Storage", TZ_OPERATION_FIELD_EXPR, .complex=true)
);

TZ_OPERATION_FIELDS(transfer_tck_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Contents",    TZ_OPERATION_FIELD_EXPR, .complex=true),
    TZ_OPERATION_FIELD("Type",        TZ_OPERATION_FIELD_EXPR, .complex=true),
    TZ_OPERATION_FIELD("Ticketer",    TZ_OPERATION_FIELD_DESTINATION),
    TZ_OPERATION_FIELD("Amount",      TZ_OPERATION_FIELD_NAT),
    TZ_OPERATION_FIELD("Destination", TZ_OPERATION_FIELD_DESTINATION),
    TZ_OPERATION_FIELD("Entrypoint",  TZ_OPERATION_FIELD_STRING)
);

TZ_OPERATION_FIELDS(soru_add_msg_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Message", TZ_OPERATION_FIELD_SORU_MESSAGES)
);

TZ_OPERATION_FIELDS(soru_exe_msg_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Rollup",       TZ_OPERATION_FIELD_SR),
    TZ_OPERATION_FIELD("Commitment",   TZ_OPERATION_FIELD_SRC),
    TZ_OPERATION_FIELD("Output proof", TZ_OPERATION_FIELD_BINARY, .complex=true)
);

TZ_OPERATION_FIELDS(soru_origin_fields,
    TZ_OPERATION_MANAGER_OPERATION_FIELDS,
    TZ_OPERATION_FIELD("Kind",       TZ_OPERATION_FIELD_SORU_KIND),
    TZ_OPERATION_FIELD("Kernel",     TZ_OPERATION_FIELD_BINARY, .complex=true),
    TZ_OPERATION_FIELD("Parameters", TZ_OPERATION_FIELD_EXPR,   .complex=true),
    TZ_OPERATION_OPTION_FIELD("Whitelist",
        TZ_OPERATION_FIELD("Whitelist",  TZ_OPERATION_FIELD_PKH_LIST),
        .display_none=false)
);

#define FA2_REQUIRE(state, cond) \
    do { \
        if (!(cond)) \
            return fa2_fallback_to_binary(state); \
    } while (0)

/**
 * @brief Array of all handled operations
 */
const tz_operation_descriptor tz_operation_descriptors[] = {
    {TZ_OPERATION_TAG_PROPOSALS,    "Proposals",                  proposals_fields   },
    {TZ_OPERATION_TAG_BALLOT,       "Ballot",                     ballot_fields      },
    {TZ_OPERATION_TAG_FAILING_NOOP, "Failing noop",               failing_noop_fields},
    {TZ_OPERATION_TAG_REVEAL,       "Reveal",                     reveal_fields      },
    {TZ_OPERATION_TAG_TRANSACTION,  "Transaction",                transaction_fields },
    {TZ_OPERATION_TAG_ORIGINATION,  "Origination",                origination_fields },
    {TZ_OPERATION_TAG_DELEGATION,   "Delegation",                 delegation_fields  },
    {TZ_OPERATION_TAG_REG_GLB_CST,  "Register global constant",   reg_glb_cst_fields },
    {TZ_OPERATION_TAG_SET_DEPOSIT,  "Set deposit limit",          set_deposit_fields },
    {TZ_OPERATION_TAG_INC_PAID_STG, "Increase paid storage",      inc_paid_stg_fields},
    {TZ_OPERATION_TAG_SET_CONS_KEY, "Set consensus key",          set_cons_key_fields},
    {TZ_OPERATION_TAG_SET_COMP_KEY, "Set companion key",          set_comp_key_fields},
    {TZ_OPERATION_TAG_TRANSFER_TCK, "Transfer ticket",            transfer_tck_fields},
    {TZ_OPERATION_TAG_SORU_ADD_MSG, "SR: send messages",          soru_add_msg_fields},
    {TZ_OPERATION_TAG_SORU_EXE_MSG, "SR: execute outbox message", soru_exe_msg_fields},
    {TZ_OPERATION_TAG_SORU_ORIGIN,  "SR: originate",              soru_origin_fields },
    {0,                             NULL,                         0                  }
};
// clang-format on

static const char *expression_name = "Expression";   /// title for micheline
static const char *unset_message   = "Field unset";  /// title for unset field

/**
 * @brief Push a new frame onto the operations parser stack
 *
 * @param state: parser state
 * @param step: step of the new frame
 * @return tz_parser_result: parser result
 */
static tz_parser_result
push_frame(tz_parser_state *state, tz_operation_parser_step_kind step)
{
    tz_operation_state *op = &state->operation;

    if (op->frame >= &op->stack[TZ_OPERATION_STACK_DEPTH - 1]) {
        tz_raise(TOO_DEEP);
    }
    op->frame++;
    op->frame->step = step;
    tz_continue;
}

/**
 * @brief Pop the operations parser stack
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
pop_frame(tz_parser_state *state)
{
    tz_operation_state *op = &state->operation;

    if (op->frame == op->stack) {
        op->frame = NULL;
        tz_stop(DONE);
    }
    op->frame--;
    tz_continue;
}

void
tz_operation_parser_set_size(tz_parser_state *state, uint16_t size)
{
    state->operation.stack[0].stop = size;
}

void
tz_operation_parser_init(tz_parser_state *state, uint16_t size,
                         bool skip_magic)
{
    tz_operation_state *op = &state->operation;

    tz_parser_init(state);
    state->operation.seen_reveal           = 0;
    state->operation.is_fa2_candidate      = 0;
    state->operation.emit_finalize_note    = 0;
    state->operation.manager_entrypoint[0] = 0;
    state->operation.sdp_payload_start     = 0;
    memset(&state->operation.source, 0, 22);
    memset(&state->operation.destination, 0, 22);
    op->batch_index = 0;
#ifdef HAVE_SWAP
    op->last_tag  = TZ_OPERATION_TAG_END;
    op->nb_reveal = 0;
#endif  // HAVE_SWAP
    op->total_fee     = 0;
    op->total_amount  = 0;
    op->frame         = op->stack;
    op->stack[0].stop = size;
    if (!skip_magic) {
        op->stack[0].step = TZ_OPERATION_STEP_MAGIC;
    } else {
        STRLCPY(state->field_info.field_name, "Branch");
        op->stack[0].step = TZ_OPERATION_STEP_BRANCH;
        push_frame(state, TZ_OPERATION_STEP_READ_BYTES);  // ignore result,
                                                          // assume success
        op->frame->step_read_bytes.kind = TZ_OPERATION_FIELD_BH;
        op->frame->step_read_bytes.skip = true;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
    }
}

/*
 * We use a macro for CAPTURE rather than defining a ptr like:
 *
 *     uint8_t *capture = state->buffers.capture
 *
 * because sizeof(*capture) == 1, whereas sizeof(CAPTURE) is
 * the size of the buffer.  This allows us to more idiomatically
 * check the size of buffers.
 */
#define CAPTURE (state->buffers.capture)

/**
 * @brief Ask to print what has been captured
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static void
tz_manager_entrypoint_set(tz_operation_state *op, const char *name)
{
    strlcpy(op->manager_entrypoint, name, sizeof(op->manager_entrypoint));
}

static bool
tz_implicit_fee_payer_differs_from_dest(const tz_operation_state *op)
{
    if (op->destination[0] != 0x00) {
        return true;
    }
    return memcmp(op->source, op->destination + 1, 21) != 0;
}

static tz_parser_result
tz_print_string(tz_parser_state *state)
{
    tz_operation_state *op = &state->operation;

    if (op->frame->step_read_string.skip) {
        tz_must(pop_frame(state));
        tz_continue;
    }
    if (strcmp(state->field_info.field_name, "Entrypoint") == 0) {
        tz_manager_entrypoint_set(op, (const char *)CAPTURE);
    }
    op->frame->step           = TZ_OPERATION_STEP_PRINT;
    op->frame->step_print.str = (char *)CAPTURE;
    tz_continue;
}

/**
 * @brief Helper to assert the current step
 */
#define ASSERT_STEP(state, expected_step)                                    \
    do {                                                                     \
        tz_operation_parser_step_kind step = (state)->operation.frame->step; \
        if (step != TZ_OPERATION_STEP_##expected_step) {                     \
            PRINTF("[DEBUG] expected step %s but got step %s)\n",            \
                   STRING_STEP(TZ_OPERATION_STEP_##expected_step),           \
                   STRING_STEP(step));                                       \
            tz_raise(INVALID_STATE);                                         \
        }                                                                    \
    } while (0)

/**
 * @brief Try to read an optional field
 *
 *        If the field is present, ask to read it.
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_option(tz_parser_state *state)
{
    ASSERT_STEP(state, OPTION);
    tz_operation_state *op = &state->operation;
    uint8_t             present;
    tz_must(tz_parser_read(state, &present));
    if (!present) {
        if (op->frame->step_option.display_none) {
            if (op->frame->step_option.field->skip) {
                tz_raise(INVALID_STATE);
            }
            op->frame->step           = TZ_OPERATION_STEP_PRINT;
            op->frame->step_print.str = (char *)unset_message;
        } else {
            tz_must(pop_frame(state));
        }
    } else {
        op->frame->step             = TZ_OPERATION_STEP_FIELD;
        op->frame->step_field.field = op->frame->step_option.field;
    }
    tz_continue;
}

/**
 * @brief Ask to read remaining fields of a tuple field
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_tuple(tz_parser_state *state)
{
    ASSERT_STEP(state, TUPLE);
    tz_operation_state                  *op    = &state->operation;
    tz_parser_regs                      *regs  = &state->regs;
    const tz_operation_field_descriptor *field = PIC(
        &op->frame->step_tuple.fields[op->frame->step_tuple.field_index]);

    // Remaining content from previous section - display this first.
    if (regs->oofs > 0) {
        tz_stop(IM_FULL);
    }

    if (field->kind == TZ_OPERATION_FIELD_END) {
        // is_field_complex is reset after reaching the last field
        state->field_info.is_field_complex = false;
        tz_must(pop_frame(state));
    } else {
        op->frame->step_tuple.field_index++;
        tz_must(push_frame(state, TZ_OPERATION_STEP_FIELD));
        op->frame->step_field.field = field;
    }
    tz_continue;
}

/* Update the state in order to read an operation or a micheline expression
 * based on a magic byte */
/**
 * @brief Read a magic byte and plan nexts steps
 *
 *        The magic byte identifies if the data to read is a micheline
 *        expression or a batch of operations
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_magic(tz_parser_state *state)
{
    ASSERT_STEP(state, MAGIC);
    tz_operation_state *op = &state->operation;
    uint8_t             b;
    tz_must(tz_parser_read(state, &b));
    switch (b) {
    case 3:  // manager/anonymous operation
        STRLCPY(state->field_info.field_name, "Branch");
        op->stack[0].step = TZ_OPERATION_STEP_BRANCH;
        push_frame(state,
                   TZ_OPERATION_STEP_READ_BYTES);  // ignore result,
                                                   //  assume success
        op->frame->step_read_bytes.kind = TZ_OPERATION_FIELD_BH;
        op->frame->step_read_bytes.skip = true;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
        break;
    case 5:  // micheline expression
        op->frame->step = TZ_OPERATION_STEP_READ_MICHELINE;
        op->frame->step_read_micheline.inited = 0;
        op->frame->step_read_micheline.skip   = false;
        op->frame->step_read_micheline.name   = (char *)PIC(expression_name);
        op->frame->stop                       = 0;
        break;
    default:
        tz_raise(INVALID_TAG);
    }
    tz_continue;
}

/**
 * @brief Read a 4-bytes size
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_size(tz_parser_state *state)
{
    ASSERT_STEP(state, SIZE);
    tz_operation_state *op = &state->operation;
    uint8_t             b;
    tz_must(tz_parser_read(state, &b));
    if (op->frame->step_size.size > 255) {
        tz_raise(TOO_LARGE);  // enforce 16-bit restriction
    }
    op->frame->step_size.size = (op->frame->step_size.size << 8) | b;
    op->frame->step_size.size_len--;
    if (op->frame->step_size.size_len == 0) {
        op->frame[-1].stop = state->ofs + op->frame->step_size.size;
        tz_must(pop_frame(state));
    }
    tz_continue;
}

/**
 * @brief Find the operation associated to the operation tag and ask
 *        to read its fields
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_tag(tz_parser_state *state)
{
    ASSERT_STEP(state, TAG);
    tz_operation_state            *op = &state->operation;
    const tz_operation_descriptor *d;
    uint8_t                        t;
    tz_must(tz_parser_read(state, &t));
#ifdef HAVE_SWAP
    op->last_tag = t;
    if (t == TZ_OPERATION_TAG_REVEAL) {
        op->nb_reveal++;
    }
#endif  // HAVE_SWAP
    op->is_fa2_candidate = 0;
    memset(&op->destination, 0, 22);
    for (d = tz_operation_descriptors; d->tag != TZ_OPERATION_TAG_END; d++) {
        if (d->tag == t) {
            op->frame->step                   = TZ_OPERATION_STEP_TUPLE;
            op->frame->step_tuple.fields      = d->fields;
            op->frame->step_tuple.field_index = 0;
            tz_must(push_frame(state, TZ_OPERATION_STEP_PRINT));
            snprintf(state->field_info.field_name, 30, "Operation (%d)",
                     op->batch_index);
            op->frame->step_print.str = d->name;
            tz_continue;
        }
    }
    tz_raise(INVALID_TAG);
}

/* FA2 transfer parser sub-steps */
#define FA2_STEP_OUTER_SEQ_TAG   0  /* expect 0x02 (SEQ) */
#define FA2_STEP_OUTER_SEQ_SIZE  1  /* read 4-byte size */
#define FA2_STEP_OUTER_PAIR_TAG  2  /* expect 0x07 (PRIM_2_NOANNOTS) */
#define FA2_STEP_OUTER_PAIR_OP   3  /* expect 0x07 (Pair opcode) */
#define FA2_STEP_FROM_ADDR_TAG   4  /* expect 0x01 (STRING) or 0x0A (BYTES) */
#define FA2_STEP_FROM_ADDR_SIZE  5  /* read 4-byte size */
#define FA2_STEP_FROM_ADDR_BYTES 6  /* read addr_len bytes into CAPTURE */
#define FA2_STEP_TXS_SEQ_TAG     7  /* expect 0x02 (SEQ) */
#define FA2_STEP_TXS_SEQ_SIZE    8  /* read 4-byte size */
#define FA2_STEP_TXS_PAIR_TAG    9  /* expect 0x07 (PRIM_2_NOANNOTS) */
#define FA2_STEP_TXS_PAIR_OP     10 /* expect 0x07 (Pair opcode) */
#define FA2_STEP_TO_ADDR_TAG     11 /* expect 0x01 (STRING) or 0x0A (BYTES) */
#define FA2_STEP_TO_ADDR_SIZE    12 /* read 4-byte size */
#define FA2_STEP_TO_ADDR_BYTES   13 /* read addr_len bytes into CAPTURE */
#define FA2_STEP_INNER_SEQ_TAG   14 /* expect 0x02 (SEQ) */
#define FA2_STEP_INNER_SEQ_SIZE  15 /* read 4-byte size */
#define FA2_STEP_INNER_PAIR_TAG  16 /* expect 0x07 (PRIM_2_NOANNOTS) */
#define FA2_STEP_INNER_PAIR_OP   17 /* expect 0x07 (Pair opcode) */
#define FA2_STEP_TOKEN_ID_TAG    18 /* expect 0x00 (INT) */
#define FA2_STEP_TOKEN_ID_VAL    19 /* read varint (must be 0) */
#define FA2_STEP_AMOUNT_TAG      20 /* expect 0x00 (INT) */
#define FA2_STEP_AMOUNT_VAL      21 /* read varint */
#define FA2_STEP_VERIFY_END      22 /* verify no extra outer items */
#define FA2_STEP_EMIT_TO_ADDR    23 /* emit "Transfer tokens to" field */
#define FA2_STEP_EMIT_AMOUNT     24 /* emit "Token Amount" field */

/* Saved FA2 addresses: from_ in first half, to_ in second half of CAPTURE */
#define FA2_FROM_ADDR_OFS 0
#define FA2_TO_ADDR_OFS   (TZ_CAPTURE_BUFFER_SIZE / 2)
#define FA2_ADDR_MAX_LEN  (TZ_CAPTURE_BUFFER_SIZE / 2 - 1)

_Static_assert((TZ_CAPTURE_BUFFER_SIZE % 2U) == 0U,
               "TZ_CAPTURE_BUFFER_SIZE must be even for FA2 CAPTURE split");

/**
 * @brief Format an integer token amount string with token decimals and
 * symbol.
 *
 * @param str       Decimal ASCII digits buffer (in/out, NUL-terminated).
 * @param buf_size  Size of @p str (including the trailing NUL byte).
 * @param decimals  Number of fractional digits for the token.
 * @param symbol    Optional token symbol appended as " <symbol>" when it
 * fits.
 *
 * Formatting is done in three phases:
 * - left-pad with zeros when the integer has fewer than decimals+1 digits,
 * - insert a decimal separator and trim trailing fractional zeros,
 * - append the token symbol if enough space remains.
 *
 * The function never writes past @p buf_size. If the symbol would not fit, it
 * is skipped and only the formatted amount is kept.
 */
static void
tz_format_token_amount(char *str, size_t buf_size, uint8_t decimals,
                       const char *symbol)
{
    size_t len;

    if ((str == NULL) || (buf_size == 0)) {
        return;
    }

    len = strlen(str);
    if (len >= buf_size) {
        str[buf_size - 1] = 0;
        len               = buf_size - 1;
    }

    if (len == 0) {
        if (buf_size < 2) {
            str[0] = 0;
            return;
        }
        str[0] = '0';
        str[1] = 0;
        len    = 1;
    }

    if (decimals > 0) {
        size_t frac_digits = (size_t)decimals;

        /* Ensure at least decimals+1 integer digits by left-padding with '0'.
         */
        if (len <= frac_digits) {
            int pad = (int)(frac_digits + 1U - len);
            /* +1U: room for NUL terminator moved by the right-shift loop. */
            if ((len + (size_t)pad + 1U) > buf_size) {
                return;
            }
            for (int j = (int)len; j >= 0; j--) {
                str[j + pad] = str[j];
            }
            for (int j = 0; j < pad; j++) {
                str[j] = '0';
            }
            len += (size_t)pad;
        }

        /* Detect whether the whole fractional part is zero. */
        int no_decimals = 1;
        for (size_t i = 0; i < frac_digits; i++) {
            no_decimals &= (str[len - 1 - i] == '0');
        }
        if (no_decimals) {
            str[len - frac_digits] = 0;
            len -= frac_digits;
        } else {
            /* Insert '.', then trim trailing fractional zeros and a trailing
             * '.'. */
            if ((len + 1U) >= buf_size) {
                return;
            }
            for (size_t i = 0; i < frac_digits; i++) {
                str[len - i] = str[len - i - 1];
            }
            str[len - frac_digits] = '.';
            len++;
            str[len] = 0;
            while ((len > 0) && (str[len - 1] == '0')) {
                len--;
                str[len] = 0;
            }
            if ((len > 0) && (str[len - 1] == '.')) {
                len--;
                str[len] = 0;
            }
        }
    }

    if ((symbol != NULL) && symbol[0]) {
        size_t symbol_len = strlen(symbol);
        if ((len + 1U + symbol_len + 1U) <= buf_size) {
            strlcat(str, " ", buf_size);
            strlcat(str, symbol, buf_size);
        }
    }
}

/**
 * @brief Switch FA2 parser to binary fallback for remaining bytes
 *
 *        Called when the FA2 structure does not match the expected
 *        single-item token_id=0 pattern.  Remaining bytes are displayed
 *        as hex with the complex flag set.
 */
static tz_parser_result
fa2_fallback_to_binary(tz_parser_state *state)
{
    tz_operation_state *op = &state->operation;

    op->frame->step                       = TZ_OPERATION_STEP_READ_BINARY;
    op->frame->step_read_string.ofs       = 0;
    op->frame->step_read_string.skip      = 0;
    op->frame->step_read_string.check_fa2 = 0;
    state->field_info.is_field_complex    = true;
    STRLCPY(state->field_info.field_name, "Parameter");
    tz_continue;
}

/**
 * @brief Read and parse an FA2 transfer parameter for clear signing
 *
 *        Supports single-item transfers with token_id = 0.
 *        The innermost list(pair(token_id, amount)) may be Micheline-encoded
 *        either as SEQ[Pair(...)] or as a direct Pair (single-element list).
 *        Falls back to raw Micheline display for unsupported patterns.
 *
 *        See docs/FA2_CLEAR_SIGNING.md for scope, registry behavior, and
 *        fallback rules.
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_fa2_transfer(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_FA2_TRANSFER);
    tz_operation_state *op   = &state->operation;
    tz_parser_regs     *regs = &state->regs;
    uint8_t             b;

    switch (op->frame->step_read_fa2.sub_step) {
    /* ---- outer list ---- */
    case FA2_STEP_OUTER_SEQ_TAG:
        tz_must(tz_parser_read(state, &b));
        FA2_REQUIRE(state, b == 0x02);

        op->frame->step_read_fa2.sub_step = FA2_STEP_OUTER_SEQ_SIZE;
        op->frame->step_read_fa2.size_ofs = 0;
        op->frame->step_read_fa2.size_val = 0;
        tz_continue;

    case FA2_STEP_OUTER_SEQ_SIZE:
        tz_must(tz_parser_read(state, &b));
        op->frame->step_read_fa2.size_val
            = (op->frame->step_read_fa2.size_val << 8) | b;
        op->frame->step_read_fa2.size_ofs++;
        if (op->frame->step_read_fa2.size_ofs < 4) {
            tz_continue;
        }

        FA2_REQUIRE(state, op->frame->step_read_fa2.size_val != 0);

        op->frame->step_read_fa2.sub_step = FA2_STEP_OUTER_PAIR_TAG;
        tz_continue;

    case FA2_STEP_OUTER_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x07);

        op->frame->step_read_fa2.sub_step = FA2_STEP_OUTER_PAIR_OP;
        tz_continue;

    case FA2_STEP_OUTER_PAIR_OP:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x07);

        op->frame->step_read_fa2.sub_step = FA2_STEP_FROM_ADDR_TAG;
        tz_continue;

    /* ---- from_ address ---- */
    case FA2_STEP_FROM_ADDR_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x01 || b == 0x0A);

        op->frame->step_read_fa2.addr_tag = b;
        op->frame->step_read_fa2.sub_step = FA2_STEP_FROM_ADDR_SIZE;
        op->frame->step_read_fa2.size_ofs = 0;
        op->frame->step_read_fa2.size_val = 0;
        tz_continue;

    case FA2_STEP_FROM_ADDR_SIZE:
        tz_must(tz_parser_read(state, &b));
        op->frame->step_read_fa2.size_val
            = (op->frame->step_read_fa2.size_val << 8) | b;
        op->frame->step_read_fa2.size_ofs++;
        if (op->frame->step_read_fa2.size_ofs < 4) {
            tz_continue;
        }

        FA2_REQUIRE(state,
                    op->frame->step_read_fa2.size_val <= FA2_ADDR_MAX_LEN);

        op->frame->step_read_fa2.addr_len = op->frame->step_read_fa2.size_val;
        op->frame->step_read_fa2.addr_ofs = 0;
        op->frame->step_read_fa2.sub_step = FA2_STEP_FROM_ADDR_BYTES;
        tz_continue;

    case FA2_STEP_FROM_ADDR_BYTES:
        tz_must(tz_parser_read(state, &b));
        CAPTURE[FA2_FROM_ADDR_OFS + op->frame->step_read_fa2.addr_ofs] = b;
        op->frame->step_read_fa2.addr_ofs++;
        op->frame->step_read_fa2.addr_len--;
        if (op->frame->step_read_fa2.addr_len > 0) {
            tz_continue;
        }
        /* Address fully read; null-terminate for string case */
        CAPTURE[FA2_FROM_ADDR_OFS + op->frame->step_read_fa2.addr_ofs] = 0;
        if (op->frame->step_read_fa2.addr_tag == 0x0A) {
            /* Binary address: format it in-place */
            FA2_REQUIRE(state, tz_format_address(
                                   CAPTURE + FA2_FROM_ADDR_OFS,
                                   op->frame->step_read_fa2.addr_ofs,
                                   (char *)(CAPTURE + FA2_FROM_ADDR_OFS),
                                   FA2_ADDR_MAX_LEN)
                                   == 0);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_SEQ_TAG;
        tz_continue;

    /* ---- txs list ---- */
    case FA2_STEP_TXS_SEQ_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x02);

        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_SEQ_SIZE;
        op->frame->step_read_fa2.size_ofs = 0;
        op->frame->step_read_fa2.size_val = 0;
        tz_continue;

    case FA2_STEP_TXS_SEQ_SIZE:
        tz_must(tz_parser_read(state, &b));
        op->frame->step_read_fa2.size_val
            = (op->frame->step_read_fa2.size_val << 8) | b;
        op->frame->step_read_fa2.size_ofs++;
        if (op->frame->step_read_fa2.size_ofs < 4) {
            tz_continue;
        }

        FA2_REQUIRE(state, op->frame->step_read_fa2.size_val != 0);

        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_PAIR_TAG;
        tz_continue;

    case FA2_STEP_TXS_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x07);

        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_PAIR_OP;
        tz_continue;

    case FA2_STEP_TXS_PAIR_OP:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x07);

        op->frame->step_read_fa2.sub_step = FA2_STEP_TO_ADDR_TAG;
        tz_continue;

    /* ---- to_ address ---- */
    case FA2_STEP_TO_ADDR_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x01 || b == 0x0A);

        op->frame->step_read_fa2.addr_tag = b;
        op->frame->step_read_fa2.sub_step = FA2_STEP_TO_ADDR_SIZE;
        op->frame->step_read_fa2.size_ofs = 0;
        op->frame->step_read_fa2.size_val = 0;
        tz_continue;

    case FA2_STEP_TO_ADDR_SIZE:
        tz_must(tz_parser_read(state, &b));
        op->frame->step_read_fa2.size_val
            = (op->frame->step_read_fa2.size_val << 8) | b;
        op->frame->step_read_fa2.size_ofs++;
        if (op->frame->step_read_fa2.size_ofs < 4) {
            tz_continue;
        }

        FA2_REQUIRE(state,
                    op->frame->step_read_fa2.size_val <= FA2_ADDR_MAX_LEN);

        op->frame->step_read_fa2.addr_len = op->frame->step_read_fa2.size_val;
        op->frame->step_read_fa2.addr_ofs = 0;
        op->frame->step_read_fa2.sub_step = FA2_STEP_TO_ADDR_BYTES;
        tz_continue;

    case FA2_STEP_TO_ADDR_BYTES:
        tz_must(tz_parser_read(state, &b));
        CAPTURE[FA2_TO_ADDR_OFS + op->frame->step_read_fa2.addr_ofs] = b;
        op->frame->step_read_fa2.addr_ofs++;
        op->frame->step_read_fa2.addr_len--;
        if (op->frame->step_read_fa2.addr_len > 0) {
            tz_continue;
        }
        /* Address fully read; null-terminate for string case */
        CAPTURE[FA2_TO_ADDR_OFS + op->frame->step_read_fa2.addr_ofs] = 0;
        if (op->frame->step_read_fa2.addr_tag == 0x0A) {
            FA2_REQUIRE(state,
                        tz_format_address(CAPTURE + FA2_TO_ADDR_OFS,
                                          op->frame->step_read_fa2.addr_ofs,
                                          (char *)(CAPTURE + FA2_TO_ADDR_OFS),
                                          FA2_ADDR_MAX_LEN)
                            == 0);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_SEQ_TAG;
        tz_continue;

    /* ---- inner txs item (single pair: token_id + amount) ---- */
    case FA2_STEP_INNER_SEQ_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b == 0x02) { /* SEQ */
            op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_SEQ_SIZE;
            op->frame->step_read_fa2.size_ofs = 0;
            op->frame->step_read_fa2.size_val = 0;
        } else if (b == 0x07) {
            /* Direct Pair (no SEQ): single-element list encoding (e.g. Temple
             * Wallet) */
            op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_PAIR_OP;
        } else {
            return fa2_fallback_to_binary(state);
        }
        tz_continue;

    case FA2_STEP_INNER_SEQ_SIZE:
        tz_must(tz_parser_read(state, &b));
        op->frame->step_read_fa2.size_val
            = (op->frame->step_read_fa2.size_val << 8) | b;
        op->frame->step_read_fa2.size_ofs++;
        if (op->frame->step_read_fa2.size_ofs < 4) {
            tz_continue;
        }

        FA2_REQUIRE(state, op->frame->step_read_fa2.size_val != 0);

        op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_PAIR_TAG;
        tz_continue;

    case FA2_STEP_INNER_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x07);

        op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_PAIR_OP;
        tz_continue;

    case FA2_STEP_INNER_PAIR_OP:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x07);

        op->frame->step_read_fa2.sub_step = FA2_STEP_TOKEN_ID_TAG;
        tz_continue;

    /* ---- token_id (must be 0) ---- */
    case FA2_STEP_TOKEN_ID_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x00);

        op->frame->step_read_fa2.sub_step = FA2_STEP_TOKEN_ID_VAL;
        tz_continue;

    case FA2_STEP_TOKEN_ID_VAL: {
        /* Read a single Zarith byte; must be 0x00 (value 0, no more bytes) */
        tz_must(tz_parser_read(state, &b));

        /* Zarith: MSB=1 means more bytes follow; value bits are low 7 bits.
           For token_id=0, the only valid encoding is a single byte 0x00. */
        FA2_REQUIRE(state, b == 0x00);

        op->frame->step_read_fa2.sub_step = FA2_STEP_AMOUNT_TAG;
        tz_continue;
    }

    /* ---- amount ---- */
    case FA2_STEP_AMOUNT_TAG:
        tz_must(tz_parser_read(state, &b));

        FA2_REQUIRE(state, b == 0x00);

        /* Initialize num parser for amount */
        tz_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_fa2.num_state);
        op->frame->step_read_fa2.sub_step = FA2_STEP_AMOUNT_VAL;
        tz_continue;

    case FA2_STEP_AMOUNT_VAL: {
        tz_must(tz_parser_read(state, &b));
        /* Micheline int uses signed Zarith; natural=1 would double positive
         * values. */
        tz_must(tz_parse_int_step(&state->buffers.num,
                                  &op->frame->step_read_fa2.num_state, b));
        if (!op->frame->step_read_fa2.num_state.stop) {
            tz_continue;
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_VERIFY_END;
        tz_continue;
    }

    case FA2_STEP_VERIFY_END:
        /* Verify we are at the end of the parameter (no extra items) */
        FA2_REQUIRE(state, state->ofs == op->frame->stop);

        op->frame->step_read_fa2.sub_step = FA2_STEP_EMIT_TO_ADDR;
        tz_continue;

    case FA2_STEP_EMIT_TO_ADDR:
        /* Emit receiver address before token amount */
        if (regs->oofs > 0) {
            tz_stop(IM_FULL);
        }
        STRLCPY(state->field_info.field_name, "Transfer tokens to");
        state->field_info.is_field_complex = false;
        state->field_info.field_index++;
        op->frame->step_read_fa2.sub_step = FA2_STEP_EMIT_AMOUNT;
        tz_must(push_frame(state, TZ_OPERATION_STEP_PRINT));
        op->frame->step_print.str = (char *)(CAPTURE + FA2_TO_ADDR_OFS);
        tz_continue;

    case FA2_STEP_EMIT_AMOUNT:
        /* Emit "Token Amount" field: push a PRINT frame, then pop FA2 */
        if (regs->oofs > 0) {
            tz_stop(IM_FULL);
        }
        {
            const fa2_token_metadata_t *token
                = fa2_token_by_index(op->frame->step_read_fa2.token_idx);
            if (token != NULL) {
                tz_format_token_amount((char *)state->buffers.num.decimal,
                                       sizeof(state->buffers.num.decimal),
                                       token->decimals, token->symbol);
            }
        }
        STRLCPY(state->field_info.field_name, "Token Amount");
        state->field_info.is_field_complex = false;
        state->field_info.field_index++;
        /* Pop the FA2 frame first, then push PRINT so PRINT pops to parent */
        tz_must(pop_frame(state));
        tz_must(push_frame(state, TZ_OPERATION_STEP_PRINT));
        op->frame->step_print.str = (char *)state->buffers.num.decimal;
        tz_continue;

    default:
        tz_raise(INVALID_STATE);
    }
    tz_continue;
}

static tz_parser_result
sdp_fail_to_micheline(tz_parser_state *state)
{
    tz_operation_state *op   = &state->operation;
    tz_parser_regs     *regs = &state->regs;
    size_t consumed          = (size_t)(state->ofs - op->sdp_payload_start);

    regs->ilen += consumed;
    regs->iofs -= consumed;
    state->ofs                            = op->sdp_payload_start;
    state->field_info.is_field_complex    = true;
    op->frame->step                       = TZ_OPERATION_STEP_READ_MICHELINE;
    op->frame->step_read_micheline.inited = 0;
    op->frame->step_read_micheline.skip   = op->sdp_expr_skip;
    op->frame->step_read_micheline.name   = op->sdp_reparse_field_name;
    tz_micheline_parser_init(state);
    tz_continue;
}

#define SDP_STEP_SAVE_START     0
#define SDP_STEP_OUTER_PAIR_TAG 1
#define SDP_STEP_OUTER_PAIR_OP  2
#define SDP_STEP_FIRST_INT_TAG  3
#define SDP_STEP_FIRST_INT_READ 4
#define SDP_STEP_INNER_PAIR_TAG 5
#define SDP_STEP_INNER_PAIR_OP  6
#define SDP_STEP_EDGE_INT_TAG   7
#define SDP_STEP_EDGE_INT_READ  8
#define SDP_STEP_UNIT_PRIM0     9
#define SDP_STEP_UNIT_OP        10
#define SDP_STEP_EMIT_LIMIT     11
#define SDP_STEP_EMIT_EDGE      12
#define SDP_STEP_DONE           13

/**
 * @brief Read set_delegate_parameters Micheline (Pair int (Pair int Unit))
 */
static tz_parser_result
tz_step_read_set_delegate_params(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_SET_DELEGATE_PARAMS);
    tz_operation_state *op = &state->operation;
    uint8_t             b;

    switch (op->frame->step_read_sdp.sub_step) {
    case SDP_STEP_SAVE_START:
        op->sdp_payload_start             = state->ofs;
        op->frame->step_read_sdp.sub_step = SDP_STEP_OUTER_PAIR_TAG;
        tz_continue;
    case SDP_STEP_OUTER_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELINE_TAG_PRIM_2_NOANNOTS) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        op->frame->step_read_sdp.sub_step = SDP_STEP_OUTER_PAIR_OP;
        tz_continue;
    case SDP_STEP_OUTER_PAIR_OP:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELSON_OP_Pair) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        op->frame->step_read_sdp.sub_step = SDP_STEP_FIRST_INT_TAG;
        tz_continue;
    case SDP_STEP_FIRST_INT_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELINE_TAG_INT) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        tz_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_sdp.int_regs);
        op->frame->step_read_sdp.int_regs.stop = 0;
        op->frame->step_read_sdp.sub_step      = SDP_STEP_FIRST_INT_READ;
        tz_continue;
    case SDP_STEP_FIRST_INT_READ:
        tz_must(tz_parser_read(state, &b));
        tz_must(tz_parse_int_step(&state->buffers.num,
                                  &op->frame->step_read_sdp.int_regs, b));
        if (!op->frame->step_read_sdp.int_regs.stop) {
            tz_continue;
        }
        strlcpy(op->sdp_limit_decimal, state->buffers.num.decimal,
                sizeof(op->sdp_limit_decimal));
        op->frame->step_read_sdp.sub_step = SDP_STEP_INNER_PAIR_TAG;
        tz_continue;
    case SDP_STEP_INNER_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELINE_TAG_PRIM_2_NOANNOTS) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        op->frame->step_read_sdp.sub_step = SDP_STEP_INNER_PAIR_OP;
        tz_continue;
    case SDP_STEP_INNER_PAIR_OP:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELSON_OP_Pair) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        op->frame->step_read_sdp.sub_step = SDP_STEP_EDGE_INT_TAG;
        tz_continue;
    case SDP_STEP_EDGE_INT_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELINE_TAG_INT) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        tz_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_sdp.int_regs);
        op->frame->step_read_sdp.int_regs.stop = 0;
        op->frame->step_read_sdp.sub_step      = SDP_STEP_EDGE_INT_READ;
        tz_continue;
    case SDP_STEP_EDGE_INT_READ:
        tz_must(tz_parser_read(state, &b));
        tz_must(tz_parse_int_step(&state->buffers.num,
                                  &op->frame->step_read_sdp.int_regs, b));
        if (!op->frame->step_read_sdp.int_regs.stop) {
            tz_continue;
        }
        strlcpy(op->sdp_edge_decimal, state->buffers.num.decimal,
                sizeof(op->sdp_edge_decimal));
        op->frame->step_read_sdp.sub_step = SDP_STEP_UNIT_PRIM0;
        tz_continue;
    case SDP_STEP_UNIT_PRIM0:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELINE_TAG_PRIM_0_NOANNOTS) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        op->frame->step_read_sdp.sub_step = SDP_STEP_UNIT_OP;
        tz_continue;
    case SDP_STEP_UNIT_OP:
        tz_must(tz_parser_read(state, &b));
        if (b != (uint8_t)TZ_MICHELSON_OP_Unit) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        if (state->ofs != op->frame->stop) {
            tz_must(sdp_fail_to_micheline(state));
            tz_continue;
        }
        op->frame->step_read_sdp.sub_step = SDP_STEP_EMIT_LIMIT;
        tz_continue;
    case SDP_STEP_EMIT_LIMIT:
        STRLCPY(state->field_info.field_name, "Limit (stake/bake)");
        state->field_info.is_field_complex = false;
        strlcpy((char *)CAPTURE, op->sdp_limit_decimal, sizeof(CAPTURE));
        op->frame->step_read_sdp.sub_step = SDP_STEP_EMIT_EDGE;
        tz_must(push_frame(state, TZ_OPERATION_STEP_PRINT));
        op->frame->step_print.str = (char *)CAPTURE;
        tz_continue;
    case SDP_STEP_EMIT_EDGE:
        STRLCPY(state->field_info.field_name, "Edge (bake/stake)");
        state->field_info.is_field_complex = false;
        strlcpy((char *)CAPTURE, op->sdp_edge_decimal, sizeof(CAPTURE));
        op->frame->step_read_sdp.sub_step = SDP_STEP_DONE;
        tz_must(push_frame(state, TZ_OPERATION_STEP_PRINT));
        op->frame->step_print.str = (char *)CAPTURE;
        tz_continue;
    case SDP_STEP_DONE:
        tz_must(pop_frame(state));
        if (state->regs.oofs > 0) {
            tz_stop(IM_FULL);
        }
        tz_continue;
    default:
        tz_raise(INVALID_STATE);
    }
}

/**
 * @brief Read a micheline expression
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_micheline(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_MICHELINE);
    tz_operation_state *op   = &state->operation;
    tz_parser_regs     *regs = &state->regs;
    if (op->emit_finalize_note) {
        op->emit_finalize_note             = 0;
        state->field_info.is_field_complex = false;
        STRLCPY(state->field_info.field_name, "Note");
        strlcpy((char *)CAPTURE, "Fee payer is Source; staker is Destination",
                sizeof(CAPTURE));
        tz_must(push_frame(state, TZ_OPERATION_STEP_PRINT));
        op->frame->step_print.str = (char *)CAPTURE;
        tz_continue;
    }
    if (!op->frame->step_read_micheline.inited) {
        op->frame->step_read_micheline.inited = 1;
        STRLCPY(state->field_info.field_name,
                op->frame->step_read_micheline.name);
        tz_micheline_parser_init(state);
    }
    tz_micheline_parser_step(state);
    if (state->errno == TZ_BLO_DONE) {
        if (state->micheline.is_unit) {
            state->field_info.is_field_complex = false;
        }
        if ((op->frame->stop != 0) && (state->ofs != op->frame->stop)) {
            tz_raise(TOO_LARGE);
        }
        tz_must(pop_frame(state));
        if (regs->oofs > 0) {
            tz_stop(IM_FULL);
        } else {
            tz_continue;
        }
    }
    tz_reraise;
}

/**
 * @brief Format a string as an amount
 *
 * @param str: string to format
 */
static void
tz_format_amount(char *str)
{
    int len = 0;
    while (str[len]) {
        len++;
    }
    if ((len == 1) && (str[0] == 0)) {
        // just 0
        goto add_currency;
    }
    if (len < 7) {
        // less than one tez, pad left up to the '0.'
        int j;
        int pad = 7 - len;
        for (j = len; j >= 0; j--) {
            str[j + pad] = str[j];
        }
        for (j = 0; j < pad; j++) {
            str[j] = '0';
        }
        len = 7;
    }
    int no_decimals = 1;
    for (int i = 0; i < 6; i++) {
        no_decimals &= (str[len - 1 - i] == '0');
    }
    if (no_decimals) {
        // integral value, don't include the decimal part (no '.'_
        str[len - 6] = 0;
        len -= 6;
    } else {
        // more than one tez, add the '.'
        for (int i = 0; i < 6; i++) {
            str[len - i] = str[len - i - 1];
        }
        str[len - 6] = '.';
        len++;
        str[len] = 0;
        // drop trailing non significant zeroes
        while (str[len - 1] == '0') {
            len--;
            str[len] = 0;
        }
    }
add_currency:
    str[len]     = ' ';
    str[len + 1] = 'X';
    str[len + 2] = 'T';
    str[len + 3] = 'Z';
    len += 4;
    str[len] = 0;
}

/**
 * @brief Read a number
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_num(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_NUM);
    tz_operation_state *op = &state->operation;
    uint8_t             b;
    tz_must(tz_parser_read(state, &b));
    tz_must(tz_parse_num_step(&state->buffers.num,
                              &op->frame->step_read_num.state, b,
                              op->frame->step_read_num.natural));
    if (op->frame->step_read_num.state.stop) {
        uint64_t value;
        if (!tz_string_to_mutez(state->buffers.num.decimal, &value)) {
            tz_raise(INVALID_DATA);
        }
        switch (op->frame->step_read_num.kind) {
        case TZ_OPERATION_FIELD_AMOUNT:
            op->total_amount += value;
            break;
        case TZ_OPERATION_FIELD_FEE:
            op->total_fee += value;
            break;
        default:
            break;
        }
        if (op->frame->step_read_num.skip) {
            tz_must(pop_frame(state));
            tz_continue;
        }
        char *str       = state->buffers.num.decimal;
        op->frame->step = TZ_OPERATION_STEP_PRINT;
        switch (op->frame->step_read_num.kind) {
        case TZ_OPERATION_FIELD_INT:
        case TZ_OPERATION_FIELD_NAT:
            break;
        case TZ_OPERATION_FIELD_FEE:
        case TZ_OPERATION_FIELD_AMOUNT: {
            tz_format_amount(str);
            break;
        }
        default:
            tz_raise(INVALID_STATE);
        }
        op->frame->step_print.str = str;
    }
    tz_continue;
}

/**
 * @brief Read an int32
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_int32(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_INT32);
    tz_operation_state *op = &state->operation;
    uint8_t             b;
    int32_t            *value = &op->frame->step_read_int32.value;
    if (op->frame->step_read_int32.ofs < 4) {
        tz_must(tz_parser_read(state, &b));
        *value = (*value << 8) | b;
        op->frame->step_read_int32.ofs++;
    } else {
        snprintf((char *)CAPTURE, sizeof(CAPTURE), "%d", *value);
        op->frame->step_read_string.skip = op->frame->step_read_int32.skip;
        tz_must(tz_print_string(state));
    }
    tz_continue;
}

/**
 * @brief Read bytes
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_bytes(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_BYTES);
    tz_operation_state *op = &state->operation;
    if (op->frame->step_read_bytes.ofs < op->frame->step_read_bytes.len) {
        uint8_t *c;
        c = &CAPTURE[op->frame->step_read_bytes.ofs];
        tz_must(tz_parser_read(state, c));
        op->frame->step_read_bytes.ofs++;
    } else {
        if (op->frame->step_read_bytes.skip) {
            tz_must(pop_frame(state));
            tz_continue;
        }
        switch (op->frame->step_read_bytes.kind) {
        case TZ_OPERATION_FIELD_SOURCE:
            memcpy(op->source, CAPTURE, 22);
            __attribute__((fallthrough));
        case TZ_OPERATION_FIELD_PKH:
            if (tz_format_pkh(CAPTURE, 21, (char *)CAPTURE,
                              sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_PK:
            if (tz_format_pk(CAPTURE, op->frame->step_read_bytes.len,
                             (char *)CAPTURE, sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_BLS_SIG:
            if (tz_format_sig(CAPTURE, op->frame->step_read_bytes.len,
                              (char *)CAPTURE, sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_SR:
            if (tz_format_base58check("sr1", CAPTURE, 20, (char *)CAPTURE,
                                      sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_SRC:
            if (tz_format_base58check("src1", CAPTURE, 32, (char *)CAPTURE,
                                      sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_PROTO:
            if (tz_format_base58check("proto", CAPTURE, 32, (char *)CAPTURE,
                                      sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_DESTINATION:
            memcpy(op->destination, CAPTURE, 22);
            if (fa2_find_token(op->destination)) {
                tz_must(pop_frame(state));
                tz_continue;
            }
            if (tz_format_address(CAPTURE, 22, (char *)CAPTURE,
                                  sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_OPH:
            if (tz_format_oph(CAPTURE, 32, (char *)CAPTURE,
                              sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        case TZ_OPERATION_FIELD_BH:
            if (tz_format_bh(CAPTURE, 32, (char *)CAPTURE, sizeof(CAPTURE))) {
                tz_raise(INVALID_TAG);
            }
            break;
        default:
            tz_raise(INVALID_STATE);
        }
        op->frame->step           = TZ_OPERATION_STEP_PRINT;
        op->frame->step_print.str = (char *)CAPTURE;
    }
    tz_continue;
}

/**
 * @brief Plan the steps to read a batch of operations
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_branch(tz_parser_state *state)
{
    ASSERT_STEP(state, BRANCH);
    tz_operation_state *op = &state->operation;
    op->frame->step        = TZ_OPERATION_STEP_BATCH;
    tz_must(push_frame(state, TZ_OPERATION_STEP_TAG));
    tz_continue;
}

/**
 * @brief Ask to read remaining operations of a batch of operations
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_batch(tz_parser_state *state)
{
    ASSERT_STEP(state, BATCH);
    tz_operation_state *op = &state->operation;
    op->batch_index++;
    if (state->ofs == op->frame->stop) {
        tz_must(pop_frame(state));
    } else if (state->ofs > op->frame->stop) {
        tz_raise(TOO_LARGE);
    } else {
        tz_must(push_frame(state, TZ_OPERATION_STEP_TAG));
    }
    tz_continue;
}

/**
 * @brief Read a string
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_string(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_STRING);
    tz_operation_state *op = &state->operation;
    if (state->ofs == op->frame->stop) {
        CAPTURE[op->frame->step_read_string.ofs] = 0;
        if (op->frame->step_read_string.check_fa2) {
            if (strcmp((char *)CAPTURE, "transfer") == 0) {
                op->is_fa2_candidate = 1;
            }
        }
        tz_must(tz_print_string(state));
    } else {
        uint8_t b;
        if (op->frame->step_read_string.ofs >= TZ_CAPTURE_BUFFER_SIZE - 1) {
            tz_raise(TOO_LARGE);
        }
        tz_must(tz_parser_read(state, &b));
        CAPTURE[op->frame->step_read_string.ofs] = b;
        op->frame->step_read_string.ofs++;
    }
    tz_continue;
}

/**
 * @brief Read a binary
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_binary(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_BINARY);
    tz_operation_state *op = &state->operation;
    if (state->ofs == op->frame->stop) {
        CAPTURE[op->frame->step_read_string.ofs] = 0;
        tz_must(tz_print_string(state));
    } else if ((op->frame->step_read_string.ofs + 2)
               >= TZ_CAPTURE_BUFFER_SIZE) {
        CAPTURE[op->frame->step_read_string.ofs] = 0;
        op->frame->step_read_string.ofs          = 0;
        if (!op->frame->step_read_string.skip) {
            tz_must(push_frame(state, TZ_OPERATION_STEP_PARTIAL_PRINT));
            op->frame->step_print.str = (char *)CAPTURE;
        }
    } else {
        uint8_t b;
        tz_must(tz_parser_read(state, &b));
        char *buf = (char *)CAPTURE + op->frame->step_read_string.ofs;
        snprintf(buf, 4, "%02x", b);
        op->frame->step_read_string.ofs += 2;
    }
    tz_continue;
}

/**
 * @brief Emit the ASCII entrypoint name after reading a builtin index byte
 */
static tz_parser_result
emit_builtin_entrypoint_name(tz_parser_state *state, uint8_t b)
{
    const char *ep = NULL;

    switch (b) {
    case 0:
        ep = "default";
        break;
    case 1:
        ep = "root";
        break;
    case 2:
        ep = "do";
        break;
    case 3:
        ep = "set_delegate";
        break;
    case 4:
        ep = "remove_delegate";
        break;
    case 5:
        ep = "deposit";
        break;
    case 6:
        ep = "stake";
        break;
    case 7:
        ep = "unstake";
        break;
    case 8:
        ep = "finalize_unstake";
        if (tz_implicit_fee_payer_differs_from_dest(&state->operation)) {
            state->operation.emit_finalize_note = 1;
        }
        break;
    case 9:
        ep = "set_delegate_parameters";
        break;
    default:
        tz_raise(INVALID_TAG);
    }
    strlcpy((char *)CAPTURE, ep, sizeof(CAPTURE));
    tz_manager_entrypoint_set(&state->operation, ep);
    tz_must(tz_print_string(state));
    tz_continue;
}

/**
 * @brief Read a smart entrypoint
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_smart_entrypoint(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_SMART_ENTRYPOINT);
    tz_operation_state *op = &state->operation;
    uint8_t             b;

    tz_must(tz_parser_read(state, &b));

    if (b == 0xFF) {
        op->frame->step                 = TZ_OPERATION_STEP_READ_STRING;
        op->frame->step_read_string.ofs = 0;
        op->frame->step_read_string.check_fa2
            = (op->destination[0] == 1) ? 1 : 0;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 1;
        tz_continue;
    }

    tz_must(emit_builtin_entrypoint_name(state, b));
    tz_continue;
}

/**
 * @brief Plan the steps required to read the current operation field
 *
 *        Update the current field info only if the field is not
 *        ignored
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_field(tz_parser_state *state)
{
    ASSERT_STEP(state, FIELD);
    tz_operation_state                  *op    = &state->operation;
    const tz_operation_field_descriptor *field = op->frame->step_field.field;
    const char                          *name  = PIC(field->name);

    // is_field_complex is reset after reaching TZ_OPERATION_FIELD_END
    if (!field->skip) {
        STRLCPY(state->field_info.field_name, name);
        state->field_info.is_field_complex = field->complex;
        state->field_info.field_index++;
    }

    switch (field->kind) {
    case TZ_OPERATION_FIELD_OPTION: {
        op->frame->step              = TZ_OPERATION_STEP_OPTION;
        op->frame->step_option.field = PIC(field->field_option.field);
        op->frame->step_option.display_none
            = field->field_option.display_none;
        break;
    }
    case TZ_OPERATION_FIELD_TUPLE: {
        op->frame->step                   = TZ_OPERATION_STEP_TUPLE;
        op->frame->step_tuple.fields      = field->field_tuple.fields;
        op->frame->step_tuple.field_index = 0;
        break;
    }
    case TZ_OPERATION_FIELD_BINARY: {
        op->frame->step                       = TZ_OPERATION_STEP_READ_BINARY;
        op->frame->step_read_string.ofs       = 0;
        op->frame->step_read_string.skip      = field->skip;
        op->frame->step_read_string.check_fa2 = 0;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_SOURCE:
    case TZ_OPERATION_FIELD_PKH: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 21;
        break;
    }
    case TZ_OPERATION_FIELD_PK: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_PK;
        op->frame->step_read_bytes.skip = field->skip;
        break;
    }
    case TZ_OPERATION_FIELD_BLS_SIG: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_BLS_SIG;
        op->frame->step_read_bytes.skip = field->skip;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_SR: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 20;
        break;
    }
    case TZ_OPERATION_FIELD_SRC: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
        break;
    }
    case TZ_OPERATION_FIELD_PROTO: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
        break;
    }
    case TZ_OPERATION_FIELD_PROTOS: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_PROTOS;
        op->frame->step_read_list.name  = name;
        op->frame->step_read_list.index = 0;
        op->frame->step_read_list.skip  = field->skip;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_DESTINATION: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 22;
        break;
    }
    case TZ_OPERATION_FIELD_NAT:
    case TZ_OPERATION_FIELD_FEE:
    case TZ_OPERATION_FIELD_AMOUNT: {
        op->frame->step = TZ_OPERATION_STEP_READ_NUM;
        tz_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_num.state);
        op->frame->step_read_num.kind    = field->kind;
        op->frame->step_read_num.skip    = field->skip;
        op->frame->step_read_num.natural = 1;
        break;
    }
    case TZ_OPERATION_FIELD_INT: {
        op->frame->step = TZ_OPERATION_STEP_READ_NUM;
        tz_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_num.state);
        op->frame->step_read_num.kind    = field->kind;
        op->frame->step_read_num.skip    = field->skip;
        op->frame->step_read_num.natural = 0;
        break;
    }
    case TZ_OPERATION_FIELD_INT32: {
        op->frame->step                  = TZ_OPERATION_STEP_READ_INT32;
        op->frame->step_read_int32.value = 0;
        op->frame->step_read_int32.ofs   = 0;
        op->frame->step_read_int32.skip  = field->skip;
        break;
    }
    case TZ_OPERATION_FIELD_SMART_ENTRYPOINT: {
        op->frame->step = TZ_OPERATION_STEP_READ_SMART_ENTRYPOINT;
        op->frame->step_read_string.ofs       = 0;
        op->frame->step_read_string.skip      = field->skip;
        op->frame->step_read_string.check_fa2 = 0;
        break;
    }
    case TZ_OPERATION_FIELD_EXPR: {
        if (op->is_fa2_candidate && !field->skip) {
            const fa2_token_metadata_t *token;
            state->field_info.is_field_complex = false;
            op->frame->step = TZ_OPERATION_STEP_READ_FA2_TRANSFER;
            op->frame->step_read_fa2.sub_step  = FA2_STEP_OUTER_SEQ_TAG;
            op->frame->step_read_fa2.addr_ofs  = 0;
            op->frame->step_read_fa2.size_ofs  = 0;
            op->frame->step_read_fa2.size_val  = 0;
            op->frame->step_read_fa2.addr_len  = 0;
            op->frame->step_read_fa2.token_idx = -1;
            token = fa2_find_token(op->destination);
            if (token != NULL) {
                op->frame->step_read_fa2.token_idx = fa2_token_index(token);
            }
        } else if ((strcmp(op->manager_entrypoint, "set_delegate_parameters")
                    == 0)
                   && !field->skip) {
            op->frame->step = TZ_OPERATION_STEP_READ_SET_DELEGATE_PARAMS;
            op->frame->step_read_sdp.sub_step = SDP_STEP_SAVE_START;
            op->sdp_expr_skip                 = field->skip;
            STRLCPY(op->sdp_reparse_field_name, name);
            state->field_info.is_field_complex = false;
        } else {
            op->frame->step = TZ_OPERATION_STEP_READ_MICHELINE;
            op->frame->step_read_micheline.inited = 0;
            op->frame->step_read_micheline.skip   = field->skip;
            op->frame->step_read_micheline.name   = name;
        }
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_STRING: {
        op->frame->step                       = TZ_OPERATION_STEP_READ_STRING;
        op->frame->step_read_string.ofs       = 0;
        op->frame->step_read_string.skip      = field->skip;
        op->frame->step_read_string.check_fa2 = 0;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_SORU_MESSAGES: {
        op->frame->step                = TZ_OPERATION_STEP_READ_SORU_MESSAGES;
        op->frame->step_read_list.name = name;
        op->frame->step_read_list.index = 0;
        op->frame->step_read_list.skip  = field->skip;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_SORU_KIND: {
        op->frame->step                  = TZ_OPERATION_STEP_READ_SORU_KIND;
        op->frame->step_read_string.skip = field->skip;
        break;
    }
    case TZ_OPERATION_FIELD_PKH_LIST: {
        op->frame->step                 = TZ_OPERATION_STEP_READ_PKH_LIST;
        op->frame->step_read_list.name  = name;
        op->frame->step_read_list.index = 0;
        op->frame->step_read_list.skip  = field->skip;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case TZ_OPERATION_FIELD_BALLOT: {
        op->frame->step                  = TZ_OPERATION_STEP_READ_BALLOT;
        op->frame->step_read_string.skip = field->skip;
        break;
    }
    default:
        tz_raise(INVALID_STATE);
    }
    tz_continue;
}

/**
 * @brief Read a public key
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_pk(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_PK);
    tz_operation_state *op = &state->operation;
    uint8_t             b;
    tz_must(tz_parser_peek(state, &b));
    op->frame->step_read_bytes.kind = TZ_OPERATION_FIELD_PK;
    op->frame->step_read_bytes.ofs  = 0;
    switch (b) {
    case 0:  // edpk
        op->frame->step_read_bytes.len = 33;
        break;
    case 1:  // sppk
        op->frame->step_read_bytes.len = 34;
        break;
    case 2:  // p2pk
        op->frame->step_read_bytes.len = 34;
        break;
    case 3:  // BLpk
        op->frame->step_read_bytes.len = 49;
        break;
    default:
        tz_raise(INVALID_TAG);
    }
    op->frame->step = TZ_OPERATION_STEP_READ_BYTES;
    tz_continue;
}

/**
 * @brief Read a signature
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_bls_sig(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_BLS_SIG);
    tz_operation_state *op          = &state->operation;
    op->frame->step                 = TZ_OPERATION_STEP_READ_BYTES;
    op->frame->step_read_bytes.kind = TZ_OPERATION_FIELD_BLS_SIG;
    op->frame->step_read_bytes.ofs  = 0;
    // size previously computed by `tz_step_size`
    op->frame->step_read_bytes.len = op->frame->stop - state->ofs;
    if (op->frame->step_read_bytes.len != 96) {  // Must be a BLS signature
        tz_raise(INVALID_DATA);
    }
    tz_continue;
}

/**
 * @brief Read a list of public key hash
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_pkh_list(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_PKH_LIST);
    tz_operation_state *op    = &state->operation;
    tz_parser_regs     *regs  = &state->regs;
    uint8_t             skip  = op->frame->step_read_list.skip;
    const char         *name  = op->frame->step_read_list.name;
    uint16_t            index = op->frame->step_read_list.index;

    // Remaining content from previous public key hash - display this first.
    if (regs->oofs > 0) {
        tz_stop(IM_FULL);
    }

    if (op->frame->stop == state->ofs) {
        tz_must(pop_frame(state));
    } else {
        op->frame->step_read_list.index++;
        tz_must(push_frame(state, TZ_OPERATION_STEP_READ_BYTES));
        snprintf(state->field_info.field_name, TZ_FIELD_NAME_SIZE, "%s (%d)",
                 name, index);
        op->frame->step_read_bytes.kind = TZ_OPERATION_FIELD_PKH;
        op->frame->step_read_bytes.skip = skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 21;
    }
    tz_continue;
}

/**
 * @brief Read soru messages
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_soru_messages(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_SORU_MESSAGES);
    tz_operation_state *op    = &state->operation;
    tz_parser_regs     *regs  = &state->regs;
    uint8_t             skip  = op->frame->step_read_list.skip;
    const char         *name  = op->frame->step_read_list.name;
    uint16_t            index = op->frame->step_read_list.index;

    // Remaining content from previous message - display this first.
    if (regs->oofs > 0) {
        tz_stop(IM_FULL);
    }

    if (op->frame->stop == state->ofs) {
        tz_must(pop_frame(state));
    } else {
        op->frame->step_read_list.index++;
        tz_must(push_frame(state, TZ_OPERATION_STEP_READ_BINARY));
        snprintf(state->field_info.field_name, TZ_FIELD_NAME_SIZE, "%s (%d)",
                 name, index);
        op->frame->step_read_string.ofs  = 0;
        op->frame->step_read_string.skip = skip;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
    }
    tz_continue;
}

/**
 * @brief Read a soru kind
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_soru_kind(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_SORU_KIND);
    uint8_t b;
    tz_must(tz_parser_read(state, &b));
    switch (b) {
    case 0:
        strlcpy((char *)CAPTURE, "arith", sizeof(CAPTURE));
        break;
    case 1:
        strlcpy((char *)CAPTURE, "wasm_2_0_0", sizeof(CAPTURE));
        break;
    case 2:  /// Present in encoding, not activated in Oxford
        strlcpy((char *)CAPTURE, "riscv", sizeof(CAPTURE));
        break;
    default:
        tz_raise(INVALID_TAG);
    }
    tz_must(tz_print_string(state));
    tz_continue;
}

/**
 * @brief Read a ballot
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_ballot(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_BALLOT);
    uint8_t b;
    tz_must(tz_parser_read(state, &b));
    switch (b) {
    case 0:
        strlcpy((char *)CAPTURE, "yay", sizeof(CAPTURE));
        break;
    case 1:
        strlcpy((char *)CAPTURE, "nay", sizeof(CAPTURE));
        break;
    case 2:
        strlcpy((char *)CAPTURE, "pass", sizeof(CAPTURE));
        break;
    default:
        tz_raise(INVALID_TAG);
    }
    tz_must(tz_print_string(state));
    tz_continue;
}

/**
 * @brief Read a protocol list
 *
 * @param state: parser state
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_read_protos(tz_parser_state *state)
{
    ASSERT_STEP(state, READ_PROTOS);
    tz_operation_state *op    = &state->operation;
    tz_parser_regs     *regs  = &state->regs;
    uint8_t             skip  = op->frame->step_read_list.skip;
    const char         *name  = op->frame->step_read_list.name;
    uint16_t            index = op->frame->step_read_list.index;

    // Remaining content from previous proto - display this first.
    if (regs->oofs > 0) {
        tz_stop(IM_FULL);
    }

    if (op->frame->stop == state->ofs) {
        tz_must(pop_frame(state));
    } else {
        op->frame->step_read_list.index++;
        tz_must(push_frame(state, TZ_OPERATION_STEP_READ_BYTES));
        snprintf(state->field_info.field_name, 30, "%s (%d)", name, index);
        op->frame->step_read_bytes.kind = TZ_OPERATION_FIELD_PROTO;
        op->frame->step_read_bytes.skip = skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
    }
    tz_continue;
}

/**
 * @brief Print a string
 *
 * @param state: parser state
 * @param partial: If partial true, then the string is not yet
 *                 complete
 * @return tz_parser_result: parser result
 */
static tz_parser_result
tz_step_print(tz_parser_state *state, bool partial)
{
    if ((state->operation.frame->step != TZ_OPERATION_STEP_PRINT)
        && (state->operation.frame->step
            != TZ_OPERATION_STEP_PARTIAL_PRINT)) {
        PRINTF("[DEBUG] expected step %s or step %s but got step %s)\n",
               STRING_STEP(TZ_OPERATION_STEP_PRINT),
               STRING_STEP(TZ_OPERATION_STEP_PARTIAL_PRINT),
               STRING_STEP(state->operation.frame->step));
        tz_raise(INVALID_STATE);
    }
    tz_operation_state *op  = &state->operation;
    const char         *str = PIC(op->frame->step_print.str);
    if (*str) {
        tz_must(tz_parser_put(state, *str));
        op->frame->step_print.str++;
    } else {
        tz_must(pop_frame(state));
        if (!partial) {
            tz_stop(IM_FULL);
        }
    }
    tz_continue;
}

tz_parser_result
tz_operation_parser_step(tz_parser_state *state)
{
    tz_operation_state *op = &state->operation;

    // cannot restart after error
    if (TZ_IS_ERR(state->errno)) {
        tz_reraise;
    }

    // nothing else to do
    if (op->frame == NULL) {
        tz_stop(DONE);
    }

    PRINTF(
        "[DEBUG] operation(frame: %d, offset:%d/%d, ilen: %d, olen: %d, "
        "step: %s, errno: %s)\n",
        (int)(op->frame - op->stack), (int)state->ofs, (int)op->stack[0].stop,
        (int)state->regs.ilen, (int)state->regs.oofs,
        STRING_STEP(op->frame->step), tz_parser_result_name(state->errno));

    switch (op->frame->step) {
    case TZ_OPERATION_STEP_OPTION:
        tz_must(tz_step_option(state));
        break;
    case TZ_OPERATION_STEP_TUPLE:
        tz_must(tz_step_tuple(state));
        break;
    case TZ_OPERATION_STEP_MAGIC:
        tz_must(tz_step_magic(state));
        break;
    case TZ_OPERATION_STEP_SIZE:
        tz_must(tz_step_size(state));
        break;
    case TZ_OPERATION_STEP_TAG:
        tz_must(tz_step_tag(state));
        break;
    case TZ_OPERATION_STEP_READ_MICHELINE:
        tz_must(tz_step_read_micheline(state));
        break;
    case TZ_OPERATION_STEP_READ_NUM:
        tz_must(tz_step_read_num(state));
        break;
    case TZ_OPERATION_STEP_READ_INT32:
        tz_must(tz_step_read_int32(state));
        break;
    case TZ_OPERATION_STEP_READ_BYTES:
        tz_must(tz_step_read_bytes(state));
        break;
    case TZ_OPERATION_STEP_BRANCH:
        tz_must(tz_step_branch(state));
        break;
    case TZ_OPERATION_STEP_BATCH:
        tz_must(tz_step_batch(state));
        break;
    case TZ_OPERATION_STEP_READ_STRING:
        tz_must(tz_step_read_string(state));
        break;
    case TZ_OPERATION_STEP_READ_BINARY:
        tz_must(tz_step_read_binary(state));
        break;
    case TZ_OPERATION_STEP_READ_SMART_ENTRYPOINT:
        tz_must(tz_step_read_smart_entrypoint(state));
        break;
    case TZ_OPERATION_STEP_FIELD:
        tz_must(tz_step_field(state));
        break;
    case TZ_OPERATION_STEP_READ_PK:
        tz_must(tz_step_read_pk(state));
        break;
    case TZ_OPERATION_STEP_READ_BLS_SIG:
        tz_must(tz_step_read_bls_sig(state));
        break;
    case TZ_OPERATION_STEP_READ_SORU_MESSAGES:
        tz_must(tz_step_read_soru_messages(state));
        break;
    case TZ_OPERATION_STEP_READ_SORU_KIND:
        tz_must(tz_step_read_soru_kind(state));
        break;
    case TZ_OPERATION_STEP_READ_BALLOT:
        tz_must(tz_step_read_ballot(state));
        break;
    case TZ_OPERATION_STEP_READ_PROTOS:
        tz_must(tz_step_read_protos(state));
        break;
    case TZ_OPERATION_STEP_READ_PKH_LIST:
        tz_must(tz_step_read_pkh_list(state));
        break;
    case TZ_OPERATION_STEP_READ_FA2_TRANSFER:
        tz_must(tz_step_read_fa2_transfer(state));
        break;
    case TZ_OPERATION_STEP_READ_SET_DELEGATE_PARAMS:
        tz_must(tz_step_read_set_delegate_params(state));
        break;
    case TZ_OPERATION_STEP_PRINT:
    case TZ_OPERATION_STEP_PARTIAL_PRINT:
        tz_must(tz_step_print(
            state, op->frame->step == TZ_OPERATION_STEP_PARTIAL_PRINT));
        break;
    default:
        tz_raise(INVALID_STATE);
    }
    tz_continue;
}
