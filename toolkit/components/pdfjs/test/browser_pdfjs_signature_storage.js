/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

// Test js in pdf file.
add_task(async function test_js_sandbox() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_empty_test.pdf"
      );

      await SpecialPowers.spawn(browser, [], async () => {
        const HANDLE_SIGNATURE = "PDFJS:Parent:handleSignature";

        const { promise: twoChangesPromise, resolve } = Promise.withResolvers();
        let changesCount = 0;
        content.addEventListener("storedSignaturesChanged", () => {
          if (++changesCount === 2) {
            resolve();
          }
        });

        const actor = content.windowGlobalChild.getActor("Pdfjs");
        let all = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "get",
        });
        is(all.length, 0, "No signature should be present");

        const uuid = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "create",
          description: "test",
          signatureData: "1234",
        });

        all = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "get",
        });

        is(all.length, 1, "One signature should be present");
        is(all[0].description, "test", "Must have the correct description");
        is(all[0].signatureData, "1234", "Must have the correct signatureData");

        await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "delete",
          uuid,
        });

        all = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "get",
        });

        is(all.length, 0, "No signature should be present");

        await twoChangesPromise;
      });

      const request = indexedDB.deleteDatabase("pdfjs");
      const { promise, resolve, reject } = Promise.withResolvers();
      request.onsuccess = resolve;
      request.onerror = reject;

      try {
        await promise;
      } catch {
        is(false, "The DB must be deleted");
      }

      await waitForPdfJSClose(browser);
    }
  );
});
