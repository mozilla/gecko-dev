/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(
      "#trackable-player-element > article"
    );

    if (container) {
      const callback = () => {
        let text = container.querySelector(
          `:scope > div > div > div > div[role="cue"]`
        )?.innerText;
        if (!text) {
          updateCaptionsFunction("");
          return;
        }

        updateCaptionsFunction(text);
      };

      callback();

      this.captionsObserver = new MutationObserver(callback);

      this.captionsObserver.observe(container, {
        childList: true,
      });
    }
  }

  removeCaptionContainerObserver() {
    this.captionsObserver?.disconnect();
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
