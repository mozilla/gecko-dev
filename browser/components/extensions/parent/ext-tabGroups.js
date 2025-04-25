/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { ExtensionError } = ExtensionUtils;

this.tabGroups = class extends ExtensionAPIPersistent {
  get(groupId) {
    let gid = getInternalTabGroupIdForExtTabGroupId(groupId);
    if (!gid) {
      throw new ExtensionError(`No group with id: ${groupId}`);
    }
    let groups = windowTracker
      .browserWindows()
      .filter(win => this.extension.canAccessWindow(win))
      .flatMap(win => win.gBrowser.tabGroups);
    for (let group of groups) {
      if (group.id === gid) {
        return group;
      }
    }
    throw new ExtensionError(`No group with id: ${groupId}`);
  }

  convert(group) {
    return {
      collapsed: !!group.collapsed,
      color: group.color === "gray" ? "grey" : group.color,
      id: getExtTabGroupIdForInternalTabGroupId(group.id),
      title: group.name,
      windowId: windowTracker.getId(group.ownerGlobal),
    };
  }

  getAPI(_context) {
    return {
      tabGroups: {
        get: groupId => {
          return this.convert(this.get(groupId));
        },
      },
    };
  }
};
