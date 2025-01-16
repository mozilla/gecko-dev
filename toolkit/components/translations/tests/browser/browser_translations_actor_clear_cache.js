/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

  const recordsToCreate = partialMock
    .concat(partialMockInterrupted)
    .concat(completeMockTo)
    .concat(completeMockFrom)
    .concat(completeSingleMock);

  await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
    recordsToCreate,
    expectedCreatedRecordsCount: 5 * RECORDS_PER_LANGUAGE_PAIR,
  });

  const cleanupLocales = await mockLocales({
    appLocales: [mockAppLocale],
  });

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

  await cleanupLocales();
  await cleanup();
});
