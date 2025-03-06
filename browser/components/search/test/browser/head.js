/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.sys.mjs",
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.sys.mjs",
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  FormHistoryTestUtils:
    "resource://testing-common/FormHistoryTestUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

let gCUITestUtils = new CustomizableUITestUtils(window);

AddonTestUtils.initMochitest(this);
SearchTestUtils.init(this);

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 *
 * @param {object} expectedObj
 *        The expected object to find.
 * @param {object} actualObj
 *        The object to inspect.
 * @param {string} name
 *        The name of the engine, used for test detail logging.
 */
function isSubObjectOf(expectedObj, actualObj, name) {
  for (let prop in expectedObj) {
    if (typeof expectedObj[prop] == "function") {
      continue;
    }
    if (expectedObj[prop] instanceof Object) {
      is(
        actualObj[prop].length,
        expectedObj[prop].length,
        name + "[" + prop + "]"
      );
      isSubObjectOf(
        expectedObj[prop],
        actualObj[prop],
        name + "[" + prop + "]"
      );
    } else {
      is(actualObj[prop], expectedObj[prop], name + "[" + prop + "]");
    }
  }
}

function getLocale() {
  return Services.locale.requestedLocale || undefined;
}

function promiseEvent(aTarget, aEventName, aPreventDefault) {
  function cancelEvent(event) {
    if (aPreventDefault) {
      event.preventDefault();
    }

    return true;
  }

  return BrowserTestUtils.waitForEvent(aTarget, aEventName, false, cancelEvent);
}

// Get an array of the one-off buttons.
function getOneOffs() {
  let oneOffs = [];
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let oneOffsContainer = searchPopup.searchOneOffsContainer;
  let oneOff = oneOffsContainer.querySelector(".search-panel-one-offs");
  for (oneOff = oneOff.firstChild; oneOff; oneOff = oneOff.nextSibling) {
    if (oneOff.nodeType == Node.ELEMENT_NODE) {
      oneOffs.push(oneOff);
    }
  }
  return oneOffs;
}

async function typeInSearchField(browser, text, fieldName) {
  await SpecialPowers.spawn(
    browser,
    [[fieldName, text]],
    async function ([contentFieldName, contentText]) {
      // Put the focus on the search box.
      let searchInput = content.document.getElementById(contentFieldName);
      searchInput.focus();
      searchInput.value = contentText;
    }
  );
}

async function searchInSearchbar(inputText, win = window) {
  await new Promise(r => waitForFocus(r, win));
  let sb = win.document.getElementById("searchbar");
  // Write the search query in the searchbar.
  sb.focus();
  sb.value = inputText;
  sb.textbox.controller.startSearch(inputText);
  // Wait for the popup to show.
  await BrowserTestUtils.waitForEvent(sb.textbox.popup, "popupshown");
  // And then for the search to complete.
  await TestUtils.waitForCondition(
    () =>
      sb.textbox.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
    "The search in the searchbar must complete."
  );
  return sb.textbox.popup;
}

function clearSearchbarHistory() {
  info("cleanup the search history");
  return FormHistory.update({ op: "remove", fieldname: "searchbar-history" });
}

registerCleanupFunction(async () => {
  await PlacesUtils.history.clear();
});

/**
 * Fills a text field ensuring to cause expected edit events.
 *
 * @param {string} id
 *        id of the text field
 * @param {string} text
 *        text to fill in
 * @param {object} win
 *        dialog window
 */
function fillTextField(id, text, win) {
  let elt = win.document.getElementById(id);
  elt.focus();
  elt.select();
  EventUtils.synthesizeKey("a", { metaKey: true }, win);
  EventUtils.synthesizeKey("KEY_Backspace", {}, win);

  for (let c of text.split("")) {
    EventUtils.synthesizeKey(c, {}, win);
  }
}

/**
 * Wait for the user's default search engine to change.
 *
 * @param {XULBrowser} browser
 * @param {Function} searchEngineChangeFn
 *   A function that is called to change the search engine.
 */
async function promiseContentSearchChange(browser, searchEngineChangeFn) {
  // Add an event listener manually then perform the action, rather than using
  // BrowserTestUtils.addContentEventListener as that doesn't add the listener
  // early enough.
  await SpecialPowers.spawn(browser, [], async () => {
    // Store the results in a temporary place.
    content._searchDetails = {
      defaultEnginesList: [],
      listener: event => {
        if (event.detail.type == "CurrentEngine") {
          content._searchDetails.defaultEnginesList.push(
            content.wrappedJSObject.gContentSearchController.defaultEngine.name
          );
        }
      },
    };

    // Listen using the system group to ensure that it fires after
    // the default behaviour.
    content.addEventListener(
      "ContentSearchService",
      content._searchDetails.listener,
      { mozSystemGroup: true }
    );
  });

  let expectedEngineName = await searchEngineChangeFn();

  await SpecialPowers.spawn(
    browser,
    [expectedEngineName],
    async expectedEngineNameChild => {
      await ContentTaskUtils.waitForCondition(
        () =>
          content._searchDetails.defaultEnginesList &&
          content._searchDetails.defaultEnginesList[
            content._searchDetails.defaultEnginesList.length - 1
          ] == expectedEngineNameChild,
        `Waiting for ${expectedEngineNameChild} to be set`
      );
      content.removeEventListener(
        "ContentSearchService",
        content._searchDetails.listener,
        { mozSystemGroup: true }
      );
      delete content._searchDetails;
    }
  );
}
