/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/

var loop = loop || {};
loop.dom = (function() {
  "use strict";

  function createEl(tag, attributes) {
    var _node   = document.createElement(tag);
    for (var attrName in attributes) {
      if (attributes.hasOwnProperty(attrName)) {
        _node.setAttribute(attrName, attributes[attrName]);
      }
    }
    return _node;
  }

  function appendEl(el, sel, parent) {
    parent = parent || document;
    parent.querySelector(sel).appendChild(el);
  }

  function eachEl(sel, fn, parent) {
    parent = parent || document;
    [].forEach.call(parent.querySelectorAll(sel), fn);
  }

  function showEl(sel, parent) {
    eachEl(sel, (el) => el.classList.remove("hide"), parent);
  }

  function hideEl(sel, parent) {
    eachEl(sel, (el) => el.classList.add("hide"), parent);
  }

  function setElValue(sel, value, parent) {
    eachEl(sel, (el) => el.value = value, parent);
  }

  function setElText(sel, text, parent) {
    eachEl(sel, (el) => el.textContent = text, parent);
  }

  function removeEl(sel, parent) {
    eachEl(sel, (el) => el.remove(), parent);
  }

  return {
    createEl: createEl,
    appendEl: appendEl,
    eachEl: eachEl,
    showEl: showEl,
    hideEl: hideEl,
    setElValue: setElValue,
    setElText: setElText,
    removeEl: removeEl
  };
})();
