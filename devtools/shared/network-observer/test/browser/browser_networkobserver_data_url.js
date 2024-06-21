/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEX/TQBcNTh/AAAAAXRSTlPM0jRW/QAAAApJREFUeJxjYgAAAAYAAzY3fKgAAAAASUVORK5CYII=";

add_task(async function () {
  const onNetworkEvents = waitForNetworkEvents(TEST_URL, 1, true);

  await addTab(TEST_URL);

  const events = await onNetworkEvents;
  is(events.length, 1, "Received the expected number of network events");
});
