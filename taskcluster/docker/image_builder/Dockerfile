# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM golang:1.14 as skopeo

WORKDIR /go/src/
COPY build-skopeo.sh /build-skopeo.sh
RUN /build-skopeo.sh

FROM golang:1.14 as kaniko
WORKDIR /go/src/
COPY build-kaniko.sh /build-kaniko.sh
RUN /build-kaniko.sh

# Build the `build-image` command as a static binary using musl
# The setup is loosely based on a stripped down version of
# https://github.com/emk/rust-musl-builder/blob/master/Dockerfile
FROM debian:buster as build-image

COPY apt.conf /etc/apt/apt.conf.d/99taskcluster
COPY setup-build-image.sh /setup-build-image.sh
COPY build-build-image.sh /build-build-image.sh

RUN /setup-build-image.sh

# Run all further code as user `rust`, and create our working directories
# as the appropriate user.
USER rust

# Expect our source code to live in /home/rust/src.  We'll run the build as
# user `rust`, which will be uid 1000, gid 1000 outside the container.
WORKDIR /home/rust/src
# Add our source code.
ADD --chown=rust:rust build-image/ ./

RUN /build-build-image.sh

FROM scratch as empty

FROM scratch

COPY --from=skopeo /go/src/out/skopeo /kaniko-bootstrap/skopeo
COPY --from=kaniko /go/src/out/executor /kaniko-bootstrap/executor
COPY --from=build-image \
    /home/rust/src/bin/build-image \
    /kaniko-bootstrap/build-image

ADD https://mkcert.org/generate/ /kaniko-bootstrap/ssl/certs/ca-certificats.crt
ENV SSL_CERT_DIR=/kaniko/ssl/certs

ADD policy.json /kaniko-bootstrap/containers/policy.json

ENV HOME /root
ENV USER /root
WORKDIR /workspace

ENV PATH /usr/local/bin:/kaniko

VOLUME /workspace
ENTRYPOINT ["/kaniko-bootstrap/build-image"]
