// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const values = [];
  const iterator = {
    [Symbol.iterator]: () => ({
      i: 0,
      return() {
        values.push("return");
        return { done: true };
      },
      next() {
        return {
          value: {
            val: this.i++,
            [Symbol.dispose]() {
              values.push(this.val);
            }
          },
          done: false
        }
      }
    })
  }

  function testReturnsHappenAfterDisposal() {
    for (using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  testReturnsHappenAfterDisposal();
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  function* iterator() {
    let i = 0;
    try {
      while (true) {
        yield {
          val: i++,
          [Symbol.dispose]() {
            values.push(this.val);
          }
        };
      }
    } finally {
      values.push("return");
    }
  }

  function testReturnsHappenAfterDisposalWithGeneratorWithFinally() {
    for (using d of iterator()) {
      if (d.val === 3) {
        break;
      }
    }
  }

  testReturnsHappenAfterDisposalWithGeneratorWithFinally();
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  function* iterator() {
    let i = 0;
    while (true) {
      yield {
        val: i++,
        [Symbol.dispose]() {
          values.push(this.val);
        }
      };
    }
  }

  function testReturnsHappenAfterDisposalWithGeneratorWithCustomReturn() {
    const iter = iterator();
    iter.return = function () {
      values.push("return");
      return { done: true };
    };
    for (using d of iter) {
      if (d.val === 3) {
        break;
      }
    }
  }

  testReturnsHappenAfterDisposalWithGeneratorWithCustomReturn();
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  const iterator = {
    [Symbol.iterator]: () => ({
      i: 0,
      return() {
        values.push("return");
        return { done: true };
      },
      next() {
        return {
          value: {
            val: this.i++,
            [Symbol.dispose]() {
              values.push(this.val);
            }
          },
          done: false
        }
      }
    })
  }

  function testReturnsHappenAfterDisposalWithLabels() {
    outer: for (let action of ['continue', 'break']) {
      for (using d of iterator) {
        const toJump = d.val === 3;
        switch (action) {
          case 'continue':
            if (toJump) {
              {
                let a = 0, b = () => a;
                continue outer;
              }
            }
            break;
          case 'break':
            if (toJump) {
              {
                let a = 0, b = () => a;
                break outer;
              }
            }
            break;
        }
      }
    }
  }

  testReturnsHappenAfterDisposalWithLabels();
  assertArrayEq(values, [0,1,2,3,"return",0,1,2,3,"return"]);
}
