"use strict";

const formatNumber = new Intl.NumberFormat("en-US", {
  maximumSignificantDigits: 4,
}).format;

/**
 * Given a map from test names to arrays of results, report perfherder metrics
 * and log full results.
 */
function reportMetrics(journal) {
  let metrics = {};
  let text = "\nResults (ms)\n";

  const names = Object.keys(journal);
  const prefixLen = 1 + Math.max(...names.map(str => str.length));

  for (const name in journal) {
    const med = median(journal[name]);
    text += (name + ":").padEnd(prefixLen, " ") + stringify(journal[name]);
    text += "   median " + formatNumber(med) + "\n";
    metrics[name] = med;
  }

  dump(text);
  info("perfMetrics", JSON.stringify(metrics));
}

function median(arr) {
  arr = [...arr].sort((a, b) => a - b);
  const mid = Math.floor(arr.length / 2);

  if (arr.length % 2) {
    return arr[mid];
  }

  return (arr[mid - 1] + arr[mid]) / 2;
}

function stringify(arr) {
  function pad(str) {
    str = str.padStart(7, " ");
    if (str[0] != " ") {
      str = " " + str;
    }
    return str;
  }

  return arr.reduce((acc, elem) => acc + pad(formatNumber(elem)), "");
}

async function startProfiler() {
  let script = SpecialPowers.loadChromeScript(async () => {
    // See profiler doc via: $ MOZ_PROFILER_HELP=1 ./mach run
    const settings = {
      features: ["nomarkerstacks", "nostacksampling"],
      threads: ["*"],
    };

    await Services.profiler.StartProfiler(
      settings.entries,
      settings.interval,
      settings.features,
      settings.threads
    );

    sendAsyncMessage("started");
  });

  return script.promiseOneMessage("started");
}

/**
 * Returns profiler data
 * https://github.com/firefox-devtools/profiler/blob/main/docs-developer/gecko-profile-format.md
 */
async function stopProfiler() {
  let script = SpecialPowers.loadChromeScript(async () => {
    Services.profiler.getProfileDataAsync().then(profileData => {
      Services.profiler.StopProfiler();
      sendAsyncMessage("done", profileData);
    });
  });

  return script.promiseOneMessage("done");
}

/**
 * Look through profiler results for markers with name in names.
 * Return the cumulative duration in ms.
 */
function inspectProfile(pdata, names) {
  let unseen = new Set(names);
  let duration = inspectProfileInternal(pdata, new Set(unseen), unseen);
  // Error if we fail to see each name at least once
  is(unseen.size, 0, "Didn't find: " + [...unseen].join(", "));
  return duration;
}

function inspectProfileInternal(pdata, names, unseen) {
  let duration = 0;

  for (let thread of pdata.threads) {
    const nameIdx = thread.markers.schema.name;
    const startTimeIdx = thread.markers.schema.startTime;
    const endTimeIdx = thread.markers.schema.endTime;

    for (let m of thread.markers.data) {
      let markerName = thread.stringTable[m[nameIdx]];

      if (names.has(markerName)) {
        let d = m[endTimeIdx] - m[startTimeIdx];
        duration += d;
        info(`marker ${markerName}: ${formatNumber(d)} ms`);
        unseen.delete(markerName);
      }
    }
  }

  for (let process of pdata.processes) {
    // Look for markers in child processes
    duration += inspectProfileInternal(process, names, unseen);
  }

  return duration;
}
