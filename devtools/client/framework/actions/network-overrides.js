/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");

exports.setNetworkOverride = setNetworkOverride;
function setNetworkOverride(commands, url, data, win) {
  return async function ({ dispatch }) {
    const filename = url.split("/").at(-1);
    if (typeof data == "string") {
      data = new TextEncoder().encode(data);
    }

    const path = await DevToolsUtils.saveAs(win, data, filename);
    if (path) {
      const hasWatcherSupport =
        commands.targetCommand.hasTargetWatcherSupport();
      if (hasWatcherSupport) {
        const networkFront =
          await commands.targetCommand.watcherFront.getNetworkParentActor();
        await networkFront.override(url, path);
        dispatch({
          type: "SET_NETWORK_OVERRIDE",
          url,
          path,
        });
      }
    }
  };
}

exports.removeNetworkOverride = removeNetworkOverride;
function removeNetworkOverride(commands, url) {
  return async function ({ dispatch }) {
    const hasWatcherSupport = commands.targetCommand.hasTargetWatcherSupport();
    if (hasWatcherSupport) {
      const networkFront =
        await commands.targetCommand.watcherFront.getNetworkParentActor();
      await networkFront.removeOverride(url);
      dispatch({
        type: "REMOVE_NETWORK_OVERRIDE",
        url,
      });
    }
  };
}
