"use strict";

async function showNotification(aBrowser, aId) {
  info(`Show notification ${aId}`);
  let promise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  let notification = PopupNotifications.show(
    aBrowser /* browser */,
    "test-notification-" + aId /* id */,
    aId + ": Will you allow <> to perform this action?" /* message */,
    null /* anchorID */,
    {
      label: "Main Action",
      accessKey: "M",
      callback: () => {},
    } /* mainAction */,
    [
      {
        label: "Secondary Action",
        accessKey: "S",
        callback: () => {},
      },
    ] /* secondaryActions */
  );
  await promise;

  let rect = PopupNotifications.panel.getBoundingClientRect();
  return { notification, rect };
}

function waitForMouseEvent(aType, aElement) {
  return new Promise(resolve => {
    aElement.addEventListener(
      aType,
      e => {
        resolve({
          screenX: e.screenX,
          screenY: e.screenY,
          clientX: e.clientX,
          clientY: e.clientY,
        });
      },
      { once: true }
    );
  });
}

function waitForRemoteMouseEvent(aType, aBrowser) {
  return SpecialPowers.spawn(aBrowser, [aType], async aType => {
    return new Promise(
      resolve => {
        content.document.addEventListener(aType, e => {
          resolve({
            screenX: e.screenX,
            screenY: e.screenY,
            clientX: e.clientX,
            clientY: e.clientY,
          });
        });
      },
      { once: true }
    );
  });
}

function executeSoonRemote(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    return new Promise(resolve => {
      SpecialPowers.executeSoon(resolve);
    });
  });
}

function synthesizeMouseAtCenter(aRect) {
  EventUtils.synthesizeMouseAtPoint(
    aRect.left + aRect.width / 2,
    aRect.top + aRect.height / 2,
    {
      type: "mousemove",
    }
  );
}

let notificationRect;

add_setup(async function init() {
  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.events.async.enabled", true],
      // This test aims to synthesize mousemove events at specific times, so
      // disable the automatically synthesized mousemove events during reflow.
      ["layout.reflow.synthMouseMove", false],
    ],
  });

  info(`Show notification to get its size and position`);
  let { notification, rect } = await showNotification(
    gBrowser.selectedBrowser,
    "Test#Init"
  );
  PopupNotifications.remove(notification);
  notificationRect = rect;
});

add_task(async function test_mouseout_chrome() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    info(`Generate mousemove event on browser`);
    let mousemovePromise = waitForMouseEvent("mousemove", browser);
    synthesizeMouseAtCenter(notificationRect);
    let mousemoveCoordinate = await mousemovePromise;
    info(`mousemove event: ${JSON.stringify(mousemoveCoordinate)}`);

    info(`Showing notification should generate mouseout event on browser`);
    let mouseoutPromise = waitForMouseEvent("mouseout", browser);
    let { notification } = await showNotification(browser, "Test#Chrome");
    synthesizeMouseAtCenter(notificationRect);
    let mouseoutCoordinate = await mouseoutPromise;
    info(`mouseout event: ${JSON.stringify(mouseoutCoordinate)}`);

    SimpleTest.isDeeply(
      mouseoutCoordinate,
      mousemoveCoordinate,
      "Test event coordinate"
    );
    info(`Remove notification`);
    PopupNotifications.remove(notification);
  });
});

add_task(async function test_mouseout_content() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    info(`Generate mousemove event on content`);
    let mousemovePromise = waitForRemoteMouseEvent("mousemove", browser);
    // Ensure the event listener is registered in remote before synthesizing mouse event.
    await executeSoonRemote(browser);
    synthesizeMouseAtCenter(notificationRect);
    let mousemoveCoordinate = await mousemovePromise;
    info(`mousemove event on content: ${JSON.stringify(mousemoveCoordinate)}`);

    info(`Showing notification should generate mouseout event on content`);
    let mouseoutPromise = waitForRemoteMouseEvent("mouseout", browser);
    // Ensure the event listener is registered in remote before showing notification.
    await executeSoonRemote(browser);
    let { notification } = await showNotification(browser, "Test#Content");
    synthesizeMouseAtCenter(notificationRect);
    let mouseoutCoordinate = await mouseoutPromise;
    info(`remote mouseout event: ${JSON.stringify(mouseoutCoordinate)}`);

    SimpleTest.isDeeply(
      mouseoutCoordinate,
      mousemoveCoordinate,
      "Test event coordinate"
    );
    info(`Remove notification`);
    PopupNotifications.remove(notification);
  });
});
