/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Ci } = require("chrome");
const method = require("../../method/core");
const { add, remove, iterator } = require("../lang/weak-set");

let getTargetWindow = method("getTargetWindow");

getTargetWindow.define(function (target) {
  if (target instanceof Ci.nsIDOMWindow)
    return target;
  if (target instanceof Ci.nsIDOMDocument)
    return target.defaultView || null;

  return null;
});

exports.getTargetWindow = getTargetWindow;

let attachTo = method("attachTo");
exports.attachTo = attachTo;

let detachFrom = method("detatchFrom");
exports.detachFrom = detachFrom;

function attach(modification, target) {
  let window = getTargetWindow(target);

  attachTo(modification, window);

  // modification are stored per content; `window` reference can still be the
  // same even if the content is changed, therefore `document` is used instead.
  add(modification, window.document);
}
exports.attach = attach;

function detach(modification, target) {
  if (target) {
    let window = getTargetWindow(target);
    detachFrom(modification, window);
    remove(modification, window.document);
  }
  else {
    let documents = iterator(modification);
    for (let document of documents) {
      detachFrom(modification, document.defaultView);
      remove(modification, document);
    }
  }
}
exports.detach = detach;
