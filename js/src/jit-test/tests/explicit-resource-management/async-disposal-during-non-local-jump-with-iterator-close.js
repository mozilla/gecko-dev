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
            [Symbol.asyncDispose]() {
              return new Promise((resolve) => {
                values.push(this.val);
                resolve();
              });
            }
          },
          done: false
        }
      }
    })
  }

  async function testReturnsHappenAfterAsyncDisposal() {
    for (await using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  testReturnsHappenAfterAsyncDisposal();
  drainJobQueue();
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
          [Symbol.asyncDispose]() {
            return new Promise((resolve) => {
              values.push(this.val);
              resolve();
            });
          }
        };
      }
    } finally {
      values.push("return");
    }
  }

  async function testReturnsHappenAfterAsyncDisposalWithGeneratorWithFinally() {
    for (await using d of iterator()) {
      if (d.val === 3) {
        break;
      }
    }
  }

  testReturnsHappenAfterAsyncDisposalWithGeneratorWithFinally();
  drainJobQueue();
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
            [Symbol.asyncDispose]() {
              return new Promise((resolve) => {
                values.push(this.val);
                resolve();
              });
            }
          },
          done: false
        }
      }
    })
  }

  async function testReturnsHappenAfterDisposalWithLabels() {
    outer: for (let action of ['continue', 'break']) {
      for (await using d of iterator) {
        const toJump = d.val === 3;
        switch (action) {
          case 'continue':
            if (toJump) {
              // The line is to force the creation of an environment object.
              let a = 0, b = () => a;
              values.push(action);
              continue outer;
            }
            break;
          case 'break':
            if (toJump) {
              let a = 0, b = () => a;
              values.push(action);
              break outer;
            }
            break;
        }
      }
    }
  }

  testReturnsHappenAfterDisposalWithLabels();
  drainJobQueue();
  assertArrayEq(values, [0,1,2,'continue', 3,"return",0,1,2,'break',3,"return"]);
}
