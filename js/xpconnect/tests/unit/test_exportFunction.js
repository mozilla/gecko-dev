function run_test() {
  var Cu = Components.utils;
  var epsb = new Cu.Sandbox(["http://example.com", "http://example.org"], { wantExportHelpers: true });
  var subsb = new Cu.Sandbox("http://example.com", { wantGlobalProperties: ["XMLHttpRequest"] });
  var subsb2 = new Cu.Sandbox("http://example.com", { wantGlobalProperties: ["XMLHttpRequest"] });
  var xorigsb = new Cu.Sandbox("http://test.com", { wantGlobalProperties: ["XMLHttpRequest"] });

  epsb.subsb = subsb;
  epsb.xorigsb = xorigsb;
  epsb.do_check_true = do_check_true;
  epsb.do_check_eq = do_check_eq;
  subsb.do_check_true = do_check_true;

  // Exporting should work if prinicipal of the source sandbox
  // subsumes the principal of the target sandbox.
  Cu.evalInSandbox("(" + function() {
    var wasCalled = false;
    this.funToExport = function(expectedThis, a, obj, native, mixed, callback) {
      do_check_eq(a, 42);
      do_check_eq(obj, subsb.tobecloned);
      do_check_eq(obj.cloned, "cloned");
      do_check_eq(native, subsb.native);
      do_check_eq(expectedThis, this);
      do_check_eq(mixed.xrayed, subsb.xrayed);
      do_check_eq(mixed.xrayed2, subsb.xrayed2);
      if (typeof callback == 'function') {
        do_check_eq(typeof subsb.callback, 'function');
        do_check_eq(callback, subsb.callback);
        callback();
      }
      wasCalled = true;
    };
    this.checkIfCalled = function() {
      do_check_true(wasCalled);
      wasCalled = false;
    }
    exportFunction(funToExport, subsb, { defineAs: "imported", allowCallbacks: true });
    exportFunction((x) => x, subsb, { defineAs: "echoAllowXO", allowCallbacks: true, allowCrossOriginArguments: true });
  }.toSource() + ")()", epsb);

  subsb.xrayed = Cu.evalInSandbox("(" + function () {
      return new XMLHttpRequest();
  }.toSource() + ")()", subsb2);

  // Exported function should be able to be call from the
  // target sandbox. Native arguments should be just wrapped
  // every other argument should be cloned.
  Cu.evalInSandbox("(" + function () {
    native = new XMLHttpRequest();
    xrayed2 = XPCNativeWrapper(new XMLHttpRequest());
    mixed = { xrayed: xrayed, xrayed2: xrayed2 };
    tobecloned = { cloned: "cloned" };
    invokedCallback = false;
    callback = function() { invokedCallback = true; };
    imported(this, 42, tobecloned, native, mixed, callback);
    do_check_true(invokedCallback);
  }.toSource() + ")()", subsb);

  // Invoking an exported function with cross-origin arguments should throw.
  subsb.xoNative = Cu.evalInSandbox('new XMLHttpRequest()', xorigsb);
  try {
    Cu.evalInSandbox('imported(this, xoNative)', subsb);
    do_check_true(false);
  } catch (e) {
    do_check_true(/denied|insecure/.test(e));
  }

  // Callers can opt-out of the above.
  subsb.xoNative = Cu.evalInSandbox('new XMLHttpRequest()', xorigsb);
  try {
    do_check_eq(Cu.evalInSandbox('echoAllowXO(xoNative)', subsb), subsb.xoNative);
    do_check_true(true);
  } catch (e) {
    do_check_true(false);
  }

  // Apply should work and |this| should carry over appropriately.
  Cu.evalInSandbox("(" + function() {
    var someThis = {};
    imported.apply(someThis, [someThis, 42, tobecloned, native, mixed]);
  }.toSource() + ")()", subsb);

  Cu.evalInSandbox("(" + function() {
    checkIfCalled();
  }.toSource() + ")()", epsb);

  // Exporting should throw if principal of the source sandbox does
  // not subsume the principal of the target.
  Cu.evalInSandbox("(" + function() {
    try{
      exportFunction(function() {}, this.xorigsb, { defineAs: "denied" });
      do_check_true(false);
    } catch (e) {
      do_check_true(e.toString().indexOf('Permission denied') > -1);
    }
  }.toSource() + ")()", epsb);

  // Exporting should throw if the principal of the source sandbox does
  // not subsume the principal of the function.
  epsb.xo_function = new xorigsb.Function();
  Cu.evalInSandbox("(" + function() {
    try{
      exportFunction(xo_function, this.subsb, { defineAs: "denied" });
      do_check_true(false);
    } catch (e) {
      dump('Exception: ' + e);
      do_check_true(e.toString().indexOf('Permission denied') > -1);
    }
  }.toSource() + ")()", epsb);

  // Let's create an object in the target scope and add privileged
  // function to it as a property.
  Cu.evalInSandbox("(" + function() {
    var newContentObject = createObjectIn(subsb, { defineAs: "importedObject" });
    exportFunction(funToExport, newContentObject, { defineAs: "privMethod" });
  }.toSource() + ")()", epsb);

  Cu.evalInSandbox("(" + function () {
    importedObject.privMethod(importedObject, 42, tobecloned, native, mixed);
  }.toSource() + ")()", subsb);

  Cu.evalInSandbox("(" + function() {
    checkIfCalled();
  }.toSource() + ")()", epsb);

  // exportFunction and createObjectIn should be available from Cu too.
  var newContentObject = Cu.createObjectIn(subsb, { defineAs: "importedObject2" });
  var wasCalled = false;
  Cu.exportFunction(function(arg) { wasCalled = arg.wasCalled; },
                    newContentObject, { defineAs: "privMethod" });

  Cu.evalInSandbox("(" + function () {
    importedObject2.privMethod({wasCalled: true});
  }.toSource() + ")()", subsb);

  // 3rd argument of exportFunction should be optional.
  Cu.evalInSandbox("(" + function() {
    subsb.imported2 = exportFunction(funToExport, subsb);
  }.toSource() + ")()", epsb);

  Cu.evalInSandbox("(" + function () {
    imported2(this, 42, tobecloned, native, mixed);
  }.toSource() + ")()", subsb);

  Cu.evalInSandbox("(" + function() {
    checkIfCalled();
  }.toSource() + ")()", epsb);

  do_check_true(wasCalled, true);
}
