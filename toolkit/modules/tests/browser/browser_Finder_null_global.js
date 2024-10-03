/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// margin-top: 100000px keeps the iframe not being loaded
const URI = `
  <div>content</div>
  <iframe style="margin-top:100000px" loading="lazy"></iframe>
`;

// Test finder still works when there's a BC that doesn't have the global yet
add_task(async function test_finder_null_global() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html;charset=utf-8," + encodeURIComponent(URI),
    },
    async function (browser) {
      let finder = browser.finder;
      let listener = {
        onFindResult() {
          ok(false, "callback wasn't replaced");
        },
      };
      finder.addResultListener(listener);

      function waitForFind() {
        return new Promise(resolve => {
          listener.onFindResult = resolve;
        });
      }

      // Find content first time
      let promiseFind = waitForFind();
      finder.fastFind("content", false, false);
      let findResult = await promiseFind;
      is(
        findResult.result,
        Ci.nsITypeAheadFind.FIND_FOUND,
        "find first string"
      );

      // Find content second time
      promiseFind = waitForFind();
      finder.findAgain("content", false, false, false);
      findResult = await promiseFind;
      is(
        findResult.result,
        Ci.nsITypeAheadFind.FIND_WRAPPED,
        "find the same string second time"
      );

      finder.removeResultListener(listener);
    }
  );
});
