# This docker image is used to build python wheels.
FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Alexandre Lissy <alissy@mozilla.com>

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME  /builds/worker/.cache
VOLUME  /builds/worker/checkouts
VOLUME  /builds/worker/workspace

RUN apt-get update && apt-get install -y build-essential \
                    libdbus-1-dev \
                    libglib2.0-dev \
                    python3-dev \
                    python3-pip

CMD ["/bin/bash", "--login"]
