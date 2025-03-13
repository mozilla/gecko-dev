/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html>Test preview of Trusted* instances
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

  let message = await executeAndWaitForResultMessage(
    hud,
    `myPolicy.createHTML("hello")`,
    `TrustedHTML`
  );
  is(
    message.node.innerText.trim(),
    `TrustedHTML "<my-policy>hello</my-policy>"`,
    "Got expected result for TrustedHTML"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `myPolicy.createScript("const hello = 'world'")`,
    `TrustedScript`
  );
  is(
    message.node.innerText.trim(),
    `TrustedScript "/* myPolicy */ const hello = 'world'"`,
    "Got expected result for TrustedScript"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `myPolicy.createScriptURL("https://example.com/trusted")`,
    `TrustedScriptURL`
  );
  is(
    message.node.innerText.trim(),
    `TrustedScriptURL https://example.com/trusted?myPolicy`,
    "Got expected result for TrustedScriptURL"
  );
});
