// |jit-test| skip-if: !WasmHelpers.isSingleStepProfilingEnabled

// Test profiling of JS PI -- typical use case.

const {
  assertEqImpreciseStacks,
} = WasmHelpers;

var compute_delta = async (i) => Promise.resolve(i / 100 || 1);

var suspending_compute_delta = new WebAssembly.Suspending(
    compute_delta
);
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
      compute_delta: suspending_compute_delta,
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
      "1,1,2,1,>",                             // enter $suspending.exported
      ...wb("CurrentSuspender,1,1,2,1,>"),
      "1,1,2,1,>",
      ...wb("#ref.func function,1,1,2,1,>"),   // ref to $suspending.trampoline
      "1,1,2,1,>",
      ...wb("#update suspender state util,1,1,2,1,>"),
      "1,1,2,1,>",
      "1,>",                                   // stack switched
      "2,1,>",                                 // enter $suspending.trampoline
      "<,2,1,>",                               // $suspending.wrappedfn
      "2,1,>",
      ...wb("#ref.func function,2,1,>"),       // ref to $suspending.continue-on-suspendable
      "2,1,>",
      ...wb("AddPromiseReactions,2,1,>"),
      "2,1,>",
      "1,>",
      ...wb("#update suspender state util,1,>"),
      "1,>",                                   // exiting $promising.exported
      ">",
      "",
      ">",
      "3,>",                                   // enter $suspending.continue-on-suspendable
      "3,1,2,3,>",                             // back to "update_state_export"
      "1,1,2,3,>",                             // back to $suspending.exported
      ...wb("#update suspender state util,1,1,2,3,>"),
      "1,1,2,3,>",
      ...wb("GetSuspendingPromiseResult,1,1,2,3,>"),
      "1,1,2,3,>",
      "1,2,3,>",                               // exiting from "update_state_export"
      "2,3,>",                                 // at $promising.trampoline
      ...wb("SetPromisingPromiseResults,2,3,>"),
      "2,3,>",
      "3,>",
      ...wb("#update suspender state util,3,>"),
      "3,>",                                   // exiting $suspending.continue-on-suspendable
      ">",
      ""
    ]
  );

  disableGeckoProfiling();
});
