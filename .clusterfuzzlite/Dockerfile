FROM ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest AS app-builder

# Base image with clang toolchain
FROM gcr.io/oss-fuzz-base/base-builder:v1

# Install additional package dependencies of the SDK fuzzing framework.
RUN pip3 install --break-system-packages --no-cache-dir "pillow>=3.4.0"
RUN apt update && apt install -y ninja-build libbsd-dev pkg-config

COPY --from=app-builder /opt/flex-secure-sdk /ledger-secure-sdk

# Copy what the fuzzers are built from.
COPY app /app/app
COPY unit-tests/digestif /app/unit-tests/digestif
COPY unit-tests/shims /app/unit-tests/shims
COPY unit-tests/strlcpy.c unit-tests/test_operation_parser.c /app/unit-tests/
COPY fuzzing /app/fuzzing

# Working directory for build.sh
WORKDIR /app

# Copy build.sh into $SRC dir.
COPY ./.clusterfuzzlite/build.sh $SRC/
