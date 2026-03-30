# FA2 transfer clear-signing (Ledger app)

The operation parser can recognize a subset of FA2 `transfer` calls to a **KT1**
contract listed in the registry and show **Transfer tokens to** (receiver
address) and **Token Amount** instead of a single opaque **Parameter** field.
The **Destination** field (FA2 contract) is omitted from the review flow for
those contracts; the **Token** name field is not shown.

## When clear-signing applies

- Destination is a KT1 contract whose 20-byte hash is listed in the in-app
  **FA2 token registry** (`FA2_TOKEN_REGISTRY` in
  `app/src/parser/operation_parser.c`).
- Entrypoint name is exactly `transfer` (string match after read).
- The Micheline parameter matches the **single-transfer** shape parsed by
  `tz_step_read_fa2_transfer`: outer `list`/`Pair` of `from_`, txs `list` with
  one `Pair` of `to_` and inner `list(pair(token_id, amount))` (or the
  equivalent direct `Pair` encoding for a single inner item).
- **`token_id` must be zero** (single-byte Zarith `0x00`). Non-zero IDs fall
  back to generic parameter display (often **Parameter** with complex/binary
  presentation).
- **No extra bytes** after the parsed amount: the parser must consume the
  whole parameter expression; otherwise it falls back to binary display.

## When the app falls back

- Multiple transfers in the txs list, non-zero `token_id`, negative or
  non-standard amount encoding, or any Micheline shape that does not match the
  parser: the field is shown as **`Parameter`** with **`is_field_complex`** set
  (see `fa2_fallback_to_binary`).
- **Unknown tokens** (contract not in the registry): the amount is still shown
  as a decimal string, but **without** symbol/decimal scaling from a registry
  entry. **Transfer tokens to** still appears for successful FA2-shaped
  parameters; **Destination** stays visible if the contract is not in the
  registry.

## Token metadata

Decimals and symbols come only from the static registry compiled into the app.
There is no runtime fetch from chain or third-party APIs.

## Tests

- C unit tests in `tests/unit/ctest/tests_parser.c` (FA2-related cases).
- Hex blobs for those tests can be regenerated with
  `scripts/gen_fa2_ctest_hex.py` (requires `pip install base58`).
