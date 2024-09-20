/* eslint-env serviceworker */

const gRoot = "https://example.com/tests/dom/notification/test/mochitest/";
const gTestURL = gRoot + "test_notification_serviceworker_openWindow.html";
const gClientURL = gRoot + "file_notification_openWindow.html";

onmessage = function (event) {
  if (event.data !== "DONE") {
    dump(`ERROR: received unexpected message: ${JSON.stringify(event.data)}\n`);
  }

  event.waitUntil(
    clients.matchAll({ includeUncontrolled: true }).then(cl => {
      for (let client of cl) {
        // The |gClientURL| window closes itself after posting the DONE message,
        // so we don't need to send it anything here.
        if (client.url === gTestURL) {
          client.postMessage("DONE");
        }
      }
    })
  );
};

onnotificationclick = () => {
  clients.openWindow(gClientURL);
};
