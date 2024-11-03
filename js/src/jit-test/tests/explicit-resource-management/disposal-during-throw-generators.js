// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const values = [];

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
    }
  }
  throw new Error("err");
}

assertThrowsInstanceOf(() => {
  let x = gen();
  values.push(x.next().value);
  values.push(x.next().value);
  x.next();
}, Error);
assertArrayEq(values, ["a", "b", "c", "d"]);

const values2 = [];
function* gen2() {
  using c = {
    value: "c",
    [Symbol.dispose]() {
      values2.push(this.value);
    }
  }
  yield "a";
  yield "b";
  return;
}

assertThrowsInstanceOf(() => {
  let x = gen2();
  values2.push(x.next().value);
  x.throw(new Error("err"));
}, Error);

assertArrayEq(values2, ["a", "c"]);
