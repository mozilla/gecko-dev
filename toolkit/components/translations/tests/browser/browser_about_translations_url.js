/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// runInPage calls ContentTask.spawn, which injects ContentTaskUtils in the
// scope of the callback. Eslint doesn't know about that.
/* global ContentTaskUtils */

const languagePairs = [
  { fromLang: "en", toLang: "es" },
  { fromLang: "es", toLang: "en" },
  { fromLang: "is", toLang: "en" },
  { fromLang: "is", toLang: "es" },
  { fromLang: "es", toLang: "fr" },
  { fromLang: "detect", toLang: "fr" },
  { fromLang: "en", toLang: "fr" },
];

/**
 * Tests that the information in the translations page is
 * updated correctly to match the newly loaded URL.
 */
add_task(async function test_about_translations_url_load() {
  const { runInPage, cleanup } = await openAboutTranslations({
    languagePairs,
    dataForContent: languagePairs,
    autoDownloadFromRemoteSettings: true,
  });

  await runInPage(async ({ selectors }) => {
    const { window } = content;
    let { document } = content;

    const waitForCondition = async (condition, message) => {
      await ContentTaskUtils.waitForCondition(condition, message, 100, 200);
    };

    await waitForCondition(
      () => document.body.hasAttribute("ready"),
      "Waiting for the document to be ready."
    );

    function extractUrlParams(url) {
      const params = new URLSearchParams(url.split("#")[1]);
      const src = params.get("src") ?? "detect";
      const trg = params.get("trg") ?? "";
      const text = params.get("text") ?? "";
      return { src, trg, text };
    }

    function getLangSelectors() {
      const fromSelect = document.querySelector(selectors.fromLanguageSelect);
      const toSelect = document.querySelector(selectors.toLanguageSelect);
      return { fromSelect, toSelect };
    }

    function getTextAreas() {
      const messageArea = document.querySelector(selectors.translationTextarea);
      const resultArea = document.querySelector(selectors.translationResult);
      return { messageArea, resultArea };
    }

    async function assertValuesUpdated({ url }) {
      const { src: newSrc, trg: newTrg, text: newText } = extractUrlParams(url);
      const { fromSelect, toSelect } = getLangSelectors();
      const { messageArea, resultArea } = getTextAreas();

      await waitForCondition(
        () => fromSelect.value === newSrc,
        "The source language is updated"
      );

      await waitForCondition(
        () => toSelect.value === newTrg,
        "The target language is updated"
      );

      await waitForCondition(
        () => messageArea.value === newText,
        "The source text is updated"
      );

      if (newText && newSrc && newTrg) {
        const condition =
          newSrc === "detect"
            ? () =>
                resultArea.innerText.startsWith(`${newText.toUpperCase()} [`) &&
                resultArea.innerText.endsWith(`${newTrg}]`)
            : () =>
                resultArea.innerText ===
                `${newText.toUpperCase()} [${newSrc} to ${newTrg}]`;

        await waitForCondition(
          condition,
          `The output is updated: ${resultArea.innerText}`
        );
      }
    }

    async function assertLanguagesNotChanged({ url }) {
      const { src: newSrc, trg: newTrg } = extractUrlParams(url);
      const { fromSelect, toSelect } = getLangSelectors();

      await waitForCondition(
        () => fromSelect.value !== newSrc,
        "The source language is not changed"
      );
      await waitForCondition(
        () => toSelect.value !== newTrg,
        "The target language is not changed"
      );
    }

    async function loadURL(url, reload = true) {
      info(`Loading new URL ${url}`);

      const oldDocument = window.document;

      window.location.assign(url);
      if (reload) {
        window.location.reload();

        await waitForCondition(
          () => window.document !== oldDocument,
          "Waiting for the old document to be destroyed."
        );

        document = window.document;
      }

      await waitForCondition(
        () => document.body.hasAttribute("ready"),
        "Waiting for the document to be ready."
      );
    }

    await waitForCondition(
      () => document.body.hasAttribute("ready"),
      "Waiting for the document to be ready."
    );

    const startingURL = "about:translations#src=detect&trg=fr&text=Hello";

    await loadURL(startingURL, true);

    await assertValuesUpdated({ url: startingURL });

    const blankURL = "about:translations#src=detect";

    await loadURL(blankURL, true);

    await assertValuesUpdated({ url: blankURL });

    const newURL = "about:translations#src=en&trg=es&text=Hello";

    await loadURL(newURL, true);

    await assertValuesUpdated({ url: newURL });

    const invalidURL = "about:translations#src=zz&trg=zz&text=Hello";

    await loadURL(invalidURL, false);

    await assertLanguagesNotChanged({ url: invalidURL });

    const spanishToFrenchURL = "about:translations#src=es&trg=fr&text=Hola";

    await loadURL(spanishToFrenchURL, true);

    await assertValuesUpdated({ url: spanishToFrenchURL });

    const spanishToEnglishURL = "about:translations#src=es&trg=en&text=Hola";

    await loadURL(spanishToEnglishURL, true);

    await assertValuesUpdated({ url: spanishToEnglishURL });

    ok(true, "All url load tests passed");
  });

  await cleanup();
});

