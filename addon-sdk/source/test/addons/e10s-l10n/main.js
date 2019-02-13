/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const prefs = require("sdk/preferences/service");
const { Loader } = require('sdk/test/loader');
const { resolveURI } = require('toolkit/loader');
const { rootURI, isNative } = require("@loader/options");
const { usingJSON } = require('sdk/l10n/json/core');

const PREF_MATCH_OS_LOCALE  = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE  = "general.useragent.locale";

function setLocale(locale) {
  prefs.set(PREF_MATCH_OS_LOCALE, false);
  prefs.set(PREF_SELECTED_LOCALE, locale);
}

function resetLocale() {
  prefs.reset(PREF_MATCH_OS_LOCALE);
  prefs.reset(PREF_SELECTED_LOCALE);
}

function definePseudo(loader, id, exports) {
  let uri = resolveURI(id, loader.mapping);
  loader.modules[uri] = { exports: exports };
}

function createTest(locale, testFunction) {
  return function (assert, done) {
    let loader = Loader(module);
    // Change the locale before loading new l10n modules in order to load
    // the right .json file
    setLocale(locale);
    // Initialize main l10n module in order to load new locale files
    loader.require("sdk/l10n/loader").
      load(rootURI).
      then(null, function failure(error) {
        if (!isNative)
          assert.fail("Unable to load locales: " + error);
      }).
      then(function success(data) {
             definePseudo(loader, '@l10n/data', data ? data : null);
             // Execute the given test function
             try {
               testFunction(assert, loader, function onDone() {
                 loader.unload();
                 resetLocale();
                 done();
               });
             }
             catch(e) {
              console.exception(e);
             }
           },
           function failure(error) {
             assert.fail("Unable to load locales: " + error);
           });
  };
}

exports.testExactMatching = createTest("fr-FR", function(assert, loader, done) {
  let _ = loader.require("sdk/l10n").get;
  assert.equal(_("Not translated"), "Not translated",
                   "Key not translated");
  assert.equal(_("Translated"), "Oui",
                   "Simple key translated");

  // Placeholders
  assert.equal(_("placeholderString", "works"), "Placeholder works",
                   "Value with placeholder");
  assert.equal(_("Placeholder %s", "works"), "Placeholder works",
                   "Key without value but with placeholder");
  assert.equal(_("Placeholders %2s %1s %s.", "working", "are", "correctly"),
                   "Placeholders are working correctly.",
                   "Multiple placeholders");

  // Plurals
   assert.equal(_("downloadsCount", 0),
                   "0 téléchargement",
                   "PluralForm form 'one' for 0 in french");
  assert.equal(_("downloadsCount", 1),
                   "1 téléchargement",
                   "PluralForm form 'one' for 1 in french");
  assert.equal(_("downloadsCount", 2),
                   "2 téléchargements",
                   "PluralForm form 'other' for n > 1 in french");

  done();
});

exports.testHtmlLocalizationPageWorker = createTest("en-GB", function(assert, loader, done) {
  // Ensure initing html component that watch document creations
  // Note that this module is automatically initialized in
  // cuddlefish.js:Loader.main in regular addons. But it isn't for unit tests.
  let loaderHtmlL10n = loader.require("sdk/l10n/html");
  loaderHtmlL10n.enable();

  let uri = require("sdk/self").data.url("test-localization.html");
  let worker = loader.require("sdk/page-worker").Page({
    contentURL: uri,
    contentScript: "new " + function ContentScriptScope() {
      let nodes = document.body.querySelectorAll("*[data-l10n-id]");
      self.postMessage([nodes[0].innerHTML,
                        nodes[1].innerHTML,
                        nodes[2].innerHTML,
                        nodes[3].innerHTML,
                        nodes[4].title,
                        nodes[4].getAttribute("alt"),
                        nodes[4].getAttribute("accesskey"),
                        nodes[4].getAttribute("aria-label"),
                        nodes[4].getAttribute("aria-valuetext"),
                        nodes[4].getAttribute("aria-moz-hint"),
                        nodes[5].placeholder,
                        nodes[6].label]);
    },
    onMessage: function (data) {
      assert.equal(
        data[0],
        "Kept as-is",
        "Nodes with unknown id in .properties are kept 'as-is'"
      );
      assert.equal(data[1], "Yes", "HTML is translated");
      assert.equal(
        data[2],
        "no &lt;b&gt;HTML&lt;/b&gt; injection",
        "Content from .properties is text content; HTML can't be injected."
      );
      assert.equal(data[3], "Yes", "Multiple elements with same data-l10n-id are accepted.");

      // Attribute translation tests
      assert.equal(data[4], "Yes", "Title attributes gets translated.");
      assert.equal(data[5], "Yes", "Alt attributes gets translated.");
      assert.equal(data[6], "B", "Accesskey gets translated.");
      
      assert.equal(data[7], "Yes", "Aria-Label gets translated.");
      assert.equal(data[8], "Value", "Aria-valuetext gets translated.");
      assert.equal(data[9], "Hint", "Aria-moz-hint gets translated.");
      
      assert.equal(data[10], "Yes", "Form placeholders are translateable.");
      
      assert.equal(data[11], "Yes", "Labels of select options and context menus are translateable.");

      done();
    }
  });
});

