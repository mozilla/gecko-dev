/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use stric";

async function setClipboard(file) {
  const tmp = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  tmp.initWithPath(file);

  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor("application/x-moz-file");
  trans.setTransferData("application/x-moz-file", tmp);

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

async function createPasteEvent(browser, addListener = true) {
  await SpecialPowers.spawn(browser, [addListener], async addListener => {
    function onPaste(event) {
      // Access DataTransfer so DragEvent::GetDataTransfer() is triggered.
      event.clipboardData;
      if (addListener) {
        content.document.removeEventListener("paste", onPaste);
      }
    }

    if (addListener) {
      content.document.addEventListener("paste", onPaste);
    }
  });

  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);
}

add_task(async function paste_single_image() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "example.com"
  );
  let browser = tab.linkedBrowser;

  const file = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "test-file.jpg",
    0o600
  );

  await setClipboard(file);

  await createPasteEvent(browser);

  await Services.fog.testFlushAllChildren();

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot.length, 1, "Glean - One telemetry event gathered!");
  is(snapshot[0].name, "image_input", "Glean - Received correct event!");
  is(
    snapshot[0].extra.image_type,
    "image/jpeg",
    "Glean - Event extra correct image type!"
  );
  is(
    snapshot[0].extra.input_type,
    "Paste",
    "Glean - Event extra correct input type!"
  );

  // Cleanup
  await IOUtils.remove(file);
  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function paste_single_image_about() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab"
  );
  let browser = tab.linkedBrowser;

  const file = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "test-file.jpg",
    0o600
  );

  await setClipboard(file);

  await createPasteEvent(browser, false);

  await Services.fog.testFlushAllChildren();

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(
    snapshot,
    null,
    "Glean - No telemetry event gathered as expected on about:newtab!"
  );

  // Cleanup
  await IOUtils.remove(file);
  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

// Separate test since about:* pages use system principal
add_task(async function paste_single_image_system() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );
  let browser = tab.linkedBrowser;

  const file = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "test-file.jpg",
    0o600
  );

  await setClipboard(file);

  await createPasteEvent(browser);

  await Services.fog.testFlushAllChildren();

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(
    snapshot,
    null,
    "Glean - No telemetry event gathered as expected on other about:* pages!"
  );

  // Cleanup
  await IOUtils.remove(file);
  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function paste_single_txt() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "example.com"
  );
  let browser = tab.linkedBrowser;

  const file = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "test-file.txt",
    0o600
  );
  await IOUtils.writeUTF8(file, "Hello World!");

  await setClipboard(file);

  await createPasteEvent(browser);

  await Services.fog.testFlushAllChildren();

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - No telemetry collected for txt file!");

  // Cleanup
  await IOUtils.remove(file);
  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});
