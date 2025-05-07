/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JSObjectsTestUtils, CONTEXTS } = ChromeUtils.importESModule(
  "resource://testing-common/JSObjectsTestUtils.sys.mjs"
);
JSObjectsTestUtils.init(this);

const EXPECTED_VALUES_FILE = "browser_webconsole_previewers.snapshot.mjs";

add_task(async function () {
  // nsHttpServer does not support https
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const hud = await openNewTabAndConsole("http://example.com");

  await JSObjectsTestUtils.runTest(
    EXPECTED_VALUES_FILE,
    async function ({ context, expression }) {
      if (context == CONTEXTS.CHROME) {
        return undefined;
      }

      const message = await executeAndWaitForResultMessage(hud, expression, "");
      return message.node.innerText.trim();
    }
  );
});
