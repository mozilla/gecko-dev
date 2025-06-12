/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FileTestUtils: "resource://testing-common/FileTestUtils.sys.mjs",
  MockRegistrar: "resource://testing-common/MockRegistrar.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  ShellService: "resource:///modules/ShellService.sys.mjs",
  TaskbarTabsPin: "resource:///modules/taskbartabs/TaskbarTabsPin.sys.mjs",
  TaskbarTabsRegistry:
    "resource:///modules/taskbartabs/TaskbarTabsRegistry.sys.mjs",
});

// We want to mock the native XPCOM interfaces of the initialized
// `ShellService.shellService`, but those interfaces are frozen. Instead we
// proxy `ShellService.shellService` and mock it.
const proxyNativeShellService = {
  ...ShellService.shellService,
  createWindowsIcon: sinon.stub().resolves(),
  createShortcut: sinon.stub().resolves("dummy_path"),
  pinShortcutToTaskbar: sinon.stub().resolves(),
  getTaskbarTabShortcutPath: sinon
    .stub()
    .returns(FileTestUtils.getTempFile().parent.path),
  unpinShortcutFromTaskbar: sinon.stub(),
};

sinon.stub(ShellService, "shellService").value(proxyNativeShellService);

const kFaviconUri = Services.io.newFileURI(do_get_file("favicon-normal16.png"));
let faviconThrows = false;

let mockFaviconService = {
  QueryInterface: ChromeUtils.generateQI(["nsIFaviconService"]),
  getFaviconForPage: async () => {},
  defaultFavicon: {},
};

sinon.stub(mockFaviconService, "getFaviconForPage").callsFake(async () => {
  if (faviconThrows) {
    return null;
  }
  return { uri: kFaviconUri };
});
sinon.stub(mockFaviconService, "defaultFavicon").value(kFaviconUri);

// Favicons are written to the profile directory, ensure it exists.
do_get_profile();

function shellPinCalled() {
  ok(
    proxyNativeShellService.createWindowsIcon.called,
    `Icon creation should have been called.`
  );
  ok(
    proxyNativeShellService.createShortcut.called,
    `Shortcut creation should have been called.`
  );
  ok(
    proxyNativeShellService.pinShortcutToTaskbar.called,
    `Pin to taskbar should have been called.`
  );
}

function shellUnpinCalled() {
  ok(
    proxyNativeShellService.unpinShortcutFromTaskbar.called,
    `Unpin from taskbar should have been called.`
  );
}

MockRegistrar.register(
  "@mozilla.org/browser/favicon-service;1",
  mockFaviconService
);

const url = Services.io.newURI("https://www.test.com");
const userContextId = 0;

const registry = new TaskbarTabsRegistry();
const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);

add_task(async function test_pin_existing_favicon() {
  sinon.resetHistory();
  faviconThrows = false;
  await TaskbarTabsPin.pinTaskbarTab(taskbarTab);

  ok(
    mockFaviconService.getFaviconForPage.calledOnce,
    "The favicon for the page should have attempted to be retrieved."
  );
  ok(
    !mockFaviconService.defaultFavicon.calledOnce,
    "The default icon should not be used when a favicon exists for the page."
  );

  shellPinCalled();
});

add_task(async function test_pin_missing_favicon() {
  sinon.resetHistory();
  faviconThrows = true;
  await TaskbarTabsPin.pinTaskbarTab(taskbarTab);

  ok(
    mockFaviconService.getFaviconForPage.calledOnce,
    "The favicon for the page should have attempted to be retrieved."
  );
  ok(
    !mockFaviconService.defaultFavicon.calledOnce,
    "The default icon should be used when a favicon exists for the page."
  );
});

add_task(async function test_unpin() {
  sinon.resetHistory();
  await TaskbarTabsPin.unpinTaskbarTab(taskbarTab);

  shellUnpinCalled();
});
