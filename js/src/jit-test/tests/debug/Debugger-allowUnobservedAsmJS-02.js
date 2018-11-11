// Debugger.allowUnobservedAsmJS with off-thread parsing.

load(libdir + "asm.js");

if (helperThreadCount() == 0)
    quit();

var g = newGlobal();
g.parent = this;
g.eval("dbg = new Debugger(parent);");

assertEq(g.dbg.allowUnobservedAsmJS, false);

enableLastWarning();

var asmFunStr = USE_ASM + 'function f() {} return f';
offThreadCompileScript("(function() {" + asmFunStr + "})");
runOffThreadScript();

var msg = getLastWarning().message;
assertEq(msg === "asm.js type error: Disabled by debugger" ||
         msg === "asm.js type error: Disabled by lack of a JIT compiler" ||
         msg === "asm.js type error: Disabled by 'asmjs' runtime option" ||
         msg === "asm.js type error: Disabled by lack of compiler support",
         true);
