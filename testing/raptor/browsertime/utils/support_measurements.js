/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */
const os = require("os");

class SupportMeasurements {
  constructor(context, commands, measureCPU, measurePower, measureTime) {
    this.context = context;
    this.commands = commands;

    this.isAndroid =
      context.options.android && context.options.android.enabled == "true";
    this.application = context.options.browser;

    if (this.isAndroid) {
      if (this.application == "firefox") {
        this.androidPackage = this.context.options.firefox.android.package;
      } else {
        this.androidPackage = this.context.options.chrome.android.package;
      }
    }

    this.measurementMap = {
      cpuTime: {
        run: measureCPU,
        start: "_startMeasureCPU",
        stop: "_stopMeasureCPU",
      },
      "power-usage": {
        run: measurePower,
        start: "_startMeasurePower",
        stop: "_stopMeasurePower",
      },
      "wallclock-for-tracking-only": {
        run: measureTime,
        start: "_startMeasureTime",
        stop: "_stopMeasureTime",
      },
    };
  }

  async _gatherAndroidCPUTimes() {
    this.processIDs = await this.commands.android.shell(
      `pgrep -f "${this.androidPackage}"`
    );

    let processTimes = {};
    for (let processID of this.processIDs.split("\n")) {
      let processTimeInfo = (
        await this.commands.android.shell(`ps -p ${processID} -o name=,time=`)
      ).trim();

      if (!processTimeInfo) {
        // Sometimes a processID returns empty info
        continue;
      }

      let nameAndTime = processTimeInfo.split(" ");
      nameAndTime.forEach(el => el.trim());

      let hmsTime = nameAndTime[nameAndTime.length - 1].split(":");
      processTimes[nameAndTime[0]] =
        parseInt(hmsTime[0], 10) * 60 * 60 +
        parseInt(hmsTime[1], 10) * 60 +
        parseInt(hmsTime[2], 10);
    }

    return processTimes;
  }

  async _startMeasureCPU() {
    if (!this.isAndroid) {
      this.startCPUTimes = os.cpus().map(c => c.times);
    } else {
      this.startCPUTimes = await this._gatherAndroidCPUTimes();
    }
  }

  async _stopMeasureCPU(measurementName) {
    let totalTime = 0;

    if (!this.isAndroid) {
      let endCPUTimes = os.cpus().map(c => c.times);
      totalTime = endCPUTimes
        .map(
          (times, i) =>
            times.user -
            this.startCPUTimes[i].user +
            (times.sys - this.startCPUTimes[i].sys)
        )
        .reduce((currSum, val) => currSum + val, 0);
    } else {
      let endCPUTimes = await this._gatherAndroidCPUTimes();

      for (let processName in endCPUTimes) {
        if (Object.hasOwn(this.startCPUTimes, processName)) {
          totalTime +=
            endCPUTimes[processName] - this.startCPUTimes[processName];
        } else {
          // Assumes that the process was started during the test
          totalTime += endCPUTimes[processName];
        }
      }

      // Convert to ms
      totalTime = totalTime * 1000;
    }

    this.context.log.info(`Total CPU time: ${totalTime}ms`);
    this.commands.measure.addObject({
      [measurementName]: [totalTime],
    });
  }

  async _startMeasurePower() {
    // TODO
  }

  async _stopMeasurePower() {
    // TODO
  }

  async _startMeasureTime() {
    this.context.log.info("Starting wallclock measurement");
    this.startTime = performance.now();
  }

  async _stopMeasureTime(measurementName) {
    this.context.log.info("Taking wallclock measurement");
    this.commands.measure.addObject({
      [measurementName]: [
        parseFloat((performance.now() - this.startTime).toFixed(2)),
      ],
    });
  }

  async start() {
    for (let measurementName in this.measurementMap) {
      let measurementInfo = this.measurementMap[measurementName];
      if (!measurementInfo.run) {
        continue;
      }
      await this[measurementInfo.start](measurementName);
    }
  }

  async stop() {
    for (let measurementName in this.measurementMap) {
      let measurementInfo = this.measurementMap[measurementName];
      if (!measurementInfo.run) {
        continue;
      }
      await this[measurementInfo.stop](measurementName);
    }
  }
}

let supportMeasurementObj;
async function startMeasurements(
  context,
  commands,
  measureCPU,
  measurePower,
  measureTime
) {
  supportMeasurementObj = new SupportMeasurements(
    context,
    commands,
    measureCPU,
    measurePower,
    measureTime
  );
  await supportMeasurementObj.start();
}

async function stopMeasurements() {
  if (!supportMeasurementObj) {
    throw new Error("startMeasurements must be called before stopMeasurements");
  }
  await supportMeasurementObj.stop();
}

module.exports = {
  SupportMeasurements,
  startMeasurements,
  stopMeasurements,
};
