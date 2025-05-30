export const description = 'Tests with subcases';

import { makeTestGroup } from '../common/framework/test_group.js';
import { UnitTest } from '../unittests/unit_test.js';

export const g = makeTestGroup(UnitTest);

g.test('skip')
  .paramsSubcasesOnly(u => u.combine('y', [1, 2]))
  .fn(t => {
    t.skip('I skip!');
  });

g.test('pass_warn_fail_skip')
  .params(u =>
    u
      .combine('x', [0, 1, 2, 3]) //
      .beginSubcases()
      .combine('y', [1, 2, 3])
  )
  .fn(t => {
    const { x, y } = t.params;
    if (x + y >= 5) {
      t.fail('I fail!');
    } else if (x + y >= 4) {
      t.warn('I warn!');
    }
    if (x + y === 1 || x + y === 6) {
      t.skip('I skip!');
    }
  });

g.test('DOMException,cases')
  .params(u => u.combine('fail', [false, true]))
  .fn(t => {
    if (t.params.fail) {
      throw new DOMException('Message!', 'Name!');
    }
  });

g.test('DOMException,subcases')
  .paramsSubcasesOnly(u => u.combine('fail', [false, true]))
  .fn(t => {
    if (t.params.fail) {
      throw new DOMException('Message!', 'Name!');
    }
  });
