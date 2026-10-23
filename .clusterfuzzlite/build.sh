#!/bin/bash -eu

# Fuzzers of the parsers and the helpers: built from the sources of the app
# alone, with the seeds and the dictionary that ClusterFuzzLite looks for next
# to each of them.
pushd /app/fuzzing
cmake -Bbuild -H.
make -C build
python3 gen_seeds.py seeds

for fuzzer in build/fuzz_*; do
    if [ ! -f "${fuzzer}" ] || [ ! -x "${fuzzer}" ]; then
        continue
    fi
    name="$(basename "${fuzzer}")"
    target="${name#fuzz_}"

    mv "${fuzzer}" "${OUT}/"
    if [ -d "seeds/${target}" ]; then
        zip -j "${OUT}/${name}_seed_corpus.zip" "seeds/${target}"/*
    fi
    if [ -f "dict/${target}.dict" ]; then
        cp "dict/${target}.dict" "${OUT}/${name}.dict"
    fi
done
popd

# Fuzzer of the whole app, on the SDK fuzzing framework: delegate to the shared
# SDK script.
export BOLOS_SDK=/ledger-secure-sdk
export APP_DIR=/app/app
exec "${BOLOS_SDK}/fuzzing/scripts/cfl-build.sh"
