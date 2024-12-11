/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TabState: "resource:///modules/sessionstore/TabState.sys.mjs",
});

/**
 * Module that contains tab group state collection methods.
 */
class _TabGroupState {
  /**
   * Collect data related to a single tab group, synchronously.
   *
   * @param {MozTabbrowserTabGroup} tabGroup
   *   Tab group browser element
   * @returns {TabGroupStateData}
   *   Serialized tab group data
   */
  collect(tabGroup) {
    return {
      id: tabGroup.id,
      name: tabGroup.label,
      color: tabGroup.color,
      collapsed: tabGroup.collapsed,
    };
  }

  /**
   * Collect data related to a single tab group, including all of the tabs
   * within that group.
   *
   * @param {MozTabbrowserTabGroup} tabGroup
   *   Tab group browser element
   * @returns {object}
   *   Serialized tab group data
   */
  clone(tabGroup) {
    const groupState = this.collect(tabGroup);
    groupState.tabs = tabGroup.tabs.map(tab => lazy.TabState.collect(tab));
    return groupState;
  }
}

export const TabGroupState = new _TabGroupState();
