/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html>Test for object previews in console
    <script>
      globalThis.myPolicy = trustedTypes.createPolicy("myPolicy", {
        createHTML: s => "<my-policy>" + s + "</my-policy>",
        createScript: s => "/* myPolicy */ " + s,
        createScriptURL: s => s + "?myPolicy",
      });
    </script>`;

add_task(async function () {
  await pushPref("dom.security.trusted_types.enabled", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  const TESTS = [
    {
      input: `myPolicy.createHTML("hello")`,
      preview: `TrustedHTML "<my-policy>hello</my-policy>"`,
    },
    {
      input: `myPolicy.createScript("const hello = 'world'")`,
      preview: `TrustedScript "/* myPolicy */ const hello = 'world'"`,
    },
    {
      input: `myPolicy.createScriptURL("https://example.com/trusted")`,
      preview: `TrustedScriptURL https://example.com/trusted?myPolicy`,
    },
    {
      input: `new BigInt64Array(Array.from({length: 20}, (_, i) => BigInt(i)))`,
      preview: `BigInt64Array(20) [ 0n, 1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, … ]`,
    },
  ];

  for (const { input, preview } of TESTS) {
    const message = await executeAndWaitForResultMessage(hud, input, "");
    is(
      message.node.innerText.trim(),
      preview,
      `Got expected preview for \`${input}\``
    );
  }
});
