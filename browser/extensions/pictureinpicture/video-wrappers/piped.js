/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".player-container");

    if (container) {
      updateCaptionsFunction("");
      const callback = function () {
        let textNodeList = container
          .querySelector(".shaka-text-wrapper")
          ?.querySelectorAll('span[style="background-color: black;"]');
        if (!textNodeList) {
          updateCaptionsFunction("");
          return;
        }

        updateCaptionsFunction(
          Array.from(textNodeList, x => x.textContent).join("\n")
        );
      };

      // immediately invoke the callback function to add subtitles to the PiP window
      callback([1], null);

      this.captionsObserver = new MutationObserver(callback);

      this.captionsObserver.observe(container, {
        attributes: false,
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
