/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { Region, ViewDimensions } from "./avatarSelectionHelpers.mjs";

const VIEWS = {
  ICON: "icon",
  CUSTOM: "custom",
  CROP: "crop",
};

const STATES = {
  SELECTED: "selected",
  RESIZING: "resizing",
};

/**
 * Element used for displaying an avatar on the about:editprofile and about:newprofile pages.
 */
export class ProfileAvatarSelector extends MozLitElement {
  #moverId = "";

  static properties = {
    value: { type: String },
    view: { type: String },
  };

  static queries = {
    input: "#custom-image-input",
    saveButton: "#save-button",
    customAvatarCropArea: ".custom-avatar-crop-area",
    customAvatarImage: "#custom-avatar-image",
    avatarSelectionContainer: "#avatar-selection-container",
    highlight: "#highlight",
    iconTabButton: "#icon",
    customTabButton: "#custom",
    topLeftMover: "#mover-topLeft",
    topRightMover: "#mover-topRight",
    bottomLeftMover: "#mover-bottomLeft",
    bottomRightMover: "#mover-bottomRight",
  };

  constructor() {
    super();

    this.setView(VIEWS.ICON);
    this.viewDimensions = new ViewDimensions();
    this.avatarRegion = new Region(this.viewDimensions);

    this.state = STATES.SELECTED;
  }

  setView(newView) {
    if (this.view === VIEWS.CROP) {
      this.cropViewEnd();
    }

    switch (newView) {
      case VIEWS.ICON:
        this.view = VIEWS.ICON;
        break;
      case VIEWS.CUSTOM:
        this.view = VIEWS.CUSTOM;
        break;
      case VIEWS.CROP:
        this.view = VIEWS.CROP;
        this.cropViewStart();
        break;
    }
  }

  cropViewStart() {
    window.addEventListener("pointerdown", this);
    window.addEventListener("pointermove", this);
    window.addEventListener("pointerup", this);
    document.documentElement.classList.add("disable-text-selection");
  }

  cropViewEnd() {
    window.removeEventListener("pointerdown", this);
    window.removeEventListener("pointermove", this);
    window.removeEventListener("pointerup", this);
    document.documentElement.classList.remove("disable-text-selection");
  }

  getAvatarL10nId(value) {
    switch (value) {
      case "book":
        return "book-avatar";
      case "briefcase":
        return "briefcase-avatar";
      case "flower":
        return "flower-avatar";
      case "heart":
        return "heart-avatar";
      case "shopping":
        return "shopping-avatar";
      case "star":
        return "star-avatar";
    }

    return "";
  }

  handleTabClick(event) {
    event.stopImmediatePropagation();
    if (event.target.id === "icon") {
      this.setView(VIEWS.ICON);
    } else {
      this.setView(VIEWS.CUSTOM);
    }
  }

  iconTabContentTemplate() {
    let avatars = [
      "star",
      "flower",
      "briefcase",
      "heart",
      "book",
      "shopping",
      "present",
      "plane",
      "barbell",
      "bike",
      "craft",
      "diamond",
      "hammer",
      "heart-rate",
      "leaf",
      "makeup",
      "palette",
      "musical-note",
      "paw-print",
      "sparkle-single",
      "soccer",
      "video-game-controller",
      "default-favicon",
      "canvas",
      "history",
      "folder",
      "message",
      "lightbulb",
    ];

    // TODO: Bug 1966951 should remove the line below.
    // The browser_custom_avatar_test.js test will crash because the icon
    // files don't exist.
    avatars = avatars.slice(0, 6);

    return html`<moz-visual-picker
      value=${this.avatar}
      name="avatar"
      id="avatars"
      @change=${this.handleAvatarChange}
      >${avatars.map(
        avatar =>
          html`<moz-visual-picker-item
            l10nId=${this.getAvatarL10nId(avatar)}
            value=${avatar}
            ><moz-button
              class="avatar-button"
              type="ghost"
              iconSrc="chrome://browser/content/profiles/assets/16_${avatar}.svg"
              tabindex="-1"
            ></moz-button
          ></moz-visual-picker-item>`
      )}</moz-visual-picker
    >`;
  }

  customTabUploadFileContentTemplate() {
    return html`<div
        class="custom-avatar-add-image-header"
        data-l10n-id="avatar-selector-add-image"
      ></div>
      <div class="custom-avatar-upload-area">
        <input
          @change=${this.handleFileUpload}
          id="custom-image-input"
          type="file"
          accept="image/*"
          label="Upload a file"
        />
        <div id="file-messages">
          <img src="chrome://browser/skin/open.svg" />
          <span
            id="upload-text"
            data-l10n-id="avatar-selector-upload-file"
          ></span>
          <span id="drag-text" data-l10n-id="avatar-selector-drag-file"></span>
        </div>
      </div>`;
  }

