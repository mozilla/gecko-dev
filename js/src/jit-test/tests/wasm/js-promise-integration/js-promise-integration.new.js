// JS promise integration API tests
// Modified https://github.com/WebAssembly/js-promise-integration/tree/main/test/js-api/js-promise-integration

var tests = Promise.resolve();
function test(fn, n) {
  if (n == null) n = (new Error()).stack.split('\n')[1];
  tests = tests.then(() => {
    let t = {res: null};
    print("# " + n);
    fn(t);
    return t.res;
  }, (e) => {
    throw e;
  });
}
function promise_test(fn, n) {
  if (n == null) n = (new Error()).stack.split('\n')[1];
  tests = tests.then(() => {
    let t = {res: null};
    print("# " + n);
    return fn(t);
  });
}
function assert_true(f) { assertEq(f, true); }
function assert_false(f) { assertEq(f, false); }
function assert_equals(a, b) { assertEq(a, b); }
function assert_array_equals(a, b) {
  assert_equals(a.length, a.length);
  for (let i = 0; i < a.length; i++) {
    assert_equals(a[i], b[i]);
  }
}
function assert_throws(ex, fn, msg) {
  try {
    fn();
  } catch(e) {
    assertEq(e instanceof ex, true, `Type of e: ${e.constructor.name}`);
    return;
  }
  assertEq(false, true, "fail expected");
}
function promise_rejects(t, obj, p) {
  t.res = p.then(() => {
    assertEq(true, false);
  }, (e) => {
    assertEq(e instanceof obj.constructor, true);
  });
}

function Promising(wasm_export) {
  return WebAssembly.promising(wasm_export);
}

function Suspending(jsFun){
  return new WebAssembly.Suspending(jsFun);
}

// Test for invalid wrappers
test(() => {
  assert_throws(TypeError, () => WebAssembly.promising({}),
      /Argument 0 must be a function/);
  assert_throws(TypeError, () => WebAssembly.promising(() => {}),
      /Argument 0 must be a WebAssembly exported function/);
  assert_throws(TypeError, () => WebAssembly.Suspending(() => {}),
      /WebAssembly.Suspending must be invoked with 'new'/);
  assert_throws(TypeError, () => new WebAssembly.Suspending({}),
      /Argument 0 must be a function/);
  function asmModule() {
    "use asm";
    function x(v) {
      v = v | 0;
    }
    return x;
  }
  assert_throws(TypeError, () => WebAssembly.promising(asmModule()),
      /Argument 0 must be a WebAssembly exported function/);
});

test(() => {
  let instance = wasmEvalText(`(module
  (type (func (result i32)))
  (func $test (type 0) (result i32)
    i32.const 42
    global.set 0
    i32.const 0
  )
  (global (mut i32) i32.const 0)
  (export "g" (global 0))
  (export "test" (func $test))
)`);
  let wrapper = WebAssembly.promising(instance.exports.test);
  wrapper();
  assert_equals(42, instance.exports.g.value);
});

promise_test(async () => {
  let js_import = Suspending(() => Promise.resolve(42));
  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func (param i32) (result i32)))
  (import "m" "import" (func (type 0)))
  (func $test (type 1) (param i32) (result i32)
    local.get 0
    call 0
  )
  (export "test" (func $test))
)`, {m: {import: js_import}});
  let wrapped_export = Promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  assert_equals(await export_promise, 42);
}, "Suspend once");

promise_test(async () => {
  let i = 0;
  function js_import() {
    return Promise.resolve(++i);
  };
  let wasm_js_import = Suspending(js_import);
  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func (param i32)))
  (import "m" "import" (func (type 0)))
  (func $test (type 1) (param i32)
    (local i32)
    i32.const 5
    local.set 1
    loop ;; label = @1
      local.get 0
      call 0
      global.get 0
      i32.add
      global.set 0
      local.get 1
      i32.const 1
      i32.sub
      local.tee 1
      br_if 0 (;@1;)
    end
  )
  (global (mut i32) i32.const 0)
  (export "g" (global 0))
  (export "test" (func $test))
)`, {m: {import: wasm_js_import}});
  let wrapped_export = Promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_equals(instance.exports.g.value, 0);
  assert_true(export_promise instanceof Promise);
  await export_promise;
  assert_equals(instance.exports.g.value, 15);
}, "Suspend/resume in a loop");

