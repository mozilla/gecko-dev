/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that collapsibilities of DebugTargetPane on RuntimePage by mouse clicking.
 */

add_task(async function() {
  prepareCollapsibilitiesTest();

  const { document, tab } = await openAboutDebugging();

  for (const { title } of TARGET_PANES) {
    info("Check whether this pane is collapsed after clicking the title");
    await toggleCollapsibility(getDebugTargetPane(title, document));
    assertDebugTargetCollapsed(getDebugTargetPane(title, document), title);

    info("Check whether this pane is expanded after clicking the title again");
    await toggleCollapsibility(getDebugTargetPane(title, document));
    await assertDebugTargetExpanded(getDebugTargetPane(title, document), title);
  }

  await removeTab(tab);
});

async function assertDebugTargetCollapsed(paneEl, title) {
  info("Check debug target is collapsed");

  // check list height
  const listEl = paneEl.querySelector(".js-debug-target-list");
  is(listEl.clientHeight, 0, "Height of list element is zero");
  // check title
  const titleEl = paneEl.querySelector(".js-debug-target-pane-title");
  const expectedTitle =
    `${ title } (${ listEl.querySelectorAll(".js-debug-target-item").length })`;
  is(titleEl.textContent, expectedTitle, "Collapsed title is correct");
}

async function assertDebugTargetExpanded(paneEl, title) {
  info("Check debug target is expanded");

  // check list height
  const listEl = paneEl.querySelector(".js-debug-target-list");
  await waitUntil(() => listEl.clientHeight > 0);
  ok(true, "Height of list element is greater than zero");
  // check title
  const titleEl = paneEl.querySelector(".js-debug-target-pane-title");
  is(titleEl.textContent, title, "Expanded title is correct");
}
