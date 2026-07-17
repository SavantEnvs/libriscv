# syntax=docker/dockerfile:1.7
#
# Commit-image Dockerfile (SMALL form, PREFERRED) — builds ON TOP of the org C/C++ base
# image (ghcr.io/mayhemheroes/base). The base already provides debian + clang/llvm +
# libFuzzer/afl++ + the identities + the build-contract ENV, so this only adds the
# per-commit build (built on top of the prebuilt base, rather than building the toolchain in-image).
#
# Build (from the repo root):
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile \
#       --build-arg COMMIT=$(git rev-parse HEAD) \
#       [--build-arg SOURCE_DATE_EPOCH=$(date +%s)] \
#       -t mytag .

# The org base, pinned to a fixed YEAR-MONTH base (shared by all integrations in that
# period). Bump deliberately; pin by digest in production: ...base@sha256:<digest>.
FROM ghcr.io/savantenvs/base:20260621

ARG COMMIT
ARG REPO_URL
# SOURCE_DATE_EPOCH stays an ARG (available to build.sh); do NOT bake it as an ENV — an
# empty value breaks clang ("must be a non-negative decimal integer") both here and for
# any in-image rebuild. build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH

# Sanitizer override: the base exports SANITIZER_FLAGS (ASan+UBSan, halting) as the default.
# To change it for a build (e.g. ASan-only, or add MSan): --build-arg SANITIZER_FLAGS="...".
# Declare it WITHOUT a default (no '='): an undefaulted ARG lets the base ENV flow through to
# build.sh when not passed, and an explicit --build-arg overrides it. (An empty default — `=` —
# would instead shadow the base ENV and silently wipe the sanitizers.)
ARG SANITIZER_FLAGS
# Build parallelism (optional). build.sh falls back to nproc when unset: --build-arg MAYHEM_JOBS=8
ARG MAYHEM_JOBS=

# If mayhem/build.sh needs EXTRA apt packages, install them as root before the build
# (re-pin to SOURCE_DATE_EPOCH first for reproducibility — repro-sources-list.sh ships
# in the base):
#   USER root
#   RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
#       apt-get update && apt-get install -yq --no-install-recommends <pkgs> \
#    && rm -rf /var/lib/apt/lists/*
#   USER mayhem

# mayhem/test.sh builds + runs the real Catch2 unit suite, which cross-compiles tiny RISC-V guest
# programs. That needs the riscv64 cross-compiler, which the base image does not ship. Install it as
# ROOT here (test.sh later runs as the unprivileged `mayhem` user and cannot apt-install). Re-pin the
# apt snapshot first for reproducibility when SOURCE_DATE_EPOCH is set.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        gcc-riscv64-linux-gnu g++-riscv64-linux-gnu \
 && rm -rf /var/lib/apt/lists/*

# The GUEST C library the unit suite links its tiny RISC-V programs against: glibc 2.39, the version
# upstream's own CI links them against (ubuntu-latest: gcc-14-riscv64-linux-gnu + libc6-dev-riscv64-cross
# 2.39). Debian's default here is 2.41, and since glibc 2.40 a static riscv64 binary carries an IRELATIVE
# .plt stub in front of .text. libriscv's initial execute segment covers only .text, so the first ifunc
# call leaves it, and the segment libriscv then rebuilds is decoded with default MachineOptions: the
# ebreak_locations (and the libc fast-path) of the constructor are gone, which fails test_libcfast
# "Hot-patching does not break real breakpoints" (ebreaks == 0) on every 2.40+ guest (#1805). This only
# changes the guest programs' libc; the emulator, the host test binaries and the fuzz targets are untouched.
# Same Debian packages (arch: all), pinned by snapshot.debian.org file hash and sha256. --force-depends
# because the riscv64 sanitizer-runtime packages (libasan8-riscv64-cross & co., never used here: the guests
# are built without sanitizers) declare libc6-riscv64-cross >= 2.41; the hold keeps apt from undoing it.
RUN set -eu; cd /tmp; \
    fetch() { curl -fsSL --retry 3 -o "$2" "https://snapshot.debian.org/file/$1" \
              && echo "$3  $2" | sha256sum -c -; }; \
    fetch 11814063a6871bcef26344e1d3e88c926d5aad98 libc6-riscv64-cross_2.39-4cross1_all.deb \
          c367c4d890f2552d589df4e2eb9ba11460e097bcc498dfdb0f062dc44edf64a1; \
    fetch d4bc8cd115bd926c9aa549e7f12d428b75967fc3 libc6-dev-riscv64-cross_2.39-4cross1_all.deb \
          7e71199add843107b14ad46430318cea3106b01db2a1cd94182b805655881970; \
    dpkg -i --force-depends libc6-riscv64-cross_2.39-4cross1_all.deb libc6-dev-riscv64-cross_2.39-4cross1_all.deb; \
    apt-mark hold libc6-riscv64-cross libc6-dev-riscv64-cross; \
    rm -f libc6-*riscv64-cross_*.deb; \
    [ "$(dpkg-query -W -f='${Version}' libc6-dev-riscv64-cross)" = 2.39-4cross1 ]

# Rust, for test_native_rust: it builds a Rust guest for riscv64gc-unknown-linux-gnu, and without that
# target's standard library every case SKIPs and Catch2 exits 4 (#1805). Installed under /opt/toolchains/rust
# (SPEC §6.2 item 8), root-owned and world-readable: test.sh only runs rustc, which writes to /tmp. rustup-init
# is pinned by version + sha256, the toolchain by exact version: 1.98.1 is what upstream's CI runs this test
# with; 1.99 links the guest as a static PIE, which libriscv rejects ("Execution space protection fault").
# Nothing here is fetched at test time, so the grader's offline (--network none) re-runs work.
ARG RUSTUP_VERSION=1.29.1
ARG RUSTUP_INIT_SHA256=dda7234360b7f578ca8b0ddcb80145646fa61a67c1720a5abc7051b35c9fcb71
ARG RUST_TOOLCHAIN=1.98.1
ENV RUSTUP_HOME=/opt/toolchains/rust/rustup \
    CARGO_HOME=/opt/toolchains/rust/cargo \
    PATH=/opt/toolchains/rust/cargo/bin:${PATH}
RUN set -eu; install -d /opt/toolchains/rust; cd /tmp; \
    curl -fsSL --retry 3 -o rustup-init \
        "https://static.rust-lang.org/rustup/archive/${RUSTUP_VERSION}/x86_64-unknown-linux-gnu/rustup-init"; \
    echo "${RUSTUP_INIT_SHA256}  rustup-init" | sha256sum -c -; \
    chmod +x rustup-init; \
    ./rustup-init -y --no-modify-path --profile minimal \
        --default-toolchain "${RUST_TOOLCHAIN}" --target riscv64gc-unknown-linux-gnu; \
    rm -f rustup-init; \
    rm -rf "$RUSTUP_HOME/downloads"/* "$RUSTUP_HOME/tmp"/*; \
    chmod -R a+rX /opt/toolchains/rust; \
    rustc --version; \
    ls "$(rustc --print target-libdir --target riscv64gc-unknown-linux-gnu)"/libstd-*.rlib

# Source enters from the build CONTEXT (the CI checkout): the FULL repo (incl. .git
# history) is copied to /mayhem as `mayhem`; build.sh builds in place.
USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN bash mayhem/build.sh

# Run the project's functional tests at build time IF the repo ships them (mayhem/test.sh is
# optional — omit it only when the repo has no tests meaningful for RL). A failing suite fails the
# image build, so the commit image is guaranteed to build AND pass its own tests.
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
