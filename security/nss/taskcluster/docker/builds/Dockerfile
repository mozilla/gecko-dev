# Dockerfile for building extra builds.  This includes more tools than the
# default image, so it's a fair bit bigger.  Only use this for builds where
# the smaller docker image is missing something.  These builds will run on
# the leaner configuration.
FROM $DOCKER_IMAGE_PARENT

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/.cache

ADD bionic.list /etc/apt/sources.list.d/bionic.list
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    clang-4.0 \
    clang-10 \
    cmake \
    g++-4.8-multilib \
    g++-5-multilib \
    libelf-dev \
    libdw-dev \
    libssl-dev \
    libssl-dev:i386 \
    valgrind \
    clang-format-10 \
    libabigail-dev \
    abigail-tools \
    software-properties-common \ 
 && add-apt-repository ppa:ubuntu-toolchain-r/test -y \ 
 && apt-get update \
 && apt-get install --no-install-recommends -y \ 
    gcc-11-multilib \
    g++-11-multilib \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

RUN update-alternatives --install /usr/bin/clang-format \
    clang-format $(which clang-format-10) 10

# Install golang for bogo tests
RUN curl https://dl.google.com/go/go1.23.1.linux-amd64.tar.gz -sLf | tar xzf - -C $HOME
ENV PATH "$PATH:$HOME/go/bin"

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
