"use strict";

/**
 * This test is used to ensure that Glean probe 'media::error' and
 * 'mediaPlayback::decodeError` can be recorded correctly.
 */

const testCases = [
  // Will fail on reading metadata
  {
    fileName: "bogus.wav",
    mediaError: {
      error_type: "SrcNotSupportedErr",
    },
  },
  {
    fileName: "404.webm",
    mediaError: {
      error_type: "SrcNotSupportedErr",
    },
  },
  // Failed with the key system
  {
    fileName: "404.mp4",
    mediaError: {
      error_type: "SrcNotSupportedErr",
      key_system: "org.w3.clearkey",
      error_name: "NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR",
    },
  },
  // Failed during decoding
  {
    fileName: "decode_error_vp9.webm",
    mediaError: {
      error_type: "DecodeErr",
      error_name: "NS_ERROR_DOM_MEDIA_DECODE_ERR",
    },
    decodeError: {
      mime_type: "video/vp9",
      error_name: "NS_ERROR_DOM_MEDIA_DECODE_ERR",
    },
  },
];

add_task(async function testGleanMediaErrorProbe() {
  const tab = await openTab();
  for (let test of testCases) {
    // always reset FOG to clean up all previous probes
    Services.fog.testResetFOG();

    info(`running test for '${test.fileName}'`);
    await PlayMediaAndWaitForError(tab, test);

    info(`waiting until glean probe is ready on the parent process`);
    await Services.fog.testFlushAllChildren();

    info(`checking the collected results for '${test.fileName}'`);
    await CheckMediaErrorProbe(test.mediaError);
    if (test.decodeError !== undefined) {
      await CheckDecodeErrorProbe(test.decodeError);
    }
  }
  BrowserTestUtils.removeTab(tab);
});

// Following are helper functions
async function PlayMediaAndWaitForError(tab, testInfo) {
  await SpecialPowers.spawn(tab.linkedBrowser, [testInfo], async testInfo => {
    const video = content.document.createElement("video");
    if (testInfo.mediaError.key_system) {
      let keySystemAccess = await content.navigator.requestMediaKeySystemAccess(
        testInfo.mediaError.key_system,
        [{ "": [{ "": "" }] }]
      );
      let mediaKeys = await keySystemAccess.createMediaKeys();
      await video.setMediaKeys(mediaKeys);
    }
    video.src = testInfo.fileName;
    video.play();
    info(`waiting for an error`);
    ok(
      await new Promise(r => (video.onerror = r)).then(
        _ => true,
        _ => false
      ),
      "Got a media error"
    );
  });
}

async function CheckMediaErrorProbe(expected) {
  const extra = await Glean.media.error.testGetValue()[0].extra;
  is(
    extra.error_type,
    expected.error_type,
    `'${extra.error_type}' is equal to expected '${expected.error_type}'`
  );
  if (expected.error_name !== undefined) {
    is(
      extra.error_name,
      expected.error_name,
      `'${extra.error_name}' is equal to expected '${expected.error_name}'`
    );
  }
  if (expected.key_system !== undefined) {
    is(
      extra.key_system,
      expected.key_system,
      `'${extra.key_system}' is equal to expected '${expected.key_system}'`
    );
  }
}

async function CheckDecodeErrorProbe(expected) {
  const extra = await Glean.mediaPlayback.decodeError.testGetValue()[0].extra;
  is(
    extra.mime_type,
    expected.mime_type,
    `'${extra.mime_type}' is equal to expected '${expected.mime_type}'`
  );
  is(
    extra.error_name,
    expected.error_name,
    `'${extra.error_name}' is equal to expected '${expected.error_name}'`
  );
}
