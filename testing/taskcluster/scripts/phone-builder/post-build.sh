#! /bin/bash -vex

if [ $UPDATE_BALROG_RELEASE ]; then
  if [ ! ${BUILD_ERROR} ]; then
    $WORKSPACE/gecko/testing/mozharness/scripts/b2g_build.py \
      --config $BALROG_SERVER_CONFIG
      --complete-mar-url https://queue.taskcluster.net/v1/task/$TASK_ID/runs/$RUN_ID/artifacts/public/build/
  fi
fi

# Don't cache backups
rm -rf $WORKSPACE/B2G/backup-*

if [ -f balrog_credentials ]; then
  rm -f balrog_credentials
fi

mkdir -p $HOME/artifacts
mkdir -p $HOME/artifacts-public

DEVICE=${TARGET%%-*}

mv $WORKSPACE/B2G/upload/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/upload/b2g-*.android-arm.tar.gz $HOME/artifacts/b2g-android-arm.tar.gz
mv $WORKSPACE/B2G/upload/${TARGET}.zip $HOME/artifacts/${TARGET}.zip
mv $WORKSPACE/B2G/upload/gaia.zip $HOME/artifacts/gaia.zip

# Upload public images as public artifacts on Nexus 4 KK and Nexus 5 L
if [ "${TARGET}" = "nexus-4-kk" -o "${TARGET}" = "nexus-5-l" ]; then
  mv $HOME/artifacts/${TARGET}.zip $HOME/artifacts-public/
fi

if [ -f $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip ]; then
  mv $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip
fi

if [ -f $WORKSPACE/B2G/upload-public/*.blobfree-dist.zip ]; then
  mv $WORKSPACE/B2G/upload-public/*.blobfree-dist.zip $HOME/artifacts-public/
fi

# FOTA full and fullimg might contain blobs
if [ -f $WORKSPACE/B2G/upload/fota-*-update-*.mar ]; then
  mv $WORKSPACE/B2G/upload/fota-*-update-*.mar $HOME/artifacts/
fi

# Gecko/Gaia OTA is clean
if [ -f $WORKSPACE/B2G/upload-public/b2g-*-gecko-update.mar ]; then
  mv $WORKSPACE/B2G/upload-public/b2g-*-gecko-update.mar $HOME/artifacts-public/
fi

# Gecko/Gaia FOTA is clean
if [ -f $WORKSPACE/B2G/upload-public/fota-*-update.mar ]; then
  mv $WORKSPACE/B2G/upload-public/fota-*-update.mar $HOME/artifacts-public/
fi

ccache -s

