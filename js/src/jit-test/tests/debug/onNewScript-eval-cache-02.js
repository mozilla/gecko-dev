// onNewScript sees every script only once, even functions in cached eval scripts.

var g = newGlobal({newCompartment: true});
g.eval("function directEval(s) { return eval(s); }");

var dbg = Debugger(g);
var seen = new WeakMap;
dbg.onNewScript = function (script) {
    assertEq(seen.has(script), false);
    seen.set(script, 1);
    script.getChildScripts().forEach(dbg.onNewScript);
};

var s = "(function (x) { return x + 1; })";
for (var i = 0; i < 50; i++)
    g.directEval(s);
