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
    $("#fixtures").empty();
    sandbox.restore();
  });

  describe("#loop.panel.requestCallUrl", function() {
    it("should request for a call url", function() {
      var callback = sinon.spy();
      loop.panel.requestCallUrl(callback);

      expect(requests).to.have.length.of(1);

      requests[0].respond(200, {"Content-Type": "application/json"},
                               '{"call_url": "fakeCallUrl"}');
      sinon.assert.calledWithExactly(callback, null, "fakeCallUrl");
    });

    it("should send an error when the request fails", function() {
      var callback = sinon.spy();
      loop.panel.requestCallUrl(callback);

      expect(requests).to.have.length.of(1);

      requests[0].respond(400, {"Content-Type": "application/json"},
                               '{"error": "my error"}');
      sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
        return /HTTP error 400: Bad Request; my error/.test(err.message);
      }));
    });
  });

  describe("loop.panel.NotificationView", function() {
    describe("#render", function() {
      it("should render template with model attribute values", function() {
        var view = new loop.panel.NotificationView({
          el: $("#fixtures"),
          model: new loop.panel.NotificationModel({
            level: "error",
            message: "plop"
          })
        });

        view.render();

        expect(view.$(".message").text()).eql("plop");
      });
    });
  });

  describe("loop.panel.NotificationListView", function() {
    describe("Collection events", function() {
      var coll, testNotif, view;

      beforeEach(function() {
        sandbox.stub(loop.panel.NotificationListView.prototype, "render");
        testNotif = new loop.panel.NotificationModel({
          level: "error",
          message: "plop"
        });
        coll = new loop.panel.NotificationCollection();
        view = new loop.panel.NotificationListView({collection: coll});
      });

      it("should render on notification added to the collection", function() {
        coll.add(testNotif);

        sinon.assert.calledOnce(view.render);
      });

      it("should render on notification removed from the collection",
        function() {
          coll.add(testNotif);
          coll.remove(testNotif);

          sinon.assert.calledTwice(view.render);
        });

      it("should render on collection reset",
        function() {
          coll.reset();

          sinon.assert.calledOnce(view.render);
        });
    });
  });
});