promise_test(async ()=>{
  let js_import = new WebAssembly.Suspending(() => Promise.resolve(42));
  let instance = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (func $test (type 1) (result i32)
    call 0
  )
  (export "test" (func $test))
)`, {m: {import: js_import}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);

  // Also try with a JS function with a mismatching arity.
  js_import = new WebAssembly.Suspending((unused) => Promise.resolve(42));
  instance = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (func $test (type 1) (result i32)
    call 0
  )
  (export "test" (func $test))
)`, {m: {import: js_import}});
  wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);

  // Also try with a proxy.
  js_import = new WebAssembly.Suspending(new Proxy(() => Promise.resolve(42), {}));
  instance = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (func $test (type 1) (result i32)
    call 0
  )
  (export "test" (func $test))
)`, {m: {import: js_import}});
  wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);
});

function recordAbeforeB(){
  let AbeforeB = [];
  let setA = ()=>{
    AbeforeB.push("A")
  }
  let setB = ()=>{
    AbeforeB.push("B")
  }
  let isAbeforeB = ()=>
    AbeforeB[0]=="A" && AbeforeB[1]=="B";

  let showAbeforeB = ()=>{
    console.log(AbeforeB)
  }
  return {setA : setA, setB : setB, isAbeforeB :isAbeforeB,showAbeforeB:showAbeforeB}
}

promise_test(async () => {
  let AbeforeB = recordAbeforeB();
  let import42 = Suspending(()=>Promise.resolve(42));
  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func))
  (type (func (param i32) (result i32)))
  (import "m" "import42" (func (type 0)))
  (import "m" "setA" (func (type 1)))
  (func $test (type 2) (param i32) (result i32)
    local.get 0
    call 0
    call 1
  )
  (export "test" (func $test))
)`, {m: {import42: import42, setA:AbeforeB.setA}});

  let wrapped_export = Promising(instance.exports.test);

//  AbeforeB.showAbeforeB();
  let exported_promise = wrapped_export();
//  AbeforeB.showAbeforeB();

  AbeforeB.setB();

  //print(await exported_promise);
  assert_equals(await exported_promise, 42);
//  AbeforeB.showAbeforeB();

  assert_false(AbeforeB.isAbeforeB());
}, "Make sure we actually suspend");

promise_test(async () => {
  let AbeforeB = recordAbeforeB();
  let import42 = Suspending(()=>42);
  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func))
  (type (func (param i32) (result i32)))
  (import "m" "import42" (func (type 0)))
  (import "m" "setA" (func (type 1)))
  (func $test (type 2) (param i32) (result i32)
    local.get 0
    call 0
    call 1
  )
  (export "test" (func $test))
)`, {
  m: {import42: import42, setA:AbeforeB.setA}});

  let wrapped_export = Promising(instance.exports.test);

  let exported_promise = wrapped_export();
  AbeforeB.setB();

  assert_equals(await exported_promise, 42);
  // AbeforeB.showAbeforeB();

  assert_true(AbeforeB.isAbeforeB());
}, "Do not suspend if the import's return value is not a Promise");

test(t => {
  console.log("Throw after the first suspension");
  let tag = new WebAssembly.Tag({parameters: []});
  function js_import() {
    return Promise.resolve();
  };
  let wasm_js_import = Suspending(js_import);

  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func))
  (type (func (param i32) (result i32)))
  (import "m" "import" (func (type 0)))
  (import "m" "tag" (tag (type 1)))
  (func $test (type 2) (param i32) (result i32)
    local.get 0
    call 0
    throw 0
  )
  (export "test" (func $test))
)`, {m: {import: wasm_js_import, tag: tag}});
  let wrapped_export = Promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  promise_rejects(t, new WebAssembly.Exception(tag, []), export_promise);
}, "Throw after the first suspension");

