/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that fonts usage can be revealed in the page using the FontsHighlighter.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function () {
  // Make sure the toolbox is tall enough to accomodate all fonts, otherwise mouseover
  // events simulation will fail.
  await pushPref("devtools.toolbox.footer.height", 500);

  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  await testFontHighlighting(view, inspector);

  info("Check that highlighting still works after reloading the page");
  await reloadBrowser();

  await testFontHighlighting(view, inspector);

  info("Check that highlighting works for iframe nodes");
  await testFontHighlightingInIframe(view, inspector);
});

async function testFontHighlighting(view, inspector) {
  await selectNode("body", inspector);
  // The number of window selection change events we expect to get as we hover over each
  // font in the list. Waiting for those events is how we know that text-runs were
  // highlighted in the page.
  const expectedSelectionChangeEvents = [1, 1, 1, 1, 1];

  const viewDoc = view.document;

  // Wait for the view to have all the expected used fonts.
  const fontEls = (
    await waitFor(() => {
      const els = getUsedFontsEls(viewDoc);

      // TODO: We should expect an exact match, but after removing client side
      // throttling there is sometimes a 6th font picked up on reload for this
      // test page.
      if (els.length < expectedSelectionChangeEvents.length) {
        return false;
      }

      return [...els];
    })
  ).filter(el => {
    // TODO: After removing client-side throttling, the test will request fonts
    // too quickly on reload and will sometimes pickup an extra font such as
    // "Helvetica" on macos. This font will not match any element on the page,
    // ignore it for now.
    const expectedFonts = ["ostrich", "arial", "liberation"];
    const font = el.textContent.toLowerCase();
    return expectedFonts.some(f => font.includes(f));
  });

  // See TODO above, we temporarily filter out unwanted fonts picked up on
  // reload.
  ok(
    !!fontEls.length,
    "After filtering out unwanted fonts, we still have fonts to test"
  );

  for (let i = 0; i < fontEls.length; i++) {
    info(
      `Mousing over and out of font number ${i} ("${fontEls[i].textContent}") in the list`
    );

    const expectedEvents = expectedSelectionChangeEvents[i];

    // Simulating a mouse over event on the font name and expecting a selectionchange.
    const nameEl = fontEls[i];
    let onEvents = waitForNSelectionEvents(
      gBrowser.selectedBrowser,
      expectedEvents
    );
    EventUtils.synthesizeMouse(
      nameEl,
      2,
      2,
      { type: "mouseover" },
      viewDoc.defaultView
    );
    await onEvents;

    ok(true, `${expectedEvents} selectionchange events detected on mouseover`);

    // Simulating a mouse out event on the font name and expecting a selectionchange.
    const otherEl = viewDoc.querySelector("body");
    onEvents = waitForNSelectionEvents(gBrowser.selectedBrowser, 1);
    EventUtils.synthesizeMouse(
      otherEl,
      2,
      2,
      { type: "mouseover" },
      viewDoc.defaultView
    );
    await onEvents;

    ok(true, "1 selectionchange events detected on mouseout");
  }
}

async function testFontHighlightingInIframe(view, inspector) {
  await selectNodeInFrames(["iframe", "div"], inspector);

  const viewDoc = view.document;

  // Wait for the view to have all the expected used fonts.
  const fontEls = await waitFor(() => {
    const els = getUsedFontsEls(viewDoc);

    if (!els.length) {
      return false;
    }

    return [...els];
  });

  is(fontEls.length, 1, "There's only one font used in the iframe document");
  ok(
    // On Linux, Times New Roman does not exist. Liberation Serif is used instead.
    ["Times New Roman", "Liberation Serif"].includes(fontEls[0].innerText),
    "The expected font is displayed"
  );

  const iframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );

  info(`Mousing over and out of the font  in the list`);

  // Simulating a mouse over event on the font name and expecting a selectionchange.
  const nameEl = fontEls[0];
  let onEvents = waitForNSelectionEvents(iframeBrowsingContext, 1);
  EventUtils.synthesizeMouse(
    nameEl,
    2,
    2,
    { type: "mouseover" },
    viewDoc.defaultView
  );
  await onEvents;

  ok(true, `A selectionchange events detected on mouseover`);

  // Simulating a mouse out event on the font name and expecting a selectionchange.
  const otherEl = viewDoc.querySelector("body");
  onEvents = waitForNSelectionEvents(iframeBrowsingContext, 1);
  EventUtils.synthesizeMouse(
    otherEl,
    2,
    2,
    { type: "mouseover" },
    viewDoc.defaultView
  );
  await onEvents;

  ok(true, "1 selectionchange events detected on mouseout");
}

async function waitForNSelectionEvents(browser, numberOfTimes) {
  await SpecialPowers.spawn(browser, [numberOfTimes], async function (n) {
    const win = content.wrappedJSObject;

    await new Promise(resolve => {
      let received = 0;
      win.document.addEventListener("selectionchange", function listen() {
        received++;

        if (received === n) {
          win.document.removeEventListener("selectionchange", listen);
          resolve();
        }
      });
    });
  });
}
