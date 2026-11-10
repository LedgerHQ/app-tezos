# Swap functional tests

These tests exercise the Tezos application as it is used by the
[Exchange application](https://ledgerhq.github.io/app-exchange/) through the
`os_lib_call` mechanism, i.e. the swap feature. That mode takes a different
launch path in the application and needs a different Speculos setup, which is
why it lives beside `tests/standalone/` rather than inside it.

Speculos runs the Exchange application, with the Tezos application and the
Ethereum application - which serves as the payout currency - sideloaded as
libraries.

```text
swap/
├── cal_helper.py                     # Coin configurations, as the CAL would provide them
├── conftest.py                       # Ragger configuration and fixtures
├── tezos_transaction.py              # Minimal Tezos operation encoding
├── test_swap_exchange.py             # The cases of the Exchange application, native tez
├── test_swap_fa2.py                  # The test cases
├── helper_tool_clone_dependencies.py # Clones Exchange and Ethereum (run on the host)
├── helper_tool_build_dependencies.py # Builds them (run inside the Ledger docker image)
├── snapshots/                        # Ragger UI snapshots
└── requirements.txt                  # Python dependencies
```

## Setup

Build the Tezos application for the device you want to test, in the Ledger
docker environment, as usual.

Then clone the Exchange and Ethereum applications, on the host:

```sh
pip install -U GitPython
python tests/swap/helper_tool_clone_dependencies.py
```

And build them, from the `tests/swap` directory, inside the docker image:

```sh
docker run --user "$(id -u)":"$(id -g)" --rm -ti -v "$(realpath .):/app" \
    "ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest" \
    python3 helper_tool_build_dependencies.py
```

This drops the binaries where `conftest.py` expects them:
`.test_dependencies/main/app-exchange/` and
`.test_dependencies/libraries/app-ethereum/`.

## Running

```sh
pip install -r tests/swap/requirements.txt
pytest tests/swap --device flex
```

Add `--golden_run` to regenerate the snapshots, and `-k <name>` to run a single
case. In CI the same is done by the `tests_swap` job, which builds the
dependencies declared under `[pytest.swap.dependencies]` in `ledger_app.toml`.

## What is covered

| Case | Expectation |
| --- | --- |
| `test_tezos` | The swap, sell and fund flows of the Exchange application, with native tez: a valid transaction is signed, a wrong amount, destination or fee is refused. These are the Tezos cases of the Exchange repository, see below. |
| `test_tezos_native_swap_control` | A native tez swap is signed. Guards the setup itself. |
| `test_tezos_fa2` | A swap paying with USDt is signed (LIVE-36514). |
| `test_tezos_fa2_tampered` | A tampered amount, destination or fee is refused. |
| `test_tezos_fa2_wrong_token` | Transferring a different registered token is refused. |
| `test_tezos_fa2_without_token_config` | Without a sub-coin configuration an FA2 transfer is refused. |
| `test_tezos_native_transfer_for_token_swap` | **Known limitation ([#66](https://github.com/LedgerHQ/app-tezos-dev/issues/66)).** For a USDt swap with no sub-coin configuration, a plain tez transfer with the quoted amount, fee and recipient is signed. Pins today's behaviour. |
| `test_tezos_fa2_unregistered_contract` | An FA2-shaped transfer to a contract outside the registry is refused. |
| `test_tezos_fa2_foreign_owner` | Tokens of another owner than the signing account are refused, whatever the encoding of `from_` (string, bytes, a string with a NUL) and the instruction (`INS_SIGN`, `INS_SIGN_WITH_HASH`), even though the operation source, token, amount, recipient and fee are valid. |
| `test_tezos_swap_signing_key` | A key of another curve, of another coin or with an unhardened path is refused when it is set up, and an operation whose source is not the signing key is refused when it is checked. |
| `test_tezos_swap_pubkey_during_signing` | A public key request, with or without a prompt, is refused while a signing is under way, and the signing is left as it was: the signature that follows is still produced. |
| `test_replay`, `test_replay_as_captured` | The captured Ledger Live signing requests, four component path included, are signed once the token is known, and refused without it. |

That last case is worth keeping in mind: the Exchange application forwards only
the sub-coin configuration to the coin application, never the ticker. The CAL
does not publish one for FA2 tokens on Tezos today, so the application cannot
tell which token an FA2 transfer moves and refuses to sign it - by design.
Token swaps only work once the CAL provides ticker and decimals, as it already
does for USDT on TON.

**Known limitation ([#66](https://github.com/LedgerHQ/app-tezos-dev/issues/66)) - open
today.** A swap without a coin configuration is taken as a native tez swap,
because Exchange does not tell the application which currency it quoted. The
CAL describes USDt for Tezos without a sub-coin configuration (see
`USDT_NO_TOKEN_CONFIGURATION` in `cal_helper.py`), so a swap quoted in USDt
reaches the application exactly like a tez swap. The host can then send a plain
tez transfer with the same integer amount, fee and recipient, and the
application signs it: 100106851 USDt base units become `100.106851 XTZ`, which
is also how the Exchange screen prints them.
`test_tezos_native_transfer_for_token_swap` pins that behaviour.

The application cannot refuse it without refusing every real tez swap
(`test_tezos_native_swap_control` sends exactly that). Closing it needs Exchange
to pass the ticker of the currency it quoted, or the CAL to give every non-tez
asset it lists for Tezos a sub-coin configuration.

## The Exchange application cases

`test_swap_exchange.py` and its snapshots were taken from the Exchange
repository (`test/python/test_tezos.py`, `test/python/snapshots/<device>/test_tezos_*`
of https://github.com/LedgerHQ/app-exchange-dev, at commit `3016ce50`), so that
the Tezos application is tested here and not only there. The constants are the
same, and so are the transactions: `craft_revealed_native_transfer()` builds the
bytes of the Exchange test, a reveal then a transfer, which is also what Ledger
Live sends for an account that is not revealed yet.

The cases are those of `ALL_TESTS_EXCEPT_MEMO_AND_THORSWAP` of the Exchange test
runner, so a case added to the runner has no snapshot here until it is
generated with `--golden_run`. The snapshots of these cases are the screens of
the Exchange application: when its UI changes, they are to be updated here as
they are there.

## Refusal error codes

A swap the application refuses is answered with the status word it has always
used (`0x6985`, or `0x6A80` when the key is set up), which the tests of the
Exchange application expect. The data of the answer is a two bytes error code
sent with the SDK helper `send_swap_error_simple`: the first byte is one of the
`swap_error_common_code_t` of the SDK (`0x01` wrong amount, `0x02` wrong
destination, `0x03` wrong fees, `0x04` wrong method, `0xFF` generic), the second
one tells which check of the application refused, see `tz_swap_error_t` in
`app/src/handle_swap.h`. The signing key, wrong token, no token configuration and
tez transfer with parameters cases assert it, so they fail if a refusal comes from
another check than the one they expect.
