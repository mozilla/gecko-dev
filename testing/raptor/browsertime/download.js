/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

const { logTest, logTask } = require("./utils/profiling");

async function waitForDownload(timeout, commands, context) {
  let starttime = await commands.js.run(`return performance.now();`);
  let status = "";
  let protocolInfo = await commands.js.run(
    `
    // Get all performance entries
    const entries = performance.getEntries();

    // Create an array to store the results
    const protocolInfo = entries.map(entry => ({
        name: entry.name,
        protocol: entry.nextHopProtocol,
    }));

    return protocolInfo;
    `
  );
  context.log.info("protocolInfo: " + JSON.stringify(protocolInfo));
  while (
    (await commands.js.run(`return performance.now();`)) - starttime <
      timeout &&
    status != "error" &&
    status != "success"
  ) {
    await commands.wait.byTime(10);

    status = await commands.js.run(
      `return document.getElementById('download_status').innerHTML;`
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
    download_status: status,
  };
}

module.exports = logTest("download test", async function (context, commands) {
  let downloadSiteUrl = `${context.options.browsertime.server_url}`;
  let iterations = `${context.options.browsertime.download_iterations}`;

  await commands.measure.start(downloadSiteUrl);
  let accumulatedResults = [];
  for (let iteration = 0; iteration < iterations; iteration++) {
    await logTask(context, "cycle " + iteration, async function () {
      await commands.navigate(downloadSiteUrl);

      const driver = context.selenium.driver;
      const webdriver = context.selenium.webdriver;

      const downloadItem = await driver.findElement(
        webdriver.By.id("downloadBtn")
      );

      const actions = driver.actions({ async: true });
      await actions.move({ origin: downloadItem }).click().perform();

      // Start the test and wait for the upload to complete
      let results = await waitForDownload(1200000, commands, context);
      let downloadTime = results.end - results.start;

      // Store result in megabit/seconds
      let downloadBandwidth = (32 * 8) / (downloadTime / 1000.0);
      context.log.info(
        "download results: " +
          results.download_status +
          " duration: " +
          downloadTime +
          "ms, downloadBandwidth: " +
          downloadBandwidth +
          "Mbit/s"
      );
      accumulatedResults.push(downloadBandwidth);
    });
  }

  commands.measure.addObject({
    custom_data: { "download-bandwidth": accumulatedResults },
  });
});
