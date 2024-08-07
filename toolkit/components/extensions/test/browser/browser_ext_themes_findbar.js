"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the toolbar and toolbar_field properties also theme the findbar.

function assertHasNoBorders(element) {
  let cs = window.getComputedStyle(element);
  Assert.equal(cs.borderTopWidth, "0px", "should have no top border");
  Assert.equal(cs.borderRightWidth, "0px", "should have no right border");
  Assert.equal(cs.borderBottomWidth, "0px", "should have no bottom border");
  Assert.equal(cs.borderLeftWidth, "0px", "should have no left border");
}

add_task(async function test_support_toolbar_properties_on_findbar() {
  const TOOLBAR_COLOR = "#ff00ff";
  const TOOLBAR_TEXT_COLOR = "#9400ff";
  const ACCENT_COLOR_INACTIVE = "#ffff00";
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          frame_inactive: ACCENT_COLOR_INACTIVE,
          tab_background_text: TEXT_COLOR,
          toolbar: TOOLBAR_COLOR,
          bookmark_text: TOOLBAR_TEXT_COLOR,
        },
      },
    },
  });

  await extension.startup();
  await gBrowser.getFindBar();

  let findbar_button = gFindBar.getElement("highlight");

  info("Checking findbar background is set as toolbar color");
  Assert.equal(
    window.getComputedStyle(gFindBar).backgroundColor,
    hexToCSS(ACCENT_COLOR),
    "Findbar background color should be the same as toolbar background color."
  );

  info("Checking findbar and checkbox text color use toolbar text color");
  const rgbColor = hexToCSS(TOOLBAR_TEXT_COLOR);
  Assert.equal(
    window.getComputedStyle(gFindBar).color,
    rgbColor,
    "Findbar text color should be the same as toolbar text color."
  );
  Assert.equal(
    window.getComputedStyle(findbar_button).color,
    rgbColor,
    "Findbar checkbox text color should be toolbar text color."
  );

  // Open a new window to check frame_inactive
  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal(
    window.getComputedStyle(gFindBar).backgroundColor,
    hexToCSS(ACCENT_COLOR_INACTIVE),
    "Findbar background changed in inactive window."
  );
  await BrowserTestUtils.closeWindow(window2);

  await extension.unload();
});

add_task(async function test_support_toolbar_field_properties_on_findbar() {
  let findbar_prev_button = gFindBar.getElement("find-previous");
  let findbar_next_button = gFindBar.getElement("find-next");

  assertHasNoBorders(findbar_prev_button);
  assertHasNoBorders(findbar_next_button);

  const TOOLBAR_FIELD_COLOR = "#ff00ff";
  const TOOLBAR_FIELD_TEXT_COLOR = "#9400ff";
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field: TOOLBAR_FIELD_COLOR,
          toolbar_field_text: TOOLBAR_FIELD_TEXT_COLOR,
        },
      },
    },
  });

  await extension.startup();
  await gBrowser.getFindBar();

  let findbar_textbox = gFindBar.getElement("findbar-textbox");

  info(
    "Checking findbar textbox background is set as toolbar field background color"
  );
  Assert.equal(
    window.getComputedStyle(findbar_textbox).backgroundColor,
    hexToCSS(TOOLBAR_FIELD_COLOR),
    "Findbar textbox background color should be the same as toolbar field color."
  );

  info("Checking findbar textbox color is set as toolbar field text color");
  Assert.equal(
    window.getComputedStyle(findbar_textbox).color,
    hexToCSS(TOOLBAR_FIELD_TEXT_COLOR),
    "Findbar textbox text color should be the same as toolbar field text color."
  );

  assertHasNoBorders(findbar_prev_button);
  assertHasNoBorders(findbar_next_button);

  await extension.unload();
});

// Test that theme properties are applied with a theme_frame
add_task(async function test_toolbar_properties_on_findbar_with_theme_frame() {
  const TOOLBAR_COLOR = "#ff00ff";
  const TOOLBAR_TEXT_COLOR = "#9400ff";
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar: TOOLBAR_COLOR,
          bookmark_text: TOOLBAR_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();
  await gBrowser.getFindBar();

  let findbar_button = gFindBar.getElement("highlight");

  info("Checking findbar background is set as toolbar color");
  Assert.equal(
    window.getComputedStyle(gFindBar).backgroundColor,
    hexToCSS(ACCENT_COLOR),
    "Findbar background color should be set by theme."
  );

  info("Checking findbar and button text color is set as toolbar text color");
  Assert.equal(
    window.getComputedStyle(gFindBar).color,
    hexToCSS(TOOLBAR_TEXT_COLOR),
    "Findbar text color should be set by theme."
  );
  Assert.equal(
    window.getComputedStyle(findbar_button).color,
    hexToCSS(TOOLBAR_TEXT_COLOR),
    "Findbar button text color should be set by theme."
  );

  await extension.unload();
});

add_task(
  async function test_toolbar_field_properties_on_findbar_with_theme_frame() {
    const TOOLBAR_FIELD_COLOR = "#ff00ff";
    const TOOLBAR_FIELD_TEXT_COLOR = "#9400ff";
    // The TabContextMenu initializes its strings only on a focus or mouseover event.
    // Calls focus event on the TabContextMenu early in the test.
    gBrowser.selectedTab.focus();
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        theme: {
          images: {
            theme_frame: "image1.png",
          },
          colors: {
            frame: ACCENT_COLOR,
            tab_background_text: TEXT_COLOR,
            toolbar_field: TOOLBAR_FIELD_COLOR,
            toolbar_field_text: TOOLBAR_FIELD_TEXT_COLOR,
          },
        },
      },
      files: {
        "image1.png": BACKGROUND,
      },
    });

    await extension.startup();
    await gBrowser.getFindBar();

    let findbar_textbox = gFindBar.getElement("findbar-textbox");

    Assert.equal(
      window.getComputedStyle(findbar_textbox).backgroundColor,
      hexToCSS(TOOLBAR_FIELD_COLOR),
      "Findbar textbox background color should be set by theme."
    );

    Assert.equal(
      window.getComputedStyle(findbar_textbox).color,
      hexToCSS(TOOLBAR_FIELD_TEXT_COLOR),
      "Findbar textbox text color should be set by theme."
    );

    await extension.unload();
  }
);
