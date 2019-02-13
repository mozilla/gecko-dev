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
  helpers.runTestModule(exports, "browser_gcli_inputter.js");
}

// var assert = require('../testharness/assert');
var KeyEvent = require('gcli/util/util').KeyEvent;

var latestEvent;
var latestData;

var outputted = function(ev) {
  latestEvent = ev;

  ev.output.promise.then(function() {
    latestData = ev.output.data;
  });
};


exports.setup = function(options) {
  options.requisition.commandOutputManager.onOutput.add(outputted);
};

exports.shutdown = function(options) {
  options.requisition.commandOutputManager.onOutput.remove(outputted);
};

exports.testOutput = function(options) {
  latestEvent = undefined;
  latestData = undefined;

  var terminal = options.terminal;
  if (!terminal) {
    assert.log('Skipping testInputter.testOutput due to lack of terminal.');
    return;
  }

  var focusManager = terminal.focusManager;

  terminal.setInput('tss');

  var ev0 = { keyCode: KeyEvent.DOM_VK_RETURN };
  terminal.onKeyDown(ev0);

  assert.is(terminal.getInputState().typed,
            'tss',
            'terminal should do nothing on RETURN keyDown');
  assert.is(latestEvent, undefined, 'no events this test');
  assert.is(latestData, undefined, 'no data this test');

  var ev1 = { keyCode: KeyEvent.DOM_VK_RETURN };
  return terminal.handleKeyUp(ev1).then(function() {
    assert.ok(latestEvent != null, 'events this test');
    assert.is(latestData.name, 'tss', 'last command is tss');

    assert.is(terminal.getInputState().typed,
              '',
              'terminal should exec on RETURN keyUp');

    assert.ok(focusManager._recentOutput, 'recent output happened');

    var ev2 = { keyCode: KeyEvent.DOM_VK_F1 };
    return terminal.handleKeyUp(ev2).then(function() {
      assert.ok(!focusManager._recentOutput, 'no recent output happened post F1');
      assert.ok(focusManager._helpRequested, 'F1 = help');

      var ev3 = { keyCode: KeyEvent.DOM_VK_ESCAPE };
      return terminal.handleKeyUp(ev3).then(function() {
        assert.ok(!focusManager._helpRequested, 'ESCAPE = anti help');
      });
    });

  });
};
