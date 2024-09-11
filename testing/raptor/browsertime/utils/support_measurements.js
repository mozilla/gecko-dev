/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

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

  async _startMeasureCPU() {
    // TODO
  }

  async _stopMeasureCPU() {
    // TODO
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
