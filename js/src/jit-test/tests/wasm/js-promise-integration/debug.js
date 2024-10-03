// Tests stepping through the wasm code with JS PI suspendable stack.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

// Estimate internal SP range.
var base = stackPointerInfo();
var estimatedLimit = base - 300000;
var checkFailed = false;

// Checks current SP being in the estimated range --
// the debugger callbacks must be executed on the main stack.
function checkStack(s) {
  var sp = stackPointerInfo();
  if (sp < estimatedLimit || sp > base) {
    print(`Check failed: ${sp} not in [${estimatedLimit}, ${base}], at ${s}`);
    checkFailed = true;
  }
}
checkStack();
assertEq(checkFailed, false);

dbg.onEnterFrame = function(frame) {
  checkStack("enter");
  frame.onStep = () => {
    checkStack("step");
  };
  frame.onPop = () => {
    checkStack("pop");
  }
};
dbg.onExceptionUnwind = function (f, e) {
  checkStack("exception");
};

// Run typical JS PI program: create suspendable stack, suspend execution,
// throw on suspendable stack.
g.eval(`
function wasmEvalText(t, imp) {
  var wasm = wasmTextToBinary(t)
  var mod = new WebAssembly.Module(wasm);
  var ins = new WebAssembly.Instance(mod, imp);
  return ins;
}

try{throw"";}catch(_){} // init onExceptionUnwind

var compute_delta = (i) => {
  return Promise.resolve(i/100 || 1);
};

var suspending_compute_delta = new WebAssembly.Suspending(compute_delta);
var ins = wasmEvalText(\`(module
    (import "js" "compute_delta"
      (func $compute_delta (param i32) (result f64)))
    (tag $t)
    (func (export "update_state_export") (param i32) (result f64)
      local.get 0
      call $compute_delta
      i32.const 4
      i32.const 2
      i32.add
      drop
      throw $t
    )
)\`, {
  js: {
    compute_delta: suspending_compute_delta,
  },
});

var update_state = WebAssembly.promising(ins.exports.update_state_export);

var res = update_state(4);
res.then((r) => {
  print("RES:" + r);
}).catch(e => {
  print("ERR: " + e);
});

drainJobQueue();
`);

assertEq(checkFailed, false);
