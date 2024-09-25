#!/bin/bash

TASK_ID=${1}
THIS="$(dirname "$0")"
THERE=${2}

if [ -z "${TASK_ID}" ]; then
    echo "Please provide a task ID"
    exit 1
fi

TASKCLUSTER_API_ROOT="https://firefox-ci-tc.services.mozilla.com"
ARTIFACTS="${TASKCLUSTER_API_ROOT}/api/queue/v1/task/${TASK_ID}/artifacts"

for reference in $(curl "${ARTIFACTS}" | jq -r '.artifacts | . [] | select(.name | contains("public/build/new_")) | .name');
do
    name="$(basename "${reference}")"
    final_name=${name//new_/}
    target_name=$(find "${THIS}" -type f -name "${final_name}")
    if [ -n "${THERE}" ]; then
        target_name=$(echo "${target_name}" | sed -e "s/\.png$/-${THERE}.png/g")
    fi
    url="${TASKCLUSTER_API_ROOT}/api/queue/v1/task/${TASK_ID}/artifacts/${reference}"
    echo "$url => $target_name"
    curl -SL "${url}" -o "${target_name}"
done;
