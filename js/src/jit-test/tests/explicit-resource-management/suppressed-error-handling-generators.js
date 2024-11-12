// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const values = [];
  const errorsToThrow = [new Error("test1"), new Error("test2")];
  function* gen() {
    using d = {
      value: "d",
      [Symbol.dispose]() {
        values.push(this.value);
      }
    }
    yield "a";
    yield "b";
    using c = {
      value: "c",
      [Symbol.dispose]() {
        values.push(this.value);
        throw errorsToThrow[0]; // This error will suppress the error thrown below.
      }
    }
    throw errorsToThrow[1]; // This error will be suppressed during disposal.
  }
  assertSuppressionChain(() => {
    let x = gen();
    values.push(x.next().value);
    values.push(x.next().value);
    x.next();
  }, errorsToThrow);

  assertArrayEq(values, ["a", "b", "c", "d"]);
}

{
  const values = [];
  const errorsToThrow = [new Error("test1"), new Error("test2")];
  function* gen() {
    using c = {
      value: "c",
      [Symbol.dispose]() {
        values.push(this.value);
        throw errorsToThrow[0];
      }
    }
    yield "a";
    yield "b";
    return;
  }

  assertSuppressionChain(() => {
    let x = gen();
    values.push(x.next().value);
    x.throw(errorsToThrow[1]); // This error will be suppressed during disposal.
  }, errorsToThrow);

  assertArrayEq(values, ["a", "c"]);
}
