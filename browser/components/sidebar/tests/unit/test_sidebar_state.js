/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { SidebarState } = ChromeUtils.importESModule(
  "resource:///modules/SidebarState.sys.mjs"
);

const mockElement = { toggleAttribute: sinon.stub() };
const mockGlobal = {
  document: { getElementById: () => mockElement },
  gBrowser: { tabContainer: mockElement },
};
const mockPanel = {
  setAttribute: (name, value) => (mockPanel[name] = value),
  style: { width: "200px" },
};
const mockController = {
  _box: mockPanel,
  showInitially: sinon.stub(),
  sidebarContainer: { ownerGlobal: mockGlobal },
  sidebarMain: mockElement,
  sidebarRevampEnabled: true,
  sidebarRevampVisibility: "always-show",
  sidebars: new Set(["viewBookmarksSidebar"]),
  updateToolbarButton: sinon.stub(),
};

add_task(async function test_load_legacy_session_restore_data() {
  const sidebarState = new SidebarState(mockController);

  sidebarState.loadInitialState({
    width: "300px",
    command: "viewBookmarksSidebar",
    expanded: true,
    hidden: false,
  });

  const props = sidebarState.getProperties();
  Assert.equal(props.panelWidth, 300, "The panel was resized.");
  Assert.equal(props.launcherExpanded, true, "The launcher is expanded.");
  Assert.equal(props.launcherVisible, true, "The launcher is visible.");
  Assert.ok(
    mockController.showInitially.calledWith("viewBookmarksSidebar"),
    "Bookmarks panel was shown."
  );
});
