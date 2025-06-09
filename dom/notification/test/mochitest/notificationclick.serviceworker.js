// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

onnotificationclick = function (e) {
  const {
    notification: { data, actions },
    action,
  } = e;

  self.clients.matchAll().then(function (matchedClients) {
    if (matchedClients.length === 0) {
      dump(
        "********************* CLIENTS LIST EMPTY! Test will timeout! ***********************\n"
      );
      return;
    }

    matchedClients.forEach(function (client) {
      client.postMessage({
        notification: { data, actions },
        action,
      });
    });
  });
};
