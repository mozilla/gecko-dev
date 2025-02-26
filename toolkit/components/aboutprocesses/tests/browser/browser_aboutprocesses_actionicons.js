/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let doc, tbody, tabAboutProcesses;

const rowTypes = ["process", "window", "thread-summary", "thread"];

function promiseUpdate() {
  return promiseAboutProcessesUpdated({
    doc,
    tbody,
    force: true,
    tabAboutProcesses,
  });
}

async function setTabAboutProcesses() {
  info("Setting up about:processes tab");
  tabAboutProcesses = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:processes",
    waitForLoad: true,
  });

  doc = tabAboutProcesses.linkedBrowser.contentDocument;
  tbody = doc.getElementById("process-tbody");
  await promiseUpdate();
}

add_setup(async function () {
  await setTabAboutProcesses();

  await SpecialPowers.pushPrefEnv({
    set: [
      // Ensure both types of buttons are shown
      ["toolkit.aboutProcesses.showThreads", "true"],
      ["toolkit.aboutProcesses.showProfilerIcons", "true"],
      // Change profiler URL for the test to avoid opening a remote URL and crash
      ["devtools.performance.recording.ui-base-url", "https://example.com"],
    ],
  });
});

add_task(async function test_profiler_icon_button() {
  info("Verify the Profiler button is properly marked up.");

  let processRow = tbody.querySelector(".process");
  let profilerButton = processRow.querySelector(".profiler-icon");
  Assert.ok(
    profilerButton.hasAttribute("title"),
    "The Profile Process icon button has a title"
  );
  Assert.equal(
    profilerButton.getAttribute("role") || profilerButton.tagName.toLowerCase(),
    "button",
    "The Profile Process icon is programmatically a button"
  );
  // ToDo: Remove the "tabindex" check when the button is using <button> element
  Assert.equal(
    profilerButton.getAttribute("tabindex"),
    "0",
    "The Profile Process icon button is included in the focus order"
  );
  Assert.equal(
    profilerButton.getAttribute("aria-pressed"),
    "false",
    "The Profile button has its pressed status set to false"
  );

  info("Verify the Profile button can be focused with a keyboard.");

  profilerButton.focus();

  const { display, opacity } = window.getComputedStyle(profilerButton);

  Assert.equal(
    display,
    "inline-block",
    "the Profiler image button is in the DOM"
  );
  Assert.equal(opacity, "1", "the Profiler image button is visible");

  Assert.equal(
    profilerButton,
    doc.activeElement,
    "the Profiler image button can be focused"
  );

  // Skip the rest if the profiler was already active when this test started:
  if (!Services.profiler.IsActive()) {
    let waitForPressed = BrowserTestUtils.waitForMutationCondition(
      profilerButton,
      { attributeFilter: ["aria-pressed"] },
      () => profilerButton.getAttribute("aria-pressed") == "true"
    );

    // Verify we can profile the process with a keyboard.
    EventUtils.synthesizeKey(" ");

    await waitForPressed;

    Assert.equal(
      profilerButton.getAttribute("aria-pressed"),
      "true",
      "Pressing Space on a Profile button changes its status to pressed"
    );

    Services.profiler.StopProfiler();
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(tabAboutProcesses);
});

add_task(async function test_close_icon_button() {
  await setTabAboutProcesses();

  doc = tabAboutProcesses.linkedBrowser.contentDocument;
  tbody = doc.getElementById("process-tbody");

  info("Verify the Close button is properly marked up.");

  let webTabRow = findTabRowByName(doc, "New Tab");
  let closeButtons = webTabRow.querySelectorAll(".close-icon");

  Assert.ok(webTabRow, `We should have found New Tab tab to close it`);
  Assert.equal(
    closeButtons.length,
    1,
    "This tab should have exactly one close icon"
  );
  Assert.ok(
    closeButtons[0].hasAttribute("title"),
    "The Close Tab icon button has a title"
  );
  Assert.equal(
    closeButtons[0].getAttribute("role") ||
      closeButtons[0].tagName.toLowerCase(),
    "button",
    "The Close Tab icon is programmatically a button"
  );
  // ToDo: Remove the "tabindex" check when the button is using <button> element
  Assert.equal(
    closeButtons[0].getAttribute("tabindex"),
    "0",
    "The Close Tab icon button is included in the focus order"
  );

  info("Verify the Close button can be focused with a keyboard.");

  closeButtons[0].focus();

  Assert.equal(
    closeButtons[0],
    doc.activeElement,
    "the Close image button can be focused"
  );

  const { display, opacity } = window.getComputedStyle(closeButtons[0]);

  Assert.equal(opacity, "1", "the Close image button is visible");
  Assert.equal(display, "inline-block", "the Close image button is in the DOM");

  // Verify we can kill the tab with a keyboard.
  EventUtils.synthesizeKey("KEY_Enter");

  Assert.ok(
    webTabRow.hasAttribute("aria-busy"),
    "Pressing Enter on a Close button adds an aria-busy to the row"
  );
  Assert.ok(
    webTabRow.classList.contains("killing"),
    "Pressing Enter on a Close button starts the process of killing the tab"
  );

  // Give Firefox a little time to close the tab and update about:processes.
  // This might take two updates as we're racing between collecting data and
  // processes actually being killed.
  await promiseAboutProcessesUpdated({
    doc,
    force: true,
    tabAboutProcesses,
  });
});
