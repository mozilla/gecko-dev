/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1928216 - Google Voice doesn't list microphones in Firefox 132
 *
 * As per https://bugzilla.mozilla.org/show_bug.cgi?id=1928216#c9, Google Voice
 * is not handling permissions.query() working for camera and microphone correctly,
 * so emulate an earlier version that rejects for those.
 */

/* globals exportFunction, cloneInto */

(() => {
  const nativeQuery = navigator.permissions.wrappedJSObject.query.bind(
    navigator.permissions.wrappedJSObject
  );

  Object.defineProperty(navigator.permissions.wrappedJSObject, "query", {
    value: exportFunction(function query(descriptor) {
      if (typeof descriptor == "object") {
        switch (descriptor.name) {
          case "camera":
          case "microphone": {
            console.log(
              "permissions.query() has been overwriten to not work with camera and microphone. See https://bugzilla.mozilla.org/show_bug.cgi?id=1928216#c9"
            );
            const err = new TypeError(
              `'${descriptor.name}' (value of 'name' member of PermissionDescriptor) is not a valid value for enumeration PermissionName.`
            );
            return window.Promise.reject(cloneInto(err, window));
          }
        }
      }
      return nativeQuery(descriptor);
    }, navigator.permissions),
  });
})();
