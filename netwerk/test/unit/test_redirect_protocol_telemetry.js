"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

add_setup(function () {
  do_get_profile();

  Services.fog.initializeFOG();
});

add_task(async function check_protocols() {
  // Enable the collection (during test) for all products so even products
  // that don't collect the data will be able to run the test without failure.
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  let httpserv = new HttpServer();
  httpserv.registerPathHandler("/redirect", redirectHandler);
  httpserv.registerPathHandler("/content", contentHandler);
  httpserv.start(-1);

  var responseProtocol;

  function redirectHandler(metadata, response) {
    response.setStatusLine(metadata.httpVersion, 301, "Moved");
    let location =
      responseProtocol == "data"
        ? "data:text/plain,test"
        : `${responseProtocol}://localhost:${httpserv.identity.primaryPort}/content`;
    response.setHeader("Location", location, false);
    response.setHeader("Cache-Control", "no-cache", false);
  }

  function contentHandler(metadata, response) {
    let responseBody = "Content body";
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write(responseBody, responseBody.length);
  }

  function make_test(protocol) {
    return new Promise(resolve => {
      const URL = `http://localhost:${httpserv.identity.primaryPort}/redirect`;
      responseProtocol = protocol;
      let channel = make_channel(URL);
      let p = new Promise(resolve1 =>
        channel.asyncOpen(new ChannelListener(resolve1))
      );
      p.then(() => {
        Assert.equal(
          Glean.networking.httpRedirectToSchemeSubresource[
            protocol
          ].testGetValue(),
          1,
          "The total value recorded in httpRedirectToSchemeSubresource should be 1."
        );
        resolve();
      });
    });
  }

  await make_test("http");
  await make_test("data");

  await new Promise(resolve => httpserv.stop(resolve));
});
