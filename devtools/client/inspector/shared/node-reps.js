/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyGetter(this, "MODE", function () {
  return ChromeUtils.importESModule(
    "resource://devtools/client/shared/components/reps/index.mjs"
  ).MODE;
});

loader.lazyGetter(this, "ElementNode", function () {
  return ChromeUtils.importESModule(
    "resource://devtools/client/shared/components/reps/reps/element-node.mjs"
  );
});

loader.lazyGetter(this, "TextNode", function () {
  return ChromeUtils.importESModule(
    "resource://devtools/client/shared/components/reps/reps/text-node.mjs"
  );
});

loader.lazyRequireGetter(
  this,
  "translateNodeFrontToGrip",
  "resource://devtools/client/inspector/shared/utils.js",
  true
);

/**
 * Creates either an ElementNode or a TextNode rep given a nodeFront. By default the
 * rep is created in TINY mode.
 *
 * @param {NodeFront} nodeFront
 *        The node front to create the element for.
 * @param {Object} props
 *        Props to pass to the rep.
 */
function getNodeRep(nodeFront, props = {}) {
  const object = translateNodeFrontToGrip(nodeFront);
  const { rep } = ElementNode.supportsObject(object) ? ElementNode : TextNode;

  return rep({
    object,
    mode: MODE.TINY,
    ...props,
  });
}

module.exports = getNodeRep;
