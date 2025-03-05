/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const MODE = {
  TINY: Symbol("TINY"),
  SHORT: Symbol("SHORT"),
  LONG: Symbol("LONG"),
  // Used by Debugger Preview popup
  HEADER: Symbol("HEADER"),
};

const JSON_NUMBER = Symbol("JSON_NUMBER");
export { MODE, JSON_NUMBER };
