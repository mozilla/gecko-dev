/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { CookieXPCShellUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CookieXPCShellUtils.sys.mjs"
);

CookieXPCShellUtils.init(this);

let gHits = 0;

add_task(async function () {
  do_get_profile();

  info("Disable predictor and accept all");
  Services.prefs.setBoolPref("network.predictor.enabled", false);
  Services.prefs.setBoolPref("network.predictor.enable-prefetch", false);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  const server = CookieXPCShellUtils.createServer({
    hosts: ["example.org", "foo.com"],
  });
  server.registerPathHandler("/image.png", (metadata, response) => {
    gHits++;
    response.setHeader("Cache-Control", "max-age=10000", false);
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "image/png", false);
    var body = atob(
      "iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAAEUlEQVQImWP4z8AAQTAamQkAhpcI+DeMzFcAAAAASUVORK5CYII="
    );
    response.bodyOutputStream.write(body, body.length);
  });

  server.registerPathHandler("/image", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    var body = `<img src="http://example.org/image.png">`;
    response.bodyOutputStream.write(body, body.length);
  });

  const hitsCount = 2;

  info("Clear image and network caches");
  let imageCache = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .getImgCacheForDocument(null);
  imageCache.clearCache(); // no parameter=all
  Services.cache2.clear();

  info("Reset the hits count");
  gHits = 0;

  info("Let's load a page with origin A");
  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/image"
  );
  await contentPage.close();

  info("Let's load a page with origin B");
  contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://foo.com/image"
  );
  await contentPage.close();

  Assert.equal(gHits, hitsCount, "The number of hits match");
});
