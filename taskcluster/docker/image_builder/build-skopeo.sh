#!/bin/sh

set -ex

git clone --no-checkout --depth=1 --branch=v1.15.1 https://github.com/containers/skopeo .
git checkout e2ea426918973e5e007a5e1e2457a41ab336fc41
export GO111MODULE=on CGO_ENABLED=0

# Set unixTempDirForBigFiles so skopeo will extract in a directory hidden by kaniko
go build \
        -mod=vendor -o out/skopeo \
        -tags "exclude_graphdriver_devicemapper exclude_graphdriver_btrfs containers_image_openpgp" \
        -ldflags '-X github.com/containers/image/v5/internal/tmpdir.unixTempDirForBigFiles=/workspace/tmp -X github.com/containers/image/v5/signature.systemDefaultPolicyPath=/kaniko/containers/policy.json -extldflags "-static" -w -s' \
        ./cmd/skopeo
