/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the visible delay in showing the "Language" category occurs
// very minimally

let gProvider;
let gInstall;
let gInstallProperties = [
  {
    name: "Locale Category Test",
    type: "locale",
  },
];

function installLocale() {
  return new Promise(resolve => {
    gInstall = gProvider.createInstalls(gInstallProperties)[0];
    gInstall.addTestListener({
      onInstallEnded() {
        gInstall.removeTestListener(this);
        resolve();
      },
    });
    gInstall.install();
  });
}

async function getCategoryButton(win, category) {
  await win.customElements.whenDefined("categories-box");

  let categoriesBox = win.document.getElementById("categories");
  await categoriesBox.promiseRendered;
  return categoriesBox.getButtonByName(category);
}

async function checkCategory(win, category, { expectHidden, expectSelected }) {
  let button = await getCategoryButton(win, category);

  is(
    button.hidden,
    expectHidden,
    `${category} button is ${expectHidden ? "" : "not "}hidden`
  );
  if (expectSelected !== undefined) {
    is(
      button.selected,
      expectSelected,
      `${category} button is ${expectSelected ? "" : "not "}selected`
    );
  }
}

add_setup(async function () {
  gProvider = new MockProvider();
});

add_task(async function testLocalesHiddenByDefault() {
  gProvider.blockQueryResponses();

  let viewLoaded = loadInitialView("extension", {
    async loadCallback(win) {
      await checkCategory(win, "locale", { expectHidden: true });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  await checkCategory(win, "locale", { expectHidden: true });

  await installLocale();

  await checkCategory(win, "locale", {
    expectHidden: false,
    expectSelected: false,
  });

  await closeView(win);
});

add_task(async function testLocalesShownWhenInstalled() {
  gProvider.blockQueryResponses();

  let viewLoaded = loadInitialView("extension", {
    async loadCallback(win) {
      await checkCategory(win, "locale", {
        expectHidden: false,
        expectSelected: false,
      });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  await checkCategory(win, "locale", {
    expectHidden: false,
    expectSelected: false,
  });

  await closeView(win);
});

add_task(async function testLocalesHiddenWhenUninstalled() {
  gInstall.cancel();
  gProvider.blockQueryResponses();

  let viewLoaded = loadInitialView("extension", {
    async loadCallback(win) {
      await checkCategory(win, "locale", {
        expectHidden: false,
        expectSelected: false,
      });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  // CategoriesBox updateHiddenCategories method is async
  // and it calls AddonManager getAddonsByTypes and
  // getInstallsByTypes async methods to determine which
  // of the hidden categories may need to be shown
  // and so the assertion may hit a race if any of the
  // providers is taking longer to resolve the getAddonsByTypes
  // async call.
  await BrowserTestUtils.waitForCondition(async () => {
    const button = await getCategoryButton(win, "locale");
    return button.hidden;
  }, "Wait for the locale category button to be hidden");

  await checkCategory(win, "locale", { expectHidden: true });

  await closeView(win);
});

add_task(async function testLocalesHiddenWithoutDelay() {
  gProvider.blockQueryResponses();

  let viewLoaded = loadInitialView("extension", {
    async loadCallback(win) {
      await checkCategory(win, "locale", { expectHidden: true });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  await BrowserTestUtils.waitForCondition(async () => {
    const button = await getCategoryButton(win, "locale");
    return button.hidden;
  }, "Wait for the locale category button to be hidden");

  await checkCategory(win, "locale", { expectHidden: true });

  await closeView(win);
});

add_task(async function testLocalesShownAfterDelay() {
  await installLocale();

  gProvider.blockQueryResponses();

  let viewLoaded = loadInitialView("extension", {
    async loadCallback(win) {
      await checkCategory(win, "locale", { expectHidden: true });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  await BrowserTestUtils.waitForCondition(async () => {
    const button = await getCategoryButton(win, "locale");
    return !button.hidden;
  }, "Wait for the locale category button to be shown");

  await checkCategory(win, "locale", {
    expectHidden: false,
    expectSelected: false,
  });

  await closeView(win);
});

add_task(async function testLocalesShownIfPreviousView() {
  gProvider.blockQueryResponses();

  // Passing "locale" will set the last view to locales and open the view.
  let viewLoaded = loadInitialView("locale", {
    async loadCallback(win) {
      await checkCategory(win, "locale", {
        expectHidden: false,
        expectSelected: true,
      });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  await checkCategory(win, "locale", {
    expectHidden: false,
    expectSelected: true,
  });

  await closeView(win);
});

add_task(async function testLocalesHiddenIfPreviousViewAndNoLocales() {
  gInstall.cancel();
  gProvider.blockQueryResponses();

  // Passing "locale" will set the last view to locales and open the view.
  let viewLoaded = loadInitialView("locale", {
    async loadCallback(win) {
      await checkCategory(win, "locale", {
        expectHidden: false,
        expectSelected: true,
      });
      gProvider.unblockQueryResponses();
    },
  });
  let win = await viewLoaded;

  let categoryUtils = new CategoryUtilities(win);

  await TestUtils.waitForCondition(
    () => categoryUtils.selectedCategory != "locale"
  );

  await checkCategory(win, "locale", {
    expectHidden: true,
    expectSelected: false,
  });

  is(
    categoryUtils.getSelectedViewId(),
    win.gViewController.defaultViewId,
    "default view is selected"
  );

  await closeView(win);
});
