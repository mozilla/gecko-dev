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
// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testResource.js</p>";

function test() {
  return Task.spawn(function() {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);
    gcli.addItems(mockCommands.items);

    yield helpers.runTests(options, exports);

    gcli.removeItems(mockCommands.items);
    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}

// <INJECTED SOURCE:END>

// var assert = require('../testharness/assert');

var Promise = require('gcli/util/promise').Promise;
var util = require('gcli/util/util');
var resource = require('gcli/types/resource');
var Status = require('gcli/types/types').Status;


var tempDocument;

exports.setup = function(options) {
  tempDocument = resource.getDocument();
  if (options.window) {
    resource.setDocument(options.window.document);
  }
};

exports.shutdown = function(options) {
  resource.setDocument(tempDocument);
  tempDocument = undefined;
};

exports.testAllPredictions1 = function(options) {
  if (options.isFirefox || options.isNoDom) {
    assert.log('Skipping checks due to firefox document.stylsheets support.');
    return;
  }

  var resource = options.requisition.types.createType('resource');
  return resource.getLookup().then(function(opts) {
    assert.ok(opts.length > 1, 'have all resources');

    return util.promiseEach(opts, function(prediction) {
      return checkPrediction(resource, prediction);
    });
  });
};

exports.testScriptPredictions = function(options) {
  if (options.isFirefox || options.isNoDom) {
    assert.log('Skipping checks due to firefox document.stylsheets support.');
    return;
  }

  var types = options.requisition.types;
  var resource = types.createType({ name: 'resource', include: 'text/javascript' });
  return resource.getLookup().then(function(opts) {
    assert.ok(opts.length > 1, 'have js resources');

    return util.promiseEach(opts, function(prediction) {
      return checkPrediction(resource, prediction);
    });
  });
};

exports.testStylePredictions = function(options) {
  if (options.isFirefox || options.isNoDom) {
    assert.log('Skipping checks due to firefox document.stylsheets support.');
    return;
  }

  var types = options.requisition.types;
  var resource = types.createType({ name: 'resource', include: 'text/css' });
  return resource.getLookup().then(function(opts) {
    assert.ok(opts.length >= 1, 'have css resources');

    return util.promiseEach(opts, function(prediction) {
      return checkPrediction(resource, prediction);
    });
  });
};

exports.testAllPredictions2 = function(options) {
  if (options.isNoDom) {
    assert.log('Skipping checks due to nodom document.stylsheets support.');
    return;
  }
  var types = options.requisition.types;

  var scriptRes = types.createType({ name: 'resource', include: 'text/javascript' });
  return scriptRes.getLookup().then(function(scriptOptions) {
    var styleRes = types.createType({ name: 'resource', include: 'text/css' });
    return styleRes.getLookup().then(function(styleOptions) {
      var allRes = types.createType({ name: 'resource' });
      return allRes.getLookup().then(function(allOptions) {
        assert.is(scriptOptions.length + styleOptions.length,
                  allOptions.length,
                  'split');
      });
    });
  });
};

exports.testAllPredictions3 = function(options) {
  if (options.isNoDom) {
    assert.log('Skipping checks due to nodom document.stylsheets support.');
    return;
  }

  var types = options.requisition.types;
  var res1 = types.createType({ name: 'resource' });
  return res1.getLookup().then(function(options1) {
    var res2 = types.createType('resource');
    return res2.getLookup().then(function(options2) {
      assert.is(options1.length, options2.length, 'type spec');
    });
  });
};

function checkPrediction(res, prediction) {
  var name = prediction.name;
  var value = prediction.value;

  // resources don't need context so cheat and pass in null
  var context = null;
  return res.parseString(name, context).then(function(conversion) {
    assert.is(conversion.getStatus(), Status.VALID, 'status VALID for ' + name);
    assert.is(conversion.value, value, 'value for ' + name);

    assert.is(typeof value.loadContents, 'function', 'resource for ' + name);
    assert.is(typeof value.element, 'object', 'resource for ' + name);

    return Promise.resolve(res.stringify(value, context)).then(function(strung) {
      assert.is(strung, name, 'stringify for ' + name);
    });
  });
}
