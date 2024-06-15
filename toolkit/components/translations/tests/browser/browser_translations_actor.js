/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This file contains unit tests for the translations actor. Generally it's preferable
 * to test behavior in a full integration test, but occasionally it's useful to test
 * specific implementation behavior.
 */

/**
 * Enforce the pivot language behavior in ensureLanguagePairsHavePivots.
 */
add_task(async function test_pivot_language_behavior() {
  info(
    "Expect 4 console.error messages notifying of the lack of a pivot language."
  );

  const fromLanguagePairs = [
    { fromLang: "en", toLang: "es" },
    { fromLang: "es", toLang: "en" },
    { fromLang: "en", toLang: "yue" },
    { fromLang: "yue", toLang: "en" },
    // This is not a bi-directional translation.
    { fromLang: "is", toLang: "en" },
    // These are non-pivot languages.
    { fromLang: "zh", toLang: "ja" },
    { fromLang: "ja", toLang: "zh" },
  ];

  // Sort the language pairs, as the order is not guaranteed.
  function sort(list) {
    return list.sort((a, b) =>
      `${a.fromLang}-${a.toLang}`.localeCompare(`${b.fromLang}-${b.toLang}`)
    );
  }

  const { cleanup } = await setupActorTest({
    languagePairs: fromLanguagePairs,
  });

  const { languagePairs } = await TranslationsParent.getSupportedLanguages();

  // The pairs aren't guaranteed to be sorted.
  languagePairs.sort((a, b) =>
    TranslationsParent.languagePairKey(a.fromLang, a.toLang).localeCompare(
      TranslationsParent.languagePairKey(b.fromLang, b.toLang)
    )
  );

  if (SpecialPowers.isDebugBuild) {
    Assert.deepEqual(
      sort(languagePairs),
      sort([
        { fromLang: "en", toLang: "es" },
        { fromLang: "en", toLang: "yue" },
        { fromLang: "es", toLang: "en" },
        { fromLang: "is", toLang: "en" },
        { fromLang: "yue", toLang: "en" },
      ]),
      "Non-pivot languages were removed on debug builds."
    );
  } else {
    Assert.deepEqual(
      sort(languagePairs),
      sort(fromLanguagePairs),
      "Non-pivot languages are retained on non-debug builds."
    );
  }

  return cleanup();
});

/**
 * This test case ensures that stand-alone functions to check language support match the
 * state of the available language pairs.
 */
