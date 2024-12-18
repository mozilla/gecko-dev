/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {object} TabGroupStateData
 * @property {string} id
 *   Unique ID of the tab group.
 * @property {string} name
 *   User-defined name of the tab group.
 * @property {"blue"|"purple"|"cyan"|"orange"|"yellow"|"pink"|"green"|"gray"|"red"} color
 *   User-selected color name for the tab group's label/icons.
 * @property {boolean} collapsed
 *   Whether the tab group is collapsed or expanded in the tab strip.
 */

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
}

export const TabGroupState = new _TabGroupState();
