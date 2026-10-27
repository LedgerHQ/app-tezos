FROM ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest AS app-builder

# Base image with clang toolchain
FROM gcr.io/oss-fuzz-base/base-builder:v1

# Install additional package dependencies of the SDK fuzzing framework.
RUN pip3 install --break-system-packages --no-cache-dir "pillow>=3.4.0"
RUN apt-get update \
    && apt-get install -y ninja-build libbsd-dev pkg-config \
    && rm -rf /var/lib/apt/lists/*

COPY --from=app-builder /opt/flex-secure-sdk /ledger-secure-sdk

# Copy the project's source code, with its .git: the SDK Makefiles must be run
# inside a git repository.
COPY . /app

# Working directory for build.sh
WORKDIR /app

# Copy build.sh into $SRC dir.
COPY ./.clusterfuzzlite/build.sh $SRC/
