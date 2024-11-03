// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const duringThrow = [];
function testDisposalInSwitchCaseDuringThrow(cs) {
  switch (cs) {
    case 1:
      using a = {
        value: "a",
        [Symbol.dispose]() {
          duringThrow.push(this.value);
        }
      };
      throw new Error("err 1");
  }
}
assertThrowsInstanceOf(() => testDisposalInSwitchCaseDuringThrow(1), Error);
assertArrayEq(duringThrow, ["a"]);

const duringThrow2 = [];
function testDisposalInSwitchCaseDuringThrow2(cs) {
  switch (cs) {
    case 1:
      using a = {
        value: "a",
        [Symbol.dispose]() {
          duringThrow.push(this.value);
        }
      };
      throw new Error("err 1");
    case 100:
      using b = {
        value: "b",
        [Symbol.dispose]() {
          duringThrow2.push(this.value);
        }
      };
      throw new Error("err 100");
  }
}
assertThrowsInstanceOf(() => testDisposalInSwitchCaseDuringThrow2(100), Error);
assertArrayEq(duringThrow2, ["b"]);
