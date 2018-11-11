// Test that the onPromiseSettled hook gets called when a promise settles.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

let log = "";
let pw;
dbg.onPromiseSettled = pw_ => {
  pw = pw_;
  log += "s";
};

let p = new g.Promise(function (){});
g.settlePromiseNow(p);

assertEq(log, "s");
assertEq(pw, gw.makeDebuggeeValue(p));

log = "";
dbg.enabled = false;
p = new g.Promise(function (){});
g.settlePromiseNow(p);

assertEq(log, "");
