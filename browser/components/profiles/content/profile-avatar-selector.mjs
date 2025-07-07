/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for displaying an avatar on the about:editprofile and about:newprofile pages.
 */
export class ProfileAvatarSelector extends MozLitElement {
  static properties = {
    value: { type: String },
    state: { type: String },
  };

  static queries = {
    input: "#custom-image",
    saveButton: "#save-button",
    avatars: "#avatars",
  };

  constructor() {
    super();

    this.state = "icon";
  }

  getAvatarL10nId(value) {
    switch (value) {
      case "barbell":
        return "barbell-avatar";
      case "bike":
        return "bike-avatar";
      case "book":
        return "book-avatar";
      case "briefcase":
        return "briefcase-avatar";
      case "canvas":
        return "canvas-avatar";
      case "craft":
        return "craft-avatar";
      case "default-favicon":
        return "default-favicon-avatar";
      case "diamond":
        return "diamond-avatar";
      case "flower":
        return "flower-avatar";
      case "folder":
        return "folder-avatar";
      case "heart":
        return "heart-avatar";
      case "heart-rate":
        return "heart-rate-avatar";
      case "history":
        return "history-avatar";
      case "leaf":
        return "leaf-avatar";
      case "lightbulb":
        return "lightbulb-avatar";
      case "makeup":
        return "makeup-avatar";
      case "message":
        return "message-avatar";
      case "musical-note":
        return "musical-note-avatar";
      case "paw-print":
        return "paw-print-avatar";
      case "sparkle-single":
        return "sparkle-single-avatar";
      case "soccer":
        return "soccer-avatar";
      case "star":
        return "star-avatar";
      case "video-game-controller":
        return "video-game-controller-avatar";
      default:
        return "custom-avatar";
    }
  }

  handleAvatarChange() {
    const selectedAvatar = this.avatars.value;

    document.dispatchEvent(
      new CustomEvent("Profiles:AvatarSelected", {
        detail: { avatar: selectedAvatar },
      })
    );
  }

  handleTabClick(event, tabName) {
    event.stopImmediatePropagation();
    this.state = tabName;
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
    return html`<div class="custom-avatar-area">
      <input
        @change=${this.handleFileUpload}
        id="custom-image"
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

  handleCancelClick(event) {
    event.stopImmediatePropagation();

    this.state = "custom";
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

    const size = 512;
    const canvas = new OffscreenCanvas(size, size);
    canvas.width = canvas.height = size;
    const ctx = canvas.getContext("2d");

    ctx.beginPath();
    ctx.arc(size / 2, size / 2, size / 2, 0, Math.PI * 2);
    ctx.clip();

    const scale = size / Math.min(img.width, img.height);
    const x = (size - img.width * scale) / 2;
    const y = (size - img.height * scale) / 2;
    ctx.drawImage(img, x, y, img.width * scale, img.height * scale);

    const blob = await canvas.convertToBlob({ type: "image/png" });
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

    this.state = "custom";
    this.hidden = true;
  }

  customTabViewImageTemplate() {
    return html`<div class="custom-avatar-area">
        <img id="custom-avatar-image" src=${this.blobURL} />
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

  handleFileUpload(event) {
    const [file] = event.target.files;
    this.file = file;

    if (this.blobURL) {
      URL.revokeObjectURL(this.blobURL);
    }

    this.blobURL = URL.createObjectURL(file);
    this.state = "crop";
  }

  contentTemplate() {
    switch (this.state) {
      case "icon": {
        return this.iconTabContentTemplate();
      }
      case "custom": {
        return this.customTabUploadFileContentTemplate();
      }
      case "crop": {
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
        <div class="button-group">
          <moz-button
            type=${this.state === "icon" ? "primary" : "default"}
            size="small"
            data-l10n-id="avatar-selector-icon-tab"
            @click=${e => this.handleTabClick(e, "icon")}
          ></moz-button>
          <moz-button
            type=${this.state === "custom" ? "primary" : "default"}
            size="small"
            data-l10n-id="avatar-selector-custom-tab"
            @click=${e => this.handleTabClick(e, "custom")}
          ></moz-button>
        </div>
        ${this.contentTemplate()}
      </moz-card>`;
  }
}

customElements.define("profile-avatar-selector", ProfileAvatarSelector);
