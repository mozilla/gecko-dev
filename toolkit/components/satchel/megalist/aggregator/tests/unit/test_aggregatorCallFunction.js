/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function test_aggregatorCallFunction() {
  const aggregator = new Aggregator();
  const mockSource = new MockLoginDataSource();
  const testLogin = {
    id: 1,
    username: "username",
    password: "password",
  };

  aggregator.addSource(() => mockSource);
  aggregator.callFunction("MockLoginDataSource", "addLines", testLogin);

  Assert.equal(mockSource.lines.length, 1);
  Assert.deepEqual(
    mockSource.lines[0],
    testLogin,
    "Aggregator called function found in source"
  );
});
