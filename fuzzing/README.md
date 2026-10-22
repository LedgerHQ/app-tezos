# Fuzzing of the operation parser

The operation parser decodes data that is fully controlled by the host, so
it is fuzzed with [libFuzzer](https://llvm.org/docs/LibFuzzer.html) under
AddressSanitizer and UndefinedBehaviorSanitizer. If a sanitizer reports a
problem, the fuzzing stops and the input that triggered it is written to
`fuzzing/crashes/crash-*`.

The fuzz target, `fuzz_operation_parser.c`, feeds arbitrary bytes to
`tz_operation_parser_step` the way `handle_data_apdu_clear` does in the app:
the input is split into the data of a sequence of APDUs, the output buffer
is flushed whenever it is full, and the total size is set with the last
APDU. The first byte of the input is the magic byte, so micheline
expressions are fuzzed as well. Each input is parsed with several APDU and
output buffer sizes.

## Run

```console
make fuzz
```

It builds the fuzzer in the `ledger-app-builder` container (see
`make docker_ledger_app_builder`) and runs it for 60 seconds. Use
`FUZZ_TIME` to change that:

```console
make fuzz FUZZ_TIME=600
```

The run is seeded with the operations of `unit-tests/test_operation_parser.c`
(`gen_seeds.py` extracts them), and what libFuzzer finds on top of them is
added to `fuzzing/corpus`. The corpus is not committed, but it is kept
between runs, so a later run starts from it.

## Reproduce a crash

From the repository root, replay the crashing input in the container:

```console
docker run --rm -v "$(realpath .):/app" ledger-app-builder:latest \
    /app/fuzzing/build/fuzz_operation_parser /app/fuzzing/crashes/crash-<hash>
```

Once fixed, the operation can be added to the unit tests, and it then becomes
a seed.

## Manual usage

To work on the fuzzer, enter the container from the repository root:

```console
docker run --rm -ti --user "$(id -u):$(id -g)" -v "$(realpath .):/app" ledger-app-builder:latest
```

and then build and run it by hand:

```console
cd /app/fuzzing
cmake -S . -B build -DCMAKE_C_COMPILER=clang
cmake --build build
python3 gen_seeds.py corpus
./build/fuzz_operation_parser corpus -detect_leaks=0
```

## Continuous fuzzing

The `ClusterFuzzLite fuzzing tests` workflow runs the fuzzer on pull
requests, and weekly in batch mode, using the `.clusterfuzzlite` files. It is
the same setup as in the boilerplate application. More info can be found
here: <https://google.github.io/clusterfuzzlite/>

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
  reported.
