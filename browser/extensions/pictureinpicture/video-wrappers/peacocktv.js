/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This wrapper is used for:
 * Peacock TV
 * Now TV
 * Showmax
 * Sky Showtime
 */
class PictureInPictureVideoWrapper {
  constructor(video) {
    this.video = video.wrappedJSObject;
  }

  hasCvsdkSession() {
    return (
      this.video.cvsdkSession !== undefined && this.video.cvsdkSession !== null
    );
  }

  play() {
    if (!this.hasCvsdkSession()) {
      this.video.play();
      return;
    }
    this.video.cvsdkSession.play();
  }

  pause() {
    if (!this.hasCvsdkSession()) {
      this.video.pause();
      return;
    }
    this.video.cvsdkSession.pause();
  }

  setCurrentTime(video, position, wasPlaying) {
    if (!this.hasCvsdkSession()) {
      this.video.currentTime = position;
      return;
    }
    this.video.cvsdkSession.seek(position, wasPlaying);
  }

  /**
   * In the absence of a cvsdkSession, try to derive caption information
   * from the parent video element's DOM.
   */
  setDefaultCaptionMutationObserver(video, updateCaptionsFunction) {
    /*
     * Actual subtitles DOM element may be not present initially or
     * change during playback
     * So no need to check if it exists before setting the observer
     * Assume some sort of captions may be available
     *
     * Actual attributes on the subtitles element may vary with the build
     * and environment, so try a couple different selectors.
     */
    const captionsQuery = "[data-t-subtitles=true], [data-t=subtitles]";
    const getContainer = () => {
      return video.parentElement.querySelector(captionsQuery);
    };

    updateCaptionsFunction("");

    this.captionsObserver = new MutationObserver(() => {
      let text = getContainer().innerText;
      updateCaptionsFunction(text);
    });

    this.captionsObserver.observe(video.parentElement, {
      attributes: false,
      childList: true,
      subtree: true,
    });
  }

  setCaptionContainerObserver(video, updateCaptionsFunction) {
    if (!this.hasCvsdkSession()) {
      this.setDefaultCaptionMutationObserver(video, updateCaptionsFunction);
      return;
    }
    this.video.cvsdkSession.setSimpleCueHandler(updateCaptionsFunction);
  }

  getDuration() {
    if (!this.hasCvsdkSession()) {
      return this.video.duration;
    }
    return this.video.cvsdkSession.getDuration();
  }

  getCurrentTime() {
    if (!this.hasCvsdkSession()) {
      return this.video.currentTime;
    }
    return this.video.cvsdkSession.getCurrentTime();
  }

  setMuted(video, shouldMute) {
    if (!this.hasCvsdkSession()) {
      this.video.muted = shouldMute;
      return;
    }
    this.video.cvsdkSession.setMute(shouldMute);
  }

  getMuted() {
    if (!this.hasCvsdkSession()) {
      return this.video.muted;
    }
    return this.video.cvsdkSession.isMuted();
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
