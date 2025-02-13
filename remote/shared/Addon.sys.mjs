/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",

  AppInfo: "chrome://remote/content/shared/AppInfo.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
});

// from https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager/AddonManager#AddonInstall_errors
const ERRORS = {
  [-1]: "ERROR_NETWORK_FAILURE: A network error occured.",
  [-2]: "ERROR_INCORRECT_HASH: The downloaded file did not match the expected hash.",
  [-3]: "ERROR_CORRUPT_FILE: The file appears to be corrupt.",
  [-4]: "ERROR_FILE_ACCESS: There was an error accessing the filesystem.",
  [-5]: "ERROR_SIGNEDSTATE_REQUIRED: The addon must be signed and isn't.",
  [-6]: "ERROR_UNEXPECTED_ADDON_TYPE: The downloaded add-on had a different type than expected (during an update).",
  [-7]: "ERROR_INCORRECT_ID: The addon did not have the expected ID (during an update).",
  [-8]: "ERROR_INVALID_DOMAIN: The addon install_origins does not list the 3rd party domain.",
  [-9]: "ERROR_UNEXPECTED_ADDON_VERSION: The downloaded add-on had a different version than expected (during an update).",
  [-10]: "ERROR_BLOCKLISTED: The add-on is blocklisted.",
  [-11]:
    "ERROR_INCOMPATIBLE: The add-on is incompatible (w.r.t. the compatibility range).",
  [-12]:
    "ERROR_UNSUPPORTED_ADDON_TYPE: The add-on type is not supported by the platform.",
};

async function installAddon(file, temporary, allowPrivateBrowsing) {
  let addon;
  try {
    if (temporary) {
      addon = await lazy.AddonManager.installTemporaryAddon(file);
    } else {
      const install = await lazy.AddonManager.getInstallForFile(file, null, {
        source: "internal",
      });

      if (install == null) {
        throw new lazy.error.UnknownError("Unknown error");
      }

      try {
        addon = await install.install();
      } catch {
        throw new lazy.error.UnknownError(ERRORS[install.error]);
      }
    }
  } catch (e) {
    throw new lazy.error.UnknownError(
      `Could not install add-on: ${e.message}`,
      e
    );
  }

  if (allowPrivateBrowsing) {
    const perms = {
      permissions: ["internal:privateBrowsingAllowed"],
      origins: [],
    };
    await lazy.ExtensionPermissions.add(addon.id, perms);
    await addon.reload();
  }

  return addon;
}

/** Installs addons by path and uninstalls by ID. */
export class Addon {
  /**
   * Install a Firefox addon with provided base64 string representation.
   *
   * Temporary addons will automatically be uninstalled on shutdown and
   * do not need to be signed, though they must be restartless.
   *
   * @param {string} base64
   *     Base64 string representation of the extension package archive.
   * @param {boolean=} temporary
   *     True to install the addon temporarily, false (default) otherwise.
   * @param {boolean=} allowPrivateBrowsing
   *     True to install the addon that is enabled in Private Browsing mode,
   *     false (default) otherwise.
   *
   * @returns {Promise.<string>}
   *     Addon ID.
   *
   * @throws {UnknownError}
   *     If there is a problem installing the addon.
   */
  static async installWithBase64(base64, temporary, allowPrivateBrowsing) {
    const decodedString = atob(base64);
    const fileContent = Uint8Array.from(decodedString, m => m.codePointAt(0));

    let path;
    try {
      path = PathUtils.join(
        PathUtils.profileDir,
        `addon-test-${lazy.generateUUID()}.xpi`
      );
      await IOUtils.write(path, fileContent);
    } catch (e) {
      throw new lazy.error.UnknownError(
        `Could not write add-on to file: ${e.message}`,
        e
      );
    }

    let addon;
    try {
      const file = new lazy.FileUtils.File(path);
      addon = await installAddon(file, temporary, allowPrivateBrowsing);
    } finally {
      await IOUtils.remove(path);
    }

    return addon.id;
  }

  /**
   * Install a Firefox addon with provided path.
   *
   * Temporary addons will automatically be uninstalled on shutdown and
   * do not need to be signed, though they must be restartless.
   *
   * @param {string} path
   *     Full path to the extension package archive.
   * @param {boolean=} temporary
   *     True to install the addon temporarily, false (default) otherwise.
   * @param {boolean=} allowPrivateBrowsing
   *     True to install the addon that is enabled in Private Browsing mode,
   *     false (default) otherwise.
   *
   * @returns {Promise.<string>}
   *     Addon ID.
   *
   * @throws {UnknownError}
   *     If there is a problem installing the addon.
   */
  static async installWithPath(path, temporary, allowPrivateBrowsing) {
    let file;

    // On Windows we can end up with a path with mixed \ and /
    // which doesn't work in Firefox.
    if (lazy.AppInfo.isWindows) {
      path = path.replace(/\//g, "\\");
    }

    try {
      file = new lazy.FileUtils.File(path);
    } catch (e) {
      throw new lazy.error.UnknownError(`Expected absolute path: ${e}`, e);
    }

    if (!file.exists()) {
      throw new lazy.error.UnknownError(`No such file or directory: ${path}`);
    }

    const addon = await installAddon(file, temporary, allowPrivateBrowsing);

    return addon.id;
  }

  /**
   * Uninstall a Firefox addon.
   *
   * If the addon is restartless it will be uninstalled right away.
   * Otherwise, Firefox must be restarted for the change to take effect.
   *
   * @param {string} id
   *     ID of the addon to uninstall.
   *
   * @returns {Promise}
   *
   * @throws {UnknownError}
   *     If there is a problem uninstalling the addon.
   */
  static async uninstall(id) {
    let candidate = await lazy.AddonManager.getAddonByID(id);
    if (candidate === null) {
      // `AddonManager.getAddonByID` never rejects but instead
      // returns `null` if the requested addon cannot be found.
      throw new lazy.error.UnknownError(`Addon ${id} is not installed`);
    }

    return new Promise(resolve => {
      let listener = {
        onOperationCancelled: addon => {
          if (addon.id === candidate.id) {
            lazy.AddonManager.removeAddonListener(listener);
            throw new lazy.error.UnknownError(
              `Uninstall of ${candidate.id} has been canceled`
            );
          }
        },

        onUninstalled: addon => {
          if (addon.id === candidate.id) {
            lazy.AddonManager.removeAddonListener(listener);
            resolve();
          }
        },
      };

      lazy.AddonManager.addAddonListener(listener);
      candidate.uninstall();
    });
  }
}
