// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//

/* eslint-env serviceworker */

onnotificationclick = (e) => {
  const {
    notification: {
      title,
      dir,
      body,
      tag,
      icon,
      requireInteraction,
      silent,
      data,
      actions,
    },
    action,
  } = e;

  self.clients.matchAll({ includeUncontrolled: true }).then(function (clients) {
    if (clients.length === 0) {
      dump(
        "********************* CLIENTS LIST EMPTY! Test will timeout! ***********************\n"
      );
      return;
    }

    clients.forEach(function (client) {
      client.postMessage({
        notification: {
          title,
          dir,
          body,
          tag,
          icon,
          requireInteraction,
          silent,
          data,
          actions
        },
        action,
      });
    });
  })
};
