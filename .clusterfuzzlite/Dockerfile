# Base image with clang toolchain
FROM gcr.io/oss-fuzz-base/base-builder:v1

# Copy what the fuzzer is built from. The parser does not need the SDK.
COPY app/src/parser $SRC/app-tezos/app/src/parser
COPY unit-tests/digestif $SRC/app-tezos/unit-tests/digestif
COPY unit-tests/strlcpy.c unit-tests/test_operation_parser.c $SRC/app-tezos/unit-tests/
COPY fuzzing $SRC/app-tezos/fuzzing

# Working directory for build.sh
WORKDIR $SRC/app-tezos

# Copy build.sh into $SRC dir.
COPY ./.clusterfuzzlite/build.sh $SRC/
