/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

function ensureProfilerInitialized() {
  if (Services.profiler.IsActive()) {
    return;
  }
  // Starting and stopping the profiler with the "stackwalk" flag will cause the
  // profiler's stackwalking features to be synchronously initialized. This
  // should prevent us from not initializing BHR quickly enough.
  let features = ["stackwalk"];
  Services.profiler.StartProfiler(1000, 10, features);
  Services.profiler.StopProfiler();
}

add_task(async function test_BHRObserver() {
  if (!Services.telemetry.canRecordExtended) {
    ok("Hang reporting not enabled.");
    return;
  }

  ensureProfilerInitialized();
  do_get_profile();

  Services.fog.initializeFOG();
  Assert.equal(
    null,
    Glean.hangs.modules.testGetValue(),
    "no module reported to glean before the beginning of the test"
  );
  Assert.equal(
    null,
    Glean.hangs.reports.testGetValue(),
    "no hang reported to glean before the beginning of the test"
  );

  let telSvc =
    Cc["@mozilla.org/bhr-telemetry-service;1"].getService().wrappedJSObject;
  ok(telSvc, "Should have BHRTelemetryService");
  let beforeLen = telSvc.payload.hangs.length;

  if (Services.appinfo.OS === "Linux" || Services.appinfo.OS === "Android") {
    // We use the rt_tgsigqueueinfo syscall on Linux which requires a
    // certain kernel version. It's not an error if the system running
    // the test is older than that.
    let kernel =
      Services.sysinfo.get("kernel_version") || Services.sysinfo.get("version");
    if (Services.vc.compare(kernel, "2.6.31") < 0) {
      ok("Hang reporting not supported for old kernel.");
      return;
    }
  }

  let hangsPromise = new Promise(resolve => {
    let hangs = [];
    const onThreadHang = subject => {
      let hang = subject.QueryInterface(Ci.nsIHangDetails);
      if (hang.thread.startsWith("Gecko")) {
        hangs.push(hang);
        if (hangs.length >= 3) {
          Services.obs.removeObserver(onThreadHang, "bhr-thread-hang");
          resolve(hangs);
        }
      }
    };
    Services.obs.addObserver(onThreadHang, "bhr-thread-hang");
  });

  // We're going to trigger two hangs, of various lengths. One should be a
  // transient hang, and the other a permanent hang. We'll wait for the hangs to
  // be recorded.

  // We would like one of our hangs to have annotations but not the other.
  UserInteraction.start("testing.interaction", "val");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => {
    UserInteraction.finish("testing.interaction");
  }, 2000);

  executeSoon(() => {
    let startTime = Date.now();
    // eslint-disable-next-line no-empty
    while (Date.now() - startTime < 10000) {}
  });

  executeSoon(() => {
    let startTime = Date.now();
    // eslint-disable-next-line no-empty
    while (Date.now() - startTime < 1000) {}
  });

  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.OverridePreRelease,
    true
  );
  let childDone = run_test_in_child("child_cause_hang.js");

  // Now we wait for the hangs to have their bhr-thread-hang message fired for
  // them, collect them, and analyze the response.
  let hangs = await hangsPromise;
  equal(hangs.length, 3);
  hangs.forEach(hang => {
    Assert.greater(hang.duration, 0);
    ok(hang.thread == "Gecko" || hang.thread == "Gecko_Child");
    equal(typeof hang.runnableName, "string");

    // hang.stack
    ok(Array.isArray(hang.stack));
    ok(!!hang.stack.length);
    hang.stack.forEach(entry => {
      // Each stack frame entry is either a native or pseudostack entry. A
      // native stack entry is an array with module index (number), and offset
      // (hex string), while the pseudostack entry is a bare string.
      if (Array.isArray(entry)) {
        equal(entry.length, 2);
        equal(typeof entry[0], "number");
        equal(typeof entry[1], "string");
      } else {
        equal(typeof entry, "string");
      }
    });

    // hang.modules
    ok(Array.isArray(hang.modules));
    hang.modules.forEach(module => {
      ok(Array.isArray(module));
      equal(module.length, 2);
      equal(typeof module[0], "string");
      equal(typeof module[1], "string");
    });

    // hang.annotations
    ok(Array.isArray(hang.annotations));
    hang.annotations.forEach(annotation => {
      ok(Array.isArray(annotation));
      equal(annotation.length, 2);
      equal(typeof annotation[0], "string");
      equal(typeof annotation[1], "string");
    });
  });

  // The annotations feature seems unreliable, its test
  // telemetry/tests/unit/test_UserInteraction_annotations.js is disabled on
  // most platforms for very frequent intermittent failures.
  let hasHangWithAnnotations = hangs.some(hang => !!hang.annotations.length);
  (hasHangWithAnnotations ? ok : todo_check_true)(
    hasHangWithAnnotations,
    "at least one hang has annotations"
  );
  ok(
    hangs.some(hang => !hang.annotations.length),
    "at least one hang has no annotation"
  );

  // Check that the telemetry service collected pings which make sense
  Assert.greaterOrEqual(telSvc.payload.hangs.length - beforeLen, 3);
  ok(Array.isArray(telSvc.payload.modules));
  telSvc.payload.modules.forEach(module => {
    ok(Array.isArray(module));
    equal(module.length, 2);
    equal(typeof module[0], "string");
    equal(typeof module[1], "string");
  });

  telSvc.payload.hangs.forEach(hang => {
    Assert.greater(hang.duration, 0);
    ok(hang.thread == "Gecko" || hang.thread == "Gecko_Child");
    equal(typeof hang.runnableName, "string");

    // hang.stack
    ok(Array.isArray(hang.stack));
    ok(!!hang.stack.length);
    hang.stack.forEach(entry => {
      // Each stack frame entry is either a native or pseudostack entry. A
      // native stack entry is an array with module index (number), and offset
      // (hex string), while the pseudostack entry is a bare string.
      if (Array.isArray(entry)) {
        equal(entry.length, 2);
        equal(typeof entry[0], "number");
        Assert.less(entry[0], telSvc.payload.modules.length);
        equal(typeof entry[1], "string");
      } else {
        equal(typeof entry, "string");
      }
    });

    // hang.annotations
    ok(Array.isArray(hang.annotations));
    hang.annotations.forEach(annotation => {
      ok(Array.isArray(annotation));
      equal(annotation.length, 2);
      equal(typeof annotation[0], "string");
      equal(typeof annotation[1], "string");
    });
  });

  do_send_remote_message("bhr_hangs_detected");
  await childDone;

  let pingSubmitted = false;
  GleanPings.hangReport.testBeforeNextSubmit(() => {
    Assert.deepEqual(
      telSvc.payload.modules,
      Glean.hangs.modules.testGetValue()
    );
    let hangs = telSvc.payload.hangs;
    let gleanHangs = Glean.hangs.reports.testGetValue();
    Assert.equal(
      hangs.length,
      gleanHangs.length,
      "the expected hang count has been reported"
    );
    for (let i = 0; i < hangs.length; ++i) {
      let hang = hangs[i];
      let gleanHang = gleanHangs[i];
      Assert.equal(
        Math.round(hang.duration),
        gleanHang.duration,
        "the hang duration is correct"
      );
      Assert.equal(
        Math.round(hang.stack.length),
        gleanHang.stack.length,
        "the reported stack has the expected length"
      );
      for (let j = 0; j < hang.stack.length; ++j) {
        let frame = hang.stack[j];
        let gleanFrame = gleanHang.stack[j];
        if (typeof frame == "string") {
          Assert.deepEqual(
            { frame },
            gleanFrame,
            "label or JS frame is correct"
          );
        } else {
          let module;
          [module, frame] = frame;
          Assert.deepEqual(
            { frame, module },
            gleanFrame,
            "native frame is correct"
          );
        }
      }
      if (hang.annotations.length) {
        Assert.deepEqual(
          hang.annotations,
          gleanHang.annotations,
          "annotations have been copied to glean"
        );
      } else {
        Assert.equal(
          "undefined",
          typeof gleanHang.annotations,
          "no annotation"
        );
      }
      for (let field of ["process", "thread", "runnableName"]) {
        Assert.equal(hang[field], gleanHang[field], `the ${field} is correct`);
      }
      if (hang.remoteType) {
        Assert.equal(
          hang.remoteType,
          gleanHang.remoteType,
          "the remote type is correct"
        );
      } else {
        Assert.equal(
          "undefined",
          typeof gleanHang.remoteType,
          "no remote type"
        );
      }
    }
    pingSubmitted = true;
  });

  Services.prefs.setBoolPref("toolkit.telemetry.bhrPing.enabled", true);
  telSvc.submit();
  Assert.ok(pingSubmitted, "the glean 'hang-report' ping has been submitted");
});
