# FA2 transfer clear-signing (Ledger app)

The operation parser can recognize a subset of FA2 `transfer` calls to a **KT1**
contract listed in the registry and show **Transfer tokens from** (the owner
whose balance is debited, which may not be the operation source), **Transfer
tokens to** (receiver address) and **Token Amount** instead of a single opaque
**Parameter** field.
The **Destination** field (FA2 contract) is omitted from the review flow for
those contracts; the **Token** name field is not shown.

## When clear-signing applies

- Destination is a KT1 contract whose 20-byte hash is listed in the in-app
  **FA2 token registry** (`FA2_TOKEN_REGISTRY` in
  `app/src/parser/fa2_tokens.c`). The entrypoint name and the parameter are
  chosen by the host, so only this registry can give the fields their
  meaning: a KT1 that is not listed is never decoded as FA2, whatever its
  entrypoint and parameter look like.
- Entrypoint name is exactly `transfer` (string match after read).
- The Micheline parameter matches the **single-transfer** shape parsed by
  `tz_step_read_fa2_transfer`: outer `list`/`Pair` of `from_`, txs `list` with
  one `Pair` of `to_` and inner `list(pair(token_id, amount))` (or the
  equivalent direct `Pair` encoding for a single inner item).
- The **`(contract, token_id)` pair must be listed** in the registry. A listed
  contract called with a `token_id` the registry does not list falls back to
  generic parameter display (**Parameter**, complex).
- **No extra bytes** after the parsed amount: the parser must consume the
  whole parameter expression; otherwise it falls back to binary display.

## When the app falls back

- Multiple transfers in the txs list, non-zero `token_id`, negative or
  non-standard amount encoding, or any Micheline shape that does not match the
  parser: the field is shown as **`Parameter`** with **`is_field_complex`** set
  (see `fa2_fallback_to_binary`).
- **Unknown contracts or tokens** (the contract, or the `(contract, token_id)`
  pair, is not in the registry): the parameter is shown as **`Parameter`**
  with **`is_field_complex`** set. No FA2 label (**Transfer tokens from**,
  **Transfer tokens to**, **Token ID**, **Token Amount**, **Amount (raw)**) is
  shown. **Destination** is always shown.
- **`from_` string with a NUL inside** (a string whose C-string value is
  shorter than its declared length): never decoded as FA2, so the truncated
  value is not what the FA2 labels show and a swap has no owner to validate.
- **Negative amount** on a registered token: never a valid transfer, so it is
  shown raw and signed (**Amount (raw)**, preceded by **Token ID**), with
  **`is_field_complex`** set.

## Token metadata

Decimals and symbols come only from the static registry compiled into the app.
There is no runtime fetch from chain or third-party APIs.

## Tests

- C unit tests in `unit-tests/test_operation_parser.c` (FA2-related cases).
- Hex blobs for those tests can be regenerated with
  `scripts/gen_fa2_ctest_hex.py` (requires `pip install base58`).
