/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1929881 - A test to ensure that we don't treat the top-level view-source
 * local host requests as foreign. The test creates a localhost server and a
 * page that sets a cookie. It then opens the view-source URL for the page and
 * ensures that the cookie is set in the unpartitioned cookie jar.
 */

let { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const TEST_PAGE = TEST_DOMAIN_HTTPS + TEST_PATH + "cookies.sjs";

let gHttpServer = null;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.CHIPS.enabled", true]],
  });

  Services.cookies.removeAll();

  // Create a http server for the test.
  if (!gHttpServer) {
    gHttpServer = new HttpServer();
    gHttpServer.registerPathHandler("/setCookie", loadSetCookieHandler);
    gHttpServer.start(-1);
  }

  registerCleanupFunction(async _ => {
    Services.cookies.removeAll();

    if (gHttpServer) {
      await new Promise(resolve => {
        gHttpServer.stop(() => {
          gHttpServer = null;
          resolve();
        });
      });
    }
  });
});

function loadSetCookieHandler(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Set-Cookie", "test=1", false);

  let body = `<!DOCTYPE HTML>
      <html>
        <head>
          <meta charset='utf-8'>
          <title>SetCookie Test</title>
        </head>
        <body>
        </body>
    </html>`;

  response.write(body);
}

add_task(async function runTest() {
  const TEST_URL = `http://localhost:${gHttpServer.identity.primaryPort}/setCookie`;

  const TEST_VIEW_SOURCE_PAGE = `view-source:${TEST_URL}`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_VIEW_SOURCE_PAGE
  );

  let cookies = Services.cookies.getCookiesFromHost("localhost", {});

  is(cookies.length, 1, "One cookie should be set");

  cookies = Services.cookies.getCookiesFromHost("localhost", {
    partitionKey: `(http,localhost,${gHttpServer.identity.primaryPort})`,
  });

  is(cookies.length, 0, "No cookies should be set for the partition");

  BrowserTestUtils.removeTab(tab);
});
