## Cross building
I recommend you should use docker environment

#### Dockerfile
```dockerfile
FROM ubuntu:21.04
MAINTAINER Makoto Kato <m_kato@ga2.so-net.ne.jp>

ADD sources.list /etc/apt/
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture riscv64 && \
    apt-get update && \
    apt-get install -y clang g++ mercurial g++-riscv64-linux-gnu curl gyp ninja-build make python-is-python3 libssl-dev zlib1g-dev nodejs build-essential libpython3-dev m4 unzip zip uuid git python3-pip && \
    apt-get install -y zlib1g-dev:riscv64 libssl-dev:riscv64 libffi-dev:riscv64 libasound2-dev:riscv64 libcurl4-openssl-dev:riscv64 libdbus-1-dev:riscv64 libdbus-glib-1-dev:riscv64 libdrm-dev:riscv64 libgtk-3-dev:riscv64 libpulse-dev:riscv64 libx11-xcb-dev:riscv64 libxt-dev:riscv64 xvfb:riscv64 libstdc++-10-dev:riscv64 && \
    apt-get clean
WORKDIR /root
ENV PATH="/root/.cargo/bin:${PATH}"
RUN curl https://sh.rustup.rs -s -o install.sh && sh install.sh -y && rm install.sh && rustup target add riscv64gc-unknown-linux-gnu
RUN cargo install cbindgen
```

#### sources.list
```
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute main restricted
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute-updates main restricted
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute universe
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute-updates universe
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute multiverse
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute-updates multiverse
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ hirsute-backports main restricted universe multiverse
deb [arch=amd64] http://security.ubuntu.com/ubuntu/ hirsute-security main restricted
deb [arch=amd64] http://security.ubuntu.com/ubuntu/ hirsute-security universe
deb [arch=amd64] http://security.ubuntu.com/ubuntu/ hirsute-security multiverse

deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute main restricted
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-updates main restricted
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute universe
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-updates universe
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute multiverse
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-updates multiverse
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-backports main restricted universe multiverse
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-security main restricted
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-security universe
deb [arch=riscv64] http://ports.ubuntu.com/ubuntu-ports hirsute-security multiverse
```

## mozconfig
```
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/objdir
mk_add_options AUTOCLOBBER=1

ac_add_options --disable-debug
ac_add_options --enable-optimize

ac_add_options --target=riscv64
export CC=riscv64-linux-gnu-gcc
export CXX=riscv64-linux-gnu-g++
export HOST_CC=gcc
export HOST_CXX=g++
ac_add_options --disable-bootstrap
ac_add_options --without-wasm-sandboxed-libraries
ac_add_options --enable-webrtc
ac_add_options --disable-crashreporter
ac_add_options --disable-jit
```

## How to build
1. `./mach build`
2. `./mach package`
