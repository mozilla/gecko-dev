// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Lazy %AsyncIterator.prototype% methods access specified properties only.
info: |
  Iterator Helpers proposal 2.1.6
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/
//

let log;
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args.filter(x => typeof x != 'object').map(x => x.toString())}`);
    return Reflect[key](...args);
  },
});

class TestIterator extends AsyncIterator {
  value = 0;
  async next() {
    if (this.value < 1)
      return new Proxy({done: false, value: this.value++}, handlerProxy);
    return new Proxy({done: true, value: undefined}, handlerProxy);
  }
}

const methods = [
  ['map', x => x],
  ['filter', x => true],
  ['take', 4],
  ['drop', 0],
  ['asIndexedPairs', undefined],
  ['flatMap', async function*(x) { yield x; }],
];

(async () => {
  for (const [method, argument] of methods) {
    log = [];
    const iteratorProxy = new Proxy(new TestIterator(), handlerProxy);
    const iteratorHelper = iteratorProxy[method](argument);

    await iteratorHelper.next();
    await iteratorHelper.next();
    const {done} = await iteratorHelper.next();
    assert.sameValue(done, true);
    assert.sameValue(
      log.join('\n'),
      `get: ${method}
get: next
get: value
get: value
set: value,1
getOwnPropertyDescriptor: value
defineProperty: value
get: then
get: done
get: value
get: value
get: then
get: done`
    );
  }
})();


reportCompare(0, 0);
