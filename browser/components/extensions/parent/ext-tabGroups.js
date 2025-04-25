/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

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
    onCreated({ fire }) {
      let onCreate = event => {
        fire.async(this.convert(event.originalTarget));
      };
      windowTracker.addListener("TabGroupCreate", onCreate);
      return {
        unregister() {
          windowTracker.removeListener("TabGroupCreate", onCreate);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onMoved({ fire }) {
      let onMove = event => {
        fire.async(this.convert(event.originalTarget));
      };
      windowTracker.addListener("TabGroupMoved", onMove);
      return {
        unregister() {
          windowTracker.removeListener("TabGroupMoved", onMove);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onRemoved({ fire }) {
      let onRemove = event => {
        fire.async(this.convert(event.originalTarget));
      };
      windowTracker.addListener("TabGroupRemoved", onRemove);
      return {
        unregister() {
          windowTracker.removeListener("TabGroupRemoved", onRemove);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
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
    const { windowManager } = this.extension;
    return {
      tabGroups: {
        get: groupId => {
          return this.convert(this.get(groupId));
        },

        move: (groupId, { index, windowId }) => {
          let group = this.get(groupId);
          let win = group.ownerGlobal;

          if (windowId != null) {
            win = windowTracker.getWindow(windowId, context);
            if (
              PrivateBrowsingUtils.isWindowPrivate(group.ownerGlobal) !==
              PrivateBrowsingUtils.isWindowPrivate(win)
            ) {
              throw new ExtensionError(
                "Can't move groups between private and non-private windows"
              );
            }
            if (windowManager.getWrapper(win).type !== "normal") {
              throw new ExtensionError(
                "Groups can only be moved to normal windows."
              );
            }
          }

          if (win !== group.ownerGlobal) {
            // Always adopt at the end when moving to another window,
            // so that moveBefore/moveAfter logic works as expected.
            let last = win.gBrowser.tabContainer.ariaFocusableItems.length;
            group = win.gBrowser.adoptTabGroup(group, last);
          }
          if (index >= 0 && index < win.gBrowser.tabs.length) {
            win.gBrowser.moveTabTo(group, { tabIndex: index });
          } else if (win.gBrowser.tabs.at(-1) !== group.tabs.at(-1)) {
            win.gBrowser.moveTabAfter(group, win.gBrowser.tabs.at(-1));
          }
          return this.convert(group);
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

        onCreated: new EventManager({
          context,
          module: "tabGroups",
          event: "onCreated",
          extensionApi: this,
        }).api(),

        onMoved: new EventManager({
          context,
          module: "tabGroups",
          event: "onMoved",
          extensionApi: this,
        }).api(),

        onRemoved: new EventManager({
          context,
          module: "tabGroups",
          event: "onRemoved",
          extensionApi: this,
        }).api(),

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
