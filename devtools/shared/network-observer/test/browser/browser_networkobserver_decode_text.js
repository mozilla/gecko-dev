/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = URL_ROOT + "doc_network-observer.html";
const JSON_URL = URL_ROOT + "slow_json.sjs";
const JSON_URL2 = URL_ROOT + "slow_json.sjs?complete";
const JSON_URL3 = URL_ROOT + "slow_json.sjs?partial";

// NetworkEventOwner which holds a channel and response content.
class ResponseContentHoldingOwner extends NetworkEventOwner {
  addResponseContent(content, { truncated }) {
    super.addResponseContent();
    this.responseContent = content.text;
    this.truncated = truncated;
  }
}

add_task(async function testDecodingResponseContent() {
  const tab = await addTab(TEST_URL);

  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel =>
      channel.URI.spec !== JSON_URL && channel.URI.spec !== JSON_URL3,
    onNetworkEvent: () => {
      const owner = new ResponseContentHoldingOwner();
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [JSON_URL, JSON_URL2],
    (_url, _url2) => {
      const xhr = new content.wrappedJSObject.XMLHttpRequest();
      xhr.addEventListener("progress", e => {
        if (e.loaded > 0) {
          const xhr2 = new content.wrappedJSObject.XMLHttpRequest();
          xhr2.open("GET", _url2);
          xhr2.send();
        }
      });
      xhr.open("GET", _url);
      xhr.send();
    }
  );

  info("Wait for all network events to be received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 1);
  is(events.length, 1, "Received the expected number of network events");

  await BrowserTestUtils.waitForCondition(() => events[0].hasResponseContent);

  is(events[0].responseContent, '"\u3042"', "expected response content");
  is(events[0].truncated, false, "response content should not be truncated");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [JSON_URL3], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  info("Wait for all network events to be received");
  await BrowserTestUtils.waitForCondition(() => events.length >= 2);
  is(events.length, 2, "Received the expected number of network events");

  await BrowserTestUtils.waitForCondition(() => events[1].hasResponseContent);

  is(events[1].responseContent, '"', "expected response content");
  is(events[1].truncated, true, "response content would be truncated");

  networkObserver.destroy();
  gBrowser.removeTab(tab);
});
