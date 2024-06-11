#!/usr/bin/bash

for i in $(seq 1 8); do
  ffmpeg -f lavfi -i anullsrc=r=100:cl=mono \
         -c:a libopus \
         -t 0.0$i -y "audio_${i}0ms.opus"
  ffmpeg -f lavfi -i anullsrc=r=100:cl=mono \
         -f lavfi -i color=black:size=2x2:r=100 \
         -c:a libopus -c:v libvpx \
         -t 0.0$i -y "video_${i}0ms.webm"
done
