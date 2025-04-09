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

// structs and struct fields
{
  const counts = { 100: 0, 200: 0, 300: 0, 400: 0 };

  const { test1, test2 } = wasmEvalText(`(module
    (type $a (struct (field i32)))
    (type $s (struct (field (ref $a)) (field (ref $a))))

    (import "" "dummy" (func $dummy (param anyref)))
    (import "" "count" (func $count (param i32)))

    (func (export "test1")
      (local anyref)

      (struct.new $a (i32.const 100))
      (struct.new $a (i32.const 200))
      struct.new $s
      local.tee 0
      call $dummy

      local.get 0
      ref.cast (ref $s)
      struct.get $s 0
      ref.cast (ref $a)
      struct.get $a 0
      call $count

      local.get 0
      ref.cast (ref $s)
      struct.get $s 1
      ref.cast (ref $a)
      struct.get $a 0
      call $count
    )
    (func (export "test2")
      (local anyref i32)

      (struct.new $a (i32.const 100))
      (struct.new $a (i32.const 200))
      struct.new $s
      local.tee 0
      call $dummy

      loop
        local.get 0
        ref.cast (ref $s)
        struct.get $s 0
        ref.cast (ref $a)
        struct.get $a 0
        call $count

        local.get 0
        ref.cast (ref $s)
        struct.get $s 1
        ref.cast (ref $a)
        struct.get $a 0
        call $count

        (i32.ge_s (local.get 1) (i32.const 4))
        if
          (struct.new $a (i32.const 300))
          (struct.new $a (i32.const 400))
          struct.new $s
          local.set 0
        end

        (local.tee 1 (i32.add (local.get 1) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      dummy() {},
      count(n) {
        counts[n] += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(counts[100], 100);
  assertEq(counts[200], 100);
  assertEq(counts[300], 0);
  assertEq(counts[400], 0);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(counts[100], 600);
  assertEq(counts[200], 600);
  assertEq(counts[300], 500);
  assertEq(counts[400], 500);
}

// arrays and array elems
{
  const counts = { 100: 0, 200: 0, 300: 0, 400: 0 };

  const { test1, test2 } = wasmEvalText(`(module
    (type $e (array i32))
    (type $a (array (ref $e)))

    (import "" "dummy" (func $dummy (param anyref)))
    (import "" "count" (func $count (param i32)))

    (func (export "test1")
      (local anyref)

      (array.new_fixed $e 1 (i32.const 100))
      (array.new_fixed $e 1 (i32.const 200))
      array.new_fixed $a 2
      local.tee 0
      call $dummy

      local.get 0
      ref.cast (ref $a)
      (array.get $a (i32.const 0))
      ref.cast (ref $e)
      (array.get $e (i32.const 0))
      call $count

      local.get 0
      ref.cast (ref $a)
      (array.get $a (i32.const 1))
      ref.cast (ref $e)
      (array.get $e (i32.const 0))
      call $count
    )
    (func (export "test2")
      (local anyref i32)

      (array.new_fixed $e 1 (i32.const 100))
      (array.new_fixed $e 1 (i32.const 200))
      array.new_fixed $a 2
      local.tee 0
      call $dummy

      loop
        local.get 0
        ref.cast (ref $a)
        (array.get $a (i32.const 0))
        ref.cast (ref $e)
        (array.get $e (i32.const 0))
        call $count

        local.get 0
        ref.cast (ref $a)
        (array.get $a (i32.const 1))
        ref.cast (ref $e)
        (array.get $e (i32.const 0))
        call $count

        (i32.ge_s (local.get 1) (i32.const 4))
        if
          (array.new_fixed $e 1 (i32.const 300))
          (array.new_fixed $e 1 (i32.const 400))
          array.new_fixed $a 2
          local.set 0
        end

        (local.tee 1 (i32.add (local.get 1) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      dummy() {},
      count(n) {
        counts[n] += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(counts[100], 100);
  assertEq(counts[200], 100);
  assertEq(counts[300], 0);
  assertEq(counts[400], 0);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(counts[100], 600);
  assertEq(counts[200], 600);
  assertEq(counts[300], 500);
  assertEq(counts[400], 500);
}

// table elems
{
  const counts = { 100: 0, 200: 0, 300: 0, 400: 0 };

  const { test1, test2 } = wasmEvalText(`(module
    (type $f (func))

    (import "" "dummy" (func $dummy (param anyref)))
    (import "" "count" (func $count (param i32)))

    (table funcref (elem (ref.func $count100) (ref.func $count200)))
    (table (ref null $f) (elem (ref.func $count300) (ref.func $count400)))

    (func $count100 (type $f)
      (call $count (i32.const 100))
    )
    (func $count200 (type $f)
      (call $count (i32.const 200))
    )
    (func $count300 (type $f)
      (call $count (i32.const 300))
    )
    (func $count400 (type $f)
      (call $count (i32.const 400))
    )

    (func (export "test1")
      (table.get 0 (i32.const 0))
      ref.cast (ref null $f)
      call_ref $f

      (table.get 0 (i32.const 1))
      ref.cast (ref null $f)
      call_ref $f

      (table.get 1 (i32.const 0))
      ref.cast (ref null $f)
      call_ref $f

      (table.get 1 (i32.const 1))
      ref.cast (ref null $f)
      call_ref $f
    )
    (func (export "test2")
      (local i32)

      (table.get 0 (i32.const 0))
      ref.cast (ref null $f)
      call_ref $f

      (table.get 1 (i32.const 0))
      ref.cast (ref null $f)
      call_ref $f

      loop
        (table.get 0 (i32.const 0))
        ref.cast (ref null $f)
        call_ref $f

        (table.get 0 (i32.const 1))
        ref.cast (ref null $f)
        call_ref $f

        (table.get 1 (i32.const 0))
        ref.cast (ref null $f)
        call_ref $f

        (table.get 1 (i32.const 1))
        ref.cast (ref null $f)
        call_ref $f

        (local.tee 0 (i32.add (local.get 0) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      dummy() {},
      count(n) {
        counts[n] += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(counts[100], 100);
  assertEq(counts[200], 100);
  assertEq(counts[300], 100);
  assertEq(counts[400], 100);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(counts[100], 1200);
  assertEq(counts[200], 1100);
  assertEq(counts[300], 1200);
  assertEq(counts[400], 1100);
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

// parameters and calls
{
  let numNull = 0;
  let numNonNull = 0;

  // Note that we are ok with inlining kicking in eventually (and have
  // constructed these tests to trigger that).
  const { test1, test2 } = wasmEvalText(`(module
    (type $f_anyref (func (param anyref)))
    (type $f_nullref (func (param nullref)))

    (import "" "thing" (global $thing externref))
    (import "" "countNull" (func $countNull))
    (import "" "countNonNull" (func $countNonNull))

    (table funcref (elem
      (ref.func $testNull_ref.is_null)
      (ref.func $mustNull_ref.is_null)
      (ref.func $testNull_ref.test)
      (ref.func $mustNull_ref.test)
    ))

    (func $makeNull_Loose (result anyref)
      ref.null any
    )
    (func $makeNonNull_Loose (result anyref)
      (any.convert_extern (global.get $thing))
    )
    (func $makeNull_Precise (result nullref)
      ref.null none
    )
    (func $makeNonNull_Precise (result (ref any))
      (ref.as_non_null (any.convert_extern (global.get $thing)))
    )

    (func $testNull_ref.is_null (param anyref)
      (ref.is_null (local.get 0))
      if
        call $countNull
      else
        call $countNonNull
      end
    )
    (func $mustNull_ref.is_null (param nullref)
      (ref.is_null (local.get 0)) ;; trivial!
      if
        call $countNull
      end
    )
    (func $testNull_ref.test (param anyref)
      (ref.test nullref (local.get 0)) ;; trivial!
      if
        call $countNull
      else
        call $countNonNull
      end
    )
    (func $mustNull_ref.test (param nullref)
      (ref.test nullref (local.get 0)) ;; trivial!
      if
        call $countNull
      end
    )

    (func (export "test1")
      (local anyref)

      (local.set 0 (call $makeNull_Loose))
      (call $testNull_ref.is_null (local.get 0))
      (call $testNull_ref.test (local.get 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 2))
      (ref.is_null (local.get 0)) ;; guaranteed to succeed
      if
        (call $mustNull_ref.is_null (ref.cast nullref (local.get 0)))
        (call $mustNull_ref.test (ref.cast nullref (local.get 0)))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 1))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 3))
      end

      (local.set 0 (call $makeNull_Precise))
      (call $testNull_ref.is_null (local.get 0))
      (call $testNull_ref.test (local.get 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 2))
      (ref.is_null (local.get 0)) ;; guaranteed to succeed
      if
        (call $mustNull_ref.is_null (ref.cast nullref (local.get 0)))
        (call $mustNull_ref.test (ref.cast nullref (local.get 0)))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 1))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 3))
      end

      (local.set 0 (call $makeNonNull_Loose))
      (call $testNull_ref.is_null (local.get 0))
      (call $testNull_ref.test (local.get 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 2))
      (ref.is_null (local.get 0)) ;; guaranteed to fail
      if
        (call $mustNull_ref.is_null (ref.cast nullref (local.get 0)))
        (call $mustNull_ref.test (ref.cast nullref (local.get 0)))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 1))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 3))
      end

      (local.set 0 (call $makeNonNull_Precise))
      (call $testNull_ref.is_null (local.get 0))
      (call $testNull_ref.test (local.get 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 0))
      (call_indirect (type $f_anyref) (local.get 0) (i32.const 2))
      (ref.is_null (local.get 0)) ;; guaranteed to fail
      if
        (call $mustNull_ref.is_null (ref.cast nullref (local.get 0)))
        (call $mustNull_ref.test (ref.cast nullref (local.get 0)))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 1))
        (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 3))
      end
    )
    (func (export "test2")
      (local anyref i32)

      block
        loop
          (i32.eq (local.get 1) (i32.const 0))
          if
            (local.set 0 (call $makeNull_Loose))
          else
            (i32.eq (local.get 1) (i32.const 1))
            if
              (local.set 0 (call $makeNull_Precise))
            else
              (i32.eq (local.get 1) (i32.const 2))
              if
                (local.set 0 (call $makeNonNull_Loose))
              else
                (i32.eq (local.get 1) (i32.const 3))
                if
                  (local.set 0 (call $makeNonNull_Precise))
                else
                  return
                end
              end
            end
          end

          (call $testNull_ref.is_null (local.get 0))
          (call $testNull_ref.test (local.get 0))
          (call_indirect (type $f_anyref) (local.get 0) (i32.const 0))
          (call_indirect (type $f_anyref) (local.get 0) (i32.const 2))
          (ref.is_null (local.get 0))
          if
            (call $mustNull_ref.is_null (ref.cast nullref (local.get 0)))
            (call $mustNull_ref.test (ref.cast nullref (local.get 0)))
            (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 1))
            (call_indirect (type $f_nullref) (ref.cast nullref (local.get 0)) (i32.const 3))
          end

          (local.set 1 (i32.add (local.get 1) (i32.const 1)))
          br 0
        end
      end
    )
  )`, {
    "": {
      "thing": "hello I am extern guy",
      countNull() {
        numNull += 1;
      },
      countNonNull() {
        numNonNull += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(numNull, 1600);
  assertEq(numNonNull, 800);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(numNull, 3200);
  assertEq(numNonNull, 1600);
}

// globals
{
  const counts = { 100: 0, 200: 0 };

  const { test1, test2 } = wasmEvalText(`(module
    (type $f (func))

    (import "" "dummy" (func $dummy (param anyref)))
    (import "" "count" (func $count (param i32)))

    (global $a (mut (ref $f)) (ref.func $count100))
    (global $b (mut (ref $f)) (ref.func $count200))

    (func $count100 (type $f)
      (call $count (i32.const 100))
    )
    (func $count200 (type $f)
      (call $count (i32.const 200))
    )

    (func (export "test1")
      global.get $a
      call_ref $f

      global.get $b
      call_ref $f
    )
    (func (export "test2")
      (local funcref i32)

      (local.set 0 (global.get $a))

      loop
        local.get 0
        ref.cast (ref $f)
        call_ref $f

        (i32.ge_s (local.get 1) (i32.const 4))
        if
          (local.set 0 (global.get $b))
        end

        (local.tee 1 (i32.add (local.get 1) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      dummy() {},
      count(n) {
        counts[n] += 1;
      },
    },
  }).exports;

  for (let i = 0; i < 100; i++) {
    test1();
  }

  assertEq(counts[100], 100);
  assertEq(counts[200], 100);

  for (let i = 0; i < 100; i++) {
    test2();
  }

  assertEq(counts[100], 600);
  assertEq(counts[200], 600);
}

