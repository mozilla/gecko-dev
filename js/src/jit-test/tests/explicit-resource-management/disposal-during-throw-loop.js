// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const duringThrow = [];
function testDisposalInLoopWithThrow() {
  const disposables = [
    {
      val: "a",
      [Symbol.dispose]() {
        duringThrow.push(this.val);
      }
    },
    {
      val: "b",
      [Symbol.dispose]() {
        duringThrow.push(this.val);
      }
    },
    {
      val: "c",
      [Symbol.dispose]() {
        duringThrow.push(this.val);
      }
    }
  ];
  for (using d of disposables) {
    if (d.val === "b") {
      throw new Error("err 1");
    }
  }
}
assertThrowsInstanceOf(testDisposalInLoopWithThrow, Error);
assertArrayEq(duringThrow, ["a", "b"]);

const duringThrow2 = [];
function testDisposalInLoopWithIteratorClose() {
  const disposables = [
    {
      val: "a",
      [Symbol.dispose]() {
        duringThrow2.push(this.val);
      }
    },
    {
      val: "b",
      [Symbol.dispose]() {
        duringThrow2.push(this.val);
      }
    },
    {
      val: "c",
      [Symbol.dispose]() {
        duringThrow2.push(this.val);
      }
    }
  ];
  const iterable = {
    [Symbol.iterator]() {
      let i = 0;
      return {
        next() {
          if (i === disposables.length) {
            return { done: true };
          }
          return { value: disposables[i++], done: false };
        },
        return() {
          duringThrow2.push("return");
          return { done: true };
        }
      }
    }
  }
  for (using d of iterable) {
    if (d.val === "b") {
      throw new Error("err 2");
    }
  }
}
assertThrowsInstanceOf(testDisposalInLoopWithIteratorClose, Error);
assertArrayEq(duringThrow2, ["a", "b", "return"]);
