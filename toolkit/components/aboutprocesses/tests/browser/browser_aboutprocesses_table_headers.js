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

add_setup(async function () {
  info("Setting up about:processes");
  tabAboutProcesses = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:processes",
    waitForLoad: true,
  });

  doc = tabAboutProcesses.linkedBrowser.contentDocument;
  tbody = doc.getElementById("process-tbody");
  await promiseUpdate();
});

add_task(function testTableHeadersMarkup() {
  let sortingToggles = doc.querySelectorAll(".clickable");

  info("Verify table headers are properly marked up.");

  sortingToggles.forEach(btn => {
    Assert.equal(
      btn.tagName,
      "BUTTON",
      "Sortable table headers are marked up as buttons"
    );
    Assert.ok(
      btn.hasAttribute("aria-sort"),
      "All sortable table headers have an aria-sort"
    );
    Assert.equal(
      btn.getAttribute("aria-sort"),
      "none",
      "Sortable table headers are not sorted by default"
    );
    Assert.ok(
      doc.l10n.getAttributes(btn).id,
      "Sortable table headers have a localization ID set up"
    );
  });
});

add_task(function testTableHeadersClicking() {
  let sortingName = doc.getElementById("column-name");
  let sortingMemory = doc.getElementById("column-memory-resident");
  let sortingCpu = doc.getElementById("column-cpu-total");

  info("Verify we can toggle table sorting by clicking table header buttons.");

  sortingName.click();

  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "descending",
    "Clicked sortable table header is now sorted in descending order"
  );
  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );

  sortingName.click();

  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "ascending",
    "Clicked sortable table header is now sorted in ascending order"
  );
  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );

  sortingMemory.click();

  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "descending",
    "Clicked sortable table header is now sorted in descending order"
  );
  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );

  sortingCpu.click();

  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "descending",
    "Clicked sortable table header is now sorted in descending order"
  );
  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );
});

add_task(function testTableHeadersKeypressing() {
  let sortingName = doc.getElementById("column-name");
  let sortingMemory = doc.getElementById("column-memory-resident");
  let sortingCpu = doc.getElementById("column-cpu-total");

  info(
    "Verify we can toggle table sorting by pressing Enter or Space on table header buttons."
  );

  sortingName.focus();

  Assert.equal(
    sortingName,
    doc.activeElement,
    "Sortable table header can be focused (1)"
  );

  // Test Enter key on the Name column:
  EventUtils.synthesizeKey("KEY_Enter");

  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "descending",
    "Pressing Enter on a sortable table header is sorting its column"
  );
  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );

  // Test Space key on the Name column:
  EventUtils.synthesizeKey(" ");

  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "ascending",
    "Pressing Space on a sortable table header is sorting its column"
  );
  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );

  EventUtils.synthesizeKey("KEY_Tab");

  Assert.equal(
    sortingMemory,
    doc.activeElement,
    "Sortable table header can be focused (2)"
  );

  // Test Enter key on the Memory column:
  EventUtils.synthesizeKey("KEY_Enter");

  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "descending",
    "Pressing Enter on a sortable table header is sorting its column"
  );
  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );

  EventUtils.synthesizeKey("KEY_Tab");

  Assert.equal(
    sortingCpu,
    doc.activeElement,
    "Sortable table header can be focused (3)"
  );

  // Test Space key on the CPU column:
  EventUtils.synthesizeKey(" ");

  Assert.equal(
    sortingCpu.getAttribute("aria-sort"),
    "descending",
    "Pressing Enter on a sortable table header is sorting its column"
  );
  Assert.equal(
    sortingName.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (1)"
  );
  Assert.equal(
    sortingMemory.getAttribute("aria-sort"),
    "none",
    "Other sortable table headers are not sorted (2)"
  );
});

registerCleanupFunction(function () {
  doc = null;
  tbody = null;
  BrowserTestUtils.removeTab(tabAboutProcesses);
});
