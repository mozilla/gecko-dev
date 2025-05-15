/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

var { ExtensionError } = ExtensionUtils;

const spellColour = color => (color === "grey" ? "gray" : color);

/**
 * @param {MozTabbrowserTabGroup} group Group to move.
 * @param {DOMWindow} window Browser window to move to.
 * @param {integer} index The desired position of the group within the window
 * @returns {integer} The tab index that the group should move to, such that
 *   after the move operation, the group's position is at the given index.
 */
function adjustIndexForMove(group, window, index) {
  let tabIndex = index < 0 ? window.gBrowser.tabs.length : index;
  if (group.ownerGlobal === window) {
    let group_tabs = group.tabs;
    if (tabIndex > group_tabs[0]._tPos) {
      // When group is moving to a higher index, we need to increase the
      // index to account for the fact that the act of moving tab groups
      // causes all following tabs to have a decreased index.
      tabIndex += group_tabs.length;
    }
  }
  tabIndex = Math.min(tabIndex, window.gBrowser.tabs.length);

  let prevTab = tabIndex > 0 ? window.gBrowser.tabs.at(tabIndex - 1) : null;
  let nextTab = window.gBrowser.tabs.at(tabIndex);
  if (nextTab?.pinned) {
    throw new ExtensionError(
      "Cannot move the group to an index that is in the middle of pinned tabs."
    );
  }
  if (prevTab && nextTab?.group && prevTab.group === nextTab.group) {
    throw new ExtensionError(
      "Cannot move the group to an index that is in the middle of another group."
    );
  }

  return tabIndex;
}

this.tabGroups = class extends ExtensionAPIPersistent {
  queryGroups({ collapsed, color, title, windowId } = {}) {
    color = spellColour(color);
    let glob = title != null && new MatchGlob(title);
    let window =
      windowId != null && windowTracker.getWindow(windowId, null, false);
    return windowTracker
      .browserWindows()
      .filter(
        win =>
          this.extension.canAccessWindow(win) &&
          (windowId == null || win === window)
      )
      .flatMap(win => win.gBrowser.tabGroups)
      .filter(
        group =>
          (collapsed == null || group.collapsed === collapsed) &&
          (color == null || group.color === color) &&
          (title == null || glob.matches(group.name))
      );
  }

  get(groupId) {
    let gid = getInternalTabGroupIdForExtTabGroupId(groupId);
    if (!gid) {
      throw new ExtensionError(`No group with id: ${groupId}`);
    }
    for (let group of this.queryGroups()) {
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
        if (event.detail.isAdoptingGroup) {
          // Tab group moved from a different window.
          return;
        }
        if (!this.extension.canAccessWindow(event.originalTarget.ownerGlobal)) {
          return;
        }
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
        if (!this.extension.canAccessWindow(event.originalTarget.ownerGlobal)) {
          return;
        }
        fire.async(this.convert(event.originalTarget));
      };
      let onCreate = event => {
        if (!event.detail.isAdoptingGroup) {
          // We are only interested in tab groups moved from a different window.
          return;
        }
        if (!this.extension.canAccessWindow(event.originalTarget.ownerGlobal)) {
          return;
        }
        fire.async(this.convert(event.originalTarget));
      };
      windowTracker.addListener("TabGroupMoved", onMove);
      windowTracker.addListener("TabGroupCreate", onCreate);
      return {
        unregister() {
          windowTracker.removeListener("TabGroupMoved", onMove);
          windowTracker.removeListener("TabGroupCreate", onCreate);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onRemoved({ fire }) {
      let onRemove = event => {
        if (event.originalTarget.removedByAdoption) {
          // Tab group moved to a different window.
          return;
        }
        if (!this.extension.canAccessWindow(event.originalTarget.ownerGlobal)) {
          return;
        }
        fire.async(this.convert(event.originalTarget), {
          isWindowClosing: false,
        });
      };
      let onClosed = window => {
        if (!this.extension.canAccessWindow(window)) {
          return;
        }
        for (const group of window.gBrowser.tabGroups) {
          fire.async(this.convert(group), { isWindowClosing: true });
        }
      };
      windowTracker.addListener("TabGroupRemoved", onRemove);
      windowTracker.addListener("domwindowclosed", onClosed);
      return {
        unregister() {
          windowTracker.removeListener("TabGroupRemoved", onRemove);
          windowTracker.removeListener("domwindowclosed", onClosed);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onUpdated({ fire }) {
      let onUpdate = event => {
        if (!this.extension.canAccessWindow(event.originalTarget.ownerGlobal)) {
          return;
        }
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

          let tabIndex = adjustIndexForMove(group, win, index);
          if (win !== group.ownerGlobal) {
            group = win.gBrowser.adoptTabGroup(group, { tabIndex });
          } else {
            win.gBrowser.moveTabTo(group, { tabIndex });
          }
          return this.convert(group);
        },

        query: query => {
          return Array.from(this.queryGroups(query), group =>
            this.convert(group)
          );
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
