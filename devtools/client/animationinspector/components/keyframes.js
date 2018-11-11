/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {createNode} = require("devtools/client/animationinspector/utils");

/**
 * UI component responsible for displaying a list of keyframes.
 */
function Keyframes() {
  EventEmitter.decorate(this);
  this.onClick = this.onClick.bind(this);
}

exports.Keyframes = Keyframes;

Keyframes.prototype = {
  init: function (containerEl) {
    this.containerEl = containerEl;

    this.keyframesEl = createNode({
      parent: this.containerEl,
      attributes: {"class": "keyframes"}
    });

    this.containerEl.addEventListener("click", this.onClick);
  },

  destroy: function () {
    this.containerEl.removeEventListener("click", this.onClick);
    this.keyframesEl.remove();
    this.containerEl = this.keyframesEl = this.animation = null;
  },

  render: function ({keyframes, propertyName, animation}) {
    this.keyframes = keyframes;
    this.propertyName = propertyName;
    this.animation = animation;

    let iterationStartOffset =
      animation.state.iterationStart % 1 == 0
      ? 0
      : 1 - animation.state.iterationStart % 1;

    this.keyframesEl.classList.add(animation.state.type);
    for (let frame of this.keyframes) {
      let offset = frame.offset + iterationStartOffset;
      createNode({
        parent: this.keyframesEl,
        attributes: {
          "class": "frame",
          "style": `left:${offset * 100}%;`,
          "data-offset": frame.offset,
          "data-property": propertyName,
          "title": frame.value
        }
      });
    }
  },

  onClick: function (e) {
    // If the click happened on a frame, tell our parent about it.
    if (!e.target.classList.contains("frame")) {
      return;
    }

    e.stopPropagation();
    this.emit("frame-selected", {
      animation: this.animation,
      propertyName: this.propertyName,
      offset: parseFloat(e.target.dataset.offset),
      value: e.target.getAttribute("title"),
      x: e.target.offsetLeft + e.target.closest(".frames").offsetLeft
    });
  }
};
