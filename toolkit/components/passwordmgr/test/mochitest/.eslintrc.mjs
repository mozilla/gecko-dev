/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default [
  {
    languageOptions: {
      globals: {
        promptDone: true,
        startTest: true,
        // Make no-undef happy with our runInParent mixed environments since you
        // can't indicate a single function is a new env.
        assert: true,
        addMessageListener: true,
        sendAsyncMessage: true,
        Assert: true,
      },
    },
    rules: {
      "no-var": "off",
    },
  },
];
