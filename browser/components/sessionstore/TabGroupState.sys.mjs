/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Module that contains tab group state collection methods.
 */
export const TabGroupState = Object.freeze({
  /**
   * @param {MozTabbrowserTabGroup} tabGroup
   *   Tab group browser element
   * @returns {TabGroupStateData}
   *   Serialized tab group data
   */
  collect(tabGroup) {
    return TabGroupStateInternal.collect(tabGroup);
  },
});

const TabGroupStateInternal = {
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
  },
};
