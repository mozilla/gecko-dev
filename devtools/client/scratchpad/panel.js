/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

function ScratchpadPanel(iframeWindow, toolbox) {
  const { Scratchpad } = iframeWindow;
  this._toolbox = toolbox;
  this.panelWin = iframeWindow;
  this.scratchpad = Scratchpad;

  Scratchpad.target = this.target;
  Scratchpad.hideMenu();

  this._readyObserver = new Promise(resolve => {
    Scratchpad.addObserver({
      onReady() {
        Scratchpad.removeObserver(this);
        resolve();
      },
    });
  });

  EventEmitter.decorate(this);
}
exports.ScratchpadPanel = ScratchpadPanel;

ScratchpadPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor. For the ScratchpadPanel,
   * by the time this is called, the Scratchpad will already be ready.
   */
  open() {
    return this._readyObserver.then(() => {
      this.isReady = true;
      this.emit("ready");
      return this;
    });
  },

  get target() {
    return this._toolbox.target;
  },

  destroy() {
    this.emit("destroyed");
    return Promise.resolve();
  },
};
