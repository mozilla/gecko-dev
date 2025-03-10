/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 595223.

const TEST_FILE = "test-network.html";

var hud;

add_task(async function () {
  // Display network requests
  await pushPref("devtools.webconsole.filter.net", true);

  await addTab("about:blank");
  hud = await openConsole();
  await clearOutput(hud);

  const jar = getJar(getRootDirectory(gTestPath));
  const dir = jar
    ? extractJarToTmp(jar)
    : getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_FILE);
  const uri = Services.io.newFileURI(dir);

  const onConsoleMessage = waitForMessageByType(
    hud,
    "running network console logging tests",
    ".console-api"
  );
  const onHTMLFileMessage = waitForMessageByType(
    hud,
    "test-network.html",
    ".network"
  );
  const onImageFileMessage = waitForMessageByType(
    hud,
    "test-image.png",
    ".network"
  );
  const onJSFileMessage = waitForMessageByType(
    hud,
    "testscript.js?foo",
    ".network"
  );

  navigateTo(uri.spec);

  await onConsoleMessage;
  ok(true, "console message is displayed");

  let message = await onHTMLFileMessage;
  ok(
    message.node.querySelector(".url").innerText.startsWith("file://"),
    "HTML file request is displayed"
  );

  message = await onImageFileMessage;
  ok(
    message.node.querySelector(".url").innerText.startsWith("file://"),
    "image file request is displayed"
  );

  message = await onJSFileMessage;
  ok(
    message.node.querySelector(".url").innerText.startsWith("file://"),
    "JS file request is displayed"
  );
});
