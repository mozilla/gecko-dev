/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use stric";

async function createDropEventAndSetFiles(browser, files, addListener = true) {
  await SpecialPowers.spawn(
    browser,
    [files, addListener],
    async (files, addListener) => {
      async function onDrop(event) {
        // Access DataTransfer so DragEvent::GetDataTransfer() is triggered.
        event.dataTransfer;
      }

      const dataTransfer = new content.DataTransfer();
      dataTransfer.dropEffect = "copy";
      files.forEach(file => {
        dataTransfer.items.add(file);
      });

      if (addListener) {
        content.document.addEventListener("drop", onDrop);
      }

      let event = content.document.createEvent("DragEvent");
      event.initDragEvent(
        "drop",
        false,
        false,
        content.window,
        0,
        0,
        0,
        0,
        0,
        false,
        false,
        false,
        false,
        0,
        content.document.body,
        dataTransfer
      );
      content.document.dispatchEvent(event);

      if (addListener) {
        content.document.removeEventListener("drop", onDrop);
      }
    }
  );
}

add_task(async function drag_single_image() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("example.com", async browser => {
    const files = new File(["some data"], "hello.jpg", { type: "image/jpg" });

    await createDropEventAndSetFiles(browser, [files]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot.length, 1, "Glean - One telemetry event gathered!");
  is(snapshot[0].name, "image_input", "Glean - Received correct event!");
  is(
    snapshot[0].extra.image_type,
    "image/jpg",
    "Glean - Event extra correct image type!"
  );
  is(
    snapshot[0].extra.input_type,
    "Drop",
    "Glean - Event extra correct input type!"
  );

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function drag_single_image_about() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("about:newtab", async browser => {
    const files = new File(["some data"], "hello.jpg", { type: "image/jpg" });

    await createDropEventAndSetFiles(browser, [files], false);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(
    snapshot,
    null,
    "Glean - No telemetry event gathered as expected on about:newtab!"
  );

  // Cleanup
  Services.fog.testResetFOG();
});

// Separate test since about:* pages use system principal
add_task(async function drag_single_image_system() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    const files = new File(["some data"], "hello.jpg", { type: "image/jpg" });

    await createDropEventAndSetFiles(browser, [files]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(
    snapshot,
    null,
    "Glean - No telemetry event gathered as expected on other about:* pages!"
  );

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function drag_single_txt() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("example.com", async browser => {
    const files = new File(["some data"], "hello.txt", { type: "text/plain" });

    await createDropEventAndSetFiles(browser, [files]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - No telemetry collected for txt file!");

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function drag_multiple_images() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("example.com", async browser => {
    let files = [];
    kKnownImageMIMETypes.forEach(mimeType => {
      const image = new File(["some data"], "hello", { type: mimeType });
      files.push(image);
    });

    await createDropEventAndSetFiles(browser, files);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(
    snapshot.length,
    kKnownImageMIMETypes.length,
    "Glean - Collected telemetry for all known image types!"
  );

  snapshot.forEach(function (event, i) {
    let eventMimeType = event.extra.image_type;
    is(
      kKnownImageMIMETypes[i],
      eventMimeType,
      "Glean - Correct MIME-Type: " + eventMimeType
    );
    is("Drop", event.extra.input_type, "Glean - Correct input type!");
  });

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function drag_multiple_mixed() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("example.com", async browser => {
    const image1 = new File(["some data"], "A", { type: "plain/text" });
    const txt1 = new File(["some data"], "B", { type: "image/png" });
    const image2 = new File(["some data"], "C", { type: "plain/text" });
    const txt2 = new File(["some data"], "D", { type: "image/bmp" });

    await createDropEventAndSetFiles(browser, [image1, txt1, image2, txt2]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot.length, 2, "Glean - Correct number of events recorded!");
  is(
    snapshot[0].extra.image_type,
    "image/png",
    "Glean - Correct first image type!"
  );
  is(snapshot[0].extra.input_type, "Drop", "Glean - Correct first input type!");
  is(
    snapshot[1].extra.image_type,
    "image/bmp",
    "Glean - Correct second image type!"
  );
  is(
    snapshot[1].extra.input_type,
    "Drop",
    "Glean - Correct second input type!"
  );

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function drag_multiple_txt() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab("example.com", async browser => {
    const txt1 = new File(["some data"], "A", { type: "plain/text" });
    const txt2 = new File(["some data"], "B", { type: "plain/text" });
    const txt3 = new File(["some data"], "C", { type: "plain/text" });
    const txt4 = new File(["some data"], "D", { type: "plain/text" });

    await createDropEventAndSetFiles(browser, [txt1, txt2, txt3, txt4]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - No non-image input events collected!");

  // Cleanup
  Services.fog.testResetFOG();
});
