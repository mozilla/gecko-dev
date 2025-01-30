const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function ctrl_d() {
  if (AppConstants.platform == "macosx") {
    return;
  }

  // Even if modifier of a shortcut key same as modifier of content access key,
  // the shortcut key should be executed if (remote) content doesn't handle it.
  // This test uses existing shortcut key declaration on Linux and Windows.
  // If you remove or change Alt + D, you need to keep check this with changing
  // the pref or result check.

  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["ui.key.generalAccessKey", -1],
          ["ui.key.chromeAccess", 0 /* disabled */],
          ["ui.key.contentAccess", 4 /* Alt */],
        ],
      },
      resolve
    );
  });
  let searchBar = await gCUITestUtils.addSearchBar();

  const kTestPage = "data:text/html,<body>simple web page</body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestPage);

  searchBar.focus();

  function promiseURLBarHasFocus() {
    return new Promise(resolve => {
      if (gURLBar.focused) {
        ok(true, "The URL bar already has focus");
        resolve();
        return;
      }
      info("Waiting focus event...");
      gURLBar.addEventListener(
        "focus",
        () => {
          ok(true, "The URL bar gets focus");
          resolve();
        },
        { once: true }
      );
    });
  }

  function promiseURLBarSelectsAllText() {
    return new Promise(resolve => {
      function isAllTextSelected() {
        return (
          gURLBar.inputField.selectionStart === 0 &&
          gURLBar.inputField.selectionEnd === gURLBar.inputField.value.length
        );
      }
      if (isAllTextSelected()) {
        ok(true, "All text of the URL bar is already selected");
        isnot(
          gURLBar.inputField.value,
          "",
          "The URL bar should have non-empty text"
        );
        resolve();
        return;
      }
      info("Waiting selection changes...");
      function tryToCheckItLater() {
        if (!isAllTextSelected()) {
          SimpleTest.executeSoon(tryToCheckItLater);
          return;
        }
        ok(true, "All text of the URL bar should be selected");
        isnot(
          gURLBar.inputField.value,
          "",
          "The URL bar should have non-empty text"
        );
        resolve();
      }
      SimpleTest.executeSoon(tryToCheckItLater);
    });
  }

  // Alt + D is a shortcut key to move focus to the URL bar and selects its text.
  info("Pressing Alt + D in the search bar...");
  EventUtils.synthesizeKey("d", { altKey: true });

  await promiseURLBarHasFocus();
  await promiseURLBarSelectsAllText();

  // Alt + D in the URL bar should select all text in it.
  await gURLBar.focus();
  await promiseURLBarHasFocus();
  gURLBar.inputField.selectionStart = gURLBar.inputField.selectionEnd =
    gURLBar.inputField.value.length;

  info("Pressing Alt + D in the URL bar...");
  EventUtils.synthesizeKey("d", { altKey: true });
  await promiseURLBarHasFocus();
  await promiseURLBarSelectsAllText();

  gBrowser.removeCurrentTab();
});

add_task(async function ctrl_alt() {
  if (AppConstants.platform != "macosx") {
    return;
  }

  // Ctrl + Alt is content access key for Mac. But some key bindings supported
  // by native conflict with them. In that case, if the element is editable,
  // prioritize the native key binding over the content access key.

  const TEST_DATA = [
    {
      key: "ctrl_alt_a",
      initial_focus: "browser_urlbar",
      expected_focus: "content_text_a",
    },
    {
      // Ctrl+Alt+b is mapped as moveWordBackward.
      key: "ctrl_alt_b",
      initial_focus: "browser_urlbar",
      expected_focus: "browser_urlbar",
    },
    {
      key: "ctrl_alt_a",
      initial_focus: "content_text_a",
      expected_focus: "content_text_a",
    },
    {
      key: "ctrl_alt_b",
      initial_focus: "content_text_a",
      expected_focus: "content_text_a",
    },
    {
      key: "ctrl_alt_a",
      initial_focus: "browser_button",
      expected_focus: "content_text_a",
    },
    {
      key: "ctrl_alt_b",
      initial_focus: "browser_button",
      expected_focus: "content_text_b",
    },
    {
      key: "ctrl_alt_a",
      initial_focus: "content_checkbox",
      expected_focus: "content_text_a",
    },
    {
      key: "ctrl_alt_b",
      initial_focus: "content_checkbox",
      expected_focus: "content_text_b",
    },
  ];

  const kTestPage = `data:text/html,<input id="content_text_a" accesskey="a"><input id="content_text_b" accesskey="b"><input id="content_checkbox" type="checkbox">`;
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestPage);

  for (const { key, initial_focus, expected_focus } of TEST_DATA) {
    info(`Test for ${JSON.stringify({ key, initial_focus, expected_focus })}`);

    switch (initial_focus) {
      case "browser_urlbar": {
        gURLBar.inputField.focus();
        break;
      }
      case "browser_button": {
        const button = document.getElementById("reload-button");
        button.setAttribute("tabindex", "-1");
        button.focus();
        button.removeAttribute("tabindex");
        break;
      }
      case "content_text_a": {
        await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
          content.document.getElementById("content_text_a").focus();
        });
        break;
      }
      case "content_checkbox": {
        await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
          content.document.getElementById("content_checkbox").focus();
        });
        break;
      }
    }

    switch (key) {
      case "ctrl_alt_a": {
        EventUtils.synthesizeKey("a", { ctrlKey: true, altKey: true });
        break;
      }
      case "ctrl_alt_b": {
        EventUtils.synthesizeKey("b", { ctrlKey: true, altKey: true });
        break;
      }
    }

    if (initial_focus == expected_focus) {
      // It may be before the focus has shifted, so wait a bit.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, 500));
    }

    switch (expected_focus) {
      case "browser_urlbar": {
        await BrowserTestUtils.waitForCondition(
          () => document.activeElement == gURLBar.inputField
        );
        break;
      }
      case "content_text_a": {
        await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
          await ContentTaskUtils.waitForCondition(
            () => content.document.activeElement.id === "content_text_a"
          );
        });
        break;
      }
      case "content_text_b": {
        await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
          await ContentTaskUtils.waitForCondition(
            () => content.document.activeElement.id === "content_text_b"
          );
        });
        break;
      }
    }

    Assert.ok(true);
  }

  gBrowser.removeTab(tab);
});
