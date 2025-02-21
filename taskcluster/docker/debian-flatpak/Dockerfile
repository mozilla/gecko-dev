FROM $DOCKER_IMAGE_PARENT AS build

RUN apt-get update && apt-get -y --no-install-recommends install gcc libc6-dev

ADD close_range.c /tmp/close_range.c
RUN ["gcc", "-Wall", "-shared", "-o", "/tmp/close_range.so", "/tmp/close_range.c"]

FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mozilla Releng <release@mozilla.com>
 
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace

ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES

RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES && \
    apt-get update && \
    apt-get install librsvg2-common flatpak ostree appstream-util appstream-compose && \
    /usr/local/sbin/clean_packages.sh $DOCKER_IMAGE_PACKAGES

# The docker version in fxci workers doesn't support the close_range system call
COPY --from=build /tmp/close_range.so /usr/local/lib/close_range.so
ENV LD_PRELOAD /usr/local/lib/close_range.so

# flatpak as non-root wants to connect to the system bus
COPY dbus.sh /usr/local/bin/dbus.sh
RUN chmod +x /usr/local/bin/dbus.sh
ENTRYPOINT ["/usr/local/bin/dbus.sh"]
