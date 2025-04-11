/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// runInPage calls ContentTask.spawn, which injects ContentTaskUtils in the
// scope of the callback. Eslint doesn't know about that.
/* global ContentTaskUtils */

/**
 * Check that the error message bar is displayed
 * when getSupportedLanguages fails
 */
add_task(
  async function test_about_translations_language_load_error_message_bar() {
    // Simulate the getSupportedLanguages error
    const realGetSupportedLanguages = TranslationsParent.getSupportedLanguages;
    TranslationsParent.getSupportedLanguages = () => {
      TranslationsParent.getSupportedLanguages = realGetSupportedLanguages;
      throw new Error("Simulating getSupportedLanguagesError()");
    };

    const { runInPage, cleanup } = await openAboutTranslations({
      autoDownloadFromRemoteSettings: true,
    });

    await runInPage(async ({ selectors }) => {
      const { document } = content;

      await ContentTaskUtils.waitForCondition(
        () => {
          const bar = document.querySelector(
            selectors.languageLoadErrorMessage
          );
          return bar;
        },
        "Waiting for the error message bar to render",
        100,
        200
      );
      const messageBar = document.querySelector(
        selectors.languageLoadErrorMessage
      );
      ok(messageBar, "Error message bar exists in the DOM");
      is(messageBar.hidden, false, "Message bar is visible (not hidden)");
      is(
        messageBar.getAttribute("type"),
        "error",
        "Message bar has type='error'"
      );
    });

    await cleanup();
  }
);
