FROM $DOCKER_IMAGE_PARENT

RUN apt-get update && \
    apt-get install curl zstd

# %include taskcluster/scripts/misc/fetch-chromium.py
ADD topsrcdir/taskcluster/scripts/misc/fetch-chromium.py /builds/worker/bin/fetch-chromium.py

# %include taskcluster/scripts/misc/fetch-cft-chromedriver.py
ADD topsrcdir/taskcluster/scripts/misc/fetch-cft-chromedriver.py /builds/worker/bin/fetch-cft-chromedriver.py

# %include taskcluster/scripts/misc/fetch-onnxruntime-deps.sh
ADD topsrcdir/taskcluster/scripts/misc/fetch-onnxruntime-deps.sh /builds/worker/bin/fetch-onnxruntime-deps.sh
