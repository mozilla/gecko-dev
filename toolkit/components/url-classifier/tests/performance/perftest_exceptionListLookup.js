/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var perfMetadata = {
  owner: "Privacy Team",
  name: "UrlClassifier.ExceptionListLookup",
  description: "Test the speed of nsIUrlClassifierExceptionList#matches.",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "UrlClassifier.ExceptionListLookup iterations",
          unit: "iterations",
        },
        {
          name: "UrlClassifier.ExceptionListLookup accumulatedTime",
          unit: "ms",
        },
        { name: "UrlClassifier.ExceptionListLookup perCallTime", unit: "ms" },
      ],
      verbose: true,
    },
  },
  tags: ["url-classifier"],
};

/**
 * Convert a JS object from RemoteSettings to an nsIUrlClassifierExceptionListEntry.
 * @param {Object} rsObject - The JS object from RemoteSettings to convert.
 * @returns {nsIUrlClassifierExceptionListEntry} The converted nsIUrlClassifierExceptionListEntry.
 */
function rsObjectToEntry(rsObject) {
  let entry = Cc[
    "@mozilla.org/url-classifier/exception-list-entry;1"
  ].createInstance(Ci.nsIUrlClassifierExceptionListEntry);

  let {
    category: categoryStr,
    urlPattern,
    topLevelUrlPattern = "",
    isPrivateBrowsingOnly = false,
    filterContentBlockingCategories = [],
    classifierFeatures = [],
  } = rsObject;

  const CATEGORY_STR_TO_ENUM = {
    "internal-pref":
      Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_INTERNAL_PREF,
    baseline: Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_BASELINE,
    convenience: Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_CONVENIENCE,
  };

  let category = CATEGORY_STR_TO_ENUM[categoryStr];

  entry.init(
    category,
    urlPattern,
    topLevelUrlPattern,
    isPrivateBrowsingOnly,
    filterContentBlockingCategories,
    classifierFeatures
  );

  return entry;
}

function generateExceptionList() {
  let list = Cc["@mozilla.org/url-classifier/exception-list;1"].createInstance(
    Ci.nsIUrlClassifierExceptionList
  );

  for (let i = 0; i < 2000; i++) {
    list.addEntry(
      rsObjectToEntry({
        urlPattern: `*://tracker${i}.com/*`,
      })
    );
  }

  for (let i = 0; i < 100; i++) {
    for (let j = 0; j < 100; j++) {
      list.addEntry(
        rsObjectToEntry({
          category: "baseline",
          urlPattern: `*://tracker${i}.com/*`,
          topLevelUrlPattern: `*://site${j}.com/*`,
        })
      );
    }
  }

  return list;
}

add_task(function measure_exceptionListLookup() {
  let list = generateExceptionList();

  const measureLookup = measureIterations("UrlClassifier.ExceptionListLookup");

  for (let i = 0; i < 1000; i++) {
    measureLookup.start();
    list.matches(
      Services.io.newURI(`https://unrelated${i}.com/test`),
      Services.io.newURI(`https://toplevel${i}.com/test`),
      false
    );
    measureLookup.stop();
  }

  measureLookup.reportMetrics();
});
