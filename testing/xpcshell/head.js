/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains common code that is loaded before each test file(s).
 * See http://developer.mozilla.org/en/docs/Writing_xpcshell-based_unit_tests
 * for more information.
 */

var _quit = false;
var _passed = true;
var _tests_pending = 0;
var _passedChecks = 0, _falsePassedChecks = 0;
var _todoChecks = 0;
var _cleanupFunctions = [];
var _pendingTimers = [];
var _profileInitialized = false;

// Register the testing-common resource protocol early, to have access to its
// modules.
_register_modules_protocol_handler();

let _Promise = Components.utils.import("resource://gre/modules/Promise.jsm", this).Promise;

// Support a common assertion library, Assert.jsm.
let AssertCls = Components.utils.import("resource://testing-common/Assert.jsm", null).Assert;
// Pass a custom report function for xpcshell-test style reporting.
let Assert = new AssertCls(function(err, message, stack) {
  if (err) {
    do_report_result(false, err.message, err.stack);
  } else {
    do_report_result(true, message, stack);
  }
});

let _log = function (action, params) {
  if (typeof _XPCSHELL_PROCESS != "undefined") {
    params.process = _XPCSHELL_PROCESS;
  }
  params.action = action;
  params._time = Date.now();
  dump("\n" + JSON.stringify(params) + "\n");
}

function _dump(str) {
  let start = /^TEST-/.test(str) ? "\n" : "";
  if (typeof _XPCSHELL_PROCESS == "undefined") {
    dump(start + str);
  } else {
    dump(start + _XPCSHELL_PROCESS + ": " + str);
  }
}

// Disable automatic network detection, so tests work correctly when
// not connected to a network.
let (ios = Components.classes["@mozilla.org/network/io-service;1"]
           .getService(Components.interfaces.nsIIOService2)) {
  ios.manageOfflineStatus = false;
  ios.offline = false;
}

// Determine if we're running on parent or child
let runningInParent = true;
try {
  runningInParent = Components.classes["@mozilla.org/xre/runtime;1"].
                    getService(Components.interfaces.nsIXULRuntime).processType
                    == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
} 
catch (e) { }

// Only if building of places is enabled.
if (runningInParent &&
    "mozIAsyncHistory" in Components.interfaces) {
  // Ensure places history is enabled for xpcshell-tests as some non-FF
  // apps disable it.
  let (prefs = Components.classes["@mozilla.org/preferences-service;1"]
               .getService(Components.interfaces.nsIPrefBranch)) {
    prefs.setBoolPref("places.history.enabled", true);
  };
}

try {
  if (runningInParent) {
    let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);

    // disable necko IPC security checks for xpcshell, as they lack the
    // docshells needed to pass them
    prefs.setBoolPref("network.disable.ipc.security", true);

    // Disable IPv6 lookups for 'localhost' on windows.
    if ("@mozilla.org/windows-registry-key;1" in Components.classes) {
      prefs.setCharPref("network.dns.ipv4OnlyDomains", "localhost");
    }
  }
}
catch (e) { }

// Configure crash reporting, if possible
// We rely on the Python harness to set MOZ_CRASHREPORTER,
// MOZ_CRASHREPORTER_NO_REPORT, and handle checking for minidumps.
// Note that if we're in a child process, we don't want to init the
// crashreporter component.
try {
  if (runningInParent &&
      "@mozilla.org/toolkit/crash-reporter;1" in Components.classes) {
    let (crashReporter =
          Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
          .getService(Components.interfaces.nsICrashReporter)) {
      crashReporter.UpdateCrashEventsDir();
      crashReporter.minidumpPath = do_get_minidumpdir();
    }
  }
}
catch (e) { }

/**
 * Date.now() is not necessarily monotonically increasing (insert sob story
 * about times not being the right tool to use for measuring intervals of time,
 * robarnold can tell all), so be wary of error by erring by at least
 * _timerFuzz ms.
 */
const _timerFuzz = 15;

function _Timer(func, delay) {
  delay = Number(delay);
  if (delay < 0)
    do_throw("do_timeout() delay must be nonnegative");

  if (typeof func !== "function")
    do_throw("string callbacks no longer accepted; use a function!");

  this._func = func;
  this._start = Date.now();
  this._delay = delay;

  var timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(this, delay + _timerFuzz, timer.TYPE_ONE_SHOT);

  // Keep timer alive until it fires
  _pendingTimers.push(timer);
}
_Timer.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsITimerCallback) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notify: function(timer) {
    _pendingTimers.splice(_pendingTimers.indexOf(timer), 1);

    // The current nsITimer implementation can undershoot, but even if it
    // couldn't, paranoia is probably a virtue here given the potential for
    // random orange on tinderboxen.
    var end = Date.now();
    var elapsed = end - this._start;
    if (elapsed >= this._delay) {
      try {
        this._func.call(null);
      } catch (e) {
        do_throw("exception thrown from do_timeout callback: " + e);
      }
      return;
    }

    // Timer undershot, retry with a little overshoot to try to avoid more
    // undershoots.
    var newDelay = this._delay - elapsed;
    do_timeout(newDelay, this._func);
  }
};

