/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ManageAddresses } from "chrome://formautofill/content/manageDialog.mjs";

new ManageAddresses({
  records: document.getElementById("addresses"),
  controlsContainer: document.getElementById("controls-container"),
  remove: document.getElementById("remove"),
  add: document.getElementById("add"),
  edit: document.getElementById("edit"),
});
