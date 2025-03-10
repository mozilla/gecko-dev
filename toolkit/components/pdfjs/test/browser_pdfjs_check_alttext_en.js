/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

const REQ_LOC_CHANGE_EVENT = "intl:requested-locales-changed";

function promiseLocaleChanged(requestedLocale) {
  return new Promise(resolve => {
    let localeObserver = {
      observe(_aSubject, aTopic) {
        switch (aTopic) {
          case REQ_LOC_CHANGE_EVENT: {
            const reqLocs = Services.locale.requestedLocales;
            Assert.equal(reqLocs[0], requestedLocale);
            Services.obs.removeObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
            resolve();
          }
        }
      },
    };
    Services.obs.addObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
  });
}

add_setup(async function () {
  const { availableLocales, requestedLocales } = Services.locale;
  registerCleanupFunction(function () {
    Services.locale.availableLocales = availableLocales;
    Services.locale.requestedLocales = requestedLocales;
  });
});

async function test_ml_alt_text_enabled(locale) {
  // The pref may have been set by another test, so we need to clear it.
  Services.prefs.clearUserPref("browser.ml.enable");
  const defaultBrowserMLPrefValue =
    Services.prefs.getBoolPref("browser.ml.enable");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["pdfjs.enableAltTextForEnglish", false],
      ["pdfjs.enableAltText", false],
      ["browser.ml.enable", !defaultBrowserMLPrefValue],
    ],
  });

  const localPromise = promiseLocaleChanged(locale);
  Services.locale.availableLocales = Services.locale.requestedLocales = [
    locale,
  ];
  await localPromise;

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJSCanvas(browser, `${TESTROOT}file_empty_test.pdf`);
      await waitForPdfJSClose(browser);
    }
  );

  const isEn = locale.startsWith("en");

  // In nightly the pref is true by default: but before opening the pdf, we set
  // it to false.
  Assert.equal(
    Services.prefs.getBoolPref("browser.ml.enable"),
    !defaultBrowserMLPrefValue
  );
  Assert.equal(Services.prefs.getBoolPref("pdfjs.enableAltText"), isEn);
  Assert.equal(
    Services.prefs.getBoolPref("pdfjs.enableAltTextForEnglish"),
    true
  );

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_french() {
  await test_ml_alt_text_enabled("fr-FR");
});

add_task(async function test_english() {
  await test_ml_alt_text_enabled("en-CA");
});
