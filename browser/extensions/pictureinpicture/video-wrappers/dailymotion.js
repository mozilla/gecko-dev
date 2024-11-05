/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = video.closest(".player");

    if (container) {
      updateCaptionsFunction("");
      const callback = (mutationList = []) => {
        if (mutationList.length) {
          let changed = false;
          for (const mutation of mutationList) {
            if (mutation.target.matches?.(".subtitles-text")) {
              changed = true;
              break;
            }
          }

          if (!changed) {
            return;
          }
        }

        let textNodeList = container
          .querySelector(".subtitles")
          ?.querySelectorAll("div");

        if (!textNodeList?.length) {
          updateCaptionsFunction("");
          return;
        }

        updateCaptionsFunction(
          Array.from(textNodeList, x => x.innerText).join("\n")
        );
      };

      // immediately invoke the callback function to add subtitles to the PiP window
      callback();

      this.captionsObserver = new MutationObserver(callback);

      this.captionsObserver.observe(container, {
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