promise_test(async (t) => {
  console.log("Rejecting promise");
  let tag = new WebAssembly.Tag({parameters: ['i32']});
  function js_import() {
    return Promise.reject(new WebAssembly.Exception(tag, [42]));
  };
  let wasm_js_import = Suspending(js_import);

  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func (param i32)))
  (type (func (param i32) (result i32)))
  (import "m" "import" (func (type 0)))
  (import "m" "tag" (tag (type 1) (param i32)))
  (func $test (type 2) (param i32) (result i32)
    try (result i32) ;; label = @1
      local.get 0
      call 0
    catch 0
    end
  )
  (export "test" (func $test))
)`, {m: {import: wasm_js_import, tag: tag}});
  let wrapped_export = Promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  assert_equals(await export_promise, 42);
}, "Rejecting promise");

async function TestNestedSuspenders(suspend) {
  console.log("nested suspending "+suspend);
  // Nest two suspenders. The call chain looks like:
  // outer (wasm) -> outer (js) -> inner (wasm) -> inner (js)
  // If 'suspend' is true, the inner JS function returns a Promise, which
  // suspends the inner wasm function, which returns a Promise, which suspends
  // the outer wasm function, which returns a Promise. The inner Promise
  // resolves first, which resumes the inner continuation. Then the outer
  // promise resolves which resumes the outer continuation.
  // If 'suspend' is false, the inner JS function returns a regular value and
  // no computation is suspended.

  let inner = Suspending(() => suspend ? Promise.resolve(42) : 43);

  let export_inner;
  let outer = Suspending(() => export_inner());

  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func (param i32) (result i32)))
  (type (func (param i32) (result i32)))
  (type (func (param i32) (result i32)))
  (import "m" "inner" (func (type 0)))
  (import "m" "outer" (func (type 1)))
  (func $outer (type 2) (param i32) (result i32)
    local.get 0
    call 1
  )
  (func $inner (type 3) (param i32) (result i32)
    local.get 0
    call 0
  )
  (export "outer" (func $outer))
  (export "inner" (func $inner))
)`, {m: {inner, outer}});
  export_inner = Promising(instance.exports.inner);
  let export_outer = Promising(instance.exports.outer);
  let result = export_outer();
  assert_true(result instanceof Promise);
  if(suspend)
    assert_equals(await result, 42);
  else
    assert_equals(await result, 43);
}

promise_test(async () => {
  TestNestedSuspenders(true);
}, "Test nested suspenders with suspension");

promise_test(async () => {
  TestNestedSuspenders(false);
}, "Test nested suspenders with no suspension");

test(() => {
  console.log("Call import with an invalid suspender");
  let js_import = Suspending(() => Promise.resolve(42));
  let instance = wasmEvalText(`(module
  (type (func (param i32) (result i32)))
  (type (func (param i32) (result i32)))
  (type (func (param i32) (result i32)))
  (import "m" "import" (func (type 0)))
  (func $test (type 1) (param i32) (result i32)
    local.get 0
    call 0
  )
  (func $return_suspender (type 2) (param i32) (result i32)
    local.get 0
  )
  (export "test" (func $test))
  (export "return_suspender" (func $return_suspender))
)`, {m: {import: js_import}});
  let suspender = Promising(instance.exports.return_suspender)();
  for (s of [suspender, null, undefined, {}]) {
    assert_throws(WebAssembly.RuntimeError, () => instance.exports.test(s));
  }
}, "Call import with an invalid suspender");

