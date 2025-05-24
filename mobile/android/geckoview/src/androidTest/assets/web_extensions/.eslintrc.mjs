/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import globals from "globals";

export default [
  {
    languageOptions: {
      globals: {
        ...globals.webextensions,
        ExtensionAPI: true,
        // available to frameScripts
        addMessageListener: false,
        content: false,
        sendAsyncMessage: false,
      },
    },
  },
];
