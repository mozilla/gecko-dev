/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.panel", function() {
  "use strict";

  var sandbox, notifier, fakeXHR, requests = [], savedMozLoop;

  function createTestRouter(fakeDocument) {
    return new loop.panel.PanelRouter({
      notifier: notifier,
      document: fakeDocument
    });
  }

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };
    notifier = {
      clear: sandbox.spy(),
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy()
    };

    navigator.mozLoop = {
      doNotDisturb: true,
      get serverUrl() {
        return "http://example.com";
      },
      getStrings: function() {
        return "{}";
      },
      get locale() {
        return "en-US";
      }
    };
    document.mozL10n.initialize(navigator.mozLoop);
  });

  afterEach(function() {
    delete navigator.mozLoop;
    $("#fixtures").empty();
    sandbox.restore();
  });

  describe("loop.panel.PanelRouter", function() {
    describe("#constructor", function() {
      it("should require a notifier", function() {
        expect(function() {
          new loop.panel.PanelRouter();
        }).to.Throw(Error, /missing required notifier/);
      });

      it("should require a document", function() {
        expect(function() {
          new loop.panel.PanelRouter({notifier: notifier});
        }).to.Throw(Error, /missing required document/);
      });
    });

    describe("constructed", function() {
      var router;

      beforeEach(function() {
        router = createTestRouter({
          hidden: true,
          addEventListener: sandbox.spy()
        });

        sandbox.stub(router, "loadView");
        sandbox.stub(router, "loadReactComponent");
      });

      describe("#home", function() {
        it("should reset the PanelView", function() {
          sandbox.stub(router, "reset");

          router.home();

          sinon.assert.calledOnce(router.reset);
        });
      });

      describe("#reset", function() {
        it("should clear all pending notifications", function() {
          router.reset();

          sinon.assert.calledOnce(notifier.clear);
        });

        it("should load the home view", function() {
          router.reset();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWithExactly(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isComponentOfType(
                value, loop.panel.PanelView);
            }));
        });
      });

      describe("Events", function() {
        it("should listen to document visibility changes", function() {
          var fakeDocument = {
            hidden: true,
            addEventListener: sandbox.spy()
          };

          var router = createTestRouter(fakeDocument);

          sinon.assert.calledOnce(fakeDocument.addEventListener);
          sinon.assert.calledWith(fakeDocument.addEventListener,
                                  "visibilitychange");
        });

        it("should trigger panel:open when the panel document is visible",
          function(done) {
            var router = createTestRouter({
              hidden: false,
              addEventListener: function(name, cb) {
                setTimeout(function() {
                  cb({currentTarget: {hidden: false}});
                }, 0);
              }
            });

            router.once("panel:open", function() {
              done();
            });
          });

        it("should trigger panel:closed when the panel document is hidden",
          function(done) {
            var router = createTestRouter({
              addEventListener: function(name, cb) {
                hidden: true,
                setTimeout(function() {
                  cb({currentTarget: {hidden: true}});
                }, 0);
              }
            });

            router.once("panel:closed", function() {
              done();
            });
          });
      });
    });
  });

  describe("loop.panel.DoNotDisturb", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(loop.panel.DoNotDisturb());
    });

    describe("#handleCheckboxChange", function() {
      beforeEach(function() {
        navigator.mozLoop.doNotDisturb = false;

        var checkbox = TestUtils.findRenderedDOMComponentWithTag(view, "input");
        TestUtils.Simulate.change(checkbox);
      });

      it("should toggle the value of mozLoop.doNotDisturb", function() {
        expect(navigator.mozLoop.doNotDisturb).eql(true);
      });

      it("should update the DnD checkbox value", function() {
        expect(view.getDOMNode().querySelector("input").checked).eql(true);
      });
    });
  });

  describe("loop.panel.CallUrlForm", function() {
    var fakeClient, view;

    beforeEach(function() {
      fakeClient = {};
      view = TestUtils.renderIntoDocument(loop.panel.CallUrlForm({
        notifier: notifier,
        client: fakeClient
      }));
    });

    describe("#handleFormSubmit", function() {
      function submitForm(callerValue) {
        // fill caller field
        TestUtils.Simulate.change(
          TestUtils.findRenderedDOMComponentWithTag(view, "input"), {
            target: {value: callerValue}
          });

        // submit form
        TestUtils.Simulate.submit(
          TestUtils.findRenderedDOMComponentWithTag(view, "form"));
      }

      it.skip("should reset all pending notifications", function() {
        var submit = TestUtils.findRenderedDOMComponentWithTag(view, "button");

        TestUtils.Simulate.click(submit);

        sinon.assert.calledOnce(view.notifier.clear, "clear");
      });

      it("should request a call url to the server", function() {
        fakeClient.requestCallUrl = sandbox.stub();

        submitForm("foo");

        sinon.assert.calledOnce(fakeClient.requestCallUrl);
        sinon.assert.calledWith(fakeClient.requestCallUrl, "foo");
      });

      it("should set the call url form in a pending state", function() {
        fakeClient.requestCallUrl = sandbox.stub();

        submitForm("foo");

        expect(view.state.pending).eql(true);
      });

      it("should clear the pending state when a response is received",
        function(done) {
          fakeClient.requestCallUrl = function(_, cb) {
              cb(null, "fake");
              expect(view.state.pending).eql(false);
              done();
          };

          submitForm("foo");
        });

      it("should notify the user when the operation failed", function(done) {
        fakeClient.requestCallUrl = function(_, cb) {
            cb("fake error");
            sinon.assert.calledOnce(notifier.errorL10n);
            sinon.assert.calledWithExactly(notifier.errorL10n,
                                           "unable_retrieve_url");
            done();
        };

        submitForm("foo");
      });
    });

    describe("#onCallUrlReceived", function() {
      var callUrlData;

      beforeEach(function() {
        callUrlData = {
          call_url: "http://call.me/",
          expiresAt: 1000
        };
      });

      it("should update the text field with the call url", function() {
        var view = new loop.panel.PanelView({notifier: notifier});
        view.render();

        view.onCallUrlReceived(callUrlData);

        expect(view.$("#call-url").val()).eql("http://call.me/");
      });

      it("should reset all pending notifications", function() {
        var view = new loop.panel.PanelView({notifier: notifier}).render();

        view.onCallUrlReceived(callUrlData);

        sinon.assert.calledOnce(view.notifier.clear);
      });
    });

    describe("#render", function() {
      it("should render a DoNotDisturb", function() {
        var renderDnD = sandbox.stub(loop.panel.DoNotDisturb.prototype,
                                     "render");
        var view = new loop.panel.PanelView({notifier: notifier});

        view.render();

        sinon.assert.calledOnce(renderDnD);
      });
    });
  });
});
