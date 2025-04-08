/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  play(video) {
    let playButton = document.querySelector("video-ui .phx-play-btn");
    if (video.paused) {
      playButton?.click();
    }
  }

  pause(video) {
    let pauseButton = document.querySelector("video-ui .phx-pause-btn");
    if (!video.paused) {
      pauseButton?.click();
    }
  }

  setMuted(video, shouldMute) {
    let muteButton = document.querySelector("video-ui .phx-muted-btn");
    if (video.muted !== shouldMute) {
      muteButton?.click();
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
