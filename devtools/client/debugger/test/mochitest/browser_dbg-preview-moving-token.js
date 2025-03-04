/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test the debugger preview popup when moving from one token to another.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-preview.html", "preview.js");

  await selectSource(dbg, "preview.js");

  info(
    "Check that moving the mouse to another token when popup is displayed updates highlighted token and popup position"
  );
  invokeInTab("classPreview");
  await waitForPaused(dbg);

  // Scroll one line *fore* the hovered expression to guarantee showing the line entirely.
  // It may not scroll if the line 50 is partially visible.
  //
  // Also ensure scrolling so that the scrolled line is at the top/start of the viewport
  // so that the preview popup can be shown at the bottom of the token and not on its right.
  await scrollEditorIntoView(dbg, 49, 0, "start");

  // Wait for all the updates to the document to complete to make all
  // token elements have been rendered
  await waitForDocumentLoadComplete(dbg);

  info("Hover token `Foo` in `Foo.#privateStatic` expression");
  const fooTokenEl = await getTokenElAtLine(dbg, "Foo", 50, 44);
  const { element: fooPopupEl } = await tryHoverToken(dbg, fooTokenEl, "popup");
  ok(!!fooPopupEl, "popup is displayed");
  ok(
    fooTokenEl.classList.contains("preview-token"),
    "`Foo` token is highlighted"
  );

  // store original position
  const originalPopupPosition = fooPopupEl.getBoundingClientRect().x;

  info(
    "Move mouse over the `#privateStatic` token in `Foo.#privateStatic` expression"
  );
  const privateStaticTokenEl = await getTokenElAtLine(
    dbg,
    "#privateStatic",
    50,
    48
  );

  // The sequence of event to trigger the bug this is covering isn't easily reproducible
  // by firing a few chosen events (because of React async rendering), so we are going to
  // mimick moving the mouse from the `Foo` to `#privateStatic` in a given amount of time

  // So get all the different token quads to compute their center
  const fooTokenQuad = fooTokenEl.getBoxQuads()[0];
  const privateStaticTokenQuad = privateStaticTokenEl.getBoxQuads()[0];
  const fooXCenter =
    fooTokenQuad.p1.x + (fooTokenQuad.p2.x - fooTokenQuad.p1.x) / 2;
  const fooYCenter =
    fooTokenQuad.p1.y + (fooTokenQuad.p3.y - fooTokenQuad.p1.y) / 2;
  const privateStaticXCenter =
    privateStaticTokenQuad.p1.x +
    (privateStaticTokenQuad.p2.x - privateStaticTokenQuad.p1.x) / 2;
  const privateStaticYCenter =
    privateStaticTokenQuad.p1.y +
    (privateStaticTokenQuad.p3.y - privateStaticTokenQuad.p1.y) / 2;

  // we can then compute the distance to cover between the two token centers
  const xDistance = privateStaticXCenter - fooXCenter;
  const yDistance = privateStaticYCenter - fooYCenter;
  const movementDuration = 50;
  const xIncrements = xDistance / movementDuration;
  const yIncrements = yDistance / movementDuration;

  // Finally, we're going to fire a mouseover event every ms
  info("Move mousecursor between the `Foo` token to the `#privateStatic` one");
  for (let i = 0; i < movementDuration; i++) {
    const x = fooXCenter + (yDistance + i * xIncrements);
    const y = fooYCenter + (yDistance + i * yIncrements);
    EventUtils.synthesizeMouseAtPoint(
      x,
      y,
      {
        type: "mouseover",
      },
      fooTokenEl.ownerGlobal
    );
    await wait(1);
  }

  info("Wait for the popup to display the data for `#privateStatic`");
  await waitFor(() => {
    const popup = findElement(dbg, "popup");
    if (!popup) {
      return false;
    }
    // for `Foo`, the header text content is "Foo", so when it's "Object", we know the
    // popup was updated
    return (
      popup.querySelector(".preview-popup .node .objectBox")?.textContent ===
      "Object"
    );
  });
  ok(true, "Popup is displayed for #privateStatic");

  ok(
    !fooTokenEl.classList.contains("preview-token"),
    "`Foo` token is not highlighted anymore"
  );
  ok(
    privateStaticTokenEl.classList.contains("preview-token"),
    "`#privateStatic` token is highlighted"
  );

  const privateStaticPopupEl = await waitForElement(dbg, "popup");
  const newPopupPosition = privateStaticPopupEl.getBoundingClientRect().x;
  isnot(
    Math.round(newPopupPosition),
    Math.round(originalPopupPosition),
    `Popup position was updated`
  );

  // Move many times between a token, its gap and then outside to hide it many times
  // to highlight any potential race condition.
  for (let i = 0; i < 10; i++) {
    info(
      `Move out by passing over the gap, before going on the right of the token to hide the preview (try #${i + 1})`
    );
    EventUtils.synthesizeMouseAtCenter(
      privateStaticPopupEl.querySelector(".gap"),
      { type: "mousemove" },
      privateStaticPopupEl.ownerGlobal
    );
    EventUtils.synthesizeMouseAtPoint(
      privateStaticTokenQuad.p2.x + 100,
      privateStaticYCenter,
      {
        type: "mousemove",
      },
      fooTokenEl.ownerGlobal
    );
    info("Wait for popup to be hidden when going right");
    await waitUntil(() => findElement(dbg, "popup") == null);

    info("Move back in the center of the token to show the preview again");
    EventUtils.synthesizeMouseAtPoint(
      privateStaticXCenter,
      privateStaticYCenter,
      {
        type: "mousemove",
      },
      fooTokenEl.ownerGlobal
    );
    info("Wait for popup to be shown on private field again");
    await waitUntil(() => !!findElement(dbg, "popup"));
  }

  await closePreviewForToken(dbg, privateStaticTokenEl, "popup");
  await resume(dbg);
});
