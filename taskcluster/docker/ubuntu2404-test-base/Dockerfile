# This docker image only sets up packages on top of the original Ubuntu image.
# Everything else is setup is a child docker image derived from this one, such
# that changes to our scripts don't trigger the rebuild of this base image,
# possibly leading to breakage. (See bug 1503756 and bug 1678451 for examples
# of such recent breakages)
FROM          $DOCKER_IMAGE_PARENT

ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES

# %include taskcluster/docker/recipes/ubuntu2404-test-system-setup-base.sh
ADD topsrcdir/taskcluster/docker/recipes/ubuntu2404-test-system-setup-base.sh /setup/system-setup.sh
RUN     bash /setup/system-setup.sh
RUN /usr/local/sbin/clean_packages.sh $DOCKER_IMAGE_PACKAGES

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
