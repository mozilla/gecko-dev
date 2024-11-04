/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  webExtensionDescriptorSpec,
} = require("resource://devtools/shared/specs/descriptors/webextension.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  DescriptorMixin,
} = require("resource://devtools/client/fronts/descriptors/descriptor-mixin.js");
const DESCRIPTOR_TYPES = require("resource://devtools/client/fronts/descriptors/descriptor-types.js");

class WebExtensionDescriptorFront extends DescriptorMixin(
  FrontClassWithSpec(webExtensionDescriptorSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.traits = {};
  }

  descriptorType = DESCRIPTOR_TYPES.EXTENSION;

  form(json) {
    this.actorID = json.actor;

    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this._form = json;
    this.traits = json.traits || {};
  }

  setTarget(targetFront) {
    this._targetFront = targetFront;
  }

  get backgroundScriptStatus() {
    return this._form.backgroundScriptStatus;
  }

  get debuggable() {
    return this._form.debuggable;
  }

  get hidden() {
    return this._form.hidden;
  }

  get iconDataURL() {
    return this._form.iconDataURL;
  }

  get iconURL() {
    return this._form.iconURL;
  }

  get id() {
    return this._form.id;
  }

  get isSystem() {
    return this._form.isSystem;
  }

  get isWebExtensionDescriptor() {
    return true;
  }

  get isWebExtension() {
    return this._form.isWebExtension;
  }

  get manifestURL() {
    return this._form.manifestURL;
  }

  get name() {
    return this._form.name;
  }

  get persistentBackgroundScript() {
    return this._form.persistentBackgroundScript;
  }

  get temporarilyInstalled() {
    return this._form.temporarilyInstalled;
  }

  get url() {
    return this._form.url;
  }

  get warnings() {
    return this._form.warnings;
  }

  isServerTargetSwitchingEnabled() {
    // @backward-compat { version 133 } Firefox 133 started supporting server targets by default.
    // Once this is the only supported version, we can remove the traits and consider this true,
    // but keep this method as some other descriptor still return false.
    // At least the browser toolbox doesn't support server target switching.
    return this.traits.isServerTargetSwitchingEnabled;
  }

  getWatcher() {
    return super.getWatcher({
      isServerTargetSwitchingEnabled: this.isServerTargetSwitchingEnabled(),
    });
  }

  /**
   * Retrieve the WindowGlobalTargetFront for the top level WindowGlobal
   * currently active related to the Web Extension.
   *
   * WebExtensionDescriptors will be created for any type of addon type
   * (webextension, search plugin, themes). Only webextensions can be targets.
   * This method will throw for other addon types.
   *
   * TODO: We should filter out non-webextension & non-debuggable addons on the
   * server to avoid the isWebExtension check here. See Bug 1644355.
   */
  async getTarget() {
    if (!this.isWebExtension) {
      throw new Error(
        "Tried to create a target for an addon which is not a webextension: " +
          this.actorID
      );
    }

    if (this._targetFront && !this._targetFront.isDestroyed()) {
      return this._targetFront;
    }

    throw new Error(
      "Missing webextension target actor front. TargetCommand did not notify it (yet?) to the descriptor"
    );
  }
}

exports.WebExtensionDescriptorFront = WebExtensionDescriptorFront;
registerFront(WebExtensionDescriptorFront);
