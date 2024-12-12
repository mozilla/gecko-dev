/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test the JS Tracing feature.
 */
add_task(async function test_profile_feature_jstracing() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  await ProfilerTestUtils.startProfiler({ features: ["tracing"] });

  const url = BASE_URL_HTTPS + "tracing.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    {
      const { contentThread } = await stopProfilerNowAndGetThreads(contentPid);

      // First lookup for all our expected symbols in the string table
      const functionAFrameStringIdx = contentThread.stringTable.indexOf(
        `a (${url}:7:15)`
      );
      Assert.greater(
        functionAFrameStringIdx,
        0,
        "Found string for 'a' method call"
      );
      const functionBFrameStringIdx = contentThread.stringTable.indexOf(
        `b (${url}:10:15)`
      );
      Assert.greater(
        functionBFrameStringIdx,
        0,
        "Found string for 'b' method call"
      );
      const clickEventStringIdx = contentThread.stringTable.indexOf(`click`);
      Assert.greater(
        clickEventStringIdx,
        0,
        "Found string for 'click' DOM event"
      );
      const customEventStringIdx =
        contentThread.stringTable.indexOf(`CustomEvent`);
      Assert.greater(
        customEventStringIdx,
        0,
        "Found string for 'CustomEvent' DOM event"
      );
      const customEventHandlerStringIdx = contentThread.stringTable.indexOf(
        `customEventHandler (${url}:18:71)`
      );
      Assert.greater(
        customEventHandlerStringIdx,
        0,
        "Found string for 'customEventHandler' method call"
      );
      const navigatorUserAgentStringIdx = contentThread.stringTable.indexOf(
        `(DOM) Navigator.userAgent`
      );
      Assert.greater(
        navigatorUserAgentStringIdx,
        0,
        "Found string for 'navigator.userAgent' getter"
      );
      const clickMethodStringIdx = contentThread.stringTable.indexOf(
        `(DOM) HTMLElement.click`
      );
      Assert.greater(
        clickMethodStringIdx,
        0,
        "Found string for 'body.click' call"
      );
      const eventTargetDispatchEventStringIdx =
        contentThread.stringTable.indexOf(`(DOM) EventTarget.dispatchEvent`);
      Assert.greater(
        clickMethodStringIdx,
        0,
        "Found string for 'window.dispatchEvent' call"
      );

      // Then lookup for the matching frame, based on the string index
      const { frameTable } = contentThread;
      const FRAME_LOCATION_SLOT = frameTable.schema.location;
      const FRAME_CATEGORY_SLOT = frameTable.schema.category;
      const FUNCTION_CALL_CATEGORY = 4;
      const EVENT_CATEGORY = 8;
      const functionAFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == functionAFrameStringIdx
      );
      Assert.greater(functionAFrameIdx, 0, "Found frame for 'a' method call");
      Assert.equal(
        frameTable.data[functionAFrameIdx][FRAME_CATEGORY_SLOT],
        FUNCTION_CALL_CATEGORY
      );
      const functionBFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == functionBFrameStringIdx
      );
      Assert.greater(functionBFrameIdx, 0, "Found frame for 'b' method call");
      Assert.equal(
        frameTable.data[functionBFrameIdx][FRAME_CATEGORY_SLOT],
        FUNCTION_CALL_CATEGORY
      );
      const clickEventFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == clickEventStringIdx
      );
      Assert.greater(
        clickEventFrameIdx,
        0,
        "Found frame for 'click' DOM event"
      );
      Assert.equal(
        frameTable.data[clickEventFrameIdx][FRAME_CATEGORY_SLOT],
        EVENT_CATEGORY
      );
      const customEventFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == customEventStringIdx
      );
      Assert.greater(
        customEventFrameIdx,
        0,
        "Found frame for 'CustomEvent' DOM event"
      );
      Assert.equal(
        frameTable.data[customEventFrameIdx][FRAME_CATEGORY_SLOT],
        EVENT_CATEGORY
      );
      const customEventHandlerFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == customEventHandlerStringIdx
      );
      Assert.greater(
        customEventHandlerFrameIdx,
        0,
        "Found frame for 'b' method call"
      );
      Assert.equal(
        frameTable.data[customEventHandlerFrameIdx][FRAME_CATEGORY_SLOT],
        FUNCTION_CALL_CATEGORY
      );
      const clickMethodFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == clickMethodStringIdx
      );
      Assert.greater(
        clickMethodFrameIdx,
        0,
        "Found frame for 'body.click' method call"
      );
      const navigatorUserAgentFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == navigatorUserAgentStringIdx
      );
      Assert.greater(
        navigatorUserAgentFrameIdx,
        0,
        "Found frame for 'navigator.userAgent' getter"
      );
      Assert.equal(
        frameTable.data[navigatorUserAgentFrameIdx][FRAME_CATEGORY_SLOT],
        EVENT_CATEGORY
      );
      const eventTargetDispatchEventFrameIdx = frameTable.data.findIndex(
        frame => frame[FRAME_LOCATION_SLOT] == eventTargetDispatchEventStringIdx
      );
      Assert.greater(
        eventTargetDispatchEventFrameIdx,
        0,
        "Found frame for 'window.dispatchEvent' call"
      );
      Assert.equal(
        frameTable.data[eventTargetDispatchEventFrameIdx][FRAME_CATEGORY_SLOT],
        EVENT_CATEGORY
      );

      // Finally, assert that the stacks are correct.
      // Each symbol's frame is visible in a stack, and the stack tree is valid
      const { stackTable } = contentThread;
      const STACK_FRAME_SLOT = stackTable.schema.frame;
      const STACK_PREFIX_SLOT = stackTable.schema.prefix;
      const functionAFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == functionAFrameIdx
      );
      const functionBFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == functionBFrameIdx
      );
      const clickEventFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == clickEventFrameIdx
      );
      const customEventFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == customEventFrameIdx
      );
      const customEventHandlerEventFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == customEventHandlerFrameIdx
      );
      const clickMethodFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == clickMethodFrameIdx
      );
      const navigatorUserAgentFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == navigatorUserAgentFrameIdx
      );
      const eventTargetDispatchEventFrame = stackTable.data.find(
        stack => stack[STACK_FRAME_SLOT] == eventTargetDispatchEventFrameIdx
      );
      Assert.equal(
        getCallerNameFromStackIdx(
          contentThread,
          functionAFrame[STACK_PREFIX_SLOT]
        ),
        "load",
        "'a' was called from 'load'"
      );
      Assert.equal(
        functionBFrame[STACK_PREFIX_SLOT],
        stackTable.data.indexOf(functionAFrame),
        "'b' was called from 'a'"
      );
      Assert.equal(
        clickEventFrame[STACK_PREFIX_SLOT],
        stackTable.data.indexOf(clickMethodFrame),
        "'click' event fired from 'body.click' method call"
      );
      Assert.equal(
        clickMethodFrame[STACK_PREFIX_SLOT],
        stackTable.data.indexOf(functionBFrame),
        "'body.click' method was called from 'b'"
      );
      Assert.equal(
        navigatorUserAgentFrame[STACK_PREFIX_SLOT],
        stackTable.data.indexOf(functionBFrame),
        "'navigator.userAgent' was queried from 'b'"
      );
      Assert.equal(
        customEventFrame[STACK_PREFIX_SLOT],
        stackTable.data.indexOf(eventTargetDispatchEventFrame),
        "'CustomEvent' event fired from 'window.dispatchEvent()' method call"
      );
      Assert.equal(
        customEventHandlerEventFrame[STACK_PREFIX_SLOT],
        stackTable.data.indexOf(customEventFrame),
        "'customEventHandler' function is called because of the CustomEvent Event"
      );
    }
  });
});

function getCallerNameFromStackIdx(thread, stackIdx) {
  const { stackTable, frameTable } = thread;
  const frameIdx = stackTable.data[stackIdx][stackTable.schema.frame];
  return thread.stringTable[
    frameTable.data[frameIdx][frameTable.schema.location]
  ];
}

/**
 * This function takes a thread, and a sample tuple from the "data" array, and
 * inflates the frame to be an array of strings.
 *
 * @param {Object} thread - The thread from the profile.
 * @param {Array} sample - The tuple from the thread.samples.data array.
 * @returns {Array<string>} An array of function names.
 */
function getInflatedStackLocations(thread, sample) {
  let stackTable = thread.stackTable;
  let frameTable = thread.frameTable;
  let stringTable = thread.stringTable;
  let SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  let STACK_PREFIX_SLOT = stackTable.schema.prefix;
  let STACK_FRAME_SLOT = stackTable.schema.frame;
  let FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  let locations = [];
  while (stackIndex !== null) {
    let stackEntry = stackTable.data[stackIndex];
    let frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}
