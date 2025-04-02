/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor() {
    this.player = window.wrappedJSObject.playerObject;
  }

  play(video) {
    let playPauseButton = document.querySelector(
      `[data-player-hook="btnplaypause"]`
    );
    if (video.paused) {
      playPauseButton?.click();
    }
  }

  pause(video) {
    let playPauseButton = document.querySelector(
      `[data-player-hook="btnplaypause"]`
    );
    if (!video.paused) {
      playPauseButton?.click();
    }
  }

  setCurrentTime(video, position) {
    this.player.seek(position);
  }

  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(`[data-player-hook="plgcontainer"]`);

    const selector = [
      `[data-player-hook="subtitleelem"]`,
      ".subtitle_text",
    ].join(",");

    if (container) {
      updateCaptionsFunction("");
      const callback = mutationList => {
        if (mutationList) {
          let changed = false;
          for (const mutation of mutationList) {
            if (mutation.target.matches?.(selector)) {
              changed = true;
              break;
            }
          }

          if (!changed) {
            return;
          }
        }

        let subtitles = container.querySelectorAll(
          `.iqp-subtitle:has(> [data-player-hook="subtitleelem"] > .subtitle_text)`
        );
        if (!subtitles.length) {
          updateCaptionsFunction("");
          return;
        }

        if (subtitles.length > 1) {
          subtitles = Array.from(subtitles).sort(
            (a, b) => a.offsetTop - b.offsetTop
          );
        }

        updateCaptionsFunction(
          Array.from(subtitles, x => x.innerText.trim())
            .filter(String)
            .join("\n")
        );
      };

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
