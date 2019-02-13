/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require('chrome');
const { Loader } = require('sdk/test/loader');
const { gc } = require("sdk/test/memory");

exports['test adding item'] = function*(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let item = {};

  add(items, item);

  yield gc();

  assert.ok(has(items, item), 'the item is in the weak set');

  loader.unload();
};

exports['test remove item'] = function*(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let item = {};

  add(items, item);

  remove(items, item);

  yield gc();

  assert.ok(!has(items, item), 'the item is not in weak set');

  loader.unload();
};

exports['test iterate'] = function*(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let addedItems = [{}, {}];

  add(items, addedItems[0]);
  add(items, addedItems[1]);
  add(items, addedItems[0]); // weak set shouldn't add this twice

  yield gc();
  let count = 0;

  for (let item of iterator(items)) {
    assert.equal(item, addedItems[count],
      'item in the expected order');

    count++;
  }

  assert.equal(count, 2, 'items in the expected number');
  loader.unload();
};

exports['test clear'] = function*(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let addedItems = [{}, {}];

  add(items, addedItems[0]);
  add(items, addedItems[1]);

  clear(items)

  yield gc();
  let count = 0;

  for (let item of iterator(items)) {
    assert.fail('the loop should not be executed');
  }

  assert.equal(count, 0, 'no items in the weak set');
  loader.unload();
};

exports['test adding item without reference'] = function*(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};

  add(items, {});

  yield gc();
  let count = 0;

  for (let item of iterator(items)) {
    assert.fail('the loop should not be executed');
  }

  assert.equal(count, 0, 'no items in the weak set');

  loader.unload();
};

exports['test adding non object or null item'] = function(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};

  assert.throws(() => {
    add(items, 'foo');
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, 0);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, undefined);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, null);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, true);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');
};

exports['test adding to non object or null item'] = function(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let item = {};

  assert.throws(() => {
    add('foo', item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(0, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(undefined, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(null, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(true, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');
};

require('sdk/test').run(exports);
