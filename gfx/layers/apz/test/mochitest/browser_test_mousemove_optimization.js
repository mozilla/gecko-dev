/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

add_task(async function () {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,hello"
  );

  const mousemoveEventPromise = new Promise(resolve => {
    window.addEventListener(
      "mousemove",
      event => {
        resolve({
          type: event.type,
          clientX: event.clientX,
          clientY: event.clientY,
        });
      },
      { once: true }
    );
  });

  // Send a mousemove event on this document.
  await synthesizeNativeMouseEventWithAPZ({
    type: "mousemove",
    target: document.documentElement,
    offsetX: 100,
    offsetY: 50,
  });
  let result = await mousemoveEventPromise;
  SimpleTest.isDeeply(result, { type: "mousemove", clientX: 100, clientY: 50 });

  const mousemoveEventPromiseOnContent = SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return new Promise(resolve => {
        content.window.addEventListener(
          "mousemove",
          event => {
            resolve({
              type: event.type,
              clientX: event.clientX,
              clientY: event.clientY,
            });
          },
          { once: true }
        );
      });
    }
  );

  // Await a new SpecialPowers.spawn to flush the above queued task to the content process.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await new Promise(resolve => resolve());
  });

  // Send a mousemove event on the iframe document.
  await synthesizeNativeMouseEventWithAPZ({
    type: "mousemove",
    target: tab.linkedBrowser,
    offsetX: 100,
    offsetY: 50,
  });
  result = await mousemoveEventPromiseOnContent;
  SimpleTest.isDeeply(result, { type: "mousemove", clientX: 100, clientY: 50 });

  BrowserTestUtils.removeTab(tab);
});
