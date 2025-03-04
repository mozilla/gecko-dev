"use strict";

const BASE = "http://example.com/data/";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerDirectory("/data/", do_get_file("data"));

const TEST_URL = `${BASE}/file_webRequest_handlerBehaviorChanged.html`;
const JS_URL = `${BASE}/file_webRequest_handlerBehaviorChanged.js`;
const CSS_URL = `${BASE}/file_webRequest_handlerBehaviorChanged.css`;
const PNG_URL = `${BASE}/file_webRequest_handlerBehaviorChanged.png`;

add_task(async function test_handlerBehaviorChanged_onBeforeRequest() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/*"],
    },
    background: () => {
      const logs = [];

      browser.webRequest.handlerBehaviorChanged().then(() => {
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            logs.push(details.url);
            if (logs.length == 4) {
              browser.test.sendMessage("logs", logs);
            } else if (logs.length > 4) {
              browser.test.fail(`Unexpected request: ${details.url}`);
            }
          },
          {
            urls: [
              "http://example.com/*file_webRequest_handlerBehaviorChanged*",
            ],
          }
        );

        browser.test.sendMessage("ready");
      });
    },
  });

  const contentPage = await ExtensionTestUtils.loadContentPage(TEST_URL);

  await extension.startup();
  await extension.awaitMessage("ready");

  // NOTE: contentPage.loadURL doesn't wait for resources.
  //       Let the extension wait for all resources and then send the logs.
  await contentPage.loadURL(TEST_URL);
  const logs = await extension.awaitMessage("logs");
  Assert.deepEqual(
    logs.toSorted(),
    [CSS_URL, TEST_URL, JS_URL, PNG_URL],
    "If the extension clears the cache, all requests should be triggered"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_handlerBehaviorChanged_callback() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest"],
    },
    background: () => {
      browser.webRequest.handlerBehaviorChanged(result => {
        browser.test.assertEq(
          result,
          undefined,
          "handlerBehaviorChanged should call callback with undefined"
        );
        browser.test.sendMessage("done");
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function test_handlerBehaviorChanged_promise() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest"],
    },
    background: () => {
      browser.webRequest.handlerBehaviorChanged().then(result => {
        browser.test.assertEq(
          result,
          undefined,
          "handlerBehaviorChanged should resolve the promise with undefined"
        );
        browser.test.sendMessage("done");
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function test_handlerBehaviorChanged_limit() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest"],
    },
    background: () => {
      browser.test.onMessage.addListener((msg, count) => {
        browser.test.assertEq(msg, `call handlerBehaviorChanged`);
        const promises = [];
        for (let i = 0; i < count; i++) {
          promises.push(browser.webRequest.handlerBehaviorChanged());
        }
        Promise.all(promises).then(() => {
          browser.test.sendMessage("done");
        });
      });
    },
  });

  await extension.startup();

  const warningPattern =
    /The number of webRequest.handlerBehaviorChanged calls exceeds the limit/;

  info("Verifying that handlerBehaviorChanged can be called a few times");
  let { messages } = await promiseConsoleOutput(async () => {
    extension.sendMessage("call handlerBehaviorChanged", 20);
    await extension.awaitMessage("done");
  });
  AddonTestUtils.checkMessages(messages, {
    forbidden: [{ message: warningPattern }],
  });

  info("Verifying that handlerBehaviorChanged warns at limit");
  let { messages: messages2 } = await promiseConsoleOutput(async () => {
    extension.sendMessage("call handlerBehaviorChanged", 1);
    await extension.awaitMessage("done");
  });
  AddonTestUtils.checkMessages(messages2, {
    expected: [{ message: warningPattern }],
  });

  await extension.unload();
});
