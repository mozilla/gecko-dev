// |jit-test| skip-if: !WasmHelpers.isSingleStepProfilingEnabled

// Test profiling of JS PI non-suspending import call.

const {
  assertEqImpreciseStacks,
} = WasmHelpers;

var compute_delta = (i) => i / 100;

var ins = wasmEvalText(`(module
  (import "js" "compute_delta"
    (func $compute_delta (param i32) (result f64)))

  (global $state (mut f64) (f64.const 0))

  (func (export "update_state_export") (param i32) (result f64)
    (global.set $state (f64.add
      (global.get $state) (call $compute_delta (local.get 0))))
    (global.get $state)
  )
)`, {
    js: {
      compute_delta,
    },
});

var update_state = WebAssembly.promising(
    ins.exports.update_state_export
);

enableGeckoProfiling();

enableSingleStepProfiling();

function wb(s) {
  var p = s.split(",");
  p[0] = "<";
  var t = p.join(",");
  return [t, s, t];
}

var res = update_state(4);
var tasks = res.then((r) => {
  assertEq(r, .04);
  const stacks = disableSingleStepProfiling();
  assertEqImpreciseStacks(stacks,
    [
      "",
      ">",
      "1,>",                                   // enter $promising.exported
      ...wb("CreateSuspender,1,>"),
      "1,>",
      ...wb("CreatePromisingPromise,1,>"),
      "1,>",
      ...wb("#ref.func function,1,>"),         // ref to $promising.trampoline
      "1,>",
      ...wb("#update suspender state util,1,>"),
      "1,>",
      "2,1,>",                                 // enter $promising.trampoline
      "1,2,1,>",                               // enter "update_state_export"
      "<,1,2,1,>",                             // JS compute_delta
      "1,2,1,>",                               // exiting from "update_state_export"
      "2,1,>",                                 // at $promising.trampoline
      ...wb("SetPromisingPromiseResults,2,1,>"),
      "2,1,>",
      "1,>",
      ...wb("#update suspender state util,1,>"),
      "1,>",                                   // exiting $promising.exported
      ">",
      ""
    ]
  );

  disableGeckoProfiling();
});
