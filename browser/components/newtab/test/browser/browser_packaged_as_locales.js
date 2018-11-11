ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

// Tests are by default run with non-debug en-US configuration
const DEFAULT_URL = SpecialPowers.getBoolPref("browser.tabs.remote.separatePrivilegedContentProcess") ?
  "resource://activity-stream/prerendered/en-US/activity-stream-prerendered-noscripts.html" :
  "resource://activity-stream/prerendered/en-US/activity-stream-prerendered.html";

/**
 * Temporarily change the app locale to get the localized activity stream url
 */
async function getUrlForLocale(locale) {
  const origAvailable = Services.locale.availableLocales;
  const origRequested = Services.locale.requestedLocales;
  try {
    Services.locale.availableLocales = [locale];
    Services.locale.requestedLocales = [locale];
    return aboutNewTabService.defaultURL;
  } finally {
    // Always clean up after returning the url
    Services.locale.availableLocales = origAvailable;
    Services.locale.requestedLocales = origRequested;
  }
}

/**
 * Test that an unknown locale defaults to en-US
 */
add_task(async function test_unknown_locale() {
  const url = await getUrlForLocale("und");
  Assert.equal(url, DEFAULT_URL);
});

/**
 * Test that we at least have en-US
 */
add_task(async function test_default_locale() {
  const url = await getUrlForLocale("en-US");
  Assert.equal(url, DEFAULT_URL);
});

/**
 * Test that we use a shared language before en-US
 */
add_task(async function test_default_locale() {
  const url = await getUrlForLocale("de-UNKNOWN");
  Assert.equal(url, DEFAULT_URL.replace("en-US", "de"));
});

/**
 * Tests that all activity stream packaged locales can be referenced / accessed
 */
add_task(async function test_all_packaged_locales() {
  let gotID = false;
  const listing = await (await fetch("resource://activity-stream/prerendered/")).text();
  for (const line of listing.split("\n").slice(2)) {
    const [file, , , type] = line.split(" ").slice(1);
    if (type === "DIRECTORY") {
      const locale = file.replace("/", "");
      if (locale !== "static") {
        const url = await getUrlForLocale(locale);
        Assert.equal(url, DEFAULT_URL.replace("en-US", locale), `can reference "${locale}" files`);

        // Specially remember if we saw an ID locale packaged as it can be
        // easily ignored by source control, e.g., .gitignore
        gotID |= locale === "id";
      }
    }
  }

  Assert.ok(gotID, `"id" locale packaged and not ignored`);
});
