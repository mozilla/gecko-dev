/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  isLive() {
    return document.getElementById("video-player").duration == 0x40000000;
  }

  getDuration(video) {
    if (this.isLive(video)) {
      return Infinity;
    }
    return video.duration;
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
