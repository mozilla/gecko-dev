/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let bd = {};
bd.detect = async function () {
  return { bot: false };
};
let load = async function () {
  return bd;
};

export { load, load as default };
