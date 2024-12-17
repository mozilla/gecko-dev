/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_detected_language() {
  const { cleanup, tab } = await loadTestPage({
    // This page will get its language changed by the test.
    page: ENGLISH_PAGE_URL,
    autoDownloadFromRemoteSettings: true,
    languagePairs: [
      // Spanish
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },

      // Norwegian Bokmål
      { fromLang: "nb", toLang: "en" },
      { fromLang: "en", toLang: "nb" },

      // Chinese (Simplified)
      { fromLang: "zh-Hans", toLang: "en" },
      { fromLang: "en", toLang: "zh-Hans" },

      // Chinese (Traditional)
      { fromLang: "zh-Hant", toLang: "en" },
      { fromLang: "en", toLang: "zh-Hant" },
    ],
  });

  async function getDetectedLanguagesFor(docLangTag) {
    await ContentTask.spawn(
      tab.linkedBrowser,
      { docLangTag },
      function changeLanguage({ docLangTag }) {
        content.document.body.parentNode.setAttribute("lang", docLangTag);
      }
    );
    // Clear out the cached values.
    getTranslationsParent().languageState.detectedLanguages = null;
    return getTranslationsParent().getDetectedLanguages(docLangTag);
  }

  Assert.deepEqual(
    await getDetectedLanguagesFor("es"),
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "Spanish is detected as a supported language."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("chr"),
    {
      docLangTag: "chr",
      userLangTag: "en",
      isDocLangTagSupported: false,
    },
    "Cherokee is detected, but is not a supported language."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("no"),
    {
      docLangTag: "nb",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "The Norwegian macro language is detected, but it defaults to Norwegian Bokmål."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-Hans"),
    {
      docLangTag: "zh-Hans",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-Hans' represents Simplified Chinese, commonly used in Mainland China, Malaysia, and Singapore."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh"),
    {
      docLangTag: "zh-Hans",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "The 'zh' language tag defaults to 'zh-Hans' (Simplified Chinese)."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-CN"),
    {
      docLangTag: "zh-Hans",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-CN' maps to 'zh-Hans' (Simplified Chinese), commonly used in Mainland China."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-MY"),
    {
      docLangTag: "zh-Hans",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-MY' maps to 'zh-Hans' (Simplified Chinese), commonly used in Malaysia."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-SG"),
    {
      docLangTag: "zh-Hans",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-SG' maps to 'zh-Hans' (Simplified Chinese), commonly used in Singapore."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-Hant"),
    {
      docLangTag: "zh-Hant",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-Hant' represents Traditional Chinese, commonly used in Hong Kong, Macau, and Taiwan."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-TW"),
    {
      docLangTag: "zh-Hant",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-TW' maps to 'zh-Hant' (Traditional Chinese), commonly used in Taiwan."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-HK"),
    {
      docLangTag: "zh-Hant",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-HK' maps to 'zh-Hant' (Traditional Chinese), commonly used in Hong Kong."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-MO"),
    {
      docLangTag: "zh-Hant",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "'zh-MO' maps to 'zh-Hant' (Traditional Chinese), commonly used in Macau."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-Hant-CN"),
    {
      docLangTag: "zh-Hant",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "An explicit 'Hant' script tag takes precedence, even though 'Hans' is commonly used in Mainland China."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("zh-Hans-TW"),
    {
      docLangTag: "zh-Hans",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "An explicit 'Hans' script tag takes precedence, even though 'Hant' is commonly used in Taiwan."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("es-Latn-ES"),
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "A maximized lang tag with a script will resolve to the model's lang tag if the script matches the expectation."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("es-Hans"),
    {
      docLangTag: "es-Hans",
      userLangTag: "en",
      isDocLangTagSupported: false,
    },
    "A valid lang tag with a nonstandard script will be recognized, but is not resolvable to a model due to the script mismatch."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("spa"),
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    'The three letter "spa" locale is canonicalized to "es".'
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("gibberish"),
    {
      docLangTag: "en",
      userLangTag: null,
      isDocLangTagSupported: true,
    },
    "A gibberish locale is discarded, and the language is detected."
  );

  return cleanup();
});
