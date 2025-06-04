/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const gChromeBaseURL = getRootDirectory(gTestPath);
const gBaseURL = gChromeBaseURL.replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function synthesizeWheelFromParent() {
  async function testSynthesizeWheelFromParent(aBrowser, aAsyncEnabled) {
    info(`Testing synthesizeWheel with asyncEnabled=${aAsyncEnabled}`);

    let haveReceiveWheelEvent = false;
    const onWheel = event => {
      info(
        `Received wheel event: ${event.type} ${event.deltaX} ${event.deltaY} ${event.deltaZ} ${event.deltaMode} ${event.detail}`
      );
      haveReceiveWheelEvent = true;
    };
    aBrowser.addEventListener("wheel", onWheel, { once: true });
    await new Promise(resolve => {
      EventUtils.synthesizeWheel(
        aBrowser,
        10,
        10,
        {
          deltaMode: WheelEvent.DOM_DELTA_LINE,
          deltaY: 1.0,
          asyncEnabled: aAsyncEnabled,
        },
        window,
        () => {
          ok(haveReceiveWheelEvent, "Should have received wheel event");
          aBrowser.removeEventListener("wheel", onWheel);
          resolve();
        }
      );
    });
  }

  await BrowserTestUtils.withNewTab(
    gBaseURL + "dummy.html",
    async function (browser) {
      await testSynthesizeWheelFromParent(browser, false);
      await testSynthesizeWheelFromParent(browser, true);
    }
  );
});

add_task(async function synthesizeWheelFromContent() {
  async function testSynthesizeWheelFromContent(aBrowser, aAsyncEnabled) {
    info(`Testing synthesizeWheel with asyncEnabled=${aAsyncEnabled}`);

    await SpecialPowers.spawn(
      aBrowser,
      [aAsyncEnabled],
      async aAsyncEnabled => {
        let haveReceiveWheelEvent = false;
        const onWheel = event => {
          info(
            `Received wheel event: ${event.type} ${event.deltaX} ${event.deltaY} ${event.deltaZ} ${event.deltaMode} ${event.detail}`
          );
          haveReceiveWheelEvent = true;
        };
        content.document.addEventListener("wheel", onWheel, { once: true });
        await new Promise(resolve => {
          try {
            EventUtils.synthesizeWheel(
              content.document.body,
              10,
              10,
              {
                deltaMode: content.WheelEvent.DOM_DELTA_LINE,
                deltaY: 1.0,
                asyncEnabled: aAsyncEnabled,
              },
              content.window,
              () => {
                ok(haveReceiveWheelEvent, "Should have received wheel event");
                content.document.removeEventListener("wheel", onWheel);
                resolve();
              }
            );
            ok(!aAsyncEnabled, "synthesizeWheel should not throw");
          } catch (e) {
            ok(aAsyncEnabled, `synthesizeWheel should throw error: ${e}`);
            content.document.removeEventListener("wheel", onWheel);
            resolve();
          }
        });
      }
    );
  }

  await BrowserTestUtils.withNewTab(
    gBaseURL + "dummy.html",
    async function (browser) {
      await testSynthesizeWheelFromContent(browser, false);
      await testSynthesizeWheelFromContent(browser, true);
    }
  );
});
