/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

add_task(async function run_test() {
  info("Starting test...");

  await BrowserTestUtils.withNewTab({ gBrowser, url: emptyPage }, () =>
    GleanPings.userCharacteristics.testSubmission(
      () => {
        // Did we assign a value we got out of about:fingerprintingprotection?
        const value = Glean.characteristics.canvasdata1.testGetValue();
        Assert.notEqual(value, "");
      },
      async () => {
        const populated = TestUtils.topicObserved(
          "user-characteristics-populating-data-done",
          () => true
        );
        Services.obs.notifyObservers(
          null,
          "user-characteristics-testing-please-populate-data"
        );
        await populated;
        GleanPings.userCharacteristics.submit();
      }
    )
  );
});
