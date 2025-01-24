/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global NodeServer */

// This test checks that npm packages can be installed and used.

"use strict";

add_task(async function test_socks5_installed() {
  let id = await NodeServer.fork();
  // This is just for testing. Once we start using more useful npm packages
  // we can remove left-pad.
  equal(
    await NodeServer.execute(
      id,
      `const leftPad = require('left-pad'); leftPad('foo', 5)`
    ),
    "  foo"
  );
  await NodeServer.kill(id);
});
