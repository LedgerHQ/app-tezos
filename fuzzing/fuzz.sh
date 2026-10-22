#!/usr/bin/env bash
# Build the operation parser fuzzer and run it for the given number of seconds
# (60 by default). To be run from the ledger-app-builder image, see `make fuzz`
# and README.md.
set -euo pipefail

cd "$(dirname "$0")"

FUZZ_TIME="${1:-60}"

cmake -S . -B build -DCMAKE_C_COMPILER="${CC:-clang}"
cmake --build build

# Seed with the vectors of the unit tests. What libFuzzer finds on top of them
# is added to the corpus directory.
python3 gen_seeds.py corpus
mkdir -p crashes

export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}"

# The parser does not allocate, and LeakSanitizer does not work in a container
# without ptrace.
./build/fuzz_operation_parser corpus \
    -detect_leaks=0 \
    -timeout=10 \
    -max_total_time="$FUZZ_TIME" \
    -artifact_prefix=crashes/
