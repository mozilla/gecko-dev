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
      "2,>",                                   // enter $promising.exported
      "<,2,>",                                 // JS $promising.create-suspender
      "2,>",
      ...wb("#ref.func function,2,>"),         // ref to $promising.trampoline
      "2,>",
      ...wb("#update suspender state util,2,>"),
      "2,>",
      "3,2,>",                                 // enter $promising.trampoline
      "1,3,2,>",                               // enter "update_state_export"
      "2,1,3,2,>",                             // enter $suspending.exported
      ...wb("CurrentSuspender,2,1,3,2,>"),
      "2,1,3,2,>",
      ...wb("#ref.func function,2,1,3,2,>"),   // ref to $suspending.trampoline
      "2,1,3,2,>",
      ...wb("#update suspender state util,2,1,3,2,>"),
      "2,1,3,2,>",
      "2,>",                                   // stack switched
      "3,2,>",                                 // enter $suspending.trampoline
      "<,3,2,>",                               // $suspending.wrappedfn
      "3,2,>",
      ...wb("#ref.func function,3,2,>"),       // ref to $suspending.continue-on-suspendable
      "3,2,>",
      "<,3,2,>",                               // $suspending.add-promise-reactions
      "3,2,>",
      "2,>",
      ...wb("#update suspender state util,2,>"),
      "2,>",                                   // exiting $promising.exported
      ">",
      "",
      ">",
      "4,>",                                   // enter $suspending.continue-on-suspendable
      "4,1,3,4,>",                             // back to "update_state_export"
      "2,1,3,4,>",                             // back to $suspending.exported
      ...wb("#update suspender state util,2,1,3,4,>"),
      "2,1,3,4,>",
      ...wb("GetSuspendingPromiseResult,2,1,3,4,>"),
      "2,1,3,4,>",
      "1,3,4,>",                               // exiting from "update_state_export"
      "3,4,>",                                 // at $promising.trampoline
      ...wb("SetPromisingPromiseResults,3,4,>"),
      "3,4,>",
      "4,>",
      ...wb("#update suspender state util,4,>"),
      "4,>",                                   // exiting $suspending.continue-on-suspendable
      ">",
      ""
    ]
  );

  disableGeckoProfiling();
});
