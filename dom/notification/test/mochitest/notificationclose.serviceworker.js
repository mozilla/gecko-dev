// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

onnotificationclose = function (e) {
  e.waitUntil(
    (async function () {
      let windowOpened = true;
      await clients.openWindow("blank.html").catch(() => {
        windowOpened = false;
      });

      self.clients.matchAll().then(function (matchedClients) {
        if (matchedClients.length === 0) {
          dump("*** CLIENTS LIST EMPTY! Test will timeout! ***\n");
          return;
        }

        matchedClients.forEach(function (client) {
          client.postMessage({
            result:
              e.notification.data &&
              e.notification.data.complex &&
              e.notification.data.complex[0] == "jsval" &&
              e.notification.data.complex[1] == 5,
            windowOpened,
          });
        });
      });
    })()
  );
};
