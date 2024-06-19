/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function splitLogins(logins, numSources) {
  const sliceSize = Math.max(logins.length / numSources, 1);
  const slicedLogins = [];

  for (let i = 0; i < logins.length; i++) {
    if (sliceSize * (i + 1) <= logins.length) {
      slicedLogins.push(logins.slice(sliceSize * i, sliceSize * (i + 1)));
    }
  }

  return slicedLogins;
}

add_task(async function test_aggregatorEnumerateLines() {
  const sources = [new MockLoginDataSource(), new MockLoginDataSource()];

  const loginList = testData.loginList();
  const loginsPerSource = splitLogins(loginList, sources.length);

  const aggregator = new Aggregator();

  for (let i = 0; i < sources.length; i++) {
    sources[i].addLines(loginsPerSource[i]);
    aggregator.addSource(() => sources[i]);
  }

  const actualResultNoSearchText = Array.from(aggregator.enumerateLines(""));

  Assert.deepEqual(
    loginList,
    actualResultNoSearchText,
    "Aggregator enumerated all lines found in sources"
  );

  const expectedResultWithSearchText = loginList.filter(login =>
    MockLoginDataSource.match(login, "example.com")
  );

  const actualResultWithSearchText = Array.from(
    aggregator.enumerateLines("example.com")
  );

  Assert.deepEqual(
    expectedResultWithSearchText,
    actualResultWithSearchText,
    "Aggregator enumerated all lines with search text found in sources"
  );
});
