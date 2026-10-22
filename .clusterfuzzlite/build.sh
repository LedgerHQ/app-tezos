#!/bin/bash -eu

# build fuzzers

pushd fuzzing
cmake -Bbuild -H.
make -C build
mv ./build/fuzz_operation_parser "${OUT}"

# seed corpus, picked up by ClusterFuzzLite from <fuzzer>_seed_corpus.zip
python3 gen_seeds.py seeds
zip -j "${OUT}/fuzz_operation_parser_seed_corpus.zip" seeds/*
popd
