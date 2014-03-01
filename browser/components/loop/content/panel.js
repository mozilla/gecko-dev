/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function LoopPanel() {
}

LoopPanel.prototype = {
  init: function() {
    window.wrapperPort.onmessage = this.processMessage.bind(this);
    this.sendMessage({operation: "init_done"});
  },

  processMessage: function(msg) {
    console.log("Received message from chrome frame: " + msg.data);
  },

  sendMessage: function(msg) {
    window.wrapperPort.postMessage(JSON.stringify(msg));
  }
};

this.loopPanel = new LoopPanel();
this.loopPanel.init();
