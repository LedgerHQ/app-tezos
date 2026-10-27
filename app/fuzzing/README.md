# Tezos app fuzzing

Coverage-guided fuzzing of the Tezos app on the Ledger SDK fuzzing framework
(Absolution), laid out like the ones of the Ethereum and boilerplate apps.

**Read the framework documentation first.** Concepts (what a corpus is, how the
`[ prefix | tail ]` input works, the manifest, invariants, mocks, harnesses,
the CMake API, CI, and how to maintain it) live in the **Fuzzing Framework**
page of the SDK documentation, published at
<https://ledgerhq.github.io/ledger-secure-sdk/> (source:
`$BOLOS_SDK/fuzzing/doc/`). This file only documents what is specific to the
Tezos app.

## Quickstart

From the repository root, with `BOLOS_SDK` the path of the SDK:

```bash
BOLOS_SDK="$BOLOS_SDK" FUZZ_TIME=0 \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh --clean --app-dir "$(pwd)/app" probe-build

BOLOS_SDK="$BOLOS_SDK" OVERWRITE=1 FUZZ_TIME=300 APP_SANITIZER=address \
  "$BOLOS_SDK"/fuzzing/scripts/app-campaign.sh --app-dir "$(pwd)/app" baseline
```

`--app-dir` is `app/`, not the repository root: that is where the `Makefile`
and `src/` of the app are, and the fuzzing tree is the default `fuzzing/` of it.
Outputs land in `.fuzz-artifacts/<name>/`. The first build downloads
Absolution, see `LEDGER_FUZZ_ABSOLUTION_LOCAL_DIR` in `cmake.dox` to use a
local one. If an earlier build of the app left `app/src/glyphs.c` and
`app/src/glyphs.h`, remove them first: the framework generates its own.

## Targets

| Target | What it drives |
|--------|----------------|
| `fuzz_app`    | A sequence of fuzzed APDUs through the real `apdu_parser()` → `dispatch()` path |
| `fuzz_swap`   | The app as Exchange calls it: the swap callbacks, and the signing flow of a swap |
| `fuzz_parser` | The operation parser and the micheline parser, at their own entry points |
| `fuzz_helpers` | The helpers behind the parsers and the screens (formatting, amounts, token registry, swap key policy, the ring buffer of strings), each held to a reference |

`fuzz_swap`, `fuzz_parser` and `fuzz_helpers` exist because the APDU path
cannot reach that code, or only through a signing. The callbacks of the swap
library are called by Exchange rather than through an INS, and a swap runs a
signing with no review, validated against what Exchange gave. The parsers hold
pointers into their own state, which the invariant cannot restore, so through
`fuzz_app` they are only reached after the path of a key and the packets before
them. Both still go through the framework contract, so they get the
prefix-aware mutator and the lane split for free; control byte 1 picks the swap
mode, the parser or the helper.

`fuzz_helpers` checks what the others cannot: not that the app survives the
input, but that the answer is right. What is formatted must decode back to
what went in, with a valid checksum, and an amount must match a reference
built the other way around. The checks are in `checks/`. The harness includes
`operation_parser.c`, for the helpers that are static in it, in place of the
copy of the app.

## Why one input is a sequence

`fuzz_app` replays a *sequence* of APDUs per input, not one. A signing is the
path of the key in a first APDU (`handle_signing_key_setup()`), then the
operation in the following ones (`handle_sign()`): the hash and the parser of
the operation are built by the packets before, so a single dispatch can never
get past the first.

The fuzzer picks every field of every step, including how many steps there
are. The only structure the harness adds is the protocol's own framing: in the
structured lane, the four values of P1 that a signing knows (first, next, and
each as the last), and the single value that the public key commands take. The
raw lane leaves P1 and P2 as the input made them, to reach the refusals.

## Where values come from

The harnesses own **structure** and never author **content**. Every byte of a
path, of an operation, of the parameters of a swap, comes from the fuzzer.
`fuzz_swap` is the one that sets state itself, in `fuzz_app_reset()`: what
Exchange sets when it starts the app (`G_called_from_swap`, the address the
result is written to), which the invariant keeps clear for the others.
`mock/fuzz_input.h` is the cursor all of them read through.

## App-owned files in this tree

```text
app/fuzzing/
  fuzz-manifest.toml   targets, seeds, dictionary, coverage key files
  CMakeLists.txt       sources, include dirs, one line per target
  harness/             one fuzz_*.c per target
  mock/                the input cursor, the APDU sequence runner, app_exit()
  checks/              the checks of fuzz_helpers, their reference, the parser driver
  invariants/          zero-symbols.txt, domain-overrides.txt
```

`fuzz-manifest.toml` is the authoritative list of targets, seeds and
dictionary. The invariants (`invariants/<target>.zon`) are generated per build
and not committed. `domain-overrides.txt` holds the state machine of the app
(`global.step`) at its start, as the first packet of a signing needs
`ST_IDLE`, and keeps the key, the curve and the strings of the screens in
range. `global.ui` and `global.keys`, which hold the parser of the operation
and point into themselves, are padding to Absolution and zero at the start of
each iteration: the sequences of APDUs of the harnesses build them. An override
that matches no field is an error of the build, so read the generated model
again after a change to `global`.

`.clusterfuzzlite/build.sh` is a thin wrapper that delegates to the SDK's shared
`fuzzing/scripts/cfl-build.sh`, with `APP_DIR` set to `app/`. The CFL
Dockerfile copies the SDK from the Ledger app development image, and the whole
repository with its `.git`, which the SDK Makefiles need.
