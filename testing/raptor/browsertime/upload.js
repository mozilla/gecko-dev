/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

const { logTest, logTask } = require("./utils/profiling");

const path = require("path");

async function waitForUpload(timeout, commands, context) {
  let starttime = await commands.js.run(`return performance.now();`);
  let status = "";

  while (
    (await commands.js.run(`return performance.now();`)) - starttime <
      timeout &&
    status != "error" &&
    status != "success"
  ) {
    await commands.wait.byTime(10);

    status = await commands.js.run(
      `return document.getElementById('upload_status').innerHTML;`
    );

    if (status.startsWith("success")) {
      status = "success";
    }

    context.log.info("context.log test: " + status);
    console.log("test: " + status);
  }

  let endtime = await commands.js.run(`return performance.now();`);

  return {
    start: starttime,
    end: endtime,
    upload_status: status,
  };
}

module.exports = logTest("upload test", async function (context, commands) {
  let uploadSiteUrl = `${context.options.browsertime.server_url}`;
  let iterations = `${context.options.browsertime.upload_iterations}`;

  await commands.measure.start(uploadSiteUrl);
  let accumulatedResults = [];
  for (let iteration = 0; iteration < iterations; iteration++) {
    await logTask(context, "cycle " + iteration, async function () {
      await commands.navigate(uploadSiteUrl);

      const driver = context.selenium.driver;
      const webdriver = context.selenium.webdriver;

      const uploadItem = await driver.findElement(
        webdriver.By.id("fileUpload")
      );

      if (context.options.browsertime.moz_fetch_dir == "None") {
        context.log.error(
          "This test depends on the fetch task. Download the file, 'https://github.com/mozilla/perf-automation/raw/master/test_files/upload-test-32MB.dat' and set the os environment variable MOZ_FETCHES_DIR to that directory."
        );
      }

      let localFilePath = path.join(
        `${context.options.browsertime.moz_fetch_dir}`,
        "upload-test-32MB.dat"
      );

      context.log.info("Sending file path: " + localFilePath);
      await uploadItem.sendKeys(localFilePath);

      // Start the test and wait for the upload to complete
      let results = await waitForUpload(120000, commands, context);
      let uploadTime = results.end - results.start;

      // Store result in megabit/seconds, (Upload is a 32 MB file)
      let uploadBandwidth = (32 * 8) / (uploadTime / 1000.0);
      context.log.info(
        "upload results: " +
          results.upload_status +
          " duration: " +
          uploadTime +
          "ms, uploadBandwidth: " +
          uploadBandwidth +
          "Mbit/s"
      );
      accumulatedResults.push(uploadBandwidth);
    });
  }

  commands.measure.addObject({
    custom_data: { "upload-bandwidth": accumulatedResults },
  });
});