// Throw an exception before suspending. The export wrapper should return a
// promise rejected with the exception.
promise_test(async (t) => {
  let tag = new WebAssembly.Tag({parameters: []});

  let instance = wasmEvalText(`(module
  (type (func))
  (type (func (result i32)))
  (import "m" "tag" (tag (type 0)))
  (func $test (type 1) (result i32)
    throw 0
  )
  (export "test" (func $test))
)`, {m: {tag: tag}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();

  promise_rejects(t, new WebAssembly.Exception(tag, []), export_promise);
});

// Throw an exception after the first resume event, which propagates to the
// promise wrapper.
promise_test(async (t) => {
  let tag = new WebAssembly.Tag({parameters: []});
  function js_import() {
    return Promise.resolve(42);
  };
  let wasm_js_import = new WebAssembly.Suspending(js_import);

  let instance = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (import "m" "tag" (tag (type 1)))
  (func $test (type 2) (result i32)
    call 0
    throw 0
  )
  (export "test" (func $test))
)`, {m: {import: wasm_js_import, tag: tag}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();

  promise_rejects(t, new WebAssembly.Exception(tag, []), export_promise);
});

promise_test(async () => {
  let tag = new WebAssembly.Tag({parameters: ['i32']});
  function js_import() {
    return Promise.reject(new WebAssembly.Exception(tag, [42]));
  };
  let wasm_js_import = new WebAssembly.Suspending(js_import);

  let instance = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (param i32)))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (import "m" "tag" (tag (type 1) (param i32)))
  (func $test (type 2) (result i32)
    try (result i32) ;; label = @1
      call 0
    catch 0
    end
  )
  (export "test" (func $test))
)`, {m: {import: wasm_js_import, tag: tag}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);
});

test(() => {
  console.log("no return allowed");
  // Check that a promising function with no return is allowed.
  let instance = wasmEvalText(`(module
  (type (func))
  (func $export (type 0))
  (export "export" (func $export))
)`);
  let export_wrapper = WebAssembly.promising(instance.exports.export);
  assert_true(export_wrapper instanceof Function);
}, "wrapper type");

promise_test(async (t) => {
  let instance = wasmEvalText(`(module
  (type (func (result i32)))
  (func $test (type 0) (result i32)
    call $test
  )
  (export "test" (func $test))
)`);
  let wrapper = WebAssembly.promising(instance.exports.test);

  promise_rejects(t, new Error(), wrapper(), /Maximum call stack size exceeded/);
});

promise_test(async (t) => {
  // The call stack of this test looks like:
  // export1 -> import1 -> export2 -> import2
  // Where export1 is "promising" and import2 is "suspending". Returning a
  // promise from import2 should trap because of the JS import in the middle.
  let instance;
  function import1() {
    // import1 -> export2 (unwrapped)
    instance.exports.export2();
  }
  function import2() {
    return Promise.resolve(0);
  }
  import2 = new WebAssembly.Suspending(import2);
  instance = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (result i32)))
  (type (func (result i32)))
  (type (func (result i32)))
  (import "m" "import1" (func (type 0)))
  (import "m" "import2" (func (type 1)))
  (func $export1 (type 2) (result i32)
    call 0
  )
  (func $export2 (type 3) (result i32)
    call 1
  )
  (export "export1" (func $export1))
  (export "export2" (func $export2))
)`,
      {'m':
        {'import1': import1,
         'import2': import2
        }});
  // export1 (promising)
  let wrapper = WebAssembly.promising(instance.exports.export1);
  promise_rejects(t, new WebAssembly.RuntimeError(), wrapper(),
      /trying to suspend JS frames/);
});

promise_test(async () => {
  let js_import = new WebAssembly.Suspending(() => Promise.resolve(1));
  let instance1 = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (func $f (type 1) (result i32)
    call 0
    i32.const 1
    i32.add
  )
  (export "f" (func $f))
)`, {m: {import: js_import}});
  let instance2 = wasmEvalText(`(module
  (type (func (result i32)))
  (type (func (result i32)))
  (import "m" "import" (func (type 0)))
  (func $main (type 1) (result i32)
    call 0
    i32.const 1
    i32.add
  )
  (export "main" (func $main))
)`, {m: {import: instance1.exports.f}});
  let wrapped_export = WebAssembly.promising(instance2.exports.main);
  assert_equals(await wrapped_export(), 3);
});

tests.then(() => print('Done'));
