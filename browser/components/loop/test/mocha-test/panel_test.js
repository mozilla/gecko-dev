/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.panel", function() {
  "use strict";

  var sandbox, fakeXHR, requests = [];

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#requestCallUrl", function() {
    it("should request for a call url", function() {
      var callback = sinon.spy();
      loop.panel.requestCallUrl(callback);

      expect(requests).to.have.length.of(1);

      requests[0].respond(200, {"Content-Type": "application/json"},
                               '{"call_url": "foo"}');
      sinon.assert.calledWithExactly(callback, null, {call_url: "foo"});
    });

    it("should send an error when the request fails", function() {
      var callback = sinon.spy();
      loop.panel.requestCallUrl(callback);

      expect(requests).to.have.length.of(1);

      requests[0].respond(400, {"Content-Type": "application/json"},
                               '{"error": "foo"}');
      sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Failed HTTP request: 400.*foo/.test(err.message);
      }));
    });
  });
});