function _do_main() {
  if (_quit)
    return;

  _log("test_info",
       {_message: "TEST-INFO | (xpcshell/head.js) | running event loop\n"});

  var thr = Components.classes["@mozilla.org/thread-manager;1"]
                      .getService().currentThread;

  while (!_quit)
    thr.processNextEvent(true);

  while (thr.hasPendingEvents())
    thr.processNextEvent(true);
}

function _do_quit() {
  _log("test_info",
       {_message: "TEST-INFO | (xpcshell/head.js) | exiting test\n"});
  _Promise.Debugging.flushUncaughtErrors();
  _quit = true;
}

function _format_exception_stack(stack) {
  if (typeof stack == "object" && stack.caller) {
    let frame = stack;
    let strStack = "";
    while (frame != null) {
      strStack += frame + "\n";
      frame = frame.caller;
    }
    stack = strStack;
  }
  // frame is of the form "fname@file:line"
  let frame_regexp = new RegExp("(.*)@(.*):(\\d*)", "g");
  return stack.split("\n").reduce(function(stack_msg, frame) {
    if (frame) {
      let parts = frame_regexp.exec(frame);
      if (parts) {
        let [ _, func, file, line ] = parts;
        return stack_msg + "JS frame :: " + file + " :: " +
          (func || "anonymous") + " :: line " + line + "\n";
      }
      else { /* Could be a -e (command line string) style location. */
        return stack_msg + "JS frame :: " + frame + "\n";
      }
    }
    return stack_msg;
  }, "");
}

/**
 * Overrides idleService with a mock.  Idle is commonly used for maintenance
 * tasks, thus if a test uses a service that requires the idle service, it will
 * start handling them.
 * This behaviour would cause random failures and slowdown tests execution,
 * for example by running database vacuum or cleanups for each test.
 *
 * @note Idle service is overridden by default.  If a test requires it, it will
 *       have to call do_get_idle() function at least once before use.
 */
