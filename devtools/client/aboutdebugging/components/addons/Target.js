/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  debugLocalAddon,
  debugRemoteAddon,
  getExtensionUuid,
  isTemporaryID,
  parseFileUri,
  uninstallAddon,
} = require("../../modules/addon");
const Services = require("Services");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const TEMP_ID_URL = "https://developer.mozilla.org/Add-ons" +
                    "/WebExtensions/WebExtensions_and_the_Add-on_ID";
const LEGACY_WARNING_URL = "https://wiki.mozilla.org/Add-ons/Future_of_Bootstrap";

function filePathForTarget(target) {
  // Only show file system paths, and only for temporarily installed add-ons.
  if (!target.temporarilyInstalled || !target.url || !target.url.startsWith("file://")) {
    return [];
  }
  const path = parseFileUri(target.url);
  return [
    dom.dt(
      { className: "addon-target-info-label" },
      Strings.GetStringFromName("location")),
    // Wrap the file path in a span so we can do some RTL/LTR swapping to get
    // the ellipsis on the left.
    dom.dd(
      { className: "addon-target-info-content file-path" },
      dom.span({ className: "file-path-inner", title: path }, path),
    ),
  ];
}

function addonIDforTarget(target) {
  return [
    dom.dt(
      { className: "addon-target-info-label" },
      Strings.GetStringFromName("extensionID"),
    ),
    dom.dd(
      { className: "addon-target-info-content extension-id" },
      dom.span(
        { title: target.addonID },
        target.addonID
      )
    ),
  ];
}

function internalIDForTarget(target) {
  const uuid = getExtensionUuid(target);
  if (!uuid) {
    return [];
  }

  return [
    dom.dt(
      { className: "addon-target-info-label" },
      Strings.GetStringFromName("internalUUID"),
    ),
    dom.dd(
      { className: "addon-target-info-content internal-uuid" },
      dom.span(
        { title: uuid },
        uuid
      ),
      dom.span(
        { className: "addon-target-info-more" },
        dom.a(
          { href: target.manifestURL, target: "_blank", className: "manifest-url" },
          Strings.GetStringFromName("manifestURL"),
        ),
      )
    ),
  ];
}

function showMessages(target) {
  const messages = [
    ...warningMessages(target),
    ...infoMessages(target),
  ];
  if (messages.length > 0) {
    return dom.ul(
      { className: "addon-target-messages" },
      ...messages);
  }
  return null;
}

function infoMessages(target) {
  const messages = [];
  if (isTemporaryID(target.addonID)) {
    messages.push(dom.li(
      { className: "addon-target-info-message addon-target-message" },
      Strings.GetStringFromName("temporaryID"),
      " ",
      dom.a({ href: TEMP_ID_URL, className: "temporary-id-url", target: "_blank" },
        Strings.GetStringFromName("temporaryID.learnMore")
      )));
  }

  return messages;
}

function warningMessages(target) {
  let messages = [];

  if (target.addonTargetFront.isLegacyTemporaryExtension()) {
    messages.push(dom.li(
      {
        className: "addon-target-warning-message addon-target-message",
      },
      Strings.GetStringFromName("legacyExtensionWarning"),
      " ",
      dom.a(
        {
          href: LEGACY_WARNING_URL,
          target: "_blank",
        },
        Strings.GetStringFromName("legacyExtensionWarning.learnMore"))
    ));
  }

  const warnings = target.warnings || [];
  messages = messages.concat(warnings.map((warning) => {
    return dom.li(
      { className: "addon-target-warning-message addon-target-message" },
      warning);
  }));

  return messages;
}

class AddonTarget extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      connect: PropTypes.object,
      debugDisabled: PropTypes.bool,
      target: PropTypes.shape({
        addonTargetFront: PropTypes.object.isRequired,
        addonID: PropTypes.string.isRequired,
        icon: PropTypes.string,
        name: PropTypes.string.isRequired,
        temporarilyInstalled: PropTypes.bool,
        url: PropTypes.string,
        warnings: PropTypes.array,
      }).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.debug = this.debug.bind(this);
    this.uninstall = this.uninstall.bind(this);
    this.reload = this.reload.bind(this);
  }

  debug() {
    const { client, connect, target } = this.props;

    if (connect.type === "REMOTE") {
      debugRemoteAddon(target.addonID, client);
    } else if (connect.type === "LOCAL") {
      debugLocalAddon(target.addonID);
    }
  }

  uninstall() {
    const { target } = this.props;
    uninstallAddon(target.addonID);
  }

  async reload() {
    const { target } = this.props;
    const { AboutDebugging } = window;
    try {
      await target.addonTargetFront.reload();
      AboutDebugging.emit("addon-reload");
    } catch (e) {
      throw new Error("Error reloading addon " + target.addonID + ": " + e.message);
    }
  }

  render() {
    const { target, debugDisabled } = this.props;

    return dom.li(
      { className: "card addon-target-container", "data-addon-id": target.addonID },
      dom.div({ className: "target-card-heading target" },
        dom.img({
          className: "target-icon addon-target-icon",
          role: "presentation",
          src: target.icon,
        }),
        dom.span(
          { className: "target-name addon-target-name", title: target.name },
          target.name)
      ),
      showMessages(target),
      dom.dl(
        { className: "addon-target-info" },
        ...filePathForTarget(target),
        ...addonIDforTarget(target),
        ...internalIDForTarget(target),
      ),
      dom.div({className: "target-card-actions"},
        dom.button({
          className: "target-card-action-link debug-button addon-target-button",
          onClick: this.debug,
          disabled: debugDisabled,
        }, Strings.GetStringFromName("debug")),
        target.temporarilyInstalled
          ? dom.button({
            className: "target-card-action-link reload-button addon-target-button",
            onClick: this.reload,
          }, Strings.GetStringFromName("reload"))
          : null,
        target.temporarilyInstalled
          ? dom.button({
            className: "target-card-action-link uninstall-button addon-target-button",
            onClick: this.uninstall,
          }, Strings.GetStringFromName("remove"))
          : null,
      ),
    );
  }
}

module.exports = AddonTarget;
