// META: title=CookieStore queues events when bfcached
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js

'use strict';

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();

  // Open a window with noopener so that BFCache will work.
  const rc = await rcHelper.addWindow(null, { features: "noopener" });

  await rc.executeScript(() => {
    window.events = [];
    window.addEventListener('pageshow', (event) => {
      window.events.push("pageshow:" + event.persisted);
    });

    cookieStore.addEventListener('change', () => {
      window.events.push("cookie");
    }, {once: true});
  });

  const rc2 = await rc.navigateToNew();

  await rc2.executeScript(() => {
    document.cookie = "BFCACHE=1; path=/";
  });

  await rc2.historyBack();

  assert_equals(
    await rc.executeScript(() => window.events.join("-")), "pageshow:true-cookie",
    'precondition: document was bfcached'
  );

  await rc.executeScript(async () => {
   await cookieStore.delete("BFCACHE");
  });
});
