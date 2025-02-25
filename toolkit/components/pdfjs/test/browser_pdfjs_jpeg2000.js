/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

/**
 * Get the the pixels on the drawn canvas.
 * @param {Object} browser
 * @returns {Object}
 */
async function getPixels(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    const { document } = content;
    const canvas = document.querySelector("canvas");

    Assert.ok(!!canvas, "We must have a canvas");

    return new Uint32Array(
      canvas
        .getContext("2d")
        .getImageData(0, 0, canvas.width, canvas.height).data.buffer
    );
  });
}

async function test_jpeg2000(enableWasm) {
  await SpecialPowers.pushPrefEnv({
    set: [["javascript.options.wasm", enableWasm]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(
        browser,
        `${TESTROOT}file_pdfjs_jp2_image.pdf#zoom=100`
      );

      const data = await getPixels(browser);
      if (data.every(x => x === 0xff0000ff)) {
        Assert.ok(true, "All pixels are red");
      } else {
        const i = data.findIndex(x => x !== 0xff0000ff);
        Assert.equal(
          data[i],
          0xff0000ff,
          `The pixel at index ${i} must be red`
        );
      }

      await waitForPdfJSClose(browser);
    }
  );
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_wasm_enabled() {
  await test_jpeg2000(true);
});

add_task(async function test_wasm_disabled() {
  await test_jpeg2000(false);
});
