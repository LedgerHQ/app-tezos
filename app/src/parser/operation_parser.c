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
#include "micheline_parser.h"
#include "num_parser.h"

/* Prototypes */

static tz_parser_result push_frame(tz_parser_state              *state,
                                   tz_operation_parser_step_kind step);
static tz_parser_result pop_frame(tz_parser_state *state);

typedef struct {
    const char    name[32];
    const char    symbol[12];
    uint8_t       decimals;
    const uint8_t contract_hash[20];
} fa2_token_metadata_t;

static const fa2_token_metadata_t FA2_TOKEN_REGISTRY[] = {
    /* KT1VaEsVNiBoA56eToEK6n6BcPgh1tdx9eXi */
    {"Temple Key",            "TKEY",     18, {0xE6, 0x41, 0x04, 0xE1, 0x3F, 0x4C, 0x13,
                                0x54, 0xF6, 0x78, 0x15, 0xAA, 0x19, 0xE8,
                                0xC8, 0x28, 0x3C, 0x28, 0x1F, 0x94}    },
    /* KT1M81KrJr6TxYLkZkVqcpSTNKGoya8XytWT */
    {"Ecoin Network",         "ECN",      8,  {0x89, 0x89, 0xE3, 0xFC, 0x67, 0x9C, 0x90,
                                 0xA7, 0x94, 0xED, 0xD6, 0x53, 0x70, 0xC3,
                                 0x6C, 0xD9, 0x09, 0xF2, 0x35, 0x70}    },
    /* KT193D4vozYnhGJQVtw7CoxxqphqUEEwK6Vb */
    {"Quipuswap Governance",  "QUIPU",    6,  {0x05, 0x00, 0x1A, 0x8A, 0xF8,
                                          0x13, 0x09, 0x4E, 0xE8, 0xBF,
                                          0x16, 0x2A, 0xC2, 0x09, 0x3A,
                                          0x89, 0x37, 0xE8, 0xBA, 0x83} },
    /* KT1A5P4ejnLix13jtadsfV9GCnXLMNnab8UT */
    {"Kalamint",              "KALAM",    10, {0x10, 0x61, 0x67, 0xAC, 0xE0, 0x99, 0xE2,
                               0x33, 0xD3, 0x82, 0xFC, 0x51, 0x36, 0xDE,
                               0x4A, 0x7F, 0xFE, 0xCA, 0x3F, 0x11}     },
    /* KT1LcNFDj66TTXKFBjQLQwo9J7tENbrq8AJE */
    {"TZ Apex",               "APEX",     1,  {0x83, 0xEE, 0xDF, 0xFF, 0x1D, 0xC4, 0xF6,
                            0xC3, 0xA0, 0x28, 0x10, 0x3B, 0x04, 0x7B,
                            0x29, 0x5D, 0xC1, 0x4D, 0xCD, 0x57}         },
    /* KT1ShjB24tTXX8m5oDCrBHXqynCpmNJDxpD5 */
    {"TRASH",                 "TRASH",    8,  {0xC6, 0xC3, 0x2D, 0xCF, 0xD9, 0x93, 0x07,
                           0x18, 0xDD, 0xD5, 0x26, 0xDC, 0xAE, 0x2B,
                           0x05, 0x59, 0xE7, 0x02, 0xB5, 0xD1}          },
    /* KT1C9X9s5rpVJGxwVuHEVBLYEdAQ1Qw8QDjH */
    {"TezDAO",                "TezDAO",   6,  {0x27, 0x1A, 0x12, 0xCF, 0xF3, 0x75, 0x51,
                             0x9B, 0x1D, 0xC2, 0x41, 0x47, 0x59, 0x3F,
                             0x65, 0xD7, 0xEF, 0xA1, 0xA1, 0x57}        },
    /* KT1Hc6KSHp3pLLiCXUkuHva62PRJQ8JmyAcP */
    {"mew",                   "mew",      6,  {0x62, 0xF9, 0x33, 0xC3, 0x0D, 0xE8, 0x27,
                       0x14, 0x25, 0x12, 0xF7, 0xD5, 0xA6, 0x92,
                       0xFD, 0x61, 0x8A, 0x6F, 0xCA, 0xBB}              },
    /* KT1R4KPQxpFHAkX8MKCFmdoiqTaNSSpnJXPL */
    {"Tezos Domains Vote",    "TEDv",     6,  {0xB4, 0xB7, 0xCE, 0x89, 0x3D,
                                       0x4B, 0xCD, 0xFD, 0xC2, 0xC6,
                                       0x31, 0x3D, 0xDA, 0xF4, 0xC9,
                                       0xE7, 0xF1, 0x25, 0x87, 0x9D}    },
    /* KT1AFA2mwNUMNd4SsujE1YYp29vd8BZejyKW */
    {"Hic et nunc DAO",       "hDAO",     6,  {0x12, 0x3A, 0xAF, 0x8D, 0x2C, 0x25, 0xE0,
                                    0xDD, 0xAF, 0xA6, 0xD9, 0x8E, 0x16, 0x58,
                                    0x2D, 0xE7, 0x47, 0x8F, 0x09, 0x25} },
    /* KT1XnTn74bUtxHfDtBmm2bGZAQfhPbvKWR8o */
    {"Tether USD",            "USDt",     6,  {0xFE, 0x81, 0x09, 0x59, 0xC3, 0xD6, 0x12,
                               0x7A, 0x41, 0xCB, 0xD4, 0x71, 0xE7, 0xCB,
                               0x4E, 0x91, 0xA6, 0x1B, 0x78, 0x0B}      },
    /* KT18opRzxCFTyhU6iw2g3DUfBy82GRYXqHjH */
    {"OTTEZ",                 "OTTEZ",    8,  {0x02, 0x77, 0xCC, 0xCD, 0xBB, 0x9F, 0x82,
                           0x7E, 0xED, 0x2D, 0x14, 0x93, 0xD9, 0x15,
                           0x56, 0xB6, 0x5B, 0xB1, 0x71, 0xE5}          },
    /* KT1BCzAq3PrTKPsEBKuoTGZdwX6rN6WE8rJj */
    {"Cerveza",               "CVZA",     8,  {0x1C, 0xC9, 0xBF, 0x78, 0xD8, 0x1A, 0xBE,
                            0x5D, 0x6E, 0x38, 0x3A, 0x9B, 0x1B, 0xEE,
                            0x25, 0xD8, 0x7D, 0xAB, 0x25, 0xF6}         },
    /* KT1HiHZVckhfApEmXFqCCRjWhS1dWAYQGbtm */
    {"Vendcoin",              "VEND",     6,  {0x64, 0x25, 0x15, 0x26, 0xB9, 0x69, 0x86,
                             0x77, 0xD9, 0x03, 0x72, 0xCE, 0xF2, 0x45,
                             0x85, 0x46, 0x68, 0xEE, 0x32, 0xA3}        },
    /* KT1QrtA753MSv8VGxkDrKKyJniG5JtuHHbtV */
    {"Teia DAO",              "TEIA",     6,  {0xB2, 0x8E, 0x2B, 0xB3, 0xC7, 0xF9, 0xBC,
                             0x8B, 0x49, 0x6F, 0xE1, 0x6D, 0x80, 0xCB,
                             0xB8, 0x69, 0xF8, 0x22, 0xAF, 0xC6}        },
    /* KT1UQVEDf4twF2eMbrCKQAxN7YYunTAiCTTm */
    {"Pole",                  "$POLE",    8,  {0xD9, 0x70, 0xB7, 0xF6, 0xEA, 0xFA, 0xF9,
                          0xF8, 0x18, 0x0F, 0x4E, 0xCE, 0x84, 0xC7,
                          0xCE, 0x2F, 0x98, 0x06, 0x89, 0xDB}           },
    /* KT1Wa8yqRBpFCusJWgcQyjhRz7hUQAmFxW7j */
    {"FLAME",                 "FLAME",    6,  {0xF1, 0x34, 0x3A, 0x45, 0xAE, 0x95, 0x91,
                           0xFC, 0xCB, 0x7A, 0x2F, 0x9D, 0xAF, 0xB5,
                           0x87, 0xF8, 0xC3, 0x35, 0x3E, 0x4F}          },
    /* KT1Xobej4mc6XgEjDoJoHtTKgbD1ELMvcQuL */
    {"youves YOU Governance", "YOU",      12, {0xFE, 0xB8, 0x06, 0x3B, 0x90,
                                          0x18, 0xF8, 0x3C, 0x8E, 0xC5,
                                          0xF4, 0x64, 0x55, 0xE9, 0x36,
                                          0x6D, 0x7B, 0xF4, 0xD2, 0xDA}},
    /* KT1Avad9GZXRSoKzUukD5qSen44WxPRGsQCN */
    {"Tezos Inu",             "TEZINU",   6,  {0x19, 0xAF, 0x6F, 0xE4, 0xA7, 0xB1, 0xC8,
                                0xBB, 0xAC, 0xE5, 0x4F, 0x87, 0x66, 0x08,
                                0x53, 0x9D, 0x95, 0x62, 0x93, 0x35}     },
    /* KT1MZg99PxMDEENwB4Fi64xkqAVh5d1rv8Z9 */
    {"Tezos Pepe",            "PEPE",     2,  {0x8E, 0x64, 0xB0, 0xEB, 0xE6, 0x15, 0x8C,
                               0x5F, 0x6B, 0xD1, 0x8C, 0x6C, 0x6B, 0x1B,
                               0xC1, 0x7B, 0x57, 0x50, 0xD6, 0x07}      },
    /* KT1BYfAK4j2aYSBNoxtbrCdKezNV6NTSdeSB */
    {"rugged",                "RUGD",     2,  {0x20, 0x82, 0x33, 0x07, 0x86, 0x17, 0xC5,
                           0xA0, 0xF8, 0x60, 0xD4, 0x6D, 0x2F, 0xEC,
                           0x81, 0x30, 0x8B, 0xDB, 0xF0, 0x71}          },
    /* KT1WapdVeFqhCfqwdHWwTzSTX7yXoHgiPRPU */
    {"TezID",                 "IDZ",      8,  {0xF1, 0x55, 0x53, 0x3F, 0xA1, 0xB0, 0x22,
                         0x96, 0xAE, 0x18, 0xCB, 0x0A, 0x2C, 0x67,
                         0x38, 0xA0, 0x89, 0x6F, 0xDE, 0x2C}            },
    /* KT1JBERhXmBgzBAfN27LSBbuJdXfn12vYrrF */
    {"GeckoCoin",             "GECKO",    2,  {0x69, 0x3D, 0xB2, 0x77, 0x5E, 0x28, 0xAF,
                               0x83, 0xAF, 0xC0, 0xBE, 0x4C, 0x10, 0x7E,
                               0xF6, 0xEE, 0x31, 0xF5, 0x03, 0x6A}      },
    /* KT1ErKVqEhG9jxXgUG2KGLW3bNM7zXHX8SDF */
    {"Unobtanium",            "UNO",      9,  {0x44, 0xC1, 0xA6, 0x57, 0xFC, 0x0A, 0xEB,
                              0xB8, 0x00, 0xAA, 0xA2, 0x94, 0x17, 0xBF,
                              0x61, 0x9D, 0x4A, 0xBF, 0x77, 0xB5}       },
    /* KT18hYjnko76SBVv6TaCT4kU6B32mJk6JWLZ */
    {"MathMakesArt",          "MATH",     6,  {0x01, 0x48, 0x34, 0x43, 0x55, 0x49, 0x52,
                                 0x58, 0x3D, 0xA0, 0xCC, 0x0A, 0x69, 0x9E,
                                 0x2F, 0x55, 0xFA, 0x82, 0x0B, 0xAE}    },
    /* KT1Vfc3EMLzHdZapxxyC9zForN3Qas9TEed7 */
    {"clint",                 "$clint",   6,  {0xE7, 0x44, 0xC4, 0xDC, 0x21, 0x64, 0x0E,
                            0xFE, 0x8D, 0x2F, 0xAE, 0x24, 0x11, 0x72,
                            0x3D, 0x5E, 0x55, 0x2E, 0xCF, 0xE5}         },
    /* KT1JXxK3bd39ayLiiBdKm2cdReYnVSG3bkzK */
    {"Hera Network",          "HERA",     3,  {0x6D, 0x28, 0xFA, 0xFD, 0xE8, 0xB3, 0xC7,
                                 0xBA, 0xBE, 0x04, 0xB7, 0x16, 0x58, 0xF2,
                                 0x01, 0xFD, 0x80, 0xE5, 0xCA, 0x14}    },
    /* KT1XPFjZqCULSnqfKaaYy8hJjeY63UNSGwXg */
    {"Crunchy DAO",           "crDAO",    8,  {0xFA, 0x1D, 0x6A, 0x70, 0x36, 0x14, 0x39,
                                 0xBC, 0x3E, 0xCA, 0x1B, 0xCF, 0x91, 0xE0,
                                 0xAC, 0x57, 0xFA, 0xD6, 0x67, 0xC0}    },
    /* KT1Cjx8hYwzaCAke6rLWoZBLp8w89VeAduAR */
    {"Taco DAO",              "tDAO",     4,  {0x2D, 0x9D, 0x11, 0x81, 0x30, 0xB4, 0x29,
                             0xA4, 0x90, 0x8B, 0x5C, 0x01, 0x4E, 0xB7,
                             0x80, 0x36, 0x72, 0xF1, 0x90, 0x65}        },
    /* KT1F1mn2jbqQCJcsNgYKVAQjvenecNMY2oPK */
    {"Pixel",                 "PXL",      6,  {0x46, 0x8B, 0x54, 0x8C, 0xDE, 0x1C, 0xC8,
                         0xC8, 0x03, 0x66, 0xE2, 0x8F, 0x8F, 0x44,
                         0xE5, 0x7B, 0xA4, 0xDD, 0xC3, 0xCC}            },
    /* KT1C96evxB1mkEszQczc9ki3je4kSQBhR1et */
    {"Zenith",                "ZNT",      5,  {0x27, 0x05, 0x9F, 0x74, 0xBF, 0xE5, 0xEF,
                          0x88, 0xD6, 0xB9, 0x29, 0x89, 0x68, 0xFA,
                          0x52, 0xE1, 0x18, 0x35, 0x52, 0xB7}           },
    /* KT1981tPmXh4KrUQKZpQKb55kREX7QGJcF3E */
    {"Sebuh.net",             "SEB",      2,  {0x05, 0xE8, 0xD9, 0x7A, 0xA3, 0x69, 0x10,
                             0xA8, 0xDC, 0x5C, 0x02, 0x6F, 0x94, 0x09,
                             0xAA, 0x4D, 0x1C, 0xFA, 0xF6, 0x4C}        },
    /* KT1RTsRfBxLf6egtsXrQX1CcXCqKL3XwpuFT */
    {"DoctaGonz",             "GONZ",     6,  {0xB9, 0x2C, 0x1E, 0x54, 0x37, 0x55, 0x4D,
                              0x99, 0x97, 0x08, 0x19, 0xD1, 0x52, 0x8F,
                              0x61, 0x77, 0x35, 0x40, 0x2F, 0xF0}       },
    /* KT1Fzw3uNEM7Ez8tysopUGwzDsoU2YZKUYuA */
    {"DEEPLOY",               "DEEP",     5,  {0x51, 0x5A, 0xC8, 0x07, 0xB7, 0x34, 0xE8,
                            0x91, 0xFF, 0x77, 0x69, 0x3D, 0x2E, 0x36,
                            0x5E, 0x4B, 0x12, 0x12, 0xE3, 0x1B}         },
    /* KT1SZ4ABJ6zER1sJpg11GNVKWXSno1CAK5GM */
    {"Hitchens",              "HITCH",    18, {0xC5, 0x1F, 0x48, 0x99, 0xB5, 0xB1, 0x47,
                               0xD7, 0x87, 0x51, 0x37, 0xED, 0xC8, 0x53,
                               0xA8, 0xF6, 0x00, 0x43, 0x9C, 0xA3}     },
    /* KT1XTxpQvo7oRCqp85LikEZgAZ22uDxhbWJv */
    {"GIF DAO",               "GIF",      9,  {0xFB, 0x01, 0x5F, 0xDC, 0x02, 0xAA, 0xE8,
                           0xEE, 0x28, 0xD9, 0x16, 0xE8, 0xF6, 0x36,
                           0x3A, 0x34, 0x3B, 0xE0, 0x2B, 0xBE}          },
    /* KT1HZFZiJxexeBU9Lfp5GrM3yZLq7LPHgtVt */
    {"KORA Asset",            "KORA",     2,  {0x62, 0x6F, 0xAC, 0x2F, 0x81, 0x17, 0xA4,
                               0xF5, 0x24, 0x06, 0x9F, 0x48, 0xC7, 0x2F,
                               0xBC, 0x9C, 0x0A, 0x54, 0x2D, 0x72}      },
    /* KT1TxyzQh7AqVHjjnPAAakuBwiCaWvVLp2pT */
    {"Tacos",                 "TACOS",    2,  {0xD4, 0x9D, 0xE9, 0xAF, 0x47, 0x92, 0x19,
                           0xC7, 0x73, 0x7A, 0xC6, 0x7C, 0x07, 0x57,
                           0xF6, 0x97, 0x69, 0xCC, 0xFD, 0xB4}          },
    /* KT1QoDjTpkG9jmAMwrPCsaRR78xHDcRKydBp */
    {"tChicken",              "tChicken", 6,  {0xB1, 0xDC, 0xD8, 0xEC, 0xE0, 0xF6, 0x1C,
                                 0x9A, 0x75, 0x7D, 0x06, 0xC3, 0xB3, 0xF8,
                                 0x91, 0x23, 0x4D, 0x7E, 0xDC, 0xC8}    },
};

