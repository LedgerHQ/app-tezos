#!/usr/bin/env bash
# Build the fuzzers and run them for the given number of seconds (60 by
# default), shared between them. The targets to run can be given after it,
# without the "fuzz_" of their name, e.g. `fuzz.sh 600 operation_parser`.
# To be run from the ledger-app-builder image, see `make fuzz` and README.md.
set -euo pipefail

cd "$(dirname "$0")"

FUZZ_TIME="${1:-60}"
shift || true

cmake -S . -B build -DCMAKE_C_COMPILER="${CC:-clang}"
cmake --build build

targets=("$@")
if [ ${#targets[@]} -eq 0 ]; then
    for fuzzer in build/fuzz_*; do
        if [ -f "${fuzzer}" ] && [ -x "${fuzzer}" ]; then
            targets+=("${fuzzer#build/fuzz_}")
        fi
    done
fi
if [ ${#targets[@]} -eq 0 ]; then
    echo "no fuzzer was built" >&2
    exit 1
fi

# Seed with the vectors of the unit tests, and a few inputs for each target.
# What libFuzzer finds on top of them is added to the corpus of the target.
python3 gen_seeds.py seeds
mkdir -p crashes

# The stack of a UBSan report is not printed: with it, a report freezes the
# fuzzer now and then (the symbolizer stays idle, and libFuzzer ends on its
# timeout). Replay the input of the crash on its own to get the stack.
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=0}"

per_target=$((FUZZ_TIME / ${#targets[@]}))
if [ "${per_target}" -lt 5 ]; then
    per_target=5
fi

failed=()
for target in "${targets[@]}"; do
    mkdir -p "corpus/${target}"
    args=("corpus/${target}")
    if [ -d "seeds/${target}" ]; then
        args+=("seeds/${target}")
    fi
    if [ -f "dict/${target}.dict" ]; then
        args+=("-dict=dict/${target}.dict")
    fi

    echo "=== ${target}: ${per_target}s"
    # The code under test does not allocate, and LeakSanitizer does not work
    # in a container without ptrace.
    if ! "./build/fuzz_${target}" "${args[@]}" \
        -detect_leaks=0 \
        -timeout=10 \
        -max_total_time="${per_target}" \
        -artifact_prefix="crashes/${target}-"; then
        failed+=("${target}")
    fi
done

if [ ${#failed[@]} -gt 0 ]; then
    echo "failed: ${failed[*]}, see fuzzing/crashes" >&2
    exit 1
fi
