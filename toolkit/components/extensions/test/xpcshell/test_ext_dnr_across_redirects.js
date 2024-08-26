/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const server = createHttpServer({ hosts: ["example.com", "redir"] });

server.registerPathHandler("/redirect_from_303", (req, res) => {
  res.setStatusLine(req.httpVersion, 303, "See Other");
  res.setHeader("Location", "/redirect_to?303");
});

server.registerPathHandler("/redirect_from_307", (req, res) => {
  res.setStatusLine(req.httpVersion, 307, "Temporary Redirect");
  res.setHeader("Location", "/redirect_to?307");
});

server.registerPathHandler("/redirect_to", (req, res) => {
  info(`Received request: ${req.method} ${req.url}`);
  // Return the method as seen by the server, so that the test can confirm that
  // the method as seen by the server matches the method as seen by DNR.
  res.setHeader("outcome", "server-" + req.method);
});

add_task(async function test_match_across_redirects() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      host_permissions: ["<all_urls>"],
      permissions: ["declarativeNetRequestWithHostAccess"],
    },
    async background() {
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: {
              requestMethods: ["get", "post"],
              urlFilter: "/redirect_from",
            },
            action: {
              type: "modifyHeaders",
              responseHeaders: [
                // Because the request is redirected, the fetch() caller should
                // not observe this header:
                { operation: "append", header: "outcome", value: "pre_redir" },
              ],
            },
          },
          {
            id: 2,
            // HTTP 302 and 303 should convert redirects to GET:
            condition: {
              requestMethods: ["get"],
              urlFilter: "/redirect_to",
            },
            action: {
              type: "modifyHeaders",
              responseHeaders: [
                { operation: "append", header: "outcome", value: "match_get" },
              ],
            },
          },
          {
            // HTTP 307 should preserve HTTP method.
            id: 3,
            condition: {
              requestMethods: ["post"],
              urlFilter: "/redirect_to",
            },
            action: {
              type: "modifyHeaders",
              responseHeaders: [
                { operation: "append", header: "outcome", value: "match_post" },
              ],
            },
          },
        ],
      });

      async function getOutcome(method, path) {
        const url = `http://example.com${path}`;
        const { headers } = await fetch(url, { method });
        return headers.get("outcome");
      }

      // Regression test for bug 1909081:
      browser.test.assertEq(
        "server-GET, match_get",
        await getOutcome("POST", "/redirect_from_303"),
        "HTTP 303 changes POST to GET, and DNR should match GET"
      );

      browser.test.assertEq(
        "server-POST, match_post",
        await getOutcome("POST", "/redirect_from_307"),
        "HTTP 307 keeps POST as POST, and DNR should match POST"
      );

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
