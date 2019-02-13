/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let { devtools } =
  Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  getChromeActors((client, response) => {
    let options = {
      form: response,
      client: client,
      chrome: true
    };

    devtools.TargetFactory.forRemoteTab(options).then(target => {
      target.on("close", () => {
        ok(true, "Target was closed");
        finish();
      });
      client.close();
    });
  });
}
