/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ShellService: "resource:///modules/ShellService.sys.mjs",
  TaskbarTabsUtils: "resource:///modules/taskbartabs/TaskbarTabsUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  Favicons: ["@mozilla.org/browser/favicon-service;1", "nsIFaviconService"],
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "TaskbarTabs",
    maxLogLevel: "Warn",
  });
});

/**
 * Provides functionality to pin and unping Taskbar Tabs.
 */
export const TaskbarTabsPin = {
  /**
   * Pins the provided Taskbar Tab to the taskbar.
   *
   * @param {TaskbarTab} aTaskbarTab - A Taskbar Tab to pin to the taskbar.
   * @returns {Promise} Resolves once finished.
   */
  async pinTaskbarTab(aTaskbarTab) {
    lazy.logConsole.info("Pinning Taskbar Tab to the taskbar.");

    let iconPath = await createTaskbarIconFromFavicon(aTaskbarTab);

    let shortcut = await createShortcut(aTaskbarTab, iconPath);

    try {
      await lazy.ShellService.pinShortcutToTaskbar(aTaskbarTab.id, shortcut);
    } catch (e) {
      lazy.logConsole.error(`An error occurred while pinning: ${e.message}`);
    }
  },

  /**
   * Unpins the provided Taskbar Tab from the taskbar.
   *
   * @param {TaskbarTab} aTaskbarTab - The Taskbar Tab to unpin from the taskbar.
   * @returns {Promise} Resolves once finished.
   */
  async unpinTaskbarTab(aTaskbarTab) {
    lazy.logConsole.info("Unpinning Taskbar Tab from the taskbar.");

    let shortcutFilename = generateName(aTaskbarTab);
    let shortcutPath =
      lazy.ShellService.getTaskbarTabShortcutPath(shortcutFilename);

    lazy.ShellService.unpinShortcutFromTaskbar(shortcutPath);

    let iconFile = getIconFile(aTaskbarTab);

    lazy.logConsole.debug(`Deleting ${shortcutPath}`);
    lazy.logConsole.debug(`Deleting ${iconFile.path}`);

    await Promise.all([
      IOUtils.remove(shortcutPath),
      IOUtils.remove(iconFile.path),
    ]);
  },
};

/**
 * Fetches the favicon from the provided Taskbar Tab's start url, and saves it
 * to an icon file.
 *
 * @param {TaskbarTab} aTaskbarTab - The Taskbar Tab to generate an icon file for.
 * @returns {Promise<nsIFile>} The created icon file.
 */
async function createTaskbarIconFromFavicon(aTaskbarTab) {
  lazy.logConsole.info("Creating Taskbar Tabs shortcut icon.");

  let url = Services.io.newURI(aTaskbarTab.startUrl);
  let favicon = await lazy.Favicons.getFaviconForPage(url);

  let imgContainer;
  if (favicon) {
    lazy.logConsole.debug(`Using favicon at URI ${favicon.uri.spec}.`);
    try {
      imgContainer = await getImageFromUri(favicon.uri);
    } catch (e) {
      lazy.logConsole.error(
        `${e.message}, falling through to default favicon.`
      );
    }
  }

  if (!imgContainer) {
    lazy.logConsole.debug(
      `Unable to retrieve icon for ${aTaskbarTab.startUrl}, using default favicon at ${lazy.Favicons.defaultFavicon.spec}.`
    );
    imgContainer = await getImageFromUri(lazy.Favicons.defaultFavicon);
  }

  let iconFile = getIconFile(aTaskbarTab);

  lazy.logConsole.debug(`Using icon path: ${iconFile.path}`);

  await IOUtils.makeDirectory(iconFile.parent.path);

  await lazy.ShellService.createWindowsIcon(iconFile, imgContainer);

  return iconFile;
}

/**
 * Retrieves an image given a URI.
 *
 * @param {nsIURI} aUri - The URI to retrieve an image from.
 * @returns {Promise<imgIContainer>} Resolves to an image container.
 */
async function getImageFromUri(aUri) {
  const channel = Services.io.newChannelFromURI(
    aUri,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_IMAGE
  );

  return await new Promise((resolve, reject) => {
    const imgTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
    let imageContainer;

    let observer = imgTools.createScriptedObserver({
      sizeAvailable() {
        resolve(imageContainer);
        imageContainer = null;
      },
    });

    imgTools.decodeImageFromChannelAsync(
      aUri,
      channel,
      (img, status) => {
        if (!Components.isSuccessCode(status)) {
          reject(new Error(`Error retrieving image from URI ${aUri.spec}`));
        } else {
          imageContainer = img;
        }
      },
      observer
    );
  });
}

/**
 * Creates a shortcut that opens Firefox with relevant Taskbar Tabs flags.
 *
 * @param {TaskbarTab} aTaskbarTab - The Taskbar Tab to generate a shortcut for.
 * @param {nsIFile} aFileIcon - The icon file to use for the shortcut.
 * @returns {Promise<string>} The path to the created shortcut.
 */
async function createShortcut(aTaskbarTab, aFileIcon) {
  lazy.logConsole.info("Creating Taskbar Tabs shortcut.");

  let name = generateName(aTaskbarTab);
  let lnkFile = name + ".lnk";

  lazy.logConsole.debug(`Using shortcut filename: ${lnkFile}`);

  let targetfile = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  let profileFolder = Services.dirsvc.get("ProfD", Ci.nsIFile);

  const l10n = new Localization(["preview/taskbartabs.ftl"]);
  const description = await l10n.formatValue(
    "taskbar-tab-shortcut-description",
    { name }
  );

  return await lazy.ShellService.createShortcut(
    targetfile,
    [
      "-taskbar-tab",
      aTaskbarTab.id,
      "-new-window",
      aTaskbarTab.startUrl, // In case Taskbar Tabs is disabled, provide fallback url.
      "-profile",
      profileFolder.path,
      "-container",
      aTaskbarTab.userContextId,
    ],
    description,
    aFileIcon,
    0,
    aTaskbarTab.id, // AUMID
    "Programs",
    lnkFile
  );
}

/**
 * Generates a name for the Taskbar Tab appropriate for user facing UI.
 *
 * @param {TaskbarTab} aTaskbarTab - The Taskbar Tab to generate a name for.
 * @returns {string} A name suitable for user facing UI.
 */
function generateName(aTaskbarTab) {
  // https://www.subdomain.example.co.uk/test
  let uri = Services.io.newURI(aTaskbarTab.startUrl);

  // ["www", "subdomain", "example", "co", "uk"]
  let hostParts = uri.host.split(".");

  // ["subdomain", "example", "co", "uk"]
  if (hostParts[0] === "www") {
    hostParts.shift();
  }

  let suffixDomainCount = Services.eTLD
    .getKnownPublicSuffix(uri)
    .split(".").length;

  // ["subdomain", "example"]
  hostParts.splice(-suffixDomainCount);

  let name = hostParts
    // ["example", "subdomain"]
    .reverse()
    // ["Example", "Subdomain"]
    .map(s => s.charAt(0).toUpperCase() + s.slice(1))
    // "Example Subdomain"
    .join(" ");

  return name;
}

/**
 * Generates a file path to use for the Taskbar Tab icon file.
 *
 * @param {TaskbarTab} aTaskbarTab - A Taskbar Tab to generate an icon file path for.
 * @returns {nsIFile} The icon file path for the Taskbar Tab.
 */
function getIconFile(aTaskbarTab) {
  let iconPath = lazy.TaskbarTabsUtils.getTaskbarTabsFolder();
  iconPath.append("icons");
  iconPath.append(aTaskbarTab.id + ".ico");
  return iconPath;
}
