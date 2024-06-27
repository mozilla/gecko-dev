Services.prefs.setBoolPref("javascript.options.wasm_js_promise_integration", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("javascript.options.wasm_js_promise_integration");
});

// The tests runs code in tight loop with the profiler enabled. It is testing
// behavior of JS PI specific methods and generated code.
// It is not guarantee 100% hit since the profiler probes stacks every 1ms,
// but it will happen often enough.
add_task(async () => {
  if (!WebAssembly.promising) {
    return;
  }

  await Services.profiler.StartProfiler(10, 1, ["js"], ["GeckoMain"]);
  Assert.ok(Services.profiler.IsActive());

/* Wasm module that is tested:
(module
  (import "js" "compute_delta"
    (func $compute_delta (param i32) (result f64)))

  (func (export "update_state_export") (param i32) (result f64)
     (call $compute_delta (local.get 0))
  )
)
*/

  var compute_delta = (i) => i / 100;
  const b = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 6, 1, 96, 1, 127, 1, 124, 2, 20, 1, 2, 106,
    115, 13, 99, 111, 109, 112, 117, 116, 101, 95, 100, 101, 108, 116, 97,
    0, 0, 3, 2, 1, 0, 7, 23, 1, 19, 117, 112, 100, 97, 116, 101, 95, 115, 116,
    97, 116, 101, 95, 101, 120, 112, 111, 114, 116, 0, 1, 10, 8, 1, 6, 0, 32,
    0, 16, 0, 11, 0, 23, 4, 110, 97, 109, 101, 1, 16, 1, 0, 13, 99, 111, 109,
    112, 117, 116, 101, 95, 100, 101, 108, 116, 97
  ]);
  const ins = new WebAssembly.Instance(new WebAssembly.Module(b), {
    js: { compute_delta, },
  });
  var update_state = WebAssembly.promising(
    ins.exports.update_state_export
  );

  for (var i = 0; i < 1000; i++) {
    var r = await update_state(4);
    if (i % 222 == 0) {
      Assert.equal(r, .04);
    }
  }

  Assert.ok(true, "Done");
  await Services.profiler.StopProfiler();
});

add_task(async () => {
  if (!WebAssembly.promising) {
    return;
  }

  await Services.profiler.StartProfiler(10, 1, ["js"], ["GeckoMain"]);
  Assert.ok(Services.profiler.IsActive());

/* Wasm module that is tested:
(module
  (import "js" "compute_delta"
    (func $compute_delta (param i32) (result f64)))

  (func (export "update_state_export") (param i32) (result f64)
    (call $compute_delta (local.get 0))
  )
)
*/

  var compute_delta = async (i) => i / 100;
  var suspending_compute_delta = new WebAssembly.Suspending(
    compute_delta
  );
  const b = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 6, 1, 96, 1, 127, 1, 124, 2, 20, 1, 2, 106,
    115, 13, 99, 111, 109, 112, 117, 116, 101, 95, 100, 101, 108, 116, 97, 0,
    0, 3, 2, 1, 0, 7, 23, 1, 19, 117, 112, 100, 97, 116, 101, 95, 115, 116, 97,
    116, 101, 95, 101, 120, 112, 111, 114, 116, 0, 1, 10, 8, 1, 6, 0, 32, 0,
    16, 0, 11, 0, 23, 4, 110, 97, 109, 101, 1, 16, 1, 0, 13, 99, 111, 109, 112,
    117, 116, 101, 95, 100, 101, 108, 116, 97
  ]);
  const ins = new WebAssembly.Instance(new WebAssembly.Module(b), {
    js: { compute_delta: suspending_compute_delta, },
  });
  var update_state = WebAssembly.promising(
    ins.exports.update_state_export
  );

  for (var i = 0; i < 1000; i++) {
    var r = await update_state(4);
    if (i % 222 == 0) {
      Assert.equal(r, .04);
    }
  }

  Assert.ok(true, "Done");
  await Services.profiler.StopProfiler();
});

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test() {
  do_get_profile();
  run_next_test();
}