#define FA2_TOKEN_REGISTRY_SIZE \
    (sizeof(FA2_TOKEN_REGISTRY) / sizeof(FA2_TOKEN_REGISTRY[0]))

_Static_assert(FA2_TOKEN_REGISTRY_SIZE <= (size_t)INT16_MAX + 1U,
               "FA2 token indices must fit in int16_t");

static bool
fa2_token_idx_valid(int16_t idx)
{
    return idx >= 0 && (size_t)idx < FA2_TOKEN_REGISTRY_SIZE;
}

static const fa2_token_metadata_t *
fa2_find_token(const uint8_t *destination_22bytes)
{
    if (destination_22bytes[0] != 1) {
        return NULL;
    }
    for (size_t i = 0; i < FA2_TOKEN_REGISTRY_SIZE; i++) {
        if (memcmp(destination_22bytes + 1,
                   FA2_TOKEN_REGISTRY[i].contract_hash, 20)
            == 0) {
            return &FA2_TOKEN_REGISTRY[i];
        }
    }
    return NULL;
}

#ifdef TEZOS_DEBUG
const char *const tz_operation_parser_step_name[] = {"OPTION",
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
                                                     "READ_FA2_TRANSFER"};

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
    state->operation.seen_reveal      = 0;
    state->operation.is_fa2_candidate = 0;
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
static tz_parser_result
tz_print_string(tz_parser_state *state)
{
    tz_operation_state *op = &state->operation;

    if (op->frame->step_read_string.skip) {
        tz_must(pop_frame(state));
        tz_continue;
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
 * @brief Try to read an optionnal field
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
#define FA2_STEP_EMIT_AMOUNT     23 /* emit "Token Amount" field */
#define FA2_STEP_EMIT_TO_ADDR    24 /* emit "Transfer tokens to" field */

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
        if (b != 0x02) { /* SEQ */
            return fa2_fallback_to_binary(state);
        }
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
        if (op->frame->step_read_fa2.size_val == 0) {
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_OUTER_PAIR_TAG;
        tz_continue;

    case FA2_STEP_OUTER_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x07) { /* PRIM_2_NOANNOTS */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_OUTER_PAIR_OP;
        tz_continue;

    case FA2_STEP_OUTER_PAIR_OP:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x07) { /* Pair opcode */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_FROM_ADDR_TAG;
        tz_continue;

    /* ---- from_ address ---- */
    case FA2_STEP_FROM_ADDR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x01 && b != 0x0A) { /* STRING or BYTES */
            return fa2_fallback_to_binary(state);
        }
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
        if (op->frame->step_read_fa2.size_val > FA2_ADDR_MAX_LEN) {
            return fa2_fallback_to_binary(state);
        }
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
            if (tz_format_address(CAPTURE + FA2_FROM_ADDR_OFS,
                                  op->frame->step_read_fa2.addr_ofs,
                                  (char *)(CAPTURE + FA2_FROM_ADDR_OFS),
                                  FA2_ADDR_MAX_LEN)) {
                return fa2_fallback_to_binary(state);
            }
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_SEQ_TAG;
        tz_continue;

    /* ---- txs list ---- */
    case FA2_STEP_TXS_SEQ_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x02) { /* SEQ */
            return fa2_fallback_to_binary(state);
        }
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
        if (op->frame->step_read_fa2.size_val == 0) {
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_PAIR_TAG;
        tz_continue;

    case FA2_STEP_TXS_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x07) { /* PRIM_2_NOANNOTS */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TXS_PAIR_OP;
        tz_continue;

    case FA2_STEP_TXS_PAIR_OP:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x07) { /* Pair opcode */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TO_ADDR_TAG;
        tz_continue;

    /* ---- to_ address ---- */
    case FA2_STEP_TO_ADDR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x01 && b != 0x0A) { /* STRING or BYTES */
            return fa2_fallback_to_binary(state);
        }
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
        if (op->frame->step_read_fa2.size_val > FA2_ADDR_MAX_LEN) {
            return fa2_fallback_to_binary(state);
        }
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
            if (tz_format_address(CAPTURE + FA2_TO_ADDR_OFS,
                                  op->frame->step_read_fa2.addr_ofs,
                                  (char *)(CAPTURE + FA2_TO_ADDR_OFS),
                                  FA2_ADDR_MAX_LEN)) {
                return fa2_fallback_to_binary(state);
            }
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
        if (op->frame->step_read_fa2.size_val == 0) {
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_PAIR_TAG;
        tz_continue;

    case FA2_STEP_INNER_PAIR_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x07) { /* PRIM_2_NOANNOTS */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_INNER_PAIR_OP;
        tz_continue;

    case FA2_STEP_INNER_PAIR_OP:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x07) { /* Pair opcode */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TOKEN_ID_TAG;
        tz_continue;

    /* ---- token_id (must be 0) ---- */
    case FA2_STEP_TOKEN_ID_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x00) { /* INT tag */
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_TOKEN_ID_VAL;
        tz_continue;

    case FA2_STEP_TOKEN_ID_VAL: {
        /* Read a single Zarith byte; must be 0x00 (value 0, no more bytes) */
        tz_must(tz_parser_read(state, &b));
        /* Zarith: MSB=1 means more bytes follow; value bits are low 7 bits.
           For token_id=0, the only valid encoding is a single byte 0x00. */
        if (b != 0x00) {
            return fa2_fallback_to_binary(state);
        }
        op->frame->step_read_fa2.sub_step = FA2_STEP_AMOUNT_TAG;
        tz_continue;
    }

    /* ---- amount ---- */
    case FA2_STEP_AMOUNT_TAG:
        tz_must(tz_parser_read(state, &b));
        if (b != 0x00) { /* INT tag */
            return fa2_fallback_to_binary(state);
        }
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
        if (state->ofs != op->frame->stop) {
            return fa2_fallback_to_binary(state);
        }
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
        if (fa2_token_idx_valid(op->frame->step_read_fa2.token_idx)) {
            const fa2_token_metadata_t *token
                = &FA2_TOKEN_REGISTRY[op->frame->step_read_fa2.token_idx];
            tz_format_token_amount((char *)state->buffers.num.decimal,
                                   sizeof(state->buffers.num.decimal),
                                   token->decimals, token->symbol);
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
    switch (b) {
    case 0:
        strlcpy((char *)CAPTURE, "default", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 1:
        strlcpy((char *)CAPTURE, "root", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 2:
        strlcpy((char *)CAPTURE, "do", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 3:
        strlcpy((char *)CAPTURE, "set_delegate", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 4:
        strlcpy((char *)CAPTURE, "remove_delegate", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 5:
        strlcpy((char *)CAPTURE, "deposit", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 6:
        strlcpy((char *)CAPTURE, "stake", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 7:
        strlcpy((char *)CAPTURE, "unstake", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 8:
        strlcpy((char *)CAPTURE, "finalize_unstake", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 9:
        strlcpy((char *)CAPTURE, "set_delegate_parameters", sizeof(CAPTURE));
        tz_must(tz_print_string(state));
        break;
    case 0xFF:
        op->frame->step                 = TZ_OPERATION_STEP_READ_STRING;
        op->frame->step_read_string.ofs = 0;
        op->frame->step_read_string.check_fa2
            = (op->destination[0] == 1) ? 1 : 0;
        tz_must(push_frame(state, TZ_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 1;
        break;
    default:
        tz_raise(INVALID_TAG);
    }
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
                op->frame->step_read_fa2.token_idx
                    = (int16_t)(token - FA2_TOKEN_REGISTRY);
            }
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
