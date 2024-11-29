/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_file_channel_observer() {
  const TEST_URI = Services.io.newFileURI(
    new FileUtils.File(getTestFilePath("file_channel.html"))
  ).spec;

  // The channel for the document is created in the parent process.
  const waitForHTML = waitForFileChannelNotification(
    "file_channel.html",
    "text/html",
    true
  );

  // The channel for the image is created in the content process.
  // file_channel.html loads res_img.png using a relative path.
  const waitForImage = waitForFileChannelNotification(
    "res_img.png",
    "image/png",
    false
  );

  let tab = await BrowserTestUtils.addTab(gBrowser, TEST_URI);
  await Promise.all([waitForHTML, waitForImage]);
  info("We received the expected observer notifications");
  await BrowserTestUtils.removeTab(tab);
});

async function waitForFileChannelNotification(
  fileName,
  contentType,
  isDocument
) {
  let receivedNotifications = 0;
  const observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe: function observe(subject, topic) {
      switch (topic) {
        case "file-channel-opened": {
          ok(
            subject instanceof Ci.nsIFileChannel,
            "Channel should be a nsIFileChannel instance"
          );
          ok(
            subject instanceof Ci.nsIIdentChannel,
            "Channel should be a nsIIdentChannel instance"
          );

          const channel = subject.QueryInterface(Ci.nsIChannel);
          channel.QueryInterface(Ci.nsIIdentChannel);

          is(
            typeof channel.channelId,
            "number",
            "File channel has a valid channelId"
          );

          // Note: Here we don't compare the exact uri and match the filename:
          // - URIs built via newFileURI refer to the symlink in objdir
          // - channel URI will be the actual file URI
          // Using originalURI work for the html file, but not for img file.
          if (channel.URI.spec.endsWith(fileName)) {
            is(
              channel.contentType,
              contentType,
              "File channel has the expected content type"
            );

            is(
              channel.isDocument,
              isDocument,
              "File channel has the expected isDocument flag"
            );

            receivedNotifications++;
          }
          break;
        }
      }
    },
  };
  Services.obs.addObserver(observer, "file-channel-opened");
  await BrowserTestUtils.waitForCondition(() => receivedNotifications > 0);
  is(receivedNotifications, 1, "Received exactly one notification");
  Services.obs.removeObserver(observer, "file-channel-opened");
}
