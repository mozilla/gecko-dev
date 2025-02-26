# Dockerfile for running fuzzing tests.
# Used for ASAN.
# Note that when running this, you need to add `--cap-add SYS_PTRACE` to the
# docker invocation or ASAN won't work.
# On taskcluster for ASAN use `features: ["allowPtrace"]`.
# See https://github.com/google/sanitizers/issues/764#issuecomment-276700920
FROM ubuntu:noble-20240605
LABEL maintainer="Martin Thomson <martin.thomson@gmail.com>"

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/.cache

# %include-run-task

ENV DEBIAN_FRONTEND noninteractive
RUN dpkg --add-architecture i386
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang \
    clang-tools \
    cmake \
    curl \
    g++-multilib \
    git \
    gyp \
    libboost-dev \
    libboost-dev:i386 \
    libclang-rt-dev \
    libsqlite3-dev \
    libsqlite3-dev:i386 \
    libxml2-utils \
    lib32z1-dev \
    linux-libc-dev:i386 \
    llvm-dev \
    locales \
    mercurial \
    ninja-build \
    pkg-config \
    python-is-python3 \
    python3-requests \
    python3-toml \
    unzip \
    valgrind \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME $USER
ENV HOME /builds/worker
ENV LANG en_US.UTF-8
ENV LC_ALL $LANG
ENV HOST localhost
ENV DOMSUF localdomain

RUN locale-gen $LANG \
 && dpkg-reconfigure locales

RUN userdel ubuntu && \
    useradd -d $HOME -s $SHELL -m $USER && \
    mkdir -p /builds/worker/workspace && \
    mkdir -p /builds/worker/artifacts && \
    chown -R $USER: /builds/worker
WORKDIR $HOME

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
