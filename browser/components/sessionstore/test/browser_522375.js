function test() {
  var startup_info = Services.startup.getStartupInfo();
  // No .process info on mac

  // Check if we encountered a telemetry error for the the process creation
  // timestamp and turn the first test into a known failure.
  var snapshot = Services.telemetry.getHistogramById("STARTUP_MEASUREMENT_ERRORS")
                                   .snapshot();

  if (snapshot.values[0] == 0)
    ok(startup_info.process <= startup_info.main, "process created before main is run " + uneval(startup_info));
  else
    todo(false, "An error occurred while recording the process creation timestamp, skipping this test");

  // on linux firstPaint can happen after everything is loaded (especially with remote X)
  if (startup_info.firstPaint)
    ok(startup_info.main <= startup_info.firstPaint, "main ran before first paint " + uneval(startup_info));

  ok(startup_info.main < startup_info.sessionRestored, "Session restored after main " + uneval(startup_info));
}
