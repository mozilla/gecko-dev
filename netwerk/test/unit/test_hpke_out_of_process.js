/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

XPCShellContentUtils.ensureInitialized(this);

let gHttpServer;

add_setup(async function () {
  gHttpServer = new HttpServer();
  let invalidHandler = (req, res) => {
    res.setStatusLine(req.httpVersion, 500, "Oh no, it broke");
    res.write("Uh oh, it broke.");
  };
  let validHandler = (req, res) => {
    res.setHeader("Content-Type", "application/ohttp-keys");
    res.write("1234");
  };

  gHttpServer.registerPathHandler("/.wellknown/invalid", invalidHandler);
  gHttpServer.registerPathHandler("/.wellknown/valid", validHandler);

  gHttpServer.start(-1);
});

function getLocalURL(path) {
  return `http://localhost:${gHttpServer.identity.primaryPort}/.wellknown/${path}`;
}

add_task(async function test_out_of_process_use() {
  let page = await XPCShellContentUtils.loadContentPage("about:certificate", {
    remote: true,
  });

  let fetchURL = getLocalURL("valid");
  let contentFetch = await page.spawn([fetchURL], url => {
    // eslint-disable-next-line no-shadow
    let { HPKEConfigManager } = ChromeUtils.importESModule(
      "resource://gre/modules/HPKEConfigManager.sys.mjs"
    );

    return HPKEConfigManager.get(url);
  });
  Assert.deepEqual(contentFetch, new TextEncoder().encode("1234"));
  Assert.ok(
    page.browsingContext.currentWindowGlobal.domProcess.getActor(
      "HPKEConfigManager"
    ),
    "Should be able to get a parent actor for this browsingContext"
  );

  let randomPage = await XPCShellContentUtils.loadContentPage(
    "data:text/html,2",
    {
      remote: true,
    }
  );

  await Assert.rejects(
    randomPage.spawn([fetchURL], async url => {
      // eslint-disable-next-line no-shadow
      let { HPKEConfigManager } = ChromeUtils.importESModule(
        "resource://gre/modules/HPKEConfigManager.sys.mjs"
      );

      return HPKEConfigManager.get(url);
    }),
    /cannot be used/,
    "Shouldn't be able to use HPKEConfigManager from random content processes."
  );
  Assert.throws(
    () =>
      randomPage.browsingContext.currentWindowGlobal.domProcess.getActor(
        "HPKEConfigManager"
      ),
    /Process protocol .*support remote type/,
    "Should not be able to get a parent actor for a non-privilegedabout browsingContext"
  );
});
