/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createFormLayoutFromRecord } from "chrome://formautofill/content/addressFormLayout.mjs";
import { EditAddressDialog } from "chrome://formautofill/content/editDialog.mjs";

const { record, noValidate, l10nStrings } = window.arguments?.[0] ?? {};

const formElement = document.querySelector("form");
formElement.noValidate = !!noValidate;
createFormLayoutFromRecord(formElement, record, l10nStrings);

new EditAddressDialog(
  {
    title: document.querySelector("title"),
    cancel: document.getElementById("cancel"),
    save: document.getElementById("save"),
  },
  record
);
