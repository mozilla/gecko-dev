/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  play(video) {
    let container = video.parentNode;
    let playButton = container?.querySelector(".controls-play-pause-button");
    if (video.paused && playButton) {
      playButton?.click();
    }
  }

  pause(video) {
    let container = video.parentNode;
    let pauseButton = container?.querySelector(".controls-play-pause-button");
    if (!video.paused && pauseButton) {
      pauseButton?.click();
    }
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
