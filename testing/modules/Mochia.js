/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Define Mochia's helpers on the given scope.
 *
 * @param {object} scope
 *        The `globalThis` of the running test, where `add_task` is defined.
 */
const Mochia = (function () {
  /**
   * The context of each test suite.
   */
  class Context {
    static #stack = [];

    static current() {
      return Context.#stack.at(-1);
    }

    static push(ctx) {
      Context.#stack.push(ctx);
    }

    static pop() {
      Context.#stack.pop();
    }

    constructor() {
      this.description = [];
      this.beforeEach = [];
      this.afterEach = [];
    }

    clone() {
      const newCtx = new Context();
      newCtx.description.push(...this.description);
      newCtx.beforeEach.push(...this.beforeEach);
      newCtx.afterEach.push(...this.afterEach);
      return newCtx;
    }
  }

  Context.push(new Context());

  let _testScope = null;

  /**
   * @typedef {void|Promise<void>} MaybePromise
   *
   * Either undefined or a Promise that resolves to undefined.
   */

  const MochiaImpl = {
    /**
     * Describe a new test suite, which is a scoped environment for running setup
     * and teardown.
     *
     * @param {string} desc
     *        A description of the test suite.
     *
     * @param {function(): MaybePromise} suite
     *        The test suite
     */
    async describe(desc, suite) {
      const ctx = Context.current().clone();
      ctx.description.push(desc);

      Context.push(ctx);

      const p = suite();
      if (p?.then) {
        await p;
      }

      Context.pop();
    },

    /**
     * Register a setup funciton to run before each test.
     *
     * Multiple functions can be registered with `beforeEach` and they will be run
     * in order before each test in the suite and the suites nested inside of it.
     *
     * @param {function(): MaybePromise} setupFn
     *        The setup function. If this function returns a `Promise` it will be
     *        awaited.
     */
    beforeEach(setupFn) {
      Context.current().beforeEach.push(setupFn);
    },

    /**
     * Register a tear down function to run at the end of each test.
     *
     * Multiple functions can be registered with `afterEach` and they will run in
     * reverse order after each test in the suite and the suites nested inside of it.
     *
     * @param {function(): MaybePromise} tearDownFn
     *        The tear down function. If this function returns a `Promise` it will
     *        be awaited.
     */
    afterEach(tearDownFn) {
      Context.current().afterEach.push(tearDownFn);
    },

    /**
     * Register a test function.
     *
     * The test will be registered via `add_task`. Each setup function registered
     * before this function call will be called in order before the actual test
     * and each teardown function before this function will be called in reverse
     * order after the actual test.
     *
     * @param {string} desc
     *        A description of the test. This is logged at the start of the test.
     *
     * @param {function(): MaybePromise} testFn
     *        The test function. If this function returns a `Promise` it will be
     *        awaited.
     *
     * @returns {any}
     *          The result of calling `add_task` with the wrapped function.
     */
    it(desc, testFn) {
      return _testScope.add_task(MochiaImpl.wrap(desc, testFn));
    },

    /**
     * Register a test that will be the only test run.
     *
     * @param {string} desc
     *        A description of the test. This is logged at the start of the test.
     *
     * @param {function(): MaybePromise} testFn
     *        The test function. If this function returns a `Promise` it will be
     *        awaited.
     */
    only(desc, testFn) {
      MochiaImpl.it(desc, testFn).only();
    },

    /**
     * Register a test that will be skipped.
     *
     * @param {string} desc
     *        A description of the test. This is logged at the start of the test.
     *
     * @param {function(): MaybePromise} testFn
     *        The test function. If this function returns a `Promise` it will be
     *        awaited.
     */
    skip(desc, testFn) {
      MochiaImpl.it(desc, testFn).skip();
    },

    /**
     * Register a test that will be skipped if the provided predicate evaluates to
     * a truthy value.
     *
     * @param {string} desc
     *        A description of the test. This is logged at the start of the test.
     *
     * @param {function(): boolean} skipFn
     *        A predicate that will be called by the test harness to determine
     *        whether or not the test should be skipped.
     *
     * @param {function(): MaybePromise} testFn
     *        The test function. If this function returns a `Promise` it will be
     *        awaited.
     *
     * @returns {any}
     *          The result of calling `add_task` with the wrapped function.
     */
    skipIf(desc, skipFn, testFn) {
      return _testScope.add_task(
        { skip_if: skipFn },
        MochiaImpl.wrap(desc, testFn)
      );
    },

    /**
     * Wrap `fn` so that all the setup functions declared with `beforeEach` are
     * called before it and all the teardown functions declared with `afterEach`
     * are called after.
     *
     * @param {string} desc
     *        A description of the test.
     *
     * @param {function(): MaybePromise} fn
     *        The function to wrap.
     *
     * @returns {function(): Promise<void>}
     *          The wrapped function.
     */
    wrap(desc, fn) {
      const ctx = Context.current().clone();
      const name = [...ctx.description, desc].join(" / ");

      // This is a hack to give the function an implicit name.
      const wrapper = {
        [name]: async () => {
          _testScope.info(name);

          for (const before of ctx.beforeEach) {
            const p = before();
            if (p?.then) {
              await p;
            }
          }

          {
            const p = fn();
            if (p?.then) {
              await p;
            }
          }

          for (let i = ctx.afterEach.length - 1; i >= 0; i--) {
            const after = ctx.afterEach[i];
            const p = after();
            if (p?.then) {
              await p;
            }
          }
        },
      };

      return wrapper[name];
    },
  };

  Object.defineProperties(MochiaImpl.it, {
    only: {
      configurable: false,
      value: MochiaImpl.only,
    },
    skip: {
      configurable: false,
      value: MochiaImpl.skip,
    },
    skipIf: {
      configurable: false,
      get: MochiaImpl.skipIf,
    },
  });

  return function (scope) {
    _testScope = scope;

    Object.assign(_testScope, {
      describe: MochiaImpl.describe,
      beforeEach: MochiaImpl.beforeEach,
      afterEach: MochiaImpl.afterEach,
      it: MochiaImpl.it,
    });
  };
})();
