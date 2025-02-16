/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

// Test js in pdf file.
add_task(async function test_js_signature_storage() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_empty_test.pdf"
      );

      await SpecialPowers.spawn(browser, [], async () => {
        function getStoredSignaturesChangedPromise() {
          const { promise, resolve } = Promise.withResolvers();
          content.addEventListener("storedSignaturesChanged", resolve, {
            once: true,
          });
          return promise;
        }
        const HANDLE_SIGNATURE = "PDFJS:Parent:handleSignature";

        const actor = content.windowGlobalChild.getActor("Pdfjs");
        let all = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "get",
        });
        is(all.length, 0, "No signature should be present");

        let promise = getStoredSignaturesChangedPromise();
        const uuid = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "create",
          description: "test",
          signatureData: "1234",
        });
        await promise;

        all = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "get",
        });

        is(all.length, 1, "One signature should be present");
        is(all[0].description, "test", "Must have the correct description");
        is(all[0].signatureData, "1234", "Must have the correct signatureData");

        promise = getStoredSignaturesChangedPromise();
        await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "delete",
          uuid,
        });
        await promise;

        all = await actor.sendQuery(HANDLE_SIGNATURE, {
          action: "get",
        });

        is(all.length, 0, "No signature should be present");
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