// any.convert_extern and extern.convert_any
{
  let numAnyref = 0;
  let numExternref = 0;

  const { test1, test2 } = wasmEvalText(`(module
    (import "" "thing" (global $extern1 externref))
    (import "" "thing" (global $extern2 externref))
    (import "" "dummy1" (func $dummy1 (param anyref)))
    (import "" "dummy2" (func $dummy2 (param externref)))
    (import "" "countAnyref" (func $countAnyref))
    (import "" "countExternref" (func $countExternref))

    (global $any1 anyref (any.convert_extern (global.get $extern1)))
    (global $any2 anyref (any.convert_extern (global.get $extern2)))

    (func (export "test1")
      (call $dummy1 (any.convert_extern (global.get $extern1)))
      (call $dummy2 (extern.convert_any (global.get $any1)))

      ;; These tests are trivial.

      (ref.test anyref (any.convert_extern (global.get $extern1)))
      if
        call $countAnyref
      end

      (ref.test externref (extern.convert_any (global.get $any1)))
      if
        call $countExternref
      end
    )
    (func (export "test2")
      (local i32 anyref externref)

      (local.tee 1 (any.convert_extern (global.get $extern1)))
      call $dummy1
      (local.tee 2 (extern.convert_any (global.get $any1)))
      call $dummy2

      loop
        ;; These tests are again trivial.

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
          ;; These are different values but have the same type. It will not
          ;; affect the types that flow into the tests.
          (local.set 1 (any.convert_extern (global.get $extern2)))
          (local.set 2 (extern.convert_any (global.get $any2)))
        end

        (local.tee 0 (i32.add (local.get 0) (i32.const 1)))
        (i32.lt_s (i32.const 10))
        br_if 0
      end
    )
  )`, {
    "": {
      "thing": "hello I am extern guy",
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
