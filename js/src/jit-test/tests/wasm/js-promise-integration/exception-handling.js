// |jit-tests| test-also=--setpref=wasm_exnref=true;

// Test if we can handle WebAssembly.Exception on suspendable stack,
// and in suspending function.

function wasmException(i) {
  if (i == 0) {
    // no promise return
    throw new WebAssembly.Exception(ins1.exports.tag, [i]);
  }
  return Promise.reject(new WebAssembly.Exception(ins1.exports.tag, [i]));
}

var ins1 = wasmEvalText(`
(module
  (import "" "t" (func $t (param i32)))
  (tag $t1 (param i32))

  (func (export "t0")
    i32.const 0
    call $t
  )
  (func (export "t1")
    i32.const 1
    call $t
  )
  (func (export "t2")
    try
      i32.const 2
      call $t
    catch $t1
      drop
    end
  )
  (func (export "t3")
    try
      i32.const 0
      call $t
    catch $t1
      drop
    end
  )
  (export "tag" (tag $t1))
)
`, {"": {
  t: new WebAssembly.Suspending(wasmException),
}});

async function testWasmException() {
  var test0 = WebAssembly.promising(ins1.exports.t0);
  var test1 = WebAssembly.promising(ins1.exports.t1);
  var test2 = WebAssembly.promising(ins1.exports.t2);
  var test3 = WebAssembly.promising(ins1.exports.t3);
  try {
    await test0();
    assertEq(true, false);
  } catch (ex) {
    assertEq(ex instanceof WebAssembly.Exception && ex.is(ins1.exports.tag), true);
    assertEq(ex.getArg(ins1.exports.tag, 0), 0);
  }
  try {
    await test1();
    assertEq(true, false);
  } catch (ex) {
    assertEq(ex instanceof WebAssembly.Exception && ex.is(ins1.exports.tag), true);
    assertEq(ex.getArg(ins1.exports.tag, 0), 1);
  }
  await test2();
  await test3();
}

// run test asynchronously
var p = testWasmException();

// Test if we can handle JS exception/rejection on suspendable stack,
// and in suspending function.

function jsException(i) {
  if (i == 0) {
    // No promise return.
    throw new Error("test" + i);
  }
  if (i == 42) {
    // Reject with non-Error object.
    return Promise.reject(3.14);
  }
  return Promise.reject(new Error("test" + i));
}

var ins2 = wasmEvalText(`
(module
  (import "" "t" (func $t (param i32)))
  (import "" "tag" (tag $t1 (param externref)))

  (func (export "t0")
    i32.const 0
    call $t
  )
  (func (export "t1")
    i32.const 1
    call $t
  )
  (func (export "t2") (result externref)
    try
      i32.const 2
      call $t
    catch $t1
      return
    end
    unreachable
  )
  (func (export "t3") (result externref)
    try
      i32.const 0
      call $t
    catch $t1
      return
    end
    unreachable
  )
  (func (export "t42")
    i32.const 42
    call $t
  )
)
`, {"": {
  t: new WebAssembly.Suspending(jsException),
  tag: WebAssembly.JSTag,
}});

assertEq(WebAssembly.JSTag instanceof WebAssembly.Tag, true);
async function testJSException() {
  var test0 = WebAssembly.promising(ins2.exports.t0);
  var test1 = WebAssembly.promising(ins2.exports.t1);
  var test2 = WebAssembly.promising(ins2.exports.t2);
  var test3 = WebAssembly.promising(ins2.exports.t3);
  var test42 = WebAssembly.promising(ins2.exports.t42);
  try {
    await test0();
    assertEq(true, false);
  } catch (ex) {
    assertEq(ex instanceof Error && ex.message == "test0", true);
  }
  try {
    await test1();
    assertEq(true, false);
  } catch (ex) {
    assertEq(ex instanceof Error && ex.message == "test1", true);
  }
  var ex2 = await test2();
  assertEq(ex2 instanceof Error && ex2.message == "test2", true);
  var ex3 = await test3();
  assertEq(ex3 instanceof Error && ex3.message == "test0", true);
  try {
    await test42();
    assertEq(true, false);
  } catch (ex) {
    assertEq(ex, 3.14);
  }
}

// run test asynchronously
p = p.then(testJSException);
