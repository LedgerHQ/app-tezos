/* Fuzz target for the operation parser.
 *
 * The input is parsed as the data of a sequence of SIGN APDUs: it is given
 * to `tz_operation_parser_step` slice by slice, the total size is set along
 * with the last slice, and the output buffer is flushed whenever it is full,
 * like `handle_data_apdu_clear` does in the app. The first byte of the input
 * is the magic byte, so micheline expressions are fuzzed as well.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "operation_parser.h"

/* Every input is parsed once per configuration, to move the refill and flush
 * boundaries around:
 *   chunk: size of the slices of input given to the parser, 255 is the
 *          largest APDU payload, 1 and 7 make fields straddle boundaries
 *   olen:  size of the output buffer, 20 is the NBGL line width and 50 is
 *          what the unit tests use */
static const struct {
    size_t chunk;
    size_t olen;
} configs[] = {
    {255, 50},
    {7,   20},
    {1,   20},
};

static void
parse_input(const uint8_t *data, size_t size, size_t chunk, size_t olen)
{
    tz_parser_state *st   = calloc(1, sizeof(*st));
    char            *obuf = calloc(1, olen + 1);
    uint8_t         *ibuf = NULL;
    size_t           ofs  = 0;

    if (st == NULL || obuf == NULL) {
        abort();
    }

    tz_operation_parser_init(st, TZ_UNKNOWN_SIZE, false);
    tz_parser_refill(st, NULL, 0);
    tz_parser_flush(st, obuf, olen);

    while (true) {
        while (!TZ_IS_BLOCKED(tz_operation_parser_step(st))) {
            // Loop while the result is successful and not blocking
        }

        // The parser must never write past the end of the output
        if (obuf[olen] != 0) {
            abort();
        }

        if (st->errno == TZ_BLO_IM_FULL) {
            tz_parser_flush(st, obuf, olen);
        } else if (st->errno == TZ_BLO_FEED_ME && ofs < size) {
            size_t len = (size - ofs < chunk) ? (size - ofs) : chunk;

            // Each slice gets a buffer of its own, so that sanitizers catch
            // reads past the end of the slice and of a consumed one
            free(ibuf);
            ibuf = malloc(len);
            if (ibuf == NULL) {
                abort();
            }
            memcpy(ibuf, data + ofs, len);
            ofs += len;

            tz_parser_refill(st, ibuf, len);
            if (ofs == size) {
                tz_operation_parser_set_size(st, (uint16_t)size);
            }
        } else {
            // Done, failed, or out of input
            break;
        }
    }

    free(ibuf);
    free(obuf);
    free(st);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // The size of an operation is a uint16_t, TZ_UNKNOWN_SIZE being reserved
    if (size == 0 || size >= TZ_UNKNOWN_SIZE) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); i++) {
        parse_input(data, size, configs[i].chunk, configs[i].olen);
    }
    return 0;
}
