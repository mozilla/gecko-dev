/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This list allows pre-existing or 'unfixable' JS issues to remain, while we
 * detect newly occurring issues in shipping JS. It is a list of regexes
 * matching files which have errors:
 */

requestLongerTimeout(2);

const kAllowlist = new Set([
  /browser\/content\/browser\/places\/controller.js$/,
]);

// Normally we would use reflect.sys.mjs to get Reflect.parse. However, if
// we do that, then all the AST data is allocated in reflect.sys.mjs's
// zone. That exposes a bug in our GC. The GC collects reflect.sys.mjs's
// zone but not the zone in which our test code lives (since no new
// data is being allocated in it). The cross-compartment wrappers in
// our zone that point to the AST data never get collected, and so the
// AST data itself is never collected. We need to GC both zones at
// once to fix the problem.
const init = Cc["@mozilla.org/jsreflect;1"].createInstance();
init();

/**
 * Check if an error should be ignored due to matching one of the allowlist
 * objects.
 *
 * @param uri the uri to check against the allowlist
 * @return true if the uri should be skipped, false otherwise.
 */
function uriIsAllowed(uri) {
  for (let allowlistItem of kAllowlist) {
    if (allowlistItem.test(uri.spec)) {
      return true;
    }
  }
  return false;
}

function recursivelyCheckForGleanCalls(obj, parent = null) {
  if (!obj) {
    return;
  }

  if (Array.isArray(obj)) {
    for (let item of obj) {
      recursivelyCheckForGleanCalls(item, { obj, parent });
    }
    return;
  }

  for (let key in obj) {
    if (key == "loc") {
      continue;
    }
    if (typeof obj[key] == "object") {
      recursivelyCheckForGleanCalls(obj[key], { obj, parent });
    }
  }

  if (obj.type != "Identifier" || obj.name != "Glean") {
    return;
  }

  function getMemberName(object, child) {
    if (
      object.type == "MemberExpression" &&
      !object.computed &&
      object.object === child &&
      object.property.type == "Identifier"
    ) {
      return object.property.name;
    }
    return "";
  }

  let cat = getMemberName(parent.obj, obj);
  if (cat) {
    if (Glean.hasOwnProperty(cat)) {
      ok(true, `The category ${cat} should exist in the global Glean object`);
    } else {
      record(
        false,
        `The category ${cat} should exist in the global Glean object`,
        undefined,
        `${obj.loc.source}:${obj.loc.start.line}`
      );
      return;
    }

    let name = getMemberName(parent.parent.obj, parent.obj);
    if (name) {
      if (Glean[cat].hasOwnProperty(name)) {
        ok(true, `The metric ${name} should exist in the Glean.${cat} object`);
      } else {
        record(
          false,
          `The metric ${name} should exist in the Glean.${cat} object`,
          undefined,
          `${obj.loc.source}:${obj.loc.start.line}`,
          // Object metrics are not supported yet in artifact builds (see bug 1883857),
          // so we expect some failures.
          Services.prefs.getBoolPref("telemetry.fog.artifact_build", false)
            ? "fail"
            : undefined
        );
        return;
      }

      let methodOrLabel = getMemberName(
        parent.parent.parent.obj,
        parent.parent.obj
      );
      if (methodOrLabel) {
        if (methodOrLabel in Glean[cat][name]) {
          ok(true, `${methodOrLabel} should exist in Glean.${cat}.${name}`);
        } else {
          record(
            false,
            `${methodOrLabel} should exist in Glean.${cat}.${name}`,
            undefined,
            `${obj.loc.source}:${obj.loc.start.line}`
          );
          return;
        }

        let object = Glean[cat][name];
        let method = methodOrLabel;
        if (typeof Glean[cat][name][methodOrLabel] == "object") {
          method = getMemberName(
            parent.parent.parent.parent.obj,
            parent.parent.parent.obj
          );
          if (!method) {
            return;
          }
          object = Glean[cat][name][methodOrLabel];
        }

        if (method in object) {
          ok(true, `${method} exists`);
          is(
            typeof object[method],
            "function",
            `${method} should be a function`
          );
        } else {
          record(
            false,
            `${method} should exist`,
            undefined,
            `${obj.loc.source}:${obj.loc.start.line}`
          );
        }
      }
    }
  }
}

function parsePromise(uri, parseTarget) {
  return new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", uri, true);
    xhr.onreadystatechange = function () {
      if (this.readyState == this.DONE) {
        let scriptText = this.responseText;
        if (!scriptText.includes("Glean.")) {
          resolve();
          return;
        }

        try {
          info(`Checking ${parseTarget} ${uri}`);
          let parseOpts = {
            source: uri,
            target: parseTarget,
          };
          recursivelyCheckForGleanCalls(
            Reflect.parse(scriptText, parseOpts).body
          );
        } catch (ex) {
          let errorMsg = "Script error reading " + uri + ": " + ex;
          ok(false, errorMsg);
        }
        resolve();
      }
    };
    xhr.onerror = error => {
      ok(false, "XHR error reading " + uri + ": " + error);
      resolve();
    };
    xhr.overrideMimeType("application/javascript");
    xhr.send(null);
  });
}

add_task(async function checkAllTheJS() {
  // In debug builds, even on a fast machine, collecting the file list may take
  // more than 30 seconds, and parsing all files may take four more minutes.
  // For this reason, this test must be explictly requested in debug builds by
  // using the "--setpref parse=<filter>" argument to mach.  You can specify:
  //  - A case-sensitive substring of the file name to test (slow).
  //  - A single absolute URI printed out by a previous run (fast).
  //  - An empty string to run the test on all files (slowest).
  let parseRequested = Services.prefs.prefHasUserValue("parse");
  let parseValue = parseRequested && Services.prefs.getCharPref("parse");
  if (SpecialPowers.isDebugBuild) {
    if (!parseRequested) {
      ok(
        true,
        "Test disabled on debug build. To run, execute: ./mach" +
          " mochitest-browser --setpref parse=<case_sensitive_filter>" +
          " browser/base/content/test/general/browser_parsable_script.js"
      );
      return;
    }
    // Request a 15 minutes timeout (30 seconds * 30) for debug builds.
    requestLongerTimeout(30);
  }

  let uris;
  // If an absolute URI is specified on the command line, use it immediately.
  if (parseValue && parseValue.includes(":")) {
    uris = [NetUtil.newURI(parseValue)];
  } else {
    let appDir = Services.dirsvc.get("GreD", Ci.nsIFile);
    // This asynchronously produces a list of URLs (sadly, mostly sync on our
    // test infrastructure because it runs against jarfiles there, and
    // our zipreader APIs are all sync)
    let startTimeMs = Date.now();
    info("Collecting URIs");
    uris = await generateURIsFromDirTree(appDir, [".js", ".jsm", ".mjs"]);
    info("Collected URIs in " + (Date.now() - startTimeMs) + "ms");

    // Apply the filter specified on the command line, if any.
    if (parseValue) {
      uris = uris.filter(uri => {
        if (uri.spec.includes(parseValue)) {
          return true;
        }
        info("Not checking filtered out " + uri.spec);
        return false;
      });
    }
  }

  // We create an array of promises so we can parallelize all our parsing
  // and file loading activity:
  await PerfTestHelpers.throttledMapPromises(uris, uri => {
    if (uriIsAllowed(uri)) {
      info("Not checking allowlisted " + uri.spec);
      return undefined;
    }
    return parsePromise(uri.spec, uriIsESModule(uri) ? "module" : "script");
  });
  ok(true, "All files parsed");
});
