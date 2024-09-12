# Minimal image with clang-format-13
FROM debian:bookworm-20240904-slim
LABEL maintainer="John Schanck <jschanck@mozilla.com>"

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    ca-certificates \
    clang-format-13 \
    locales \
    mercurial \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

RUN update-alternatives --install /usr/bin/clang-format \
    clang-format $(which clang-format-13) 13

ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME $USER
ENV HOME /home/$USER
ENV LANG en_US.UTF-8
ENV LC_ALL $LANG
ENV HOST localhost
ENV DOMSUF localdomain

RUN locale-gen $LANG \
 && DEBIAN_FRONTEND=noninteractive dpkg-reconfigure locales

RUN useradd -d $HOME -s $SHELL -m $USER
WORKDIR $HOME

ADD bin $HOME/bin
RUN chmod +x $HOME/bin/*

USER $USER

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
