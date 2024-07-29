add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", true],
    ],
  })
);
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

let newState = {
  windows: [
    {
      tabs: [
        {
          entries: [{ url: "https://example.com", triggeringPrincipal_base64 }],
          pinned: "true",
          hidden: "false",
        },
        {
          entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
          pinned: "true",
          hidden: "false",
        },
        {
          entries: [{ url: "about:home", triggeringPrincipal_base64 }],
          hidden: "false",
        },
        {
          entries: [
            { url: "https://www.example.net/", triggeringPrincipal_base64 },
          ],
          hidden: "false",
          pinned: "true",
        },
      ],
    },
  ],
};

add_task(async function test_pinned_tabs_restored_position() {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await setWindowState(win, newState, true);

  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  let tabStrip = document.getElementById("tabbrowser-tabs");
  let verticalTabs = document.querySelector("#vertical-tabs");
  let verticalPinnedTabsContainer = document.querySelector(
    "#vertical-pinned-tabs-container"
  );

  ok(BrowserTestUtils.isVisible(verticalTabs), "Vertical tabs slot is visible");
  is(
    tabStrip.parentNode,
    verticalTabs,
    "Tabstrip is slotted into the sidebar vertical tabs container"
  );
  ok(
    BrowserTestUtils.isVisible(verticalPinnedTabsContainer),
    "Vertical pinned tabs container is visible"
  );
  is(
    verticalPinnedTabsContainer.children.length,
    win.gBrowser._numPinnedTabs,
    "Three tabs are in the vertical pinned tabs container"
  );
  await BrowserTestUtils.closeWindow(win);
});
