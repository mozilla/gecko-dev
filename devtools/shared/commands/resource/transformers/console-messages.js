/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// eslint-disable-next-line mozilla/reject-some-requires
loader.lazyRequireGetter(
  this,
  "getAdHocFrontOrPrimitiveGrip",
  "resource://devtools/client/fronts/object.js",
  true
);

module.exports = function ({ resource, targetFront }) {
  // We might need to create fronts for each of the message arguments.
  // @backward-compat { version 129 } Once Fx129 is release, CONSOLE_MESSAGE resource
  // are no longer encapsulated into a sub "message" attribute.
  // (we can remove the `if (resource.message)` branch)
  if (resource.message) {
    if (Array.isArray(resource.message.arguments)) {
      resource.message.arguments = resource.message.arguments.map(arg =>
        getAdHocFrontOrPrimitiveGrip(arg, targetFront)
      );
    }
  } else if (Array.isArray(resource.arguments)) {
    resource.arguments = resource.arguments.map(arg =>
      getAdHocFrontOrPrimitiveGrip(arg, targetFront)
    );
  }
  return resource;
};