exports.testHtmlLocalization = createTest("en-GB", function(assert, loader, done) {
  // Ensure initing html component that watch document creations
  // Note that this module is automatically initialized in
  // cuddlefish.js:Loader.main in regular addons. But it isn't for unit tests.
  let loaderHtmlL10n = loader.require("sdk/l10n/html");
  loaderHtmlL10n.enable();

  let uri = require("sdk/self").data.url("test-localization.html");
  loader.require("sdk/tabs").open({
    url: uri,
    onReady: function(tab) {
      tab.attach({
        contentURL: uri,
        contentScript: "new " + function ContentScriptScope() {
          let nodes = document.body.querySelectorAll("*[data-l10n-id]");
          self.postMessage([nodes[0].innerHTML,
                            nodes[1].innerHTML,
                            nodes[2].innerHTML,
                            nodes[3].innerHTML,
                            nodes[4].title,
                            nodes[4].getAttribute("alt"),
                            nodes[4].getAttribute("accesskey"),
                            nodes[4].getAttribute("aria-label"),
                            nodes[4].getAttribute("aria-valuetext"),
                            nodes[4].getAttribute("aria-moz-hint"),
                            nodes[5].placeholder,
                            nodes[6].label]);
        },
        onMessage: function (data) {
          assert.equal(
            data[0],
            "Kept as-is",
            "Nodes with unknown id in .properties are kept 'as-is'"
          );
          assert.equal(data[1], "Yes", "HTML is translated");
          assert.equal(
            data[2],
            "no &lt;b&gt;HTML&lt;/b&gt; injection",
            "Content from .properties is text content; HTML can't be injected."
          );
          assert.equal(data[3], "Yes", "Multiple elements with same data-l10n-id are accepted.");

          // Attribute translation tests
          assert.equal(data[4], "Yes", "Title attributes gets translated.");
          assert.equal(data[5], "Yes", "Alt attributes gets translated.");
          assert.equal(data[6], "B", "Accesskey gets translated.");
      
          assert.equal(data[7], "Yes", "Aria-Label gets translated.");
          assert.equal(data[8], "Value", "Aria-valuetext gets translated.");
          assert.equal(data[9], "Hint", "Aria-moz-hint gets translated.");
      
          assert.equal(data[10], "Yes", "Form placeholders are translateable.");
      
          assert.equal(data[11], "Yes", "Labels of select options and context menus are translateable.");

          tab.close(done);
        }
      });
    }
  });
});

exports.testEnUsLocaleName = createTest("en-US", function(assert, loader, done) {
  let _ = loader.require("sdk/l10n").get;

  assert.equal(_("Not translated"), "Not translated",
               "String w/o translation is kept as-is");
  assert.equal(_("Translated"), "Yes",
               "String with translation is correctly translated");

  // Check Unicode char escaping sequences
  assert.equal(_("unicodeEscape"), " @ ",
               "Unicode escaped sequances are correctly converted");

  // Check plural forms regular matching
  assert.equal(_("downloadsCount", 0),
                   "0 downloads",
                   "PluralForm form 'other' for 0 in english");
  assert.equal(_("downloadsCount", 1),
                   "one download",
                   "PluralForm form 'one' for 1 in english");
  assert.equal(_("downloadsCount", 2),
                   "2 downloads",
                   "PluralForm form 'other' for n != 1 in english");

  // Check optional plural forms
  assert.equal(_("pluralTest", 0),
                   "optional zero form",
                   "PluralForm form 'zero' can be optionaly specified. (Isn't mandatory in english)");
  assert.equal(_("pluralTest", 1),
                   "fallback to other",
                   "If the specific plural form is missing, we fallback to 'other'");

  // Ensure that we can omit specifying the generic key without [other]
  // key[one] = ...
  // key[other] = ...  # Instead of `key = ...`
  assert.equal(_("explicitPlural", 1),
                   "one",
                   "PluralForm form can be omitting generic key [i.e. without ...[other] at end of key)");
  assert.equal(_("explicitPlural", 10),
                   "other",
                   "PluralForm form can be omitting generic key [i.e. without ...[other] at end of key)");

  assert.equal(_("first_identifier", "ONE", "TWO"), "the entries are ONE and TWO.", "first_identifier no count");
  assert.equal(_("first_identifier", 0, "ONE", "TWO"), "the entries are ONE and TWO.", "first_identifier with count = 0");
  assert.equal(_("first_identifier", 1, "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "first_identifier with count = 1");
  assert.equal(_("first_identifier", 2, "ONE", "TWO"), "the entries are ONE and TWO.", "first_identifier with count = 2");

  assert.equal(_("second_identifier", "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "second_identifier with no count");
  assert.equal(_("second_identifier", 0, "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "second_identifier with count = 0");
  assert.equal(_("second_identifier", 1, "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "second_identifier with count = 1");
  assert.equal(_("second_identifier", 2, "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "second_identifier with count = 2");

  assert.equal(_("third_identifier", "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "third_identifier with no count");
  assert.equal(_("third_identifier", 0, "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "third_identifier with count = 0");
  assert.equal(_("third_identifier", 2, "ONE", "TWO"), "first entry is ONE and the second one is TWO.", "third_identifier with count = 2");

  done();
});

exports.testUsingJSON = function(assert) {
  assert.equal(usingJSON, !isNative, 'using json');
}

exports.testShortLocaleName = createTest("eo", function(assert, loader, done) {
  let _ = loader.require("sdk/l10n").get;
  assert.equal(_("Not translated"), "Not translated",
               "String w/o translation is kept as-is");
  assert.equal(_("Translated"), "jes",
               "String with translation is correctly translated");

  done();
});


// Before running tests, disable HTML service which is automatially enabled
// in api-utils/addon/runner.js
require('sdk/l10n/html').disable();

require("sdk/test/runner").runTestsFromModule(module);
