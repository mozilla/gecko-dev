// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// FIXME(kitcambridge): Enable when Web Crypto is exposed to
// workers (bug 842818).

// importScripts("/tests/dom/push/test/webpush.js");

this.onpush = handlePush;
this.onmessage = handleMessage;

function handlePush(event) {

  self.clients.matchAll().then(function(result) {
    // FIXME(nsm): Bug 1149195 will fix data exposure.
    if (event instanceof PushEvent) {
      if (!('data' in event)) {
        result[0].postMessage({type: "finished", okay: "yes"});
        return;
      }
      var text = event.data.text();
      result[0].postMessage({type: "finished", okay: "yes", text: text});
      return;
    }
    result[0].postMessage({type: "finished", okay: "no"});
  });
}

function handleMessage(event) {
  if (event.data.type == "publicKey") {
    self.registration.pushManager.getSubscription().then(subscription => {
      event.ports[0].postMessage(subscription.p256dh);
    });
  } else if (event.data.type == "push") {
    self.registration.pushManager.getSubscription().then(subscription =>
      webpush(subscription, "Push message from worker")
    ).then(response => {
      event.ports[0].postMessage({ status: response.status });
    });
  }
}
