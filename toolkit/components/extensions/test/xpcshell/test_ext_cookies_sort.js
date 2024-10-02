"use strict";

const server = createHttpServer({ hosts: ["example.org"] });
server.registerPathHandler("/sortCookies", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_same_cookie_name_with_multiple_paths() {
  function contentScript() {
    document.cookie = "a=1; path=/";
    document.cookie = "a=3; path=/sub/dir";
    document.cookie = "a=4; path=/sub/dir/deeper";
    document.cookie = "a=2; path=/sub";
    browser.runtime.sendMessage("do-check-cookies");
  }
  async function background() {
    await new Promise(resolve => {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq("do-check-cookies", msg, "expected message");
        resolve();
      });
    });

    const cookies = await browser.cookies.getAll({ name: "a" });
    // Cookies should be sorted according to RFC 6256, like document.cookie.
    browser.test.assertEq(4, cookies.length, "4 cookies expected");
    browser.test.assertEq("4", cookies[0].value, "4");
    browser.test.assertEq("3", cookies[1].value, "3");
    browser.test.assertEq("2", cookies[2].value, "2");
    browser.test.assertEq("1", cookies[3].value, "1");

    const cookie = await browser.cookies.get({
      name: "a",
      url: "https://example.org/sub/dir",
    });
    browser.test.assertEq("/sub/dir", cookie?.path, "Expected exact path");

    browser.test.notifyPass("cookies");
  }

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["cookies", "*://example.org/"],
      content_scripts: [
        {
          matches: ["*://example.org/sortCookies*"],
          js: ["contentscript.js"],
        },
      ],
    },
    files: {
      "contentscript.js": contentScript,
    },
  });

  await extension.startup();

  const contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.org/sortCookies"
  );

  await extension.awaitFinish("cookies");
  await contentPage.close();
  await extension.unload();
});
