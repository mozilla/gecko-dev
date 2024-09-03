// Ensure JSOp::GetBoundName executes [[Has]], [[Get]], and [[Set]] the correct
// number of times.

function testInc() {
  function outer() {
    with (env) {
      // The inner function can be Warp-compiled, but has a with-environment
      // on its scope chain.
      return function() {
        // The increment operator is compiled to JSOp::GetBoundName.
        return ++prop;
      }
    }
  }

  var count_get = 0;
  var count_has = 0;
  var count_set = 0;

  function proxify(obj) {
    return new Proxy(obj, {
      get(t, pk, r) {
        count_get++;
        return Reflect.get(t, pk, r);
      },
      has(t, pk) {
        count_has++;
        return Reflect.has(t, pk);
      },
      set(t, pk, v, r) {
        count_set++;
        return Reflect.set(t, pk, v, r);
      },
    });
  }

  var count_unscopables = 0;

  var env = {
    get [Symbol.unscopables]() {
      count_unscopables++;
    },
    prop: 0,
  };
  env = proxify(env);

  var inner = outer();
  for (let i = 0; i < 200; ++i) {
    assertEq(inner(), i + 1);
  }

  assertEq(count_unscopables, 200);
  assertEq(count_has, 400);
  assertEq(count_get, 400);
  assertEq(count_set, 200);
}
testInc();

function testCompoundAssign() {
  function outer() {
    with (env) {
      // The inner function can be Warp-compiled, but has a with-environment
      // on its scope chain.
      return function() {
        // The compound assignment operator is compiled to JSOp::GetBoundName.
        return prop += 1;
      }
    }
  }

  var count_get = 0;
  var count_has = 0;
  var count_set = 0;

  function proxify(obj) {
    return new Proxy(obj, {
      get(t, pk, r) {
        count_get++;
        return Reflect.get(t, pk, r);
      },
      has(t, pk) {
        count_has++;
        return Reflect.has(t, pk);
      },
      set(t, pk, v, r) {
        count_set++;
        return Reflect.set(t, pk, v, r);
      },
    });
  }

  var count_unscopables = 0;

  var env = {
    get [Symbol.unscopables]() {
      count_unscopables++;
    },
    prop: 0,
  };
  env = proxify(env);

  var inner = outer();
  for (let i = 0; i < 200; ++i) {
    assertEq(inner(), i + 1);
  }

  assertEq(count_unscopables, 200);
  assertEq(count_has, 400);
  assertEq(count_get, 400);
  assertEq(count_set, 200);
}
testCompoundAssign();
