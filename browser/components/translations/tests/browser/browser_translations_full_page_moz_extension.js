/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests a basic panel open, translation, and restoration to the original language.
 */
add_task(async function test_translations_moz_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      web_accessible_resources: ["test_page.html"],
    },
    files: {
      "test_page.html": `<!DOCTYPE html>
        <html lang="es">
          <body>
            <div>
              <h1>Don Quijote de La Mancha</h1>
            </div>
          </body>
        </html>`,
    },
  });

  await extension.startup();

  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: `moz-extension://${extension.uuid}/test_page.html`,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } =
    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

  is(button.getAttribute("data-l10n-id"), "urlbar-translations-button2");

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openPanel({
    expectedFromLanguage: "es",
    expectedToLanguage: "en",
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });

  await FullPageTranslationsTestUtils.clickTranslateButton({
    downloadHandler: resolveDownloads,
  });

  await FullPageTranslationsTestUtils.assertPageIsTranslated({
    fromLanguage: "es",
    toLanguage: "en",
    runInPage,
  });

  await cleanup();
  await extension.unload();
});
