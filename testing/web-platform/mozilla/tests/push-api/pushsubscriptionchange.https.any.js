// META: global=window-module
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/notifications/resources/helpers.js

let registration;

promise_setup(async () => {
  await trySettingPermission("granted");
  registration = await prepareActiveServiceWorker("push-sw.js");
});

promise_test(async (t) => {
  const promise = new Promise(r => {
    navigator.serviceWorker.addEventListener("message", r, { once: true })
  });

  const subscription = await registration.pushManager.subscribe();
  t.add_cleanup(() => subscription.unsubscribe());

  // https://w3c.github.io/push-api/#security-and-privacy-considerations
  // When a permission is revoked, the user agent MAY fire the "pushsubscriptionchange"
  // event for subscriptions created with that permission
  //
  // But Firefox fires pushsubscriptionchange on permission regrant instead of revocation.
  // https://github.com/w3c/push-api/issues/236
  await trySettingPermission("prompt");
  await trySettingPermission("granted");

  const pushSubscriptionChangeEvent = await promise;

  assert_equals(pushSubscriptionChangeEvent.data.type, "pushsubscriptionchange");
  assert_equals(pushSubscriptionChangeEvent.data.constructor, "PushSubscriptionChangeEvent");
  assert_object_equals(pushSubscriptionChangeEvent.data.oldSubscription, subscription.toJSON());
}, "Fire pushsubscriptionchange event when permission is revoked");

promise_test(async (t) => {
  const promise = new Promise(r => {
    navigator.serviceWorker.addEventListener("message", r, { once: true })
  });

  const subscription = await registration.pushManager.subscribe();
  t.add_cleanup(() => subscription.unsubscribe());

  await SpecialPowers.spawnChrome([], async () => {
    // Bug 1210943: UAID changes should drop existing push subscriptions
    const uaid = await Services.prefs.getStringPref("dom.push.userAgentID");
    await Services.prefs.setStringPref("dom.push.userAgentID", uaid + "0");
  })

  const pushSubscriptionChangeEvent = await promise;

  assert_equals(pushSubscriptionChangeEvent.data.type, "pushsubscriptionchange");
  assert_equals(pushSubscriptionChangeEvent.data.constructor, "PushSubscriptionChangeEvent");
  assert_object_equals(pushSubscriptionChangeEvent.data.oldSubscription, subscription.toJSON());
  assert_equals(pushSubscriptionChangeEvent.data.newSubscription, undefined);
}, "Fire pushsubscriptionchange event when UAID changes");
