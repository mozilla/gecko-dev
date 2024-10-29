/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE = `
<!DOCTYPE HTML>
<html>
<body>
    <input id="modeOpen" type=file></input>
    <input id="modeOpenMultiple" type=file multiple></input>
</body>
</html>
`;
const INPUT_URL = "data:text/html," + encodeURI(PAGE);

async function openFilePickerAndSetFiles(browser, files) {
  await SpecialPowers.spawn(browser, [files], async files => {
    let filePicker = content.SpecialPowers.MockFilePicker;
    filePicker.cleanup();
    filePicker.init(content.browsingContext);
    let filePickerShow = new Promise(resolve => {
      filePicker.showCallback = () => {
        filePicker.setFiles(files);
        filePicker.returnValue = filePicker.returnOk;
        info("MockFilePicker shown, files set!");
        resolve();
      };
    });

    let input =
      files.length > 1
        ? content.document.getElementById("modeOpenMultiple")
        : content.document.getElementById("modeOpen");
    content.document.notifyUserGestureActivation();
    input.click();

    return filePickerShow;
  });
}

add_task(async function filepicker_single_image() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab(INPUT_URL, async browser => {
    const files = new File(["some data"], "hello.jpg", { type: "image/jpg" });

    await openFilePickerAndSetFiles(browser, [files]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot.length, 1, "Glean - One telemertry event gathered!");
  is(snapshot[0].name, "image_input", "Glean - Received correct event!");
  is(
    snapshot[0].extra.image_type,
    "image/jpg",
    "Glean - Event extra correct image type!"
  );
  is(
    snapshot[0].extra.input_type,
    "FilePicker",
    "Glean - Event extra correct input type!"
  );

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function filepicker_single_txt() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab(INPUT_URL, async browser => {
    const files = new File(["some data"], "hello.txt", {
      type: "plain/text",
    });

    await openFilePickerAndSetFiles(browser, [files]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - No telemetry collected for txt file!");

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function filepicker_multiple_images() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab(INPUT_URL, async browser => {
    let files = [];
    kKnownImageMIMETypes.forEach(mimeType => {
      const image = new File(["some data"], "hello", { type: mimeType });
      files.push(image);
    });

    await openFilePickerAndSetFiles(browser, files);

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
    is("FilePicker", event.extra.input_type, "Glean - Correct input type!");
  });

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function filepicker_multiple_mixed() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab(INPUT_URL, async browser => {
    const image1 = new File(["some data"], "A", { type: "plain/text" });
    const txt1 = new File(["some data"], "B", { type: "image/png" });
    const image2 = new File(["some data"], "C", { type: "plain/text" });
    const txt2 = new File(["some data"], "D", { type: "image/bmp" });

    await openFilePickerAndSetFiles(browser, [image1, txt1, image2, txt2]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot.length, 2, "Glean - Correct number of events recorded!");
  is(
    snapshot[0].extra.image_type,
    "image/png",
    "Glean - Correct first image type!"
  );
  is(
    snapshot[0].extra.input_type,
    "FilePicker",
    "Glean - Correct first input type!"
  );
  is(
    snapshot[1].extra.image_type,
    "image/bmp",
    "Glean - Correct second image type!"
  );
  is(
    snapshot[1].extra.input_type,
    "FilePicker",
    "Glean - Correct second input type!"
  );

  // Cleanup
  Services.fog.testResetFOG();
});

add_task(async function filepicker_multiple_txt() {
  let snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - Initially no event gathered!");

  await BrowserTestUtils.withNewTab(INPUT_URL, async browser => {
    const txt1 = new File(["some data"], "A", { type: "plain/text" });
    const txt2 = new File(["some data"], "B", { type: "plain/text" });
    const txt3 = new File(["some data"], "C", { type: "plain/text" });
    const txt4 = new File(["some data"], "D", { type: "plain/text" });

    await openFilePickerAndSetFiles(browser, [txt1, txt2, txt3, txt4]);

    await Services.fog.testFlushAllChildren();
  });

  snapshot = Glean.imageInputTelemetry.imageInput.testGetValue();
  is(snapshot, null, "Glean - No non-image input events collected!");

  // Cleanup
  Services.fog.testResetFOG();
});
