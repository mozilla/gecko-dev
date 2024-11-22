/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* globals browser */

class AltTextModal {
  constructor() {
    this.modalId = `alt-text-ml`;
    this.#initialize();
    browser.runtime.onMessage.addListener(this.onMessage.bind(this));
  }

  #initialize() {
    if (document.getElementById(this.modalId)) {
      return;
    }
    this.modal = this.#createModal();
    this.textContainer = this.#createTextContainer();
    this.closeButton = this.#createCloseButton();
    this.#appendElements();
  }

  onMessage(message, _sender, _sendResponse) {
    this.updateProgress(message);
  }

  close() {
    browser.runtime.onMessage.removeListener(this.onMessage);
  }

  #createModal() {
    modal = document.createElement("div");
    modal.id = this.modalId;
    modal.className = "ml-progress";
    document.body.appendChild(modal);
    return modal;
  }

  #createTextContainer() {
    const textContainer = document.createElement("div");
    textContainer.className = "ml-text-container";
    textContainer.textContent = "Initializing...";
    return textContainer;
  }

  #createCloseButton() {
    const closeButton = document.createElement("button");
    closeButton.className = "close-btn";
    closeButton.textContent = "Close";
    closeButton.addEventListener("click", () => {
      document.body.removeChild(this.modal);
    });
    return closeButton;
  }

  #appendElements() {
    this.modal.appendChild(this.textContainer);
    this.modal.appendChild(this.closeButton);
  }

  updateText(message) {
    this.#initialize();
    this.textContainer.textContent = message;
  }

  updateProgress(data) {
    this.#initialize();
    const dataId = data.id;

    const progressPercentage = Math.round(data.progress) || 100;
    let progressBarContainer = document.getElementById(dataId);

    // Create a new progress bar container if it doesn't exist
    if (!progressBarContainer) {
      progressBarContainer = document.createElement("div");
      progressBarContainer.id = dataId;
      progressBarContainer.className = "progress-bar-container";

      // Create the label
      const label = document.createElement("div");
      label.textContent = data.metadata.file;
      progressBarContainer.appendChild(label);

      // Create the progress bar
      const progressBar = document.createElement("div");
      progressBar.className = "progress-bar";

      const progressBarFill = document.createElement("div");
      progressBarFill.className = "progress-bar-fill";
      progressBarFill.style.width = `${progressPercentage}%`;
      progressBarFill.textContent = `${progressPercentage}%`;
      progressBar.appendChild(progressBarFill);
      progressBarContainer.appendChild(progressBar);

      // Add the progress bar container to the modal
      this.modal.appendChild(progressBarContainer);
    } else {
      // Update the existing progress bar
      const progressBarFill =
        progressBarContainer.querySelector(".progress-bar-fill");
      progressBarFill.style.width = `${progressPercentage}%`;
      progressBarFill.textContent = `${progressPercentage}%`;
    }

    // Remove the progress bar when it reaches 100% or more
    if (progressBarContainer && progressPercentage >= 100) {
      this.modal.removeChild(progressBarContainer);
    }
  }
}

let modal;

function getModal() {
  if (modal) {
    return modal;
  }
  modal = new AltTextModal();
  window.addEventListener("beforeunload", _event => {
    modal.close();
  });
  return modal;
}
