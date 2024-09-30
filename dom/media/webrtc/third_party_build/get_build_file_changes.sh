#!/bin/bash

# Both loop-ff.sh and elm_rebase.sh check for modified build (related)
# files - in other words, files that when modified might produce a
# change in the generated moz.build files.  This script is to ensure
# that both are using the same set of files

hg status --change . \
    --include 'third_party/libwebrtc/**BUILD.gn' \
    --include 'third_party/libwebrtc/**/*.gni' \
    --include 'third_party/libwebrtc/.gn' \
    --include 'dom/media/webrtc/third_party_build/gn-configs/webrtc.json'