var _fakeIdleService = {
  get registrar() {
    delete this.registrar;
    return this.registrar =
      Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  },
  contractID: "@mozilla.org/widget/idleservice;1",
  get CID() this.registrar.contractIDToCID(this.contractID),

  activate: function FIS_activate()
  {
    if (!this.originalFactory) {
      // Save original factory.
      this.originalFactory =
        Components.manager.getClassObject(Components.classes[this.contractID],
                                          Components.interfaces.nsIFactory);
      // Unregister original factory.
      this.registrar.unregisterFactory(this.CID, this.originalFactory);
      // Replace with the mock.
      this.registrar.registerFactory(this.CID, "Fake Idle Service",
                                     this.contractID, this.factory
      );
    }
  },

  deactivate: function FIS_deactivate()
  {
    if (this.originalFactory) {
      // Unregister the mock.
      this.registrar.unregisterFactory(this.CID, this.factory);
      // Restore original factory.
      this.registrar.registerFactory(this.CID, "Idle Service",
                                     this.contractID, this.originalFactory);
      delete this.originalFactory;
    }
  },

  factory: {
    // nsIFactory
    createInstance: function (aOuter, aIID)
    {
      if (aOuter) {
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      }
      return _fakeIdleService.QueryInterface(aIID);
    },
    lockFactory: function (aLock) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },
    QueryInterface: function(aIID) {
      if (aIID.equals(Components.interfaces.nsIFactory) ||
          aIID.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  // nsIIdleService
  get idleTime() 0,
  addIdleObserver: function () {},
  removeIdleObserver: function () {},

  QueryInterface: function(aIID) {
    // Useful for testing purposes, see test_get_idle.js.
    if (aIID.equals(Components.interfaces.nsIFactory)) {
      return this.factory;
    }
    if (aIID.equals(Components.interfaces.nsIIdleService) ||
        aIID.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

/**
 * Restores the idle service factory if needed and returns the service's handle.
 * @return A handle to the idle service.
 */
function do_get_idle() {
  _fakeIdleService.deactivate();
  return Components.classes[_fakeIdleService.contractID]
                   .getService(Components.interfaces.nsIIdleService);
}

// Map resource://test/ to current working directory and
// resource://testing-common/ to the shared test modules directory.
function _register_protocol_handlers() {
  let ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  let protocolHandler =
    ios.getProtocolHandler("resource")
       .QueryInterface(Components.interfaces.nsIResProtocolHandler);

  let curDirURI = ios.newFileURI(do_get_cwd());
  protocolHandler.setSubstitution("test", curDirURI);

  _register_modules_protocol_handler();
}

function _register_modules_protocol_handler() {
  if (!this._TESTING_MODULES_DIR) {
    throw new Error("Please define a path where the testing modules can be " +
                    "found in a variable called '_TESTING_MODULES_DIR' before " +
                    "head.js is included.");
  }

  let ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  let protocolHandler =
    ios.getProtocolHandler("resource")
       .QueryInterface(Components.interfaces.nsIResProtocolHandler);

  let modulesFile = Components.classes["@mozilla.org/file/local;1"].
                    createInstance(Components.interfaces.nsILocalFile);
  modulesFile.initWithPath(_TESTING_MODULES_DIR);

  if (!modulesFile.exists()) {
    throw new Error("Specified modules directory does not exist: " +
                    _TESTING_MODULES_DIR);
  }

  if (!modulesFile.isDirectory()) {
    throw new Error("Specified modules directory is not a directory: " +
                    _TESTING_MODULES_DIR);
  }

  let modulesURI = ios.newFileURI(modulesFile);

  protocolHandler.setSubstitution("testing-common", modulesURI);
}

function _execute_test() {
  _register_protocol_handlers();

  // Override idle service by default.
  // Call do_get_idle() to restore the factory and get the service.
  _fakeIdleService.activate();

  _Promise.Debugging.clearUncaughtErrorObservers();
  _Promise.Debugging.addUncaughtErrorObserver(function observer({message, date, fileName, stack, lineNumber}) {
    let text = " A promise chain failed to handle a rejection: " +
        message + " - rejection date: " + date;
    _log_message_with_stack("test_unexpected_fail",
                            text, stack, fileName);
  });

  // _HEAD_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_HEAD_FILES);
  // _TEST_FILE is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TEST_FILE);

  // Tack Assert.jsm methods to the current scope.
  this.Assert = Assert;
  for (let func in Assert) {
    this[func] = Assert[func].bind(Assert);
  }

  try {
    do_test_pending("MAIN run_test");
    run_test();
    do_test_finished("MAIN run_test");
    _do_main();
  } catch (e) {
    _passed = false;
    // do_check failures are already logged and set _quit to true and throw
    // NS_ERROR_ABORT. If both of those are true it is likely this exception
    // has already been logged so there is no need to log it again. It's
    // possible that this will mask an NS_ERROR_ABORT that happens after a
    // do_check failure though.
    if (!_quit || e != Components.results.NS_ERROR_ABORT) {
      let msgObject = {};
      if (e.fileName) {
        msgObject.source_file = e.fileName;
        if (e.lineNumber) {
          msgObject.line_number = e.lineNumber;
        }
      } else {
        msgObject.source_file = "xpcshell/head.js";
      }
      msgObject.diagnostic = _exception_message(e);
      if (e.stack) {
        msgObject.diagnostic += " - See following stack:\n";
        msgObject.stack = _format_exception_stack(e.stack);
      }
      _log("test_unexpected_fail", msgObject);
    }
  }

  // _TAIL_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TAIL_FILES);

  // Execute all of our cleanup functions.
  let reportCleanupError = function(ex) {
    let stack, filename;
    if (ex && typeof ex == "object" && "stack" in ex) {
      stack = ex.stack;
    } else {
      stack = Components.stack.caller;
    }
    if (stack instanceof Components.interfaces.nsIStackFrame) {
      filename = stack.filename;
    } else if (ex.fileName) {
      filename = ex.fileName;
    }
    _log_message_with_stack("test_unexpected_fail",
                            ex, stack, filename);
  };

  let func;
  while ((func = _cleanupFunctions.pop())) {
    let result;
    try {
      result = func();
    } catch (ex) {
      reportCleanupError(ex);
      continue;
    }
    if (result && typeof result == "object"
        && "then" in result && typeof result.then == "function") {
      // This is a promise, wait until it is satisfied before proceeding
      let complete = false;
      let promise = result.then(null, reportCleanupError);
      promise = promise.then(() => complete = true);
      let thr = Components.classes["@mozilla.org/thread-manager;1"]
                  .getService().currentThread;
      while (!complete) {
        thr.processNextEvent(true);
      }
    }
  }

  // Restore idle service to avoid leaks.
  _fakeIdleService.deactivate();

  if (!_passed)
    return;

  var truePassedChecks = _passedChecks - _falsePassedChecks;
  if (truePassedChecks > 0) {
    _log("test_pass",
         {_message: "TEST-PASS | (xpcshell/head.js) | " + truePassedChecks + " (+ " +
                    _falsePassedChecks + ") check(s) passed\n",
          source_file: _TEST_FILE,
          passed_checks: truePassedChecks});
    _log("test_info",
         {_message: "TEST-INFO | (xpcshell/head.js) | " + _todoChecks +
                    " check(s) todo\n",
          source_file: _TEST_FILE,
          todo_checks: _todoChecks});
  } else {
    // ToDo: switch to TEST-UNEXPECTED-FAIL when all tests have been updated. (Bug 496443)
    _log("test_info",
         {_message: "TEST-INFO | (xpcshell/head.js) | No (+ " + _falsePassedChecks +
                    ") checks actually run\n",
         source_file: _TEST_FILE});
  }
}

/**
 * Loads files.
 *
 * @param aFiles Array of files to load.
 */
function _load_files(aFiles) {
  function loadTailFile(element, index, array) {
    try {
      load(element);
    } catch (e if e instanceof SyntaxError) {
      _log("javascript_error",
           {_message: "TEST-UNEXPECTED-FAIL | (xpcshell/head.js) | Source file " + element + " contains SyntaxError",
            diagnostic: _exception_message(e),
            source_file: element,
            stack: _format_exception_stack(e.stack)});
    } catch (e) {
      _log("javascript_error",
           {_message: "TEST-UNEXPECTED-FAIL | (xpcshell/head.js) | Source file " + element + " contains an error",
            diagnostic: _exception_message(e),
            source_file: element,
            stack: e.stack ? _format_exception_stack(e.stack) : null});
    }
  }

  aFiles.forEach(loadTailFile);
}

function _wrap_with_quotes_if_necessary(val) {
  return typeof val == "string" ? '"' + val + '"' : val;
}

/************** Functions to be used from the tests **************/

/**
 * Prints a message to the output log.
 */
function do_print(msg) {
  var caller_stack = Components.stack.caller;
  msg = _wrap_with_quotes_if_necessary(msg);
  _log("test_info",
       {source_file: caller_stack.filename,
        diagnostic: msg});

}

/**
 * Calls the given function at least the specified number of milliseconds later.
 * The callback will not undershoot the given time, but it might overshoot --
 * don't expect precision!
 *
 * @param delay : uint
 *   the number of milliseconds to delay
 * @param callback : function() : void
 *   the function to call
 */
function do_timeout(delay, func) {
  new _Timer(func, Number(delay));
}

function do_execute_soon(callback, aName) {
  let funcName = (aName ? aName : callback.name);
  do_test_pending(funcName);
  var tm = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService(Components.interfaces.nsIThreadManager);

  tm.mainThread.dispatch({
    run: function() {
      try {
        callback();
      } catch (e) {
        // do_check failures are already logged and set _quit to true and throw
        // NS_ERROR_ABORT. If both of those are true it is likely this exception
        // has already been logged so there is no need to log it again. It's
        // possible that this will mask an NS_ERROR_ABORT that happens after a
        // do_check failure though.
        if (!_quit || e != Components.results.NS_ERROR_ABORT) {
          if (e.stack) {
            _log("javascript_error",
                 {source_file: "xpcshell/head.js",
                  diagnostic: _exception_message(e) + " - See following stack:",
                  stack: _format_exception_stack(e.stack)});
          } else {
            _log("javascript_error",
                 {source_file: "xpcshell/head.js",
                  diagnostic: _exception_message(e)});
          }
          _do_quit();
        }
      }
      finally {
        do_test_finished(funcName);
      }
    }
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
}

/**
 * Shows an error message and the current stack and aborts the test.
 *
 * @param error  A message string or an Error object.
 * @param stack  null or nsIStackFrame object or a string containing
 *               \n separated stack lines (as in Error().stack).
 */
function do_throw(error, stack) {
  let filename = "";
  // If we didn't get passed a stack, maybe the error has one
  // otherwise get it from our call context
  stack = stack || error.stack || Components.stack.caller;

  if (stack instanceof Components.interfaces.nsIStackFrame)
    filename = stack.filename;
  else if (error.fileName)
    filename = error.fileName;

  _log_message_with_stack("test_unexpected_fail",
                          error, stack, filename);

  _passed = false;
  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function _format_stack(stack) {
  if (stack instanceof Components.interfaces.nsIStackFrame) {
    let stack_msg = "";
    let frame = stack;
    while (frame != null) {
      stack_msg += frame + "\n";
      frame = frame.caller;
    }
    return stack_msg;
  }
  return "" + stack;
}

function do_throw_todo(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _passed = false;
  _log_message_with_stack("test_unexpected_pass",
                          text, stack, stack.filename);
  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

// Make a nice display string from an object that behaves
// like Error
function _exception_message(ex) {
  let message = "";
  if (ex.name) {
    message = ex.name + ": ";
  }
  if (ex.message) {
    message += ex.message;
  }
  if (ex.fileName) {
    message += (" at " + ex.fileName);
    if (ex.lineNumber) {
      message += (":" + ex.lineNumber);
    }
  }
  if (message !== "") {
    return message;
  }
  // Force ex to be stringified
  return "" + ex;
}

function _log_message_with_stack(action, ex, stack, filename, text) {
  if (stack) {
    _log(action,
         {diagnostic: (text ? text : "") +
                      _exception_message(ex) +
                      " - See following stack:",
          source_file: filename,
          stack: _format_stack(stack)});
  } else {
    _log(action,
         {diagnostic: (text ? text : "") +
                      _exception_message(ex),
          source_file: filename});
  }
}

function do_report_unexpected_exception(ex, text) {
  var caller_stack = Components.stack.caller;
  text = text ? text + " - " : "";

  _passed = false;
  _log_message_with_stack("test_unexpected_fail", ex, ex.stack || "",
                          caller_stack.filename, text + "Unexpected exception ");
  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function do_note_exception(ex, text) {
  var caller_stack = Components.stack.caller;
  text = text ? text + " - " : "";

  _log_message_with_stack("test_info", ex, ex.stack,
                          caller_stack.filename, text + "Swallowed exception ");
}

function _do_check_neq(left, right, stack, todo) {
  Assert.notEqual(left, right);
}

function do_check_neq(left, right, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _do_check_neq(left, right, stack, false);
}

function todo_check_neq(left, right, stack) {
  if (!stack)
      stack = Components.stack.caller;

  _do_check_neq(left, right, stack, true);
}

function do_report_result(passed, text, stack, todo) {
  if (passed) {
    if (todo) {
      do_throw_todo(text, stack);
    } else {
      ++_passedChecks;
      _log("test_pass",
           {source_file: stack.filename,
            test_name: stack.name,
            line_number: stack.lineNumber,
            diagnostic: "[" + stack.name + " : " + stack.lineNumber + "] " +
                        text + "\n"});
    }
  } else {
    if (todo) {
      ++_todoChecks;
      _log("test_known_fail",
           {source_file: stack.filename,
            test_name: stack.name,
            line_number: stack.lineNumber,
            diagnostic: "[" + stack.name + " : " + stack.lineNumber + "] " +
                        text + "\n"});
    } else {
      do_throw(text, stack);
    }
  }
}

function _do_check_eq(left, right, stack, todo) {
  if (!stack)
    stack = Components.stack.caller;

  var text = _wrap_with_quotes_if_necessary(left) + " == " +
             _wrap_with_quotes_if_necessary(right);
  do_report_result(left == right, text, stack, todo);
}

function do_check_eq(left, right, stack) {
  Assert.equal(left, right);
}

function todo_check_eq(left, right, stack) {
  if (!stack)
      stack = Components.stack.caller;

  _do_check_eq(left, right, stack, true);
}

function do_check_true(condition, stack) {
  Assert.ok(condition);
}

function todo_check_true(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  todo_check_eq(condition, true, stack);
}

function do_check_false(condition, stack) {
  Assert.ok(!condition, stack);
}

function todo_check_false(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  todo_check_eq(condition, false, stack);
}

function do_check_null(condition, stack) {
  Assert.equal(condition, null);
}

function todo_check_null(condition, stack=Components.stack.caller) {
  todo_check_eq(condition, null, stack);
}
function do_check_matches(pattern, value) {
  Assert.deepEqual(pattern, value);
}

// Check that |func| throws an nsIException that has
// |Components.results[resultName]| as the value of its 'result' property.
function do_check_throws_nsIException(func, resultName,
                                      stack=Components.stack.caller, todo=false)
{
  let expected = Components.results[resultName];
  if (typeof expected !== 'number') {
    do_throw("do_check_throws_nsIException requires a Components.results" +
             " property name, not " + uneval(resultName), stack);
  }

  let msg = ("do_check_throws_nsIException: func should throw" +
             " an nsIException whose 'result' is Components.results." +
             resultName);

  try {
    func();
  } catch (ex) {
    if (!(ex instanceof Components.interfaces.nsIException) ||
        ex.result !== expected) {
      do_report_result(false, msg + ", threw " + legible_exception(ex) +
                       " instead", stack, todo);
    }

    do_report_result(true, msg, stack, todo);
    return;
  }

  // Call this here, not in the 'try' clause, so do_report_result's own
  // throw doesn't get caught by our 'catch' clause.
  do_report_result(false, msg + ", but returned normally", stack, todo);
}

// Produce a human-readable form of |exception|. This looks up
// Components.results values, tries toString methods, and so on.
function legible_exception(exception)
{
  switch (typeof exception) {
    case 'object':
    if (exception instanceof Components.interfaces.nsIException) {
      return "nsIException instance: " + uneval(exception.toString());
    }
    return exception.toString();

    case 'number':
    for (let name in Components.results) {
      if (exception === Components.results[name]) {
        return "Components.results." + name;
      }
    }

    // Fall through.
    default:
    return uneval(exception);
  }
}

function do_check_instanceof(value, constructor,
                             stack=Components.stack.caller, todo=false) {
  do_report_result(value instanceof constructor,
                   "value should be an instance of " + constructor.name,
                   stack, todo);
}

function todo_check_instanceof(value, constructor,
                             stack=Components.stack.caller) {
  do_check_instanceof(value, constructor, stack, true);
}

function do_test_pending(aName) {
  ++_tests_pending;

  _log("test_pending",
       {_message: "TEST-INFO | (xpcshell/head.js) | test" +
                  (aName ? " " + aName : "") +
                  " pending (" + _tests_pending + ")\n"});
}

function do_test_finished(aName) {
  _log("test_finish",
       {_message: "TEST-INFO | (xpcshell/head.js) | test" +
                  (aName ? " " + aName : "") +
                  " finished (" + _tests_pending + ")\n"});
  if (--_tests_pending == 0)
    _do_quit();
}

function do_get_file(path, allowNonexistent) {
  try {
    let lf = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);

    let bits = path.split("/");
    for (let i = 0; i < bits.length; i++) {
      if (bits[i]) {
        if (bits[i] == "..")
          lf = lf.parent;
        else
          lf.append(bits[i]);
      }
    }

    if (!allowNonexistent && !lf.exists()) {
      // Not using do_throw(): caller will continue.
      _passed = false;
      var stack = Components.stack.caller;
      _log("test_unexpected_fail",
           {source_file: stack.filename,
            test_name: stack.name,
            line_number: stack.lineNumber,
            diagnostic: "[" + stack.name + " : " + stack.lineNumber + "] " +
                        lf.path + " does not exist\n"});
    }

    return lf;
  }
  catch (ex) {
    do_throw(ex.toString(), Components.stack.caller);
  }

  return null;
}

// do_get_cwd() isn't exactly self-explanatory, so provide a helper
function do_get_cwd() {
  return do_get_file("");
}

function do_load_manifest(path) {
  var lf = do_get_file(path);
  const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
  do_check_true(Components.manager instanceof nsIComponentRegistrar);
  // Previous do_check_true() is not a test check.
  ++_falsePassedChecks;
  Components.manager.autoRegister(lf);
}

/**
 * Parse a DOM document.
 *
 * @param aPath File path to the document.
 * @param aType Content type to use in DOMParser.
 *
 * @return nsIDOMDocument from the file.
 */
function do_parse_document(aPath, aType) {
  switch (aType) {
    case "application/xhtml+xml":
    case "application/xml":
    case "text/xml":
      break;

    default:
      do_throw("type: expected application/xhtml+xml, application/xml or text/xml," +
                 " got '" + aType + "'",
               Components.stack.caller);
  }

  var lf = do_get_file(aPath);
  const C_i = Components.interfaces;
  const parserClass = "@mozilla.org/xmlextras/domparser;1";
  const streamClass = "@mozilla.org/network/file-input-stream;1";
  var stream = Components.classes[streamClass]
                         .createInstance(C_i.nsIFileInputStream);
  stream.init(lf, -1, -1, C_i.nsIFileInputStream.CLOSE_ON_EOF);
  var parser = Components.classes[parserClass]
                         .createInstance(C_i.nsIDOMParser);
  var doc = parser.parseFromStream(stream, null, lf.fileSize, aType);
  parser = null;
  stream = null;
  lf = null;
  return doc;
}

/**
 * Registers a function that will run when the test harness is done running all
 * tests.
 *
 * @param aFunction
 *        The function to be called when the test harness has finished running.
 */
function do_register_cleanup(aFunction)
{
  _cleanupFunctions.push(aFunction);
}

/**
 * Returns the directory for a temp dir, which is created by the
 * test harness. Every test gets its own temp dir.
 *
 * @return nsILocalFile of the temporary directory
 */
function do_get_tempdir() {
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  // the python harness sets this in the environment for us
  let path = env.get("XPCSHELL_TEST_TEMP_DIR");
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(path);
  return file;
}

/**
 * Returns the directory for crashreporter minidumps.
 *
 * @return nsILocalFile of the minidump directory
 */
function do_get_minidumpdir() {
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  // the python harness may set this in the environment for us
  let path = env.get("XPCSHELL_MINIDUMP_DIR");
  if (path) {
    let file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(path);
    return file;
  } else {
    return do_get_tempdir();
  }
}

/**
 * Registers a directory with the profile service,
 * and return the directory as an nsILocalFile.
 *
 * @return nsILocalFile of the profile directory.
 */
function do_get_profile() {
  if (!runningInParent) {
    _log("test_info",
         {_message: "TEST-INFO | (xpcshell/head.js) | Ignoring profile creation from child process.\n"});

    return null;
  }

  if (!_profileInitialized) {
    // Since we have a profile, we will notify profile shutdown topics at
    // the end of the current test, to ensure correct cleanup on shutdown.
    do_register_cleanup(function() {
      let obsSvc = Components.classes["@mozilla.org/observer-service;1"].
                   getService(Components.interfaces.nsIObserverService);
      obsSvc.notifyObservers(null, "profile-change-net-teardown", null);
      obsSvc.notifyObservers(null, "profile-change-teardown", null);
      obsSvc.notifyObservers(null, "profile-before-change", null);
    });
  }

  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  // the python harness sets this in the environment for us
  let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(profd);

  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfD" || prop == "ProfLD" || prop == "ProfDS" ||
          prop == "ProfLDS" || prop == "TmpD") {
        return file.clone();
      }
      throw Components.results.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsIDirectoryServiceProvider) ||
          iid.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Components.interfaces.nsIDirectoryService)
        .registerProvider(provider);

  let obsSvc = Components.classes["@mozilla.org/observer-service;1"].
        getService(Components.interfaces.nsIObserverService);

  // We need to update the crash events directory when the profile changes.
  if (runningInParent &&
      "@mozilla.org/toolkit/crash-reporter;1" in Components.classes) {
    let crashReporter =
        Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
                          .getService(Components.interfaces.nsICrashReporter);
    crashReporter.UpdateCrashEventsDir();
  }

  if (!_profileInitialized) {
    obsSvc.notifyObservers(null, "profile-do-change", "xpcshell-do-get-profile");
    _profileInitialized = true;
  }

  // The methods of 'provider' will retain this scope so null out everything
  // to avoid spurious leak reports.
  env = null;
  profd = null;
  dirSvc = null;
  provider = null;
  obsSvc = null;
  return file.clone();
}

/**
 * This function loads head.js (this file) in the child process, so that all
 * functions defined in this file (do_throw, etc) are available to subsequent
 * sendCommand calls.  It also sets various constants used by these functions.
 *
 * (Note that you may use sendCommand without calling this function first;  you
 * simply won't have any of the functions in this file available.)
 */
function do_load_child_test_harness()
{
  // Make sure this isn't called from child process
  if (!runningInParent) {
    do_throw("run_test_in_child cannot be called from child!");
  }

  // Allow to be called multiple times, but only run once
  if (typeof do_load_child_test_harness.alreadyRun != "undefined")
    return;
  do_load_child_test_harness.alreadyRun = 1;

  _XPCSHELL_PROCESS = "parent";

  let command =
        "const _HEAD_JS_PATH=" + uneval(_HEAD_JS_PATH) + "; "
      + "const _HTTPD_JS_PATH=" + uneval(_HTTPD_JS_PATH) + "; "
      + "const _HEAD_FILES=" + uneval(_HEAD_FILES) + "; "
      + "const _TAIL_FILES=" + uneval(_TAIL_FILES) + "; "
      + "const _XPCSHELL_PROCESS='child';";

  if (this._TESTING_MODULES_DIR) {
    command += " const _TESTING_MODULES_DIR=" + uneval(_TESTING_MODULES_DIR) + ";";
  }

  command += " load(_HEAD_JS_PATH);";
  sendCommand(command);
}

/**
 * Runs an entire xpcshell unit test in a child process (rather than in chrome,
 * which is the default).
 *
 * This function returns immediately, before the test has completed.  
 *
 * @param testFile
 *        The name of the script to run.  Path format same as load().
 * @param optionalCallback.
 *        Optional function to be called (in parent) when test on child is
 *        complete.  If provided, the function must call do_test_finished();
 */
function run_test_in_child(testFile, optionalCallback) 
{
  var callback = (typeof optionalCallback == 'undefined') ? 
                    do_test_finished : optionalCallback;

  do_load_child_test_harness();

  var testPath = do_get_file(testFile).path.replace(/\\/g, "/");
  do_test_pending("run in child");
  sendCommand("_log('child_test_start', {_message: 'CHILD-TEST-STARTED'}); "
              + "const _TEST_FILE=['" + testPath + "']; _execute_test(); "
              + "_log('child_test_end', {_message: 'CHILD-TEST-COMPLETED'});",
              callback);
}

/**
 * Execute a given function as soon as a particular cross-process message is received.
 * Must be paired with do_send_remote_message or equivalent ProcessMessageManager calls.
 */
function do_await_remote_message(name, callback)
{
  var listener = {
    receiveMessage: function(message) {
      if (message.name == name) {
        mm.removeMessageListener(name, listener);
        callback();
        do_test_finished();
      }
    }
  };

  var mm;
  if (runningInParent) {
    mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIMessageBroadcaster);
  } else {
    mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);
  }
  do_test_pending();
  mm.addMessageListener(name, listener);
}

/**
 * Asynchronously send a message to all remote processes. Pairs with do_await_remote_message
 * or equivalent ProcessMessageManager listeners.
 */
function do_send_remote_message(name) {
  var mm;
  var sender;
  if (runningInParent) {
    mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIMessageBroadcaster);
    sender = 'broadcastAsyncMessage';
  } else {
    mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);
    sender = 'sendAsyncMessage';
  }
  mm[sender](name);
}

/**
 * Add a test function to the list of tests that are to be run asynchronously.
 *
 * Each test function must call run_next_test() when it's done. Test files
 * should call run_next_test() in their run_test function to execute all
 * async tests.
 *
 * @return the test function that was passed in.
 */
let _gTests = [];
function add_test(func) {
  _gTests.push([false, func]);
  return func;
}

/**
 * Add a test function which is a Task function.
 *
 * Task functions are functions fed into Task.jsm's Task.spawn(). They are
 * generators that emit promises.
 *
 * If an exception is thrown, a do_check_* comparison fails, or if a rejected
 * promise is yielded, the test function aborts immediately and the test is
 * reported as a failure.
 *
 * Unlike add_test(), there is no need to call run_next_test(). The next test
 * will run automatically as soon the task function is exhausted. To trigger
 * premature (but successful) termination of the function, simply return or
 * throw a Task.Result instance.
 *
 * Example usage:
 *
 * add_task(function test() {
 *   let result = yield Promise.resolve(true);
 *
 *   do_check_true(result);
 *
 *   let secondary = yield someFunctionThatReturnsAPromise(result);
 *   do_check_eq(secondary, "expected value");
 * });
 *
 * add_task(function test_early_return() {
 *   let result = yield somethingThatReturnsAPromise();
 *
 *   if (!result) {
 *     // Test is ended immediately, with success.
 *     return;
 *   }
 *
 *   do_check_eq(result, "foo");
 * });
 */
function add_task(func) {
  _gTests.push([true, func]);
}
let _Task = Components.utils.import("resource://gre/modules/Task.jsm", {}).Task;
_Task.Debugging.maintainStack = true;


/**
 * Runs the next test function from the list of async tests.
 */
let _gRunningTest = null;
let _gTestIndex = 0; // The index of the currently running test.
let _gTaskRunning = false;
function run_next_test()
{
  if (_gTaskRunning) {
    throw new Error("run_next_test() called from an add_task() test function. " +
                    "run_next_test() should not be called from inside add_task() " +
                    "under any circumstances!");
  }
 
  function _run_next_test()
  {
    if (_gTestIndex < _gTests.length) {
      // Flush uncaught errors as early and often as possible.
      _Promise.Debugging.flushUncaughtErrors();
      let _isTask;
      [_isTask, _gRunningTest] = _gTests[_gTestIndex++];
      print("TEST-INFO | " + _TEST_FILE + " | Starting " + _gRunningTest.name);
      do_test_pending(_gRunningTest.name);

      if (_isTask) {
        _gTaskRunning = true;
        _Task.spawn(_gRunningTest).then(
          () => { _gTaskRunning = false; run_next_test(); },
          (ex) => { _gTaskRunning = false; do_report_unexpected_exception(ex); }
        );
      } else {
        // Exceptions do not kill asynchronous tests, so they'll time out.
        try {
          _gRunningTest();
        } catch (e) {
          do_throw(e);
        }
      }
    }
  }

  // For sane stacks during failures, we execute this code soon, but not now.
  // We do this now, before we call do_test_finished(), to ensure the pending
  // counter (_tests_pending) never reaches 0 while we still have tests to run
  // (do_execute_soon bumps that counter).
  do_execute_soon(_run_next_test, "run_next_test " + _gTestIndex);

  if (_gRunningTest !== null) {
    // Close the previous test do_test_pending call.
    do_test_finished(_gRunningTest.name);
  }
}

try {
  if (runningInParent) {
    // Always use network provider for geolocation tests
    // so we bypass the OSX dialog raised by the corelocation provider
    let prefs = Components.classes["@mozilla.org/preferences-service;1"]
      .getService(Components.interfaces.nsIPrefBranch);

    prefs.setBoolPref("geo.provider.testing", true);
  }
} catch (e) { }
