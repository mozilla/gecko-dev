// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const values = [];
  const iterator = {
    [Symbol.asyncIterator]: () => ({
      i: 0,
      async return() {
        values.push("return");
        return { done: true };
      },
      async next() {
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

  async function testAsyncReturnsHappenAfterDisposal() {
    for await (using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  testAsyncReturnsHappenAfterDisposal();
  drainJobQueue();
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  async function* iterator() {
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
      await new Promise((resolve) => {
        values.push("return");
        resolve();
      });
    }
  }

  async function testReturnsHappenAfterAsyncDisposalWithGeneratorWithFinally() {
    for await (using d of iterator()) {
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
    [Symbol.asyncIterator]: () => ({
      i: 0,
      async return() {
        values.push("return");
        return { done: true };
      },
      async next() {
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

  async function testReturnsHappenAfterDisposalWithLabels() {
    outer: for (let action of ['continue', 'break']) {
      for await (using d of iterator) {
        const toJump = d.val === 3;
        switch (action) {
          case 'continue':
            if (toJump)  {
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
