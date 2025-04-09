/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
});

const $ = document.querySelector.bind(document);

/**
 * This returns the server endpoint to upload a profile.
 * The pref "toolkit.aboutlogging.uploadProfileUrl" can change it in case it is
 * needed. It is commented out in modules/libpref/init/all.js so that users
 * don't see it, because this is mostly only useful for tests.
 * @returns {string}
 */
function uploadProfileUrl() {
  return Services.prefs.getStringPref(
    "toolkit.aboutlogging.uploadProfileUrl",
    "https://api.profiler.firefox.com/compressed-store"
  );
}

/**
 * A handy function to return a string version of a date and time looking like
 * YYYYMMDDHHMMSS, that can be used in a file name.
 * @param {Date} date
 * @reurns {string}
 */
function getStringifiedDateAndTime(date) {
  const pad = val => String(val).padStart(2, "0");
  return (
    "" +
    date.getUTCFullYear() +
    pad(date.getUTCMonth() + 1) +
    pad(date.getUTCDate()) +
    pad(date.getUTCHours()) +
    pad(date.getUTCMinutes()) +
    pad(date.getUTCSeconds())
  );
}

// This class handles all behavior about profile data handling, either saving it
// or uploading it.
export class ProfileSaveOrUploadDialog {
  // The form container containing all the other elements.
  #question = $("#upload-question-form");
  #uploadButton = $("#upload-button");
  #saveButton = $("#save-button");
  // The container for the message displayed after uploading.
  #uploadedMessageContainer = $("#uploaded-message-container");
  #uploadedMessageText = $("#uploaded-message-text");
  // This element is replaced by fluent, therefore we have to get a fresh
  // version every time we want to use it.
  get #uploadedUrl() {
    return $("#uploaded-message-url");
  }
  // The button to share the URL using the Web Share API. Note that the Web
  // Share API isn't available on Desktop. Still there's no specific code to
  // prevent the icon from being displayed when the API is missing, because all
  // this behavior normally doesn't happen on Desktop, unless the preference is
  // toggled.
  #uploadedUrlShareButton = $("#uploaded-url-share-button");
  // The container for the progress bad while uploading.
  #uploadingProgressContainer = $("#uploading-progress-container");
  #uploadingProgressText = $("#uploading-progress-text");
  #uploadingProgressElement = $("#uploading-progress-element");
  // The message after saving a profile to a file.
  #savedMessage = $("#saved-message");
  #errorElement = $("#save-upload-error");
  /**
   * The gzipped profile data
   * @type {Uint8Array | null}
   */
  #profileData = null;
  /**
   * This controls which elements are displayed. See #updateVisibilities.
   * @type {"idle" | "uploading" | "uploaded" | "saved" | "error"}
   */
  #state = "idle";

  constructor() {
    this.#saveButton.addEventListener("click", this.#saveProfileLocally);
    this.#uploadButton.addEventListener("click", this.#uploadProfileData);
    this.#uploadedUrlShareButton.addEventListener(
      "click",
      this.#shareUploadedUrl
    );
  }

  /**
   * This is called to start the upload or save process.
   * @param {Uint8Array} profileData
   */
  init(profileData) {
    this.#profileData = profileData;
    this.#question.hidden = false;
    this.#setState("idle");
  }

  reset() {
    this.#question.hidden = true;
    this.#profileData = null;
    this.#setState("idle");
  }

  /**
   * Set the state and update the visibility of the various elements.
   * @param {"idle" | "uploading" | "uploaded" | "saved" | "error"} state
   */
  #setState(state) {
    this.#state = state;
    this.#updateVisibilities();
  }

  /**
   * This updates the visibilities of various elements, by using the #state
   * variable. This is the only function controlling the display of the elements
   * inside the form.
   */
  #updateVisibilities() {
    [
      this.#uploadedMessageContainer,
      this.#uploadingProgressContainer,
      this.#savedMessage,
      this.#errorElement,
    ].forEach(elt => (elt.hidden = true));
    switch (this.#state) {
      case "idle":
        // Nothing to do
        break;
      case "uploading":
        this.#uploadingProgressContainer.hidden = false;
        this.#setProgressValue(0);
        break;
      case "uploaded":
        this.#uploadedMessageContainer.hidden = false;
        break;
      case "saved":
        this.#savedMessage.hidden = false;
        break;
      case "error":
        this.#errorElement.hidden = false;
        break;
      default:
        throw new Error(`Unknown state "${this.#state}"`);
    }
  }

  /**
   * This actually does the action of uploading the profile data to the server.
   * The progress is reported with the callback onProgress. The parameter to
   * this function is a number between 0 and 1.
   *
   * @param {Object} options
   * @param {(progress: number) => unknown} options.onProgress
   * @returns {Promise<string>} The JWT token returned by the server
   */
  #doUploadProfileData({ onProgress }) {
    // The content of this function is heavily inspired by the code in
    // https://github.com/firefox-devtools/profiler/blob/5e9182064f18954cfd04b9da276d322a57f93406/src/profile-logic/profile-store.js
    const xhr = new XMLHttpRequest();
    return new Promise((resolve, reject) => {
      xhr.onload = () => {
        switch (xhr.status) {
          case 413:
            reject(
              new Error(
                "The profile size is too large. You can try enabling some of the privacy features to trim its size down."
              )
            );
            break;
          default:
            if (xhr.status >= 200 && xhr.status <= 299) {
              // Success!
              resolve(xhr.responseText);
            } else {
              reject(
                new Error(
                  `xhr onload with status != 200, xhr.statusText: ${xhr.statusText}`
                )
              );
            }
        }
      };

      xhr.onerror = () => {
        console.error(
          "There was an XHR network error in uploadBinaryProfileData()",
          xhr
        );

        let errorMessage =
          "Unable to make a connection to publish the profile.";
        if (xhr.statusText) {
          errorMessage += ` The error response was: ${xhr.statusText}`;
        }
        reject(new Error(errorMessage));
      };

      xhr.upload.onprogress = e => {
        if (onProgress && e.lengthComputable) {
          onProgress(e.loaded / e.total);
        }
      };

      xhr.open("POST", uploadProfileUrl());
      xhr.setRequestHeader(
        "Accept",
        "application/vnd.firefox-profiler+json;version=1.0"
      );
      xhr.send(this.#profileData);
    });
  }

  /**
   * This functions sets the text and the value to the progress element while
   * uploading a profile.
   * @param {number} val A number between 0 and 1.
   */
  #setProgressValue(val) {
    document.l10n.setArgs(this.#uploadingProgressText, { percent: val });
    this.#uploadingProgressElement.value = val;
  }

  /**
   * This saves the profile to a local file, in the preferred downloads
   * directory.
   * @returns {Promise<void>}
   */
  #saveProfileLocally = async () => {
    try {
      const dirPath = await lazy.Downloads.getPreferredDownloadsDirectory();
      const filePath = await IOUtils.createUniqueFile(
        dirPath,
        "profile-" + getStringifiedDateAndTime(new Date()) + ".json.gz"
      );
      await IOUtils.write(filePath, this.#profileData);
      document.l10n.setArgs(this.#savedMessage, {
        path: filePath,
      });
      this.#setState("saved");
    } catch (e) {
      console.error("Error while saving", e);
      this.#setState("error");
      document.l10n.setAttributes(
        this.#errorElement,
        "about-logging-save-error",
        { errorText: String(e) }
      );
    }
  };

  /**
   * This uploads the profile, handling all the messages displayed to the user.
   * @returns {Promise<void>}
   */
  #uploadProfileData = async () => {
    if (this.#state === "uploading") {
      return;
    }
    try {
      this.#setState("uploading");
      const uploadResult = await this.#doUploadProfileData({
        onProgress: val => {
          this.#setProgressValue(val);
        },
      });

      const { extractProfileTokenFromJwt } = await import(
        "chrome://global/content/aboutLogging/jwt.mjs"
      );
      const hash = extractProfileTokenFromJwt(uploadResult);
      const profileUrl = "https://profiler.firefox.com/public/" + hash;
      document.l10n.setArgs(this.#uploadedMessageText, {
        url: profileUrl,
      });
      this.#uploadedUrl.href = profileUrl;
      this.#setState("uploaded");
    } catch (e) {
      console.error("Error while uploading", e);
      this.#setState("error");
      document.l10n.setAttributes(
        this.#errorElement,
        "about-logging-upload-error",
        { errorText: String(e) }
      );
    }
  };

  /**
   * This uses the Web Share API to share the URL using various ways.
   * Note that this doesn't work on Desktop (Bug 1958347).
   * @returns {Promise<void>}
   */
  #shareUploadedUrl = async () => {
    const url = this.#uploadedUrl.href;
    await navigator.share({ url });
  };
}
