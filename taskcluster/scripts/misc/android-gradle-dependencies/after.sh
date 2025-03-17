#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

# Package everything up.
pushd $WORKSPACE
mkdir -p /builds/worker/artifacts

# NEXUS_WORK is exported by `before.sh`.
cp -R ${NEXUS_WORK}/storage/mozilla android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/central android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/google android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/gradle-plugins android-gradle-dependencies

# The Gradle wrapper will have downloaded and verified the hash of exactly one
# Gradle distribution.  It will be located in $GRADLE_USER_HOME, like
# ~/.gradle/wrapper/dists/gradle-8.5-bin/$PROJECT_HASH/gradle-8.5.  We
# want to remove the version from the internal directory for use via tooltool in
# a mozconfig.
cp -a ${GRADLE_USER_HOME}/wrapper/dists/gradle-*-*/*/gradle-*/ android-gradle-dependencies/gradle-dist

tar cavf /builds/worker/artifacts/android-gradle-dependencies.tar.zst android-gradle-dependencies

# Bug 1953671
# There are intermittent issues where some files seem to be missing from the
# resulting artifacts. That causes downstream failures which are unpleasant to
# track down.
if [[ -e android-gradle-dependencies/central/com/squareup/okio/okio/2.2.2/okio-2.2.2.pom &&
    ! -e android-gradle-dependencies/central/com/squareup/okio/okio/2.2.2/okio-2.2.2.jar ]]
then
    echo "FATAL" "ERROR: incomplete dependencies file generated. try re-running task."
    exit 1
fi

popd
