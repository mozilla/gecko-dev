/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1966673 - Override "MozAppearance" in element.style and return false
 * Webcompat issue #154290 - https://github.com/webcompat/web-bugs/issues/154290
 * Webcompat issue #33886 - https://github.com/webcompat/web-bugs/issues/33886
 *
 * The site is using an older version of NiceScroll which has a bug that causes
 * Firefox on Android to not react to touch events, breaking their News links.
 * Overriding "MozAppearance" in element.style to return false fixes the problem.
 */

/* globals exportFunction, cloneInto */

console.info(
  "Overriding MozAppearance in element.style for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1966673 for details."
);

(function () {
  const ele = HTMLElement.wrappedJSObject.prototype;
  const obj = window.wrappedJSObject.Object;
  const style = obj.getOwnPropertyDescriptor(ele, "style");
  const { get } = style;
  style.get = exportFunction(function () {
    const styles = get.call(this);
    return new window.wrappedJSObject.Proxy(
      styles,
      cloneInto(
        {
          deleteProperty(target, prop) {
            return Reflect.deleteProperty(target, prop);
          },
          get(target, key) {
            const val = Reflect.get(target, key);
            if (typeof val == "function") {
              // We can't just return the function, as it's a method which
              // needs `this` to be the styles object. So we return a wrapper.
              return exportFunction(function () {
                return val.apply(styles, arguments);
              }, window);
            }
            return val;
          },
          has(target, key) {
            if (key == "MozAppearance" || key == "WebkitAppearance") {
              return false;
            }
            return Reflect.has(target, key);
          },
          ownKeys(target) {
            return Reflect.ownKeys(target);
          },
          set(target, key, value) {
            return Reflect.set(target, key, value);
          },
        },
        window,
        { cloneFunctions: true }
      )
    );
  }, window);
  obj.defineProperty(ele, "style", style);
})();
