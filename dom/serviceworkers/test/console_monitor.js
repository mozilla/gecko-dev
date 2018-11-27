ChromeUtils.import("resource://gre/modules/Services.jsm");

let consoleListener;

function ConsoleListener() {
  Services.console.registerListener(this);
}

ConsoleListener.prototype  = {
  callbacks: [],

  observe: (aMsg) => {
    if (!(aMsg instanceof Ci.nsIScriptError)) {
      return;
    }

    let msg = {
      errorMessage  : aMsg.errorMessage,
      sourceName    : aMsg.sourceName,
      sourceLine    : aMsg.sourceLine,
      lineNumber    : aMsg.lineNumber,
      columnNumber  : aMsg.columnNumber,
      category      : aMsg.category,
      windowID      : aMsg.outerWindowID,
      innerWindowID : aMsg.innerWindowID,
      isScriptError : true,
      isWarning     : ((aMsg.flags & Ci.nsIScriptError.warningFlag) === 1),
      isException   : ((aMsg.flags & Ci.nsIScriptError.exceptionFlag) === 1),
      isStrict      : ((aMsg.flags & Ci.nsIScriptError.strictFlag) === 1),
    };

    sendAsyncMessage("monitor", msg);
  }
}

addMessageListener("load", function (e) {
  consoleListener = new ConsoleListener();
  sendAsyncMessage("ready", {});
});

addMessageListener("unload", function (e) {
  Services.console.unregisterListener(consoleListener);
  consoleListener = null;
  sendAsyncMessage("unloaded", {});
});
