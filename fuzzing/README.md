# Fuzzing of the Tezos app

The app is fuzzed with [libFuzzer](https://llvm.org/docs/LibFuzzer.html) under
AddressSanitizer and UndefinedBehaviorSanitizer, in two parts:

- the targets of this folder, which build the parsers and the helpers of the
  app on the host from their sources, with nothing of the SDK. They are fast,
  and most check what they get back, not only that they do not crash;
- the target of [`../app/fuzzing`](../app/fuzzing/README.md), which runs the
  whole app, from its APDU dispatcher, on the SDK fuzzing framework.

The checks of the helper targets, and the driver of the parsers, are shared:
they live in [`../app/fuzzing/checks`](../app/fuzzing/checks), where the
`fuzz_helpers` and `fuzz_parser` targets of the framework use them as well.
The files of this folder only wrap them in a `LLVMFuzzerTestOneInput()`.

If a sanitizer reports a problem, or a check fails, the fuzzing stops and the
input is written to `fuzzing/crashes/<target>-crash-*`.

## Targets

| Target | What it fuzzes | What it is held to |
|--------|----------------|--------------------|
| `operation_parser` | `tz_operation_parser_step`, given the input as the data of a sequence of SIGN APDUs, with its magic byte, so micheline expressions are reached as well | no sanitizer report, nothing written past the output |
| `micheline_parser` | `tz_micheline_parser_step`, on its own | the same |
| `formatting` | decimal, base58, base58check and the Tezos formats built on them (`tz_format_*`) | what is formatted decodes back to what went in, with a valid checksum, and a helper refuses exactly when its output buffer is too small |
| `amounts` | the amount, token amount, `uint64` and mutez helpers, and the number parser | a reference that builds the result the other way around |
| `fa2_tokens` | the lookups of the token registry | they agree with each other, and the registry has no duplicate |
| `swap_key_policy` | which keys a swap may be signed with | the policy, as its header states it |
| `ui_strings`, `ui_strings_bagl`, `ui_strings_nanos` | the ring buffer of the strings displayed, in the size of each kind of device | a model of its content: every string where it was put, whole, and apart from the others |

The parsers are fed the way the app feeds them: in slices of 255, 7 and 1
byte, with an output buffer of 50 and 20 characters, each slice in a buffer of
its own so that reading past it is caught. The helpers are called on buffers
of exactly the size they are told, so the same goes for them.

## Run

```console
make fuzz
```

It builds the fuzzers in the `ledger-app-builder` container (see
`make docker_ledger_app_builder`) and runs them for 60 seconds in all, shared
between the targets. To change that, or to run only some of them:

```console
make fuzz FUZZ_TIME=600
make fuzz FUZZ_TIME=600 FUZZ_TARGETS="operation_parser amounts"
```

Each target starts from `seeds/<target>`, made by `gen_seeds.py`, and what
libFuzzer finds on top of them is added to `fuzzing/corpus/<target>`. The
corpus is not committed, but it is kept between runs.

On a machine with clang and its libFuzzer, the script can also be run directly,
which is what `make fuzz` does in the container:

```console
CC=clang-18 ./fuzzing/fuzz.sh 600 operation_parser
```

## Seeds

- `operation_parser` starts from the operation vectors of
  `unit-tests/test_operation_parser.c`, which `gen_seeds.py` extracts, so the
  seeds follow the tests. When the sample generator has been run (`make
  unit_tests` does), its samples are used too.
- `dict/` holds the dictionaries of the two parsers: operation tags, sizes,
  entrypoint names.
- The other targets start from a few inputs, in the layout their harness
  documents at its top.

## Reproduce a crash

From the repository root, replay the crashing input in the container:

```console
docker run --rm -v "$(realpath .):/app" ledger-app-builder:latest \
    /app/fuzzing/build/fuzz_<target> /app/fuzzing/crashes/<target>-crash-<hash>
```

Once fixed, the operation can be added to the unit tests, and it then becomes
a seed.

## Add a target

Write `fuzz_<name>.c` with a `LLVMFuzzerTestOneInput()`, add it with
`add_fuzzer()` in `CMakeLists.txt`, and give it seeds in `gen_seeds.py` and a
dictionary in `dict/` if that helps. `fuzz.sh`, the CI build and the corpus
pick it up from its name. A target that checks its result uses `CHECK()` of
`fuzz_check.h`, and `fuzz_ref.h` has the reference helpers.

## Continuous fuzzing

The `ClusterFuzzLite fuzzing tests` workflow builds these targets and the one
of `app/fuzzing`, using the `.clusterfuzzlite` files, runs them on pull
requests, and weekly in batch mode. It is the same setup as in the boilerplate
application. More info can be found here:
<https://google.github.io/clusterfuzzlite/>

The container that the workflow builds can be tried locally, from the
repository root:

```console
# Container generation, to be redone after each code modification
docker build -t app-tezos-fuzz --file .clusterfuzzlite/Dockerfile .
# Compilation
mkdir -p fuzzing/out
docker run --rm --privileged -e FUZZING_LANGUAGE=c -v "$(realpath .)/fuzzing/out:/out" -ti app-tezos-fuzz
# Run
docker run --rm --privileged -e FUZZING_ENGINE=libfuzzer -e RUN_FUZZER_MODE=interactive -v "$(realpath .)/fuzzing/out:/out" -ti gcr.io/oss-fuzz-base/base-runner run_fuzzer fuzz_operation_parser
```

## Notes

- The parser sources are listed in `CMakeLists.txt`, as they are in
  `unit-tests/CMakeLists.txt`: a new source file must be added to both.
- The sanitizers are the ones of `-fsanitize=undefined`, which includes signed
  integer overflow. Unsigned wrap-around is well defined in C, so it is not
  reported, though several of the bugs of the audit were one: building with
  `-fsanitize=unsigned-integer-overflow` as well is worth an occasional run, and
  reports intentional wrap-arounds too.
- `ui_strings` does not model appending to an empty string, as undoing that
  drops the string instead of the fragment.
