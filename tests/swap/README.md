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
| `test_tezos_native_swap_control` | A native tez swap is signed. Guards the setup itself. |
| `test_tezos_fa2` | A swap paying with USDt is signed (LIVE-36514). |
| `test_tezos_fa2_tampered` | A tampered amount, destination or fee is refused. |
| `test_tezos_fa2_wrong_token` | Transferring a different registered token is refused. |
| `test_tezos_fa2_without_token_config` | Without a sub-coin configuration the swap is refused. |

That last case is worth keeping in mind: the Exchange application forwards only
the sub-coin configuration to the coin application, never the ticker. The CAL
does not publish one for FA2 tokens on Tezos today, so the application cannot
tell which token an operation moves and refuses to sign - by design. Token
swaps only work once the CAL provides ticker and decimals, as it already does
for USDT on TON.
