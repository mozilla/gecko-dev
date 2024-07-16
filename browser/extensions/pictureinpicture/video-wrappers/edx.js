/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".video-wrapper");

    if (container) {
      updateCaptionsFunction("");
      const callback = function () {
        let text = container.querySelector(
          ".closed-captions.is-visible"
        )?.innerText;
        updateCaptionsFunction(text);
      };

      callback([1], null);

      this.captionsObserver = new MutationObserver(callback);

      this.captionsObserver.observe(container, {
        attributes: true,
        childList: true,
        subtree: true,
      });
    }
  }

  removeCaptionContainerObserver() {
    this.captionsObserver?.disconnect();
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
