/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if filtering items in the network table works correctly with new requests.
 */
const BASIC_REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined" },
  { url: "sjs_content-type-test-server.sjs?fmt=css" },
  { url: "sjs_content-type-test-server.sjs?fmt=js" },
];

const REQUESTS_WITH_MEDIA = BASIC_REQUESTS.concat([
  { url: "sjs_content-type-test-server.sjs?fmt=font" },
  { url: "sjs_content-type-test-server.sjs?fmt=image" },
  { url: "sjs_content-type-test-server.sjs?fmt=audio" },
  { url: "sjs_content-type-test-server.sjs?fmt=video" },
]);

const REQUESTS_WITH_MEDIA_AND_FLASH = REQUESTS_WITH_MEDIA.concat([
  { url: "sjs_content-type-test-server.sjs?fmt=flash" },
]);

function test() {
  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    // It seems that this test may be slow on Ubuntu builds running on ec2.
    requestLongerTimeout(2);

    let { $, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 8).then(() => {
      EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));

      isnot(RequestsMenu.selectedItem, null,
        "There should be a selected item in the requests menu.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be selected in the requests menu.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should not be hidden after toggle button was pressed.");

      testFilterButtons(aMonitor, "all");
      testContents([1, 1, 1, 1, 1, 1, 1, 1])
        .then(() => {
          info("Testing html filtering.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          testFilterButtons(aMonitor, "html");
          return testContents([1, 0, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          info("Performing more requests.");
          performRequestsInContent(REQUESTS_WITH_MEDIA_AND_FLASH);
          return waitForNetworkEvents(aMonitor, 8);
        })
        .then(() => {
          info("Testing html filtering again.");
          testFilterButtons(aMonitor, "html");
          return testContents([1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          info("Performing more requests.");
          performRequestsInContent(REQUESTS_WITH_MEDIA_AND_FLASH);
          return waitForNetworkEvents(aMonitor, 8);
        })
        .then(() => {
          info("Testing html filtering again.");
          testFilterButtons(aMonitor, "html");
          return testContents([1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          info("Resetting filters.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          testFilterButtons(aMonitor, "all");
          return testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);
    });

    function testContents(aVisibility) {
      isnot(RequestsMenu.selectedItem, null,
        "There should still be a selected item after filtering.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be still selected after filtering.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should still be visible after filtering.");

      is(RequestsMenu.items.length, aVisibility.length,
        "There should be a specific amount of items in the requests menu.");
      is(RequestsMenu.visibleItems.length, aVisibility.filter(e => e).length,
        "There should be a specific amount of visbile items in the requests menu.");

      for (let i = 0; i < aVisibility.length; i++) {
        is(RequestsMenu.getItemAtIndex(i).target.hidden, !aVisibility[i],
          "The item at index " + i + " doesn't have the correct hidden state.");
      }

      for (let i = 0; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=html", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "html",
            fullMimeType: "text/html; charset=utf-8"
        });
      }
      for (let i = 1; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=css", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "css",
            fullMimeType: "text/css; charset=utf-8"
        });
      }
      for (let i = 2; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=js", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "js",
            fullMimeType: "application/javascript; charset=utf-8"
        });
      }
      for (let i = 3; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=font", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "woff",
            fullMimeType: "font/woff"
        });
      }
      for (let i = 4; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=image", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "png",
            fullMimeType: "image/png"
        });
      }
      for (let i = 5; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=audio", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "ogg",
            fullMimeType: "audio/ogg"
        });
      }
      for (let i = 6; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=video", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "webm",
            fullMimeType: "video/webm"
        });
      }
      for (let i = 7; i < aVisibility.length; i += 8) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(i),
          "GET", CONTENT_TYPE_SJS + "?fmt=flash", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "x-shockwave-flash",
            fullMimeType: "application/x-shockwave-flash"
        });
      }

      return promise.resolve(null);
    }

    loadCommonFrameScript();
    performRequestsInContent(REQUESTS_WITH_MEDIA_AND_FLASH);
  });
}