/**
 * Tests that the url is updated correctly from the information
 * in the translations page.
 */
add_task(async function test_about_translations_url_update() {
  const { runInPage, cleanup } = await openAboutTranslations({
    languagePairs,
    dataForContent: languagePairs,
    autoDownloadFromRemoteSettings: true,
  });

  await runInPage(async ({ selectors }) => {
    const { document } = content;

    const waitForCondition = async (condition, message) => {
      await ContentTaskUtils.waitForCondition(condition, message, 100, 200);
    };

    await waitForCondition(
      () => document.body.hasAttribute("ready"),
      "Waiting for the document to be ready."
    );

    const resultArea = document.querySelector(selectors.translationResult);
    const messageArea = document.querySelector(selectors.translationTextarea);
    const fromSelect = document.querySelector(selectors.fromLanguageSelect);
    const toSelect = document.querySelector(selectors.toLanguageSelect);

    async function waitForTranslationTextUpdate() {
      const originalTranslation = resultArea.textContent;
      await waitForCondition(
        () => resultArea.textContent !== originalTranslation,
        "Waiting for the translation text to update."
      );
    }

    async function assertUrlIsExpected({ message, pair }) {
      const expectedHash =
        "#" +
        new URLSearchParams({
          ...{ src: pair.fromLang ?? "detect" },
          ...(pair.toLang ? { trg: pair.toLang } : null),
          ...(message ? { text: message } : null),
        });

      await waitForCondition(
        () => new URL(document.location.href).hash === expectedHash,
        `The URL is updated to ${expectedHash}`
      );
    }

    async function assertTextIsTranslated({ message, fromLang, toLang }) {
      await waitForCondition(
        () => messageArea.value === message,
        "The message is shown in the input text area"
      );

      await waitForCondition(
        () =>
          resultArea.textContent ===
          `${message.toUpperCase()} [${fromLang} to ${toLang}]`,
        "The translated text is shown in the output text area"
      );
    }

    async function changeFromLanguage({ fromLang }) {
      fromLang = fromLang ?? "detect";
      const translatedTextPromise =
        fromSelect.value !== fromLang &&
        toSelect.value !== fromLang &&
        messageArea.value
          ? waitForTranslationTextUpdate()
          : null;

      fromSelect.value = fromLang;
      fromSelect.dispatchEvent(new Event("input"));
      info("Set fromLang to " + fromLang);
      await translatedTextPromise;
    }

    async function changeToLanguage({ toLang }) {
      toLang = toLang ?? "";
      const translatedTextPromise =
        fromSelect.value !== toLang &&
        toSelect.value !== toLang &&
        messageArea.value
          ? waitForTranslationTextUpdate()
          : null;

      toSelect.value = toLang;
      toSelect.dispatchEvent(new Event("input"));
      info("Set toLang to " + toLang);
      await translatedTextPromise;
    }

    async function changeMessage({ message }) {
      message = message ?? "";
      const translatedTextPromise =
        fromSelect.value !== toSelect.value && messageArea.value !== message
          ? waitForTranslationTextUpdate()
          : null;

      messageArea.value = message;
      messageArea.dispatchEvent(new Event("input"));
      info("Set message to " + message);
      await translatedTextPromise;
    }

    async function changeAllValues({ fromLang, toLang, message }) {
      await changeFromLanguage({ fromLang });
      await changeToLanguage({ toLang });
      await changeMessage({ message });
    }

    async function swapLanguages() {
      const swapButton = document.querySelector(selectors.languageSwapButton);

      await waitForCondition(
        () => swapButton.disabled === false,
        "The language swap button is enabled"
      );
      swapButton.dispatchEvent(new Event("click"));
      info("Swapped Languages: " + fromSelect.value + " " + toSelect.value);
    }

    await changeAllValues({
      fromLang: "en",
      toLang: "es",
      message: "Hello, world!",
    });

    await waitForCondition(
      () => messageArea.value === "Hello, world!",
      "Wait for message to be set"
    );

    await assertUrlIsExpected({
      message: "Hello, world!",
      pair: { fromLang: "en", toLang: "es" },
    });

    await changeAllValues({
      fromLang: "es",
      toLang: "en",
      message: "Hola, mundo!",
    });

    await waitForCondition(
      () => messageArea.value === "Hola, mundo!",
      "Wait for message to be set"
    );

    await assertUrlIsExpected({
      message: "Hola, mundo!",
      pair: { fromLang: "es", toLang: "en" },
    });

    await assertTextIsTranslated({
      message: "Hola, mundo!",
      fromLang: "es",
      toLang: "en",
    });

    await changeFromLanguage({ fromLang: "is" });

    await waitForCondition(
      () => fromSelect.value === "is",
      "Wait for from language to be set"
    );

    await assertUrlIsExpected({
      message: "Hola, mundo!",
      pair: { fromLang: "is", toLang: "en" },
    });

    await assertTextIsTranslated({
      message: "Hola, mundo!",
      fromLang: "is",
      toLang: "en",
    });

    await changeToLanguage({ toLang: "es" });

    await waitForCondition(
      () => toSelect.value === "es",
      "Wait for to language to be set"
    );

    await assertUrlIsExpected({
      message: "Hola, mundo!",
      pair: { fromLang: "is", toLang: "es" },
    });

    await assertTextIsTranslated({
      message: "Hola, mundo!",
      fromLang: "is",
      toLang: "es",
    });

    await changeMessage({ message: "Halló, heimur!" });

    await waitForCondition(
      () => messageArea.value === "Halló, heimur!",
      "Wait for message to be set"
    );

    await assertUrlIsExpected({
      message: "Halló, heimur!",
      pair: { fromLang: "is", toLang: "es" },
    });

    await assertTextIsTranslated({
      message: "Halló, heimur!",
      fromLang: "is",
      toLang: "es",
    });

    await changeFromLanguage({ fromLang: "en" });

    await waitForCondition(
      () => fromSelect.value === "en",
      "Wait for from language to be set"
    );

    await assertUrlIsExpected({
      message: "Halló, heimur!",
      pair: { fromLang: "en", toLang: "es" },
    });

    await assertTextIsTranslated({
      message: "Halló, heimur!",
      fromLang: "en",
      toLang: "es",
    });

    await swapLanguages();

    await waitForCondition(
      () => fromSelect.value === "es",
      "Wait for the source language to be swapped"
    );

    await waitForCondition(
      () => toSelect.value === "en",
      "Wait for the target language to be swapped"
    );

    await assertUrlIsExpected({
      message: "HALLÓ, HEIMUR! [en to es]",
      pair: { fromLang: "es", toLang: "en" },
    });

    await assertTextIsTranslated({
      message: "HALLÓ, HEIMUR! [en to es]",
      fromLang: "es",
      toLang: "en",
    });

    await changeMessage({ message: "Hola, mundo!" });

    await waitForCondition(
      () => messageArea.value === "Hola, mundo!",
      "Wait for message to be set"
    );

    await assertUrlIsExpected({
      message: "Hola, mundo!",
      pair: { fromLang: "es", toLang: "en" },
    });

    await assertTextIsTranslated({
      message: "Hola, mundo!",
      fromLang: "es",
      toLang: "en",
    });

    await swapLanguages();

    await waitForCondition(
      () => fromSelect.value === "en",
      "Wait for the source language to be swapped"
    );

    await waitForCondition(
      () => toSelect.value === "es",
      "Wait for the target language to be swapped"
    );

    await assertUrlIsExpected({
      message: "HOLA, MUNDO! [es to en]",
      pair: { fromLang: "en", toLang: "es" },
    });

    await assertTextIsTranslated({
      message: "HOLA, MUNDO! [es to en]",
      fromLang: "en",
      toLang: "es",
    });

    await changeMessage({ message: "Hello, world!" });

    await waitForCondition(
      () => messageArea.value === "Hello, world!",
      "Wait for message to be set"
    );

    await assertUrlIsExpected({
      message: "Hello, world!",
      pair: { fromLang: "en", toLang: "es" },
    });

    await changeAllValues({
      fromLang: null,
      toLang: "fr",
      message: null,
    });

    await waitForCondition(
      () => fromSelect.value === "detect",
      "Wait for from language to be set to detect"
    );

    await waitForCondition(
      () => messageArea.value === "",
      "Wait for message to be set to empty"
    );

    await assertUrlIsExpected({
      message: null,
      pair: { fromLang: "detect", toLang: "fr" },
    });

    ok(true, "All url update tests passed");
  });

  await cleanup();
});
