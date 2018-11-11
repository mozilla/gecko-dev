/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#addAutomationEvent();
 */

add_task(async function() {
  const { target, front } = await initBackend(SIMPLE_CONTEXT_URL);
  const [_, [destNode, oscNode, gainNode]] = await Promise.all([
    front.setup({ reload: true }),
    get3(front, "create-node"),
  ]);
  let count = 0;
  const counter = () => count++;
  front.on("automation-event", counter);

  const t0 = 0, t1 = 0.1, t2 = 0.2, t3 = 0.3, t4 = 0.4, t5 = 0.6, t6 = 0.7, t7 = 1;
  const curve = [-1, 0, 1];
  await oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.2, t0]);
  await oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.3, t1]);
  await oscNode.addAutomationEvent("frequency", "setValueAtTime", [0.4, t2]);
  await oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [1, t3]);
  await oscNode.addAutomationEvent("frequency", "linearRampToValueAtTime", [0.15, t4]);
  await oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.75, t5]);
  await oscNode.addAutomationEvent("frequency", "exponentialRampToValueAtTime", [0.5, t6]);
  await oscNode.addAutomationEvent("frequency", "setValueCurveAtTime", [curve, t7, t7 - t6]);
  await oscNode.addAutomationEvent("frequency", "setTargetAtTime", [20, 2, 5]);

  ok(true, "successfully set automation events for valid automation events");

  try {
    await oscNode.addAutomationEvent("frequency", "notAMethod", 20, 2, 5);
    ok(false, "non-automation methods should not be successful");
  } catch (e) {
    ok(/invalid/.test(e.message), "AudioNode:addAutomationEvent fails for invalid automation methods");
  }

  try {
    await oscNode.addAutomationEvent("invalidparam", "setValueAtTime", 0.2, t0);
    ok(false, "automating non-AudioParams should not be successful");
  } catch (e) {
    ok(/invalid/.test(e.message), "AudioNode:addAutomationEvent fails for a non AudioParam");
  }

  front.off("automation-event", counter);

  is(count, 9,
    "when calling `addAutomationEvent`, the WebAudioActor should still fire `automation-event`.");

  await removeTab(target.tab);
});
