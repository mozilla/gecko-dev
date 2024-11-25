/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_data_channel_in_parent_observer() {
  const TEST_URI = "data:text/html;charset=utf-8,<h1>Test";
  const onDataChannelNotification = waitForDataChannelNotification(
    TEST_URI,
    "text/html",
    true
  );

  let tab = await BrowserTestUtils.addTab(gBrowser, TEST_URI);
  await onDataChannelNotification;
  info("We received the observer notification");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_data_channel_in_content_observer() {
  const IMAGE_DATE_URI =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQYV2P4DwABAQEAWk1v8QAAAABJRU5ErkJggg==";

  const onDataChannelNotification = waitForDataChannelNotification(
    IMAGE_DATE_URI,
    "image/png",
    false
  );
  const tab = await BrowserTestUtils.addTab(
    gBrowser,
    `https://example.com/document-builder.sjs?html=<img src='${IMAGE_DATE_URI}'>`
  );
  await onDataChannelNotification;
  info("We received the observer notification");
  await BrowserTestUtils.removeTab(tab);
});

async function waitForDataChannelNotification(uri, contentType, isDocument) {
  let receivedNotifications = 0;
  const observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe: function observe(subject, topic) {
      switch (topic) {
        case "data-channel-opened": {
          ok(
            subject instanceof Ci.nsIDataChannel,
            "Channel should be a nsIDataChannel instance"
          );
          ok(
            subject instanceof Ci.nsIIdentChannel,
            "Channel should be a nsIIdentChannel instance"
          );

          const channel = subject.QueryInterface(Ci.nsIChannel);
          channel.QueryInterface(Ci.nsIIdentChannel);

          if (channel.URI.spec === uri) {
            is(
              channel.contentType,
              contentType,
              "Data channel has the expected content type"
            );

            is(
              channel.isDocument,
              isDocument,
              "Data channel has the expected isDocument flag"
            );

            is(
              typeof channel.channelId,
              "number",
              "Data channel has a valid channelId"
            );

            receivedNotifications++;
          }
          break;
        }
      }
    },
  };
  Services.obs.addObserver(observer, "data-channel-opened");
  await BrowserTestUtils.waitForCondition(() => receivedNotifications > 0);
  is(receivedNotifications, 1, "Received exactly one notification");
  Services.obs.removeObserver(observer, "data-channel-opened");
}