add_task(async function test_language_support_checks() {
  const { cleanup } = await setupActorTest({
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
      // Only supported as a source language
      { fromLang: "fi", toLang: PIVOT_LANGUAGE },
      // Only supported as a target language
      { fromLang: PIVOT_LANGUAGE, toLang: "sl" },
    ],
  });

  const { languagePairs } = await TranslationsParent.getSupportedLanguages();
  for (const { fromLang, toLang } of languagePairs) {
    ok(
      await TranslationsParent.isSupportedAsFromLang(fromLang),
      "Each from-language should be supported as a translation source language."
    );

    ok(
      await TranslationsParent.isSupportedAsToLang(toLang),
      "Each to-language should be supported as a translation target language."
    );

    is(
      await TranslationsParent.isSupportedAsToLang(fromLang),
      languagePairs.some(({ toLang }) => toLang === fromLang),
      "A from-language should be supported as a to-language if it also exists in the to-language list."
    );

    is(
      await TranslationsParent.isSupportedAsFromLang(toLang),
      languagePairs.some(({ fromLang }) => fromLang === toLang),
      "A to-language should be supported as a from-language if it also exists in the from-language list."
    );
  }

  await usingAppLocale("en", async () => {
    const expected = "en";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  await usingAppLocale("es", async () => {
    const expected = "es";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  // Only supported as a source language
  await usingAppLocale("fi", async () => {
    const expected = "en";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  // Only supported as a target language
  await usingAppLocale("sl", async () => {
    const expected = "sl";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  // Not supported as a source language or a target language.
  await usingAppLocale("ja", async () => {
    const expected = "en";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  await cleanup();
});

async function usingAppLocale(locale, callback) {
  info(`Mocking the locale "${locale}", expect missing resource errors.`);
  const { availableLocales, requestedLocales } = Services.locale;
  Services.locale.availableLocales = [locale];
  Services.locale.requestedLocales = [locale];

  if (Services.locale.appLocaleAsBCP47 !== locale) {
    throw new Error("Unable to change the app locale.");
  }
  await callback();

  // Reset back to the originals.
  Services.locale.availableLocales = availableLocales;
  Services.locale.requestedLocales = requestedLocales;
}

add_task(async function test_translating_to_and_from_app_language() {
  const PIVOT_LANGUAGE = "en";

  const { cleanup } = await setupActorTest({
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  /**
   * Each language pair has multiple models. De-duplicate the language pairs and
   * return a sorted list.
   */
  function getUniqueLanguagePairs(records) {
    const langPairs = new Set();
    for (const { fromLang, toLang } of records) {
      langPairs.add(TranslationsParent.languagePairKey(fromLang, toLang));
    }
    return Array.from(langPairs)
      .sort()
      .map(langPair => {
        const [fromLang, toLang] = langPair.split(",");
        return {
          fromLang,
          toLang,
        };
      });
  }

  function assertLanguagePairs({
    app,
    requested,
    message,
    languagePairs,
    includePivotRecords,
  }) {
    return usingAppLocale(app, async () => {
      Assert.deepEqual(
        getUniqueLanguagePairs(
          await TranslationsParent.getRecordsForTranslatingToAndFromAppLanguage(
            requested,
            includePivotRecords
          )
        ),
        languagePairs,
        message
      );
    });
  }

  await assertLanguagePairs({
    message:
      "When the app locale is the pivot language, download another language.",
    app: PIVOT_LANGUAGE,
    requested: "fr",
    includePivotRecords: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "When a pivot language is required, they are both downloaded.",
    app: "fr",
    requested: "pl",
    includePivotRecords: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message:
      "When downloading the pivot language, only download the one for the app's locale.",
    app: "es",
    requested: PIVOT_LANGUAGE,
    includePivotRecords: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message:
      "Delete just the requested language when the app locale is the pivot language",
    app: PIVOT_LANGUAGE,
    requested: "fr",
    includePivotRecords: false,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "Delete just the requested language, and not the pivot.",
    app: "fr",
    requested: "pl",
    includePivotRecords: false,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "Delete just the requested language, and not the pivot.",
    app: "fr",
    requested: "pl",
    includePivotRecords: false,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "Delete just the pivot → app and app → pivot.",
    app: "es",
    requested: PIVOT_LANGUAGE,
    includePivotRecords: false,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message:
      "If the app and request language are the same, nothing is returned.",
    app: "fr",
    requested: "fr",
    includePivotRecords: true,
    languagePairs: [],
  });

  return cleanup();
});

add_task(async function test_firstVisualChange() {
  const { cleanup } = await setupActorTest({
    languagePairs: [{ fromLang: "en", toLang: "es" }],
  });

  const parent = getTranslationsParent();

  Assert.equal(
    parent.languageState.hasVisibleChange,
    false,
    "No visual translation change has occurred yet"
  );

  parent.receiveMessage({
    name: "Translations:ReportFirstVisibleChange",
  });

  Assert.equal(
    parent.languageState.hasVisibleChange,
    true,
    "A change occurred."
  );

  return cleanup();
});

/**
 * This tests deleting the cache and a few scenarios on how it should behave.
 */
add_task(async function test_delete_cache() {
  const mockAppLocale = "en";
  const { remoteClients, cleanup } = await setupActorTest({
    autoDownloadFromRemoteSettings: false,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "fr" },
      { fromLang: "fr", toLang: "en" },
      { fromLang: "qq", toLang: "en" },
    ],
  });

  // Partially downloaded one-side mock
  const partialMock = createRecordsForLanguagePair("en", "es");
  partialMock.forEach(item => (item.attachment.isDownloaded = true));
  // Only one model record was downloaded (probably a download interruption or similar)
  const partialMockInterrupted = createRecordsForLanguagePair("yy", "en");
  partialMockInterrupted[0].attachment.isDownloaded = true;
  console.log(
    `partialMockInterrupted ${JSON.stringify(partialMockInterrupted)}`
  );
  // Complete mock
  const completeMockTo = createRecordsForLanguagePair("en", "bg");
  completeMockTo.forEach(item => (item.attachment.isDownloaded = true));
  const completeMockFrom = createRecordsForLanguagePair("bg", "en");
  completeMockFrom.forEach(item => (item.attachment.isDownloaded = true));
  // Complete single direction mock
  const completeSingleMock = createRecordsForLanguagePair("nn", "en");
  completeSingleMock.forEach(item => (item.attachment.isDownloaded = true));

  const created = partialMock
    .concat(partialMockInterrupted)
    .concat(completeMockTo)
    .concat(completeMockFrom)
    .concat(completeSingleMock);

  // Sync-in downloaded records
  info('Emitting a remote client "sync" event with mock records.');
  await remoteClients.translationModels.client.emit("sync", {
    data: {
      created,
      updated: [],
      deleted: [],
    },
  });

  // Mock locale
  info(
    `Mocking the locale "${mockAppLocale}", expect missing resource errors.`
  );
  const { availableLocales, requestedLocales } = Services.locale;
  Services.locale.availableLocales = [mockAppLocale];
  Services.locale.requestedLocales = [mockAppLocale];

  // Testing initial file statuses of different scenarios

  // es -> en (not-downloaded), en-es (downloaded)
  const bidirectionalPartialDownload =
    await TranslationsParent.getDownloadedFileStatusToAndFromPair("en", "es");
  Assert.equal(
    bidirectionalPartialDownload.nonDownloadedPairs.has("es,en"),
    true,
    `The es to en side is not downloaded.`
  );
  Assert.equal(
    bidirectionalPartialDownload.downloadedPairs.has("es,en"),
    false,
    `The es to en side is not downloaded.`
  );
  Assert.equal(
    bidirectionalPartialDownload.nonDownloadedPairs.has("en,es"),
    false,
    `The en to es side is downloaded.`
  );
  Assert.equal(
    bidirectionalPartialDownload.downloadedPairs.has("en,es"),
    true,
    `The en to es side is downloaded.`
  );

  // fr -> en (not-downloaded), en-fr (not-downloaded)
  const bidirectionalNotDownloaded =
    await TranslationsParent.getDownloadedFileStatusToAndFromPair("fr", "en");
  Assert.equal(
    bidirectionalNotDownloaded.nonDownloadedPairs.size,
    2,
    `en to fr and fr to en are not downloaded.`
  );
  Assert.equal(
    bidirectionalNotDownloaded.downloadedPairs.size,
    0,
    `Neither side of en to fr is downloaded.`
  );

  // bg -> en (downloaded), en-bg (downloaded)
  const bidirectionalDownloaded =
    await TranslationsParent.getDownloadedFileStatusToAndFromPair("bg", "en");
  Assert.equal(
    bidirectionalDownloaded.nonDownloadedPairs.size,
    0,
    `Bg to en and en to bg are fully downloaded.`
  );
  Assert.equal(
    bidirectionalDownloaded.downloadedPairs.size,
    2,
    `Both sides of bg to en are downloaded.`
  );

  // en -> nn (downloaded), nn-en (does not exist)
  const unidirectionalDownloaded =
    await TranslationsParent.getDownloadedFileStatusToAndFromPair("en", "nn");
  Assert.equal(
    unidirectionalDownloaded.nonDownloadedPairs.size,
    0,
    `En to nn is fully downloaded.`
  );
  Assert.equal(
    unidirectionalDownloaded.downloadedPairs.size,
    1,
    `Nn to en is downloaded.`
  );

  // yy -> en (a portion of yy->en is downloaded), en-yy (does not exist)
  const unidirectionalIncompleteDownloaded =
    await TranslationsParent.getDownloadedFileStatusToAndFromPair("yy", "en");
  Assert.equal(
    unidirectionalIncompleteDownloaded.nonDownloadedPairs.has("yy,en"),
    true,
    `yy to en has a portion of records downloaded.`
  );
  Assert.equal(
    unidirectionalIncompleteDownloaded.downloadedPairs.has("yy,en"),
    true,
    `Yy to en has a portion of records downloaded.`
  );

  // en -> qq (not-downloaded), qq-en (does not exist)
  const unidirectionalNonDownloaded =
    await TranslationsParent.getDownloadedFileStatusToAndFromPair("en", "qq");
  Assert.equal(
    unidirectionalNonDownloaded.nonDownloadedPairs.size,
    1,
    `Qq to en are not downloaded.`
  );
  Assert.equal(
    unidirectionalNonDownloaded.downloadedPairs.size,
    0,
    `Qq to en is not downloaded.`
  );

  var deletedPairs = await TranslationsParent.deleteCachedLanguageFiles();
  Assert.equal(
    deletedPairs.size,
    2,
    "Only two deletion operation should have completed."
  );

  // Testing deletion status
  // Bi-directional, partially downloaded
  // Testing es -> en and en -> es both exist, but only en -> es is downloaded.
  Assert.equal(
    deletedPairs.has("en,es"),
    true,
    `en-es was successfully set for deletion because only one side of the pair was available.`
  );

  // Bi-directional, not downloaded
  // Testing en -> fr and fr -> en both exist, but neither is downloaded.
  Assert.equal(
    deletedPairs.has("en,fr"),
    false,
    `en-fr wasn't scheduled for deletion because neither side is downloaded.`
  );
  Assert.equal(
    deletedPairs.has("fr,en"),
    false,
    `fr-en wasn't scheduled for deletion because neither side is downloaded.`
  );

  // Bi-directional, downloaded
  // Testing bg -> en and bg -> en both exist and are both are downloaded.
  Assert.equal(
    deletedPairs.has("en,bg"),
    false,
    `en-bg wasn't scheduled for deletion because both sides are downloaded.`
  );
  Assert.equal(
    deletedPairs.has("bg,en"),
    false,
    `bg-en wasn't scheduled for deletion because both sides are downloaded.`
  );

  // Uni-directional, downloaded
  // nn -> en only exists and is downloaded
  Assert.equal(
    deletedPairs.has("en,nn"),
    false,
    `en-nn wasn't scheduled for deletion because it is not available.`
  );
  Assert.equal(
    deletedPairs.has("nn,en"),
    false,
    `nn-en wasn't scheduled for deletion because it is downloaded and complete.`
  );

  // Uni-directional, partial model download (e.g., only the lex file, but nothing else)
  // yy -> en only exists and has a portion of records downloaded
  Assert.equal(
    deletedPairs.has("en,yy"),
    false,
    `en-yy wasn't scheduled for deletion because it is not available.`
  );
  Assert.equal(
    deletedPairs.has("yy,en"),
    true,
    `en-yy was scheduled for deletion because it has only a portion of the model files.`
  );

  // Uni-directional, not downloaded
  // qq -> en only exists and is not downloaded
  Assert.equal(
    deletedPairs.has("en,qq"),
    false,
    `en-qq wasn't scheduled for deletion because it is not available.`
  );
  Assert.equal(
    deletedPairs.has("qq,en"),
    false,
    `qq-en wasn't scheduled for deletion because it is not downloaded.`
  );

  // Reset back to the originals.
  Services.locale.availableLocales = availableLocales;
  Services.locale.requestedLocales = requestedLocales;
  return cleanup();
});
