Components.utils.import("resource://gre/modules/osfile.jsm");

function getEventDir() {
  return OS.Path.join(do_get_tempdir().path, "crash-events");
}

/*
 * Run an xpcshell subprocess and crash it.
 *
 * @param setup
 *        A string of JavaScript code to execute in the subprocess
 *        before crashing. If this is a function and not a string,
 *        it will have .toSource() called on it, and turned into
 *        a call to itself. (for programmer convenience)
 *        This code will be evaluted between crasher_subprocess_head.js
 *        and crasher_subprocess_tail.js, so it will have access
 *        to everything defined in crasher_subprocess_head.js,
 *        which includes "crashReporter", a variable holding
 *        the crash reporter service.
 *
 * @param callback
 *        A JavaScript function to be called after the subprocess
 *        crashes. It will be passed (minidump, extra), where
 *         minidump is an nsILocalFile of the minidump file produced,
 *         and extra is an object containing the key,value pairs from
 *         the .extra file.
 *
 * @param canReturnZero
 *       If true, the subprocess may return with a zero exit code.
 *       Certain types of crashes may not cause the process to
 *       exit with an error.
 */
function do_crash(setup, callback, canReturnZero)
{
  // get current process filename (xpcshell)
  let ds = Components.classes["@mozilla.org/file/directory_service;1"]
    .getService(Components.interfaces.nsIProperties);
  let bin = ds.get("XREExeF", Components.interfaces.nsILocalFile);
  if (!bin.exists()) {
    // weird, can't find xpcshell binary?
    do_throw("Can't find xpcshell binary!");
  }
  // get Gre dir (GreD)
  let greD = ds.get("GreD", Components.interfaces.nsILocalFile);
  let headfile = do_get_file("crasher_subprocess_head.js");
  let tailfile = do_get_file("crasher_subprocess_tail.js");
  // run xpcshell -g GreD -f head -e "some setup code" -f tail
  let process = Components.classes["@mozilla.org/process/util;1"]
                  .createInstance(Components.interfaces.nsIProcess);
  process.init(bin);
  let args = ['-g', greD.path,
              '-f', headfile.path];
  if (setup) {
    if (typeof(setup) == "function")
      // funky, but convenient
      setup = "("+setup.toSource()+")();";
    args.push('-e', setup);
  }
  args.push('-f', tailfile.path);

  let env = Components.classes["@mozilla.org/process/environment;1"]
                              .getService(Components.interfaces.nsIEnvironment);

  let crashD = do_get_tempdir();
  crashD.append("crash-events");
  if (!crashD.exists()) {
    crashD.create(crashD.DIRECTORY_TYPE, 0o700);
  }

  env.set("CRASHES_EVENTS_DIR", crashD.path);

  try {
      process.run(true, args, args.length);
  }
  catch (ex) {} // on Windows we exit with a -1 status when crashing.
  finally {
    env.set("CRASHES_EVENTS_DIR", "");
  }

  if (!canReturnZero) {
    // should exit with an error (should have crashed)
    do_check_neq(process.exitValue, 0);
  }

  handleMinidump(callback);
}

function handleMinidump(callback)
{
  // find minidump
  let minidump = null;
  let en = do_get_tempdir().directoryEntries;
  while (en.hasMoreElements()) {
    let f = en.getNext().QueryInterface(Components.interfaces.nsILocalFile);
    if (f.leafName.substr(-4) == ".dmp") {
      minidump = f;
      break;
    }
  }

  if (minidump == null)
    do_throw("No minidump found!");

  let extrafile = minidump.clone();
  extrafile.leafName = extrafile.leafName.slice(0, -4) + ".extra";

  let memoryfile = minidump.clone();
  memoryfile.leafName = memoryfile.leafName.slice(0, -4) + ".memory.json.gz";

  // Just in case, don't let these files linger.
  do_register_cleanup(function() {
          if (minidump.exists())
              minidump.remove(false);
          if (extrafile.exists())
              extrafile.remove(false);
          if (memoryfile.exists())
              memoryfile.remove(false);
      });
  do_check_true(extrafile.exists());
  let extra = parseKeyValuePairsFromFile(extrafile);

  if (callback)
    callback(minidump, extra);

  if (minidump.exists())
    minidump.remove(false);
  if (extrafile.exists())
    extrafile.remove(false);
  if (memoryfile.exists())
    memoryfile.remove(false);
}

function do_content_crash(setup, callback)
{
  do_load_child_test_harness();
  do_test_pending();

  // Setting the minidump path won't work in the child, so we need to do
  // that here.
  let crashReporter =
      Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
      .getService(Components.interfaces.nsICrashReporter);
  crashReporter.minidumpPath = do_get_tempdir();

  let headfile = do_get_file("../unit/crasher_subprocess_head.js");
  let tailfile = do_get_file("../unit/crasher_subprocess_tail.js");
  if (setup) {
    if (typeof(setup) == "function")
      // funky, but convenient
      setup = "("+setup.toSource()+")();";
  }

  let handleCrash = function() {
    try {
      handleMinidump(callback);
    } catch (x) {
      do_report_unexpected_exception(x);
    }
    do_test_finished();
  };

  sendCommand("load(\"" + headfile.path.replace(/\\/g, "/") + "\");", () =>
    sendCommand(setup, () =>
      sendCommand("load(\"" + tailfile.path.replace(/\\/g, "/") + "\");", () =>
        do_execute_soon(handleCrash)
      )
    )
  );
}

// Import binary APIs via js-ctypes.
Components.utils.import("resource://test/CrashTestUtils.jsm");
Components.utils.import("resource://gre/modules/KeyValueParser.jsm");
