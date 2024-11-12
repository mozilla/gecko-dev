// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const duringThrow = [];
function testDisposalDuringThrow() {
  using a = {
    value: "a",
    [Symbol.dispose]() {
      duringThrow.push(this.value);
    }
  };
  throw new Error("err 1");
}
assertThrowsInstanceOf(testDisposalDuringThrow, Error);
assertArrayEq(duringThrow, ["a"]);

const duringBlockAndThrow = [];
function testDisposalDuringBlockAndThrow() {
  using a = {
    value: "a",
    [Symbol.dispose]() {
      duringBlockAndThrow.push(this.value);
    }
  };
  {
    using b = {
      value: "b",
      [Symbol.dispose]() {
        duringBlockAndThrow.push(this.value);
      }
    };
    throw new Error("err 2");
  }
}
assertThrowsInstanceOf(testDisposalDuringBlockAndThrow, Error);
assertArrayEq(duringBlockAndThrow, ["b", "a"]);
