/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { RootBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/RootBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Addon: "chrome://remote/content/shared/Addon.sys.mjs",
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
});

/**
 * A WebExtension id.
 *
 * @typedef {string} Extension
 */

/**
 * Return value of the install command.
 *
 * @typedef InstallResult
 *
 * @property {Extension} extension
 */

/**
 * Enum of types supported by the webExtension.install command.
 *
 * @readonly
 * @enum {ExtensionDataType}
 */
export const ExtensionDataType = {
  Path: "path",
  ArchivePath: "archivePath",
  Base64: "base64",
};

/**
 * Used as an argument for webExtension.install command
 * to represent a WebExtension archive.
 *
 * @typedef ExtensionArchivePath
 *
 * @property {ExtensionDataType} [type=ExtensionDataType.ArchivePath]
 * @property {string} path
 */

/**
 * Used as an argument for webExtension.install command
 * to represent an unpacked WebExtension.
 *
 * @typedef ExtensionPath
 *
 * @property {ExtensionDataType} [type=ExtensionDataType.Path]
 * @property {string} path
 */

/**
 * Used as an argument for webExtension.install command
 * to represent a WebExtension archive encoded as base64 string.
 *
 * @typedef ExtensionBase64
 *
 * @property {ExtensionDataType} [type=ExtensionDataType.Base64]
 * @property {string} value
 */

class WebExtensionModule extends RootBiDiModule {
  constructor(messageHandler) {
    super(messageHandler);
  }

  destroy() {}

  /**
   * Installs a WebExtension.
   *
   * @see https://w3c.github.io/webdriver-bidi/#command-webExtension-install
   *
   * @param {object=} options
   * @param {ExtensionArchivePath|ExtensionPath|ExtensionBase64} options.extensionData
   *     The WebExtension to be installed.
   * @param {boolean=} options.moz_permanent (moz:permanent)
   *     If true, install the web extension permanently. Defaults to `false`.
   *
   * @returns {InstallResult}
   *     The id of the installed WebExtension.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {InvalidWebExtensionError}
   *     Tried to install an invalid WebExtension.
   */
  async install(options = {}) {
    const { extensionData, "moz:permanent": permanent = false } = options;

    lazy.assert.object(
      extensionData,
      `Expected "extensionData" to be an object, ` +
        lazy.pprint`got ${extensionData}`
    );

    const { path, type, value } = extensionData;
    const extensionDataTypes = Object.values(ExtensionDataType);

    lazy.assert.that(
      extensionDataType => extensionDataTypes.includes(extensionDataType),
      `Expected "extensionData.type" to be one of ${extensionDataTypes}, ` +
        lazy.pprint`got ${type}`
    )(type);

    lazy.assert.boolean(
      permanent,
      lazy.pprint`Expected "moz:permanent" to be a boolean, got ${permanent}`
    );

    let extensionId;

    switch (type) {
      case ExtensionDataType.Base64:
        lazy.assert.string(
          value,
          lazy.pprint`Expected "extensionData.value" to be a string, got ${value}`
        );

        extensionId = await lazy.Addon.installWithBase64(
          value,
          !permanent,
          false
        );
        break;
      case ExtensionDataType.ArchivePath:
      case ExtensionDataType.Path:
        lazy.assert.string(
          path,
          lazy.pprint`Expected "extensionData.path" to be a string, got ${path}`
        );

        if (permanent && type == ExtensionDataType.Path) {
          throw new lazy.error.InvalidWebExtensionError(
            "Permanent installation of unpacked extensions is not supported"
          );
        }

        extensionId = await lazy.Addon.installWithPath(path, !permanent, false);
    }

    return {
      extension: extensionId,
    };
  }

  /**
   * Uninstalls a WebExtension.
   *
   * @see https://w3c.github.io/webdriver-bidi/#command-webExtension-uninstall
   *
   * @param {object=} options
   * @param {Extension} options.extension
   *    The id of the WebExtension to be uninstalled.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchWebExtensionError}
   *     Raised if the WebExtension with provided id could not be found.
   * @throws {UnknownError}
   *     Raised if the WebExtension cannot be uninstalled.
   */
  async uninstall(options = {}) {
    const { extension: addonId } = options;

    lazy.assert.string(
      addonId,
      lazy.pprint`Expected "extension" to be a string, got ${addonId}`
    );

    if (addonId === "") {
      throw new lazy.error.NoSuchWebExtensionError(
        `Expected "extension" to be a non-empty string, got ${addonId}`
      );
    }

    await lazy.Addon.uninstall(addonId);
  }
}

export const webExtension = WebExtensionModule;
