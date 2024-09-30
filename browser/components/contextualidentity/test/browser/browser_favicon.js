/*
 * Bug 1270678 - A test case to test does the favicon obey originAttributes.
 */

let { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  LinkHandlerParent: "resource:///actors/LinkHandlerParent.sys.mjs",
});

const USER_CONTEXTS = ["default", "personal", "work"];

let gHttpServer = null;
let gUserContextId;
let gFaviconData;

function getIconFile() {
  new Promise(resolve => {
    NetUtil.asyncFetch(
      {
        uri: "http://www.example.com/browser/browser/components/contextualidentity/test/browser/favicon-normal32.png",
        loadUsingSystemPrincipal: true,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON,
      },
      function (inputStream) {
        let size = inputStream.available();
        gFaviconData = NetUtil.readInputStreamToString(inputStream, size);
        resolve();
      }
    );
  });
}

function loadIndexHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html", false);
  let body = `
    <!DOCTYPE HTML>
      <html>
        <head>
          <meta charset='utf-8'>
          <title>Favicon Test</title>
        </head>
        <body>
          Favicon!!
        </body>
    </html>`;
  response.bodyOutputStream.write(body, body.length);
}

function loadFaviconHandler(metadata, response) {
  let expectedCookie = "userContext=" + USER_CONTEXTS[gUserContextId];

  if (metadata.hasHeader("Cookie")) {
    is(
      metadata.getHeader("Cookie"),
      expectedCookie,
      "The cookie has matched with the expected cookie."
    );
  } else {
    ok(false, "The request should have a cookie.");
  }

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "image/png", false);
  response.bodyOutputStream.write(gFaviconData, gFaviconData.length);
}

add_setup(async function () {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  // Create a http server for the image cache test.
  if (!gHttpServer) {
    gHttpServer = new HttpServer();
    gHttpServer.registerPathHandler("/", loadIndexHandler);
    gHttpServer.registerPathHandler("/favicon.png", loadFaviconHandler);
    gHttpServer.start(-1);
  }
});

registerCleanupFunction(() => {
  gHttpServer.stop(() => {
    gHttpServer = null;
  });
});

add_task(async function test() {
  waitForExplicitFinish();

  // First, get the icon data.
  await getIconFile();

  let serverPort = gHttpServer.identity.primaryPort;
  let testURL = "http://localhost:" + serverPort + "/";

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    gUserContextId = userContextId;

    // Load the page in 3 different contexts and set a cookie
    // which should only be visible in that context.

    // Open our tab in the given user context.
    let tabInfo = await openTabInUserContext(testURL, userContextId);

    // Promise that waits for the favicon is updated.
    let onFaviconReady = new Promise(resolve => {
      let listener = name => {
        if (name == "SetIcon") {
          lazy.LinkHandlerParent.removeListenerForTests(listener);
          resolve();
        }
      };

      lazy.LinkHandlerParent.addListenerForTests(listener);
    });

    // Write a cookie according to the userContext.
    await SpecialPowers.spawn(
      tabInfo.browser,
      [{ userContext: USER_CONTEXTS[userContextId] }],
      function (arg) {
        content.document.cookie = "userContext=" + arg.userContext;
        // Load favicon.
        let link = content.document.createElement("link");
        link.setAttribute("rel", "icon");
        link.setAttribute("href", "favicon.png");
        content.document.head.append(link);
      }
    );
    await onFaviconReady;

    BrowserTestUtils.removeTab(tabInfo.tab);
  }
});