  customTabViewImageTemplate() {
    return html`<div class="custom-avatar-crop-header">
        <moz-button
          id="back-button"
          @click=${this.handleCancelClick}
          type="icon ghost"
          iconSrc="chrome://global/skin/icons/arrow-left.svg"
        ></moz-button>
        <span data-l10n-id="avatar-selector-crop"></span>
        <div id="spacer"></div>
      </div>
      <div class="custom-avatar-crop-area">
        <div id="avatar-selection-container">
          <div id="highlight" class="highlight" tabindex="0">
            <div id="highlight-background"></div>
            <div
              id="mover-topLeft"
              class="mover-target direction-topLeft"
              tabindex="0"
            >
              <div class="mover"></div>
            </div>

            <div
              id="mover-topRight"
              class="mover-target direction-topRight"
              tabindex="0"
            >
              <div class="mover"></div>
            </div>

            <div
              id="mover-bottomRight"
              class="mover-target direction-bottomRight"
              tabindex="0"
            >
              <div class="mover"></div>
            </div>

            <div
              id="mover-bottomLeft"
              class="mover-target direction-bottomLeft"
              tabindex="0"
            >
              <div class="mover"></div>
            </div>
          </div>
        </div>
        <img
          id="custom-avatar-image"
          src=${this.blobURL}
          @load=${this.imageLoaded}
        />
      </div>
      <moz-button-group class="custom-avatar-actions"
        ><moz-button
          @click=${this.handleCancelClick}
          data-l10n-id="avatar-selector-cancel-button"
        ></moz-button
        ><moz-button
          type="primary"
          id="save-button"
          @click=${this.handleSaveClick}
          data-l10n-id="avatar-selector-save-button"
        ></moz-button
      ></moz-button-group>`;
  }

  handleCancelClick(event) {
    event.stopImmediatePropagation();

    this.setView(VIEWS.CUSTOM);
    if (this.blobURL) {
      URL.revokeObjectURL(this.blobURL);
    }
    this.file = null;
  }

  async handleSaveClick(event) {
    event.stopImmediatePropagation();

    const img = new Image();
    img.src = this.blobURL;
    await img.decode();

    const { width: imageWidth, height: imageHeight } = img;
    const scale =
      imageWidth <= imageHeight
        ? imageWidth / this.customAvatarCropArea.clientWidth
        : imageHeight / this.customAvatarCropArea.clientHeight;

    // eslint-disable-next-line no-shadow
    const { left, top, radius } = this.avatarRegion.dimensions;
    // eslint-disable-next-line no-shadow
    const { devicePixelRatio } = this.viewDimensions.dimensions;
    const { scrollTop, scrollLeft } = this.customAvatarCropArea;

    // Create the canvas so it is a square around the selected area.
    const scaledRadius = Math.round(radius * scale * devicePixelRatio);
    const squareSize = scaledRadius * 2;
    const squareCanvas = new OffscreenCanvas(squareSize, squareSize);
    const squareCtx = squareCanvas.getContext("2d");

    // Crop the canvas so it is a circle.
    squareCtx.beginPath();
    squareCtx.arc(scaledRadius, scaledRadius, scaledRadius, 0, Math.PI * 2);
    squareCtx.clip();

    const sourceX = Math.round((left + scrollLeft) * scale);
    const sourceY = Math.round((top + scrollTop) * scale);
    const sourceWidth = Math.round(radius * 2 * scale);

    // Draw the image onto the canvas.
    squareCtx.drawImage(
      img,
      sourceX,
      sourceY,
      sourceWidth,
      sourceWidth,
      0,
      0,
      squareSize,
      squareSize
    );

    const blob = await squareCanvas.convertToBlob({ type: "image/png" });
    const circularFile = new File([blob], this.file.name, {
      type: "image/png",
    });

    document.dispatchEvent(
      new CustomEvent("Profiles:CustomAvatarUpload", {
        detail: { file: circularFile },
      })
    );

    if (this.blobURL) {
      URL.revokeObjectURL(this.blobURL);
    }

    this.setView(VIEWS.CUSTOM);
    this.hidden = true;
  }

  updateViewDimensions() {
    let { width, height } = this.customAvatarImage;

    this.viewDimensions.dimensions = {
      width: this.customAvatarCropArea.clientWidth,
      height: this.customAvatarCropArea.clientHeight,
      devicePixelRatio: window.devicePixelRatio,
    };

    if (width > height) {
      this.customAvatarImage.classList.add("height-full");
    } else {
      this.customAvatarImage.classList.add("width-full");
    }
  }

  imageLoaded() {
    this.updateViewDimensions();
    this.setInitialAvatarSelection();
  }

