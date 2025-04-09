// Tests that exercise GVN and LICM. The general pattern for these tests is to
// create values near the top of the function and then create situations where
// equivalent values are created below, including in loops.

// ref.null
{
  let numAnyref = 0;
  let numExternref = 0;

  const { test1, test2 } = wasmEvalText(`(module
    (import "" "dummy1" (func $dummy1 (param anyref)))
    (import "" "dummy2" (func $dummy2 (param externref)))
    (import "" "countAnyref" (func $countAnyref))
    (import "" "countExternref" (func $countExternref))

    (func (export "test1")
      ;; Ensure that we keep two MWasmNullConstants with two different
      ;; hierarchies at the top of the graph.
      (call $dummy1 (ref.null none))
      (call $dummy2 (ref.null noextern))

      (ref.test anyref (ref.null none))
      if
        call $countAnyref
      end

      ;; If you're not careful, this null might be GVN'd with the null from the
      ;; other hierarchy, causing bad behavior.
      (ref.test externref (ref.null noextern))
      if
        call $countExternref
      end
    )
    (func (export "test2")
      (local i32 anyref externref)

      (local.tee 1 (ref.null none))
      call $dummy1
      (local.tee 2 (ref.null noextern))
      call $dummy2

      loop
        (ref.test anyref (local.get 1))
        if
          call $countAnyref
        end

        (ref.test externref (local.get 2))
        if
          call $countExternref
        end

        (i32.ge_s (local.get 0) (i32.const 5))
        if
          ;; Note that these use the top type instead of the bottom type.
          (local.set 1 (ref.null any))
          (local.set 2 (ref.null extern))
        end

        (local.tee 0 (i32.add (local.get 0) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      dummy1() {},
      dummy2() {},
      countAnyref() {
        numAnyref += 1;
      },
      countExternref() {
        numExternref += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(numAnyref, 100);
  assertEq(numExternref, 100);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(numAnyref, 1100);
  assertEq(numExternref, 1100);
}

// ref.i31
{
  const countsI31 = { 100: 0, 200: 0, 300: 0 };
  const countsI32 = { 100: 0, 200: 0, 300: 0 };
  const countsU32 = { 100: 0, 200: 0, 300: 0 };

  const { test1, test2 } = wasmEvalText(`(module
    (import "" "dummy" (func $dummy (param i31ref)))
    (import "" "countI31" (func $countI31 (param i31ref)))
    (import "" "countI32" (func $countI32 (param i32)))
    (import "" "countU32" (func $countU32 (param i32)))

    (func (export "test1")
      (call $dummy (ref.i31 (i32.const 100)))
      (call $dummy (ref.i31 (i32.const 200)))

      (call $countI31 (ref.i31 (i32.const 100)))
      (call $countI31 (ref.i31 (i32.const 200)))
      (call $countI32 (i31.get_s (ref.i31 (i32.const 100))))
      (call $countI32 (i31.get_s (ref.i31 (i32.const 200))))
      (call $countU32 (i31.get_u (ref.i31 (i32.const 100))))
      (call $countU32 (i31.get_u (ref.i31 (i32.const 200))))
    )
    (func (export "test2")
      (local i32 i31ref)

      (call $dummy (ref.i31 (i32.const 100)))
      (local.tee 1 (ref.i31 (i32.const 200)))
      call $dummy

      loop
        (call $countI31 (ref.i31 (i32.const 100)))
        (call $countI31 (local.get 1))
        (call $countI32 (i31.get_s (ref.i31 (i32.const 100))))
        (call $countI32 (i31.get_s (local.get 1)))
        (call $countU32 (i31.get_u (ref.i31 (i32.const 100))))
        (call $countU32 (i31.get_u (local.get 1)))

        (local.set 1 (ref.i31 (i32.const 300)))

        (local.tee 0 (i32.add (local.get 0) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      dummy() {},
      countI31(n) {
        countsI31[n] += 1;
      },
      countI32(n) {
        countsI32[n] += 1;
      },
      countU32(n) {
        countsU32[n] += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(countsI31[100], 100);
  assertEq(countsI31[200], 100);
  assertEq(countsI31[300], 0);
  assertEq(countsI32[100], 100);
  assertEq(countsI32[200], 100);
  assertEq(countsI32[300], 0);
  assertEq(countsU32[100], 100);
  assertEq(countsU32[200], 100);
  assertEq(countsU32[300], 0);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(countsI31[100], 1100);
  assertEq(countsI31[200], 200);
  assertEq(countsI31[300], 900);
  assertEq(countsI32[100], 1100);
  assertEq(countsI32[200], 200);
  assertEq(countsI32[300], 900);
  assertEq(countsU32[100], 1100);
  assertEq(countsU32[200], 200);
  assertEq(countsU32[300], 900);
}
