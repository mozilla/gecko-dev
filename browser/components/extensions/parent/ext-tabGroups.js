/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { ExtensionError } = ExtensionUtils;

const spellColour = color => (color === "grey" ? "gray" : color);

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
      /** Internally we use "gray", but Chrome uses "grey" @see spellColour. */
      color: group.color === "gray" ? "grey" : group.color,
      id: getExtTabGroupIdForInternalTabGroupId(group.id),
      title: group.name,
      windowId: windowTracker.getId(group.ownerGlobal),
    };
  }

  PERSISTENT_EVENTS = {
    onUpdated({ fire }) {
      let onUpdate = event => {
        fire.async(this.convert(event.originalTarget));
      };
      windowTracker.addListener("TabGroupCollapse", onUpdate);
      windowTracker.addListener("TabGroupExpand", onUpdate);
      windowTracker.addListener("TabGroupUpdate", onUpdate);
      return {
        unregister() {
          windowTracker.removeListener("TabGroupCollapse", onUpdate);
          windowTracker.removeListener("TabGroupExpand", onUpdate);
          windowTracker.removeListener("TabGroupUpdate", onUpdate);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    return {
      tabGroups: {
        get: groupId => {
          return this.convert(this.get(groupId));
        },

        update: (groupId, { collapsed, color, title }) => {
          let group = this.get(groupId);
          if (collapsed != null) {
            group.collapsed = collapsed;
          }
          if (color != null) {
            group.color = spellColour(color);
          }
          if (title != null) {
            group.name = title;
          }
          return this.convert(group);
        },

        onUpdated: new EventManager({
          context,
          module: "tabGroups",
          event: "onUpdated",
          extensionApi: this,
        }).api(),
      },
    };
  }
};