  setInitialAvatarSelection() {
    let diameter = Math.min(
      this.viewDimensions.width,
      this.viewDimensions.height
    );

    let left =
      Math.floor(this.viewDimensions.width / 2) - Math.floor(diameter / 2);
    // eslint-disable-next-line no-shadow
    let top =
      Math.floor(this.viewDimensions.height / 2) - Math.floor(diameter / 2);

    let right = left + diameter;
    let bottom = top + diameter;

    this.avatarRegion.resizeToSquare({ left, top, right, bottom });

    this.drawSelectionContainer();
  }

  drawSelectionContainer() {
    // eslint-disable-next-line no-shadow
    let { top, left, width, height } = this.avatarRegion.dimensions;

    this.highlight.style = `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`;
  }

  getCoordinatesFromEvent(event) {
    let { clientX, clientY, movementX, movementY } = event;
    let rect = this.avatarSelectionContainer.getBoundingClientRect();

    return { x: clientX - rect.x, y: clientY - rect.y, movementX, movementY };
  }

  handleEvent(event) {
    if (this.view !== VIEWS.CROP) {
      return;
    }

    switch (event.type) {
      case "pointerdown":
        this.handlePointerDown(event);
        break;
      case "pointermove":
        this.handlePointerMove(event);
        break;
      case "pointerup":
        this.handlePointerUp(event);
        break;
    }
  }

  handlePointerDown(event) {
    let targetId = event.originalTarget?.id;
    if (
      [
        "highlight",
        "mover-topLeft",
        "mover-topRight",
        "mover-bottomRight",
        "mover-bottomLeft",
      ].includes(targetId)
    ) {
      this.state = STATES.RESIZING;
      this.#moverId = targetId;
    }
  }

  handlePointerMove(event) {
    if (this.state === STATES.RESIZING) {
      let { x, y, movementX, movementY } = this.getCoordinatesFromEvent(event);
      this.handleResizingPointerMove(x, y, movementX, movementY);
    }
  }

  handleResizingPointerMove(x, y, movementX, movementY) {
    switch (this.#moverId) {
      case "highlight": {
        this.avatarRegion.resizeToSquare(
          {
            left: this.avatarRegion.left + movementX,
            top: this.avatarRegion.top + movementY,
            right: this.avatarRegion.right + movementX,
            bottom: this.avatarRegion.bottom + movementY,
          },
          this.#moverId
        );
        break;
      }
      case "mover-topLeft": {
        this.avatarRegion.resizeToSquare(
          {
            left: x,
            top: y,
          },
          this.#moverId
        );
        break;
      }
      case "mover-topRight": {
        this.avatarRegion.resizeToSquare(
          {
            top: y,
            right: x,
          },
          this.#moverId
        );
        break;
      }
      case "mover-bottomRight": {
        this.avatarRegion.resizeToSquare(
          {
            right: x,
            bottom: y,
          },
          this.#moverId
        );
        break;
      }
      case "mover-bottomLeft": {
        this.avatarRegion.resizeToSquare(
          {
            left: x,
            bottom: y,
          },
          this.#moverId
        );
        break;
      }
    }

    this.drawSelectionContainer();
  }

  handlePointerUp() {
    this.state = STATES.SELECTED;
    this.#moverId = "";
    this.avatarRegion.sortCoords();
  }

  handleFileUpload(event) {
    const [file] = event.target.files;
    this.file = file;

    if (this.blobURL) {
      URL.revokeObjectURL(this.blobURL);
    }

    this.blobURL = URL.createObjectURL(file);
    this.setView(VIEWS.CROP);
  }

  contentTemplate() {
    switch (this.view) {
      case VIEWS.ICON: {
        return this.iconTabContentTemplate();
      }
      case VIEWS.CUSTOM: {
        return this.customTabUploadFileContentTemplate();
      }
      case VIEWS.CROP: {
        return this.customTabViewImageTemplate();
      }
    }
    return null;
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profile-avatar-selector.css"
      />
      <moz-card id="avatar-selector">
        <div id="content">
          <div class="button-group">
            <moz-button
              id="icon"
              type=${this.view === VIEWS.ICON ? "primary" : "default"}
              size="small"
              data-l10n-id="avatar-selector-icon-tab"
              @click=${this.handleTabClick}
            ></moz-button>
            <moz-button
              id="custom"
              type=${this.view === VIEWS.ICON ? "default" : "primary"}
              size="small"
              data-l10n-id="avatar-selector-custom-tab"
              @click=${this.handleTabClick}
            ></moz-button>
          </div>
          ${this.contentTemplate()}
        </div>
      </moz-card>`;
  }
}

customElements.define("profile-avatar-selector", ProfileAvatarSelector);
