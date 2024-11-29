/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */
/* eslint-disable mozilla/avoid-Date-timing */

const os = require("os");
const path = require("path");
const fs = require("fs");

const usbPowerProfiler = require(
  path.join(
    process.env.BROWSERTIME_ROOT,
    "node_modules",
    "usb-power-profiling",
    "usb-power-profiling.js"
  )
);

const {
  gatherWindowsPowerUsage,
  getBrowsertimeResultsPath,
  startWindowsPowerProfiling,
  stopWindowsPowerProfiling,
} = require("./profiling");

class SupportMeasurements {
  constructor(context, commands, measureCPU, measurePower, measureTime) {
    this.context = context;
    this.commands = commands;
    this.testTimes = [];

    this.isWindows11 =
      os.type() == "Windows_NT" && /10.0.2[2-9]/.test(os.release());

    this.isAndroid =
      context.options.android && context.options.android.enabled == "true";
    this.application = context.options.browser;

    if (this.isAndroid) {
      if (this.application == "firefox") {
        this.androidPackage = this.context.options.firefox.android.package;
      } else if (this.application == "chrome") {
        this.androidPackage = "com.android.chrome";
      } else {
        this.androidPackage = this.context.options.chrome.android.package;
      }
    }

    this.measurementMap = {
      cpuTime: {
        run: measureCPU,
        initialize: null,
        start: "_startMeasureCPU",
        stop: "_stopMeasureCPU",
        finalize: null,
      },
      powerUsageSupport: {
        run: measurePower,
        initialize: "_initializeMeasurePower",
        start: "_startMeasurePower",
        stop: "_stopMeasurePower",
        finalize: "_finalizeMeasurePower",
      },
      "wallclock-for-tracking-only": {
        run: measureTime,
        initialize: null,
        start: "_startMeasureTime",
        stop: "_stopMeasureTime",
        finalize: null,
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
    this.context.log.info("Starting CPU Time measurements");
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

  async _initializeMeasurePower() {
    this.context.log.info("Initializing power usage measurements");
    if (this.isAndroid) {
      await usbPowerProfiler.startSampling();
    } else if (this.isWindows11) {
      await startWindowsPowerProfiling(this.context.index);
    }
  }

  async _startMeasurePower() {
    this.context.log.info("Starting power usage measurements");
    this.startPowerTime = Date.now();
  }

  async _stopMeasurePower(measurementName) {
    this.context.log.info("Taking power usage measurements");
    if (this.isAndroid) {
      let powerUsageData = await usbPowerProfiler.getPowerData(
        this.startPowerTime,
        Date.now()
      );
      let powerUsage = powerUsageData[0].samples.data.reduce(
        (currSum, currVal) => currSum + Number.parseInt(currVal[1]),
        0
      );

      const powerProfile = await usbPowerProfiler.profileFromData();
      const browsertimeResultsPath = await getBrowsertimeResultsPath(
        this.context,
        this.commands,
        true
      );

      const data = JSON.stringify(powerProfile, undefined, 2);
      await fs.promises.writeFile(
        path.join(
          browsertimeResultsPath,
          `profile_power_${this.context.index}.json`
        ),
        data
      );

      this.commands.measure.addObject({
        [measurementName]: [powerUsage],
      });
    } else if (this.isWindows11) {
      this.testTimes.push([this.startPowerTime, Date.now()]);
    }
  }

  async _finalizeMeasurePower() {
    this.context.log.info("Finalizing power usage measurements");
    if (this.isAndroid) {
      await usbPowerProfiler.stopSampling();
      await usbPowerProfiler.resetPowerData();
    } else if (this.isWindows11) {
      await stopWindowsPowerProfiling();

      let powerData = await gatherWindowsPowerUsage(this.testTimes);
      powerData.forEach((powerUsage, ind) => {
        if (!this.commands.measure.result[ind].extras.powerUsageSupport) {
          this.commands.measure.result[ind].extras.powerUsageSupport = [];
        }
        this.commands.measure.result[ind].extras.powerUsageSupport.push({
          powerUsageSupport: powerUsage,
        });
      });
    }
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

  async reset(context, commands) {
    this.testTimes = [];
    this.context = context;
    this.commands = commands;
  }

  async initialize() {
    for (let measurementName in this.measurementMap) {
      let measurementInfo = this.measurementMap[measurementName];
      if (!(measurementInfo.run && measurementInfo.initialize)) {
        continue;
      }
      await this[measurementInfo.initialize](measurementName);
    }
  }

  async start() {
    for (let measurementName in this.measurementMap) {
      let measurementInfo = this.measurementMap[measurementName];
      if (!(measurementInfo.run && measurementInfo.start)) {
        continue;
      }
      await this[measurementInfo.start](measurementName);
    }
  }

  async stop() {
    for (let measurementName in this.measurementMap) {
      let measurementInfo = this.measurementMap[measurementName];
      if (!(measurementInfo.run && measurementInfo.stop)) {
        continue;
      }
      await this[measurementInfo.stop](measurementName);
    }
  }

  async finalize() {
    for (let measurementName in this.measurementMap) {
      let measurementInfo = this.measurementMap[measurementName];
      if (!(measurementInfo.run && measurementInfo.finalize)) {
        continue;
      }
      await this[measurementInfo.finalize](measurementName);
    }
  }
}

let supportMeasurementObj;
async function initializeMeasurements(
  context,
  commands,
  measureCPU,
  measurePower,
  measureTime
) {
  if (!supportMeasurementObj) {
    supportMeasurementObj = new SupportMeasurements(
      context,
      commands,
      measureCPU,
      measurePower,
      measureTime
    );
  }

  await supportMeasurementObj.initialize();
}

async function startMeasurements(context, commands) {
  if (!supportMeasurementObj) {
    throw new Error(
      "initializeMeasurements must be called before startMeasurements"
    );
  }

  await supportMeasurementObj.reset(context, commands);
  await supportMeasurementObj.start();
}

async function stopMeasurements() {
  if (!supportMeasurementObj) {
    throw new Error(
      "initializeMeasurements must be called before stopMeasurements"
    );
  }
  await supportMeasurementObj.stop();
}

async function finalizeMeasurements() {
  if (!supportMeasurementObj) {
    throw new Error(
      "initializeMeasurements must be called before finalizeMeasurements"
    );
  }
  await supportMeasurementObj.finalize();
}

module.exports = {
  SupportMeasurements,
  initializeMeasurements,
  startMeasurements,
  stopMeasurements,
  finalizeMeasurements,
};
