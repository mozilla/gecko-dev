/**
 * This is similar to the hitTest function in apz_test_utils.js, in that it
 * does a hit-test for a point and returns the result. The difference is that
 * in the fission world, the hit-test may land on an OOPIF, which means the
 * result information will be in the APZ test data for the OOPIF process. This
 * function checks both the current process and OOPIF process to see which one
 * got a hit result, and returns the result regardless of which process got it.
 * The caller is expected to check the layers id which will allow distinguishing
 * the two cases.
 */
async function hitTestOOPIF(point, iframeElement) {
  let getIframeCompositorTestData = async iframe => {
    let data = await SpecialPowers.spawn(iframe, [], () => {
      let utils = SpecialPowers.getDOMWindowUtils(content.window);
      return JSON.stringify(utils.getCompositorAPZTestData());
    });

    return JSON.parse(data);
  };

  let utils = SpecialPowers.getDOMWindowUtils(window);

  // Get the test data before doing the actual hit-test, to get a baseline
  // of what we can ignore.
  let oldParentTestData = utils.getCompositorAPZTestData();
  let oldIframeTestData = await getIframeCompositorTestData(iframeElement);

  let hittestPromise = SpecialPowers.spawnChrome([], () => {
    return new Promise(resolve => {
      browsingContext.topChromeWindow.addEventListener(
        "MozMouseHittest",
        () => {
          resolve();
        },
        { once: true }
      );
    });
  });
  await SpecialPowers.executeAfterFlushingMessageQueue();

  // Now do the hit-test
  dump(`Hit-testing point (${point.x}, ${point.y}) in fission context\n`);
  utils.sendMouseEvent(
    "MozMouseHittest",
    point.x,
    point.y,
    0,
    0,
    0,
    true,
    0,
    0,
    true,
    false /* aIsWidgetEventSynthesized */
  );

  await hittestPromise;

  // Collect the new test data
  let newParentTestData = utils.getCompositorAPZTestData();
  let newIframeTestData = await getIframeCompositorTestData(iframeElement);

  // See which test data has new hit results
  let hitResultCount = testData => {
    return Object.keys(testData.hitResults).length;
  };

  let hitIframe =
    hitResultCount(newIframeTestData) > hitResultCount(oldIframeTestData);
  let hitParent =
    hitResultCount(newParentTestData) > hitResultCount(oldParentTestData);

  // Extract the results from the appropriate test data
  let lastHitResult = testData => {
    let lastHit =
      testData.hitResults[Object.keys(testData.hitResults).length - 1];
    return {
      hitInfo: lastHit.hitResult,
      scrollId: lastHit.scrollId,
      layersId: lastHit.layersId,
    };
  };
  if (hitIframe && hitParent) {
    throw new Error(
      "Both iframe and parent got hit-results, that is unexpected!"
    );
  } else if (hitIframe) {
    return lastHitResult(newIframeTestData);
  } else if (hitParent) {
    return lastHitResult(newParentTestData);
  } else {
    throw new Error(
      "Neither iframe nor parent got the hit-result, that is unexpected!"
    );
  }
}
