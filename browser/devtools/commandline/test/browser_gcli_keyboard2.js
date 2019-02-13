/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_keyboard2.js");
}

// var helpers = require('./helpers');

exports.testIncr = function(options) {
  return helpers.audit(options, [
    /*
    // We currently refuse to increment/decrement things with a non-valid
    // status which makes sense for many cases, and is a decent default.
    // However in theory we could do better, these tests are there for then
    {
      setup: 'tsu -70<UP>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -7<UP>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -6<UP>',
      check: { input: 'tsu -5' }
    },
    */
    {
      setup: 'tsu -5<UP>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu -4<UP>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu -3<UP>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu -2<UP>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu -1<UP>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 0<UP>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 1<UP>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 2<UP>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 3<UP>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 4<UP>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 5<UP>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 6<UP>',
      check: { input: 'tsu 9' }
    },
    {
      setup: 'tsu 7<UP>',
      check: { input: 'tsu 9' }
    },
    {
      setup: 'tsu 8<UP>',
      check: { input: 'tsu 9' }
    },
    {
      setup: 'tsu 9<UP>',
      check: { input: 'tsu 10' }
    },
    {
      setup: 'tsu 10<UP>',
      check: { input: 'tsu 10' }
    }
    /*
    // See notes above
    {
      setup: 'tsu 100<UP>',
      check: { input: 'tsu 10' }
    }
    */
  ]);
};
