/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function InspectorPanel(iframeWindow, toolbox, commands) {
  this._inspector = new iframeWindow.Inspector(toolbox, commands);
}
InspectorPanel.prototype = {
  /**
   * Initialize the inspector
   *
   * @param {Object} options: see Inspector.init
   * @returns {Inspector}
   */
  open(options = {}) {
    return this._inspector.init(options);
  },

  destroy() {
    this._inspector.destroy();
  },
};
exports.InspectorPanel = InspectorPanel;
