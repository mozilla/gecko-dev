FROM $DOCKER_IMAGE_PARENT
LABEL maintainer="Martin Thomson <martin.thomson@gmail.com>"

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/.cache

RUN sed -e 's/focal/trusty/' /etc/apt/sources.list > /etc/apt/sources.list.d/trusty.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    g++-4.4 \
    gcc-4.4 \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y
