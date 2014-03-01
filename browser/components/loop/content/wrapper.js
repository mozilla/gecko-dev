/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function LoopContent(templateId, iframeId) {
  this.template = document.getElementById(templateId);
  this.iframe = document.getElementById(iframeId);
  this.messageChannel = null;
}

LoopContent.prototype = {
  init: function() {
    var documentFragment = this.template.content.cloneNode(true);
    this.iframe.contentDocument.body.appendChild(documentFragment);
    this.messageChannel = this.iframe.contentWindow.MessageChannel();
    Object.defineProperty(this.iframe.contentWindow.wrappedJSObject,
                          "wrapperPort",
                          {
                            value: this.messageChannel.port2
                          });
    this.messageChannel.port1.onmessage = this.processMessage.bind(this);
  },

  sendMessage: function(msg) {
    this.messageChannel.port1.postMessage(JSON.stringify(msg));
  },

  processMessage: function(msg) {
    var message = JSON.parse(msg.data);
    console.log("Received message from content frame: " + msg.data);
    if (message.operation == "init_done") {
      this.sendMessage({operation: "init_ack"});
    }
  }

};

function initWrapper(templateId, iframeId) {
  this.loopContent = new LoopContent(templateId, iframeId);
  this.loopContent.init();
}
