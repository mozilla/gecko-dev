/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

const { logTest } = require("./utils/profiling");

module.exports = logTest(
  "dns lookup pageload",
  async function (context, commands) {
    context.log.info("Starting a pageload to measure DNS lookup time");

    const testType = `${context.options.browsertime.test_type}`;
    context.log.info("testType: " + testType);

    const url = "https://httpstat.us/";

    await commands.navigate("about:blank");

    // Idle to allow for confirmation
    await commands.wait.byTime(45000);

    if (testType === "trr_warm") {
      // Ensure the trr connection has been warmed up by making an arbitrary request
      await commands.navigate("https://www.w3.org");
      await commands.wait.byTime(2000);
    }

    // Start measuring
    await commands.measure.start();
    await commands.navigate(url);
    await commands.measure.stop();

    let dns_lookup_time = await commands.js.run(`
      return (window.performance.timing.domainLookupEnd - window.performance.timing.domainLookupStart);
    `);

    let connect_time = await commands.js.run(`
      return (window.performance.timing.connectEnd - window.performance.timing.navigationStart);
    `);

    context.log.info("dns_lookup_time: " + dns_lookup_time);
    context.log.info("connect_time: " + connect_time);

    await commands.measure.addObject({
      custom_data: { dns_lookup_time, connect_time },
    });

    context.log.info("DNS lookup test finished.");
    return true;
  }
);
