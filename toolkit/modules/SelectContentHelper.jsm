/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");

this.EXPORTED_SYMBOLS = [
  "SelectContentHelper"
];

this.SelectContentHelper = function (aElement, aGlobal) {
  this.element = aElement;
  this.initialSelection = aElement[aElement.selectedIndex] || null;
  this.global = aGlobal;
  this.init();
  this.showDropDown();
}

this.SelectContentHelper.prototype = {
  init: function() {
    this.global.addMessageListener("Forms:SelectDropDownItem", this);
    this.global.addMessageListener("Forms:DismissedDropDown", this);
    this.global.addEventListener("pagehide", this);
  },

  uninit: function() {
    this.global.removeMessageListener("Forms:SelectDropDownItem", this);
    this.global.removeMessageListener("Forms:DismissedDropDown", this);
    this.global.removeEventListener("pagehide", this);
    this.element = null;
    this.global = null;
  },

  showDropDown: function() {
    let rect = this._getBoundingContentRect();

    this.global.sendAsyncMessage("Forms:ShowDropDown", {
      rect: rect,
      options: this._buildOptionList(),
      selectedIndex: this.element.selectedIndex
    });
  },

  _getBoundingContentRect: function() {
    return BrowserUtils.getElementBoundingScreenRect(this.element);
  },

  _buildOptionList: function() {
    return buildOptionListForChildren(this.element);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Forms:SelectDropDownItem":
        this.element.selectedIndex = message.data.value;
        break;

      case "Forms:DismissedDropDown":
        if (this.initialSelection != this.element.item(this.element.selectedIndex)) {
          let event = this.element.ownerDocument.createEvent("Events");
          event.initEvent("change", true, true);
          this.element.dispatchEvent(event);
        }

        this.uninit();
        break;
    }
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "pagehide":
        this.global.sendAsyncMessage("Forms:HideDropDown", {});
        this.uninit();
        break;
    }
  }

}

function buildOptionListForChildren(node) {
  let result = [];
  let win = node.ownerDocument.defaultView;

  for (let child of node.children) {
    let tagName = child.tagName.toUpperCase();

    if (tagName == 'OPTION' || tagName == 'OPTGROUP') {
      let textContent =
        tagName == 'OPTGROUP' ? child.getAttribute("label")
                              : child.textContent;

      if (textContent != null) {
        textContent = textContent.trim();
      } else {
        textContent = ""
      }

      let info = {
        tagName: child.tagName,
        textContent: textContent,
        disabled: child.disabled,
        // We need to do this for every option element as each one can have
        // an individual style set for direction
        textDirection: win.getComputedStyle(child).getPropertyValue("direction"),
        // XXX this uses a highlight color when this is the selected element.
        // We need to suppress such highlighting in the content process to get
        // the option's correct unhighlighted color here.
        // We also need to detect default color vs. custom so that a standard
        // color does not override color: menutext in the parent.
        // backgroundColor: computedStyle.backgroundColor,
        // color: computedStyle.color,
        children: tagName == 'OPTGROUP' ? buildOptionListForChildren(child) : []
      };
      result.push(info);
    }
  }
  return result;
}
