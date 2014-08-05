/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.webapp", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      sandbox,
      notifier;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    notifier = {
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy(),
    };
    loop.config.pendingCallTimeout = 1000;
  });

  afterEach(function() {
    sandbox.restore();
    delete loop.config.pendingCallTimeout;
  });

  describe("#init", function() {
    var WebappRouter;

    beforeEach(function() {
      WebappRouter = loop.webapp.WebappRouter;
      sandbox.stub(WebappRouter.prototype, "navigate");
    });

    afterEach(function() {
      Backbone.history.stop();
    });

    it("should navigate to the unsupportedDevice route if the sdk detects " +
       "the device is running iOS", function() {
      sandbox.stub(loop.webapp.WebappHelper.prototype, "isIOS").returns(true);

      loop.webapp.init();

      sinon.assert.calledOnce(WebappRouter.prototype.navigate);
      sinon.assert.calledWithExactly(WebappRouter.prototype.navigate,
                                     "unsupportedDevice", {trigger: true});
    });

    it("should navigate to the unsupportedBrowser route if the sdk detects " +
       "the browser is unsupported", function() {
      sandbox.stub(loop.webapp.WebappHelper.prototype, "isIOS").returns(false);
      sandbox.stub(window.OT, "checkSystemRequirements").returns(false);

      loop.webapp.init();

      sinon.assert.calledOnce(WebappRouter.prototype.navigate);
      sinon.assert.calledWithExactly(WebappRouter.prototype.navigate,
                                     "unsupportedBrowser", {trigger: true});
    });
  });

  describe("WebappRouter", function() {
    var router, conversation;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel({}, {
        sdk: {},
        pendingCallTimeout: 1000
      });
      sandbox.stub(loop.webapp.WebappRouter.prototype, "loadReactComponent");
      router = new loop.webapp.WebappRouter({
        helper: {},
        conversation: conversation,
        notifier: notifier
      });
      sandbox.stub(router, "loadView");
      sandbox.stub(router, "navigate");
    });

    describe("#startCall", function() {
      it("should navigate back home if session token is missing", function() {
        router.startCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "home");
      });

      it("should notify the user if session token is missing", function() {
        router.startCall();

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "missing_conversation_info");
      });

      it("should navigate to call/ongoing/:token if session token is available",
        function() {
          conversation.set("loopToken", "fake");

          router.startCall();

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "call/ongoing/fake");
        });
    });

    describe("#endCall", function() {
      it("should navigate to home if session token is unset", function() {
        router.endCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "home");
      });

      it("should navigate to call/:token if session token is set", function() {
        conversation.set("loopToken", "fake");

        router.endCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fake");
      });
    });

    describe("Routes", function() {
      describe("#home", function() {
        it("should load the HomeView", function() {
          router.home();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(loop.webapp.HomeView));
        });
      });

      describe("#expired", function() {
        it("should load the CallUrlExpiredView view", function() {
          router.expired();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWith(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isDescriptorOfType(
                value, loop.webapp.CallUrlExpiredView);
            }));
        });
      });

      describe("#initiate", function() {
        it("should set the token on the conversation model", function() {
          router.initiate("fakeToken");

          expect(conversation.get("loopToken")).eql("fakeToken");
        });

        it("should load the ConversationFormView", function() {
          router.initiate("fakeToken");

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWithExactly(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isDescriptorOfType(
                value, loop.webapp.ConversationFormView);
            }));
        });

        // https://bugzilla.mozilla.org/show_bug.cgi?id=991118
        it("should terminate any ongoing call session", function() {
          sinon.stub(conversation, "endSession");
          conversation.set("ongoing", true);

          router.initiate("fakeToken");

          sinon.assert.calledOnce(conversation.endSession);
        });
      });

      describe("#loadConversation", function() {
        it("should load the ConversationView if session is set", function() {
          conversation.set("sessionId", "fakeSessionId");

          router.loadConversation();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWith(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isDescriptorOfType(
                value, loop.shared.views.ConversationView);
            }));
        });

        it("should navigate to #call/{token} if session isn't ready",
          function() {
            router.loadConversation("fakeToken");

            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
          });
      });

      describe("#unsupportedDevice", function() {
        it("should load the UnsupportedDeviceView", function() {
          router.unsupportedDevice();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(sharedViews.UnsupportedDeviceView));
        });
      });

      describe("#unsupportedBrowser", function() {
        it("should load the UnsupportedBrowserView", function() {
          router.unsupportedBrowser();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(sharedViews.UnsupportedBrowserView));
        });
      });
    });

    describe("Events", function() {
      var fakeSessionData;

      beforeEach(function() {
        fakeSessionData = {
          sessionId:    "sessionId",
          sessionToken: "sessionToken",
          apiKey:       "apiKey"
        };
        conversation.set("loopToken", "fakeToken");
      });

      it("should navigate to call/ongoing/:token once call session is ready",
        function() {
          conversation.trigger("session:ready");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ongoing/fakeToken");
        });

      it("should navigate to call/{token} when conversation ended", function() {
        conversation.trigger("session:ended");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
      });

      it("should navigate to call/{token} when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
      });

      it("should navigate to call/{token} when network disconnects",
        function() {
          conversation.trigger("session:network-disconnected");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
        });

      it("should navigate to call/expired when a session:expired event is " +
         "received", function() {
        conversation.trigger("session:expired");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWith(router.navigate, "/call/expired");
      });
    });
  });

  describe("ConversationFormView", function() {
    var conversation;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel({}, {
        sdk: {},
        pendingCallTimeout: 1000});
    });

    describe("#initialize", function() {
      it("should require a conversation option", function() {
        expect(function() {
          new loop.webapp.WebappRouter();
        }).to.Throw(Error, /missing required conversation/);
      });
    });

    describe("#initiate", function() {
      var conversation, initiate, view, fakeSubmitEvent, requestCallUrlInfo;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {
          sdk: {},
          pendingCallTimeout: 1000
        });

        fakeSubmitEvent = {preventDefault: sinon.spy()};
        initiate = sinon.stub(conversation, "initiate");

        var standaloneClientStub = {
          requestCallUrlInfo: function(token, cb) {
            cb(null, {urlCreationDate: 0});
          },
          settings: {baseServerUrl: loop.webapp.baseServerUrl}
        }

        view = React.addons.TestUtils.renderIntoDocument(
            loop.webapp.ConversationFormView({
              model: conversation,
              notifier: notifier,
              client: standaloneClientStub
            })
        );
      });

      it("should start the conversation establishment process", function() {
        var button = view.getDOMNode().querySelector("button");
        React.addons.TestUtils.Simulate.click(button);

        sinon.assert.calledOnce(initiate);
        sinon.assert.calledWith(initiate, sinon.match(function (value) {
          return !!value.outgoing &&
            (value.client.settings.baseServerUrl === loop.webapp.baseServerUrl)
        }, "outgoing: true && correct baseServerUrl"));
      });

      it("should disable current form once session is initiated", function() {
        conversation.set("loopToken", "fake");

        var button = view.getDOMNode().querySelector("button");
        React.addons.TestUtils.Simulate.click(button);

        expect(button.disabled).to.eql(true);
      });

      it("should set state.urlCreationDateString to a locale date string",
         function() {
        // wrap in a jquery object because text is broken up
        // into several span elements
        var date = new Date(0);
        var options = {year: "numeric", month: "long", day: "numeric"};
        var timestamp = date.toLocaleDateString(navigator.language, options);

        expect(view.state.urlCreationDateString).to.eql(timestamp);
      });

    });

    describe("Events", function() {
      var conversation, view, StandaloneClient, requestCallUrlInfo;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({
          loopToken: "fake"
        }, {
          sdk: {},
          pendingCallTimeout: 1000
        });

        sandbox.spy(conversation, "listenTo");
        requestCallUrlInfo = sandbox.stub();

        view = React.addons.TestUtils.renderIntoDocument(
            loop.webapp.ConversationFormView({
              model: conversation,
              notifier: notifier,
              client: {requestCallUrlInfo: requestCallUrlInfo}
            })
          );
      });

      it("should call requestCallUrlInfo", function() {
        sinon.assert.calledOnce(requestCallUrlInfo);
        sinon.assert.calledWithExactly(requestCallUrlInfo,
                                       sinon.match.string,
                                       sinon.match.func);
      });

      it("should listen for session:error events", function() {
        sinon.assert.calledOnce(conversation.listenTo);
        sinon.assert.calledWithExactly(conversation.listenTo, conversation,
                                       "session:error", sinon.match.func);
      });

      it("should trigger a notication when a session:error model event is " +
         " received", function() {
        conversation.trigger("session:error", "tech error");

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "unable_retrieve_call_info");
      });
    });
  });

  describe("PromoteFirefoxView", function() {
    describe("#render", function() {
      it("should not render when using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(loop.webapp.PromoteFirefoxView({
          helper: {isFirefox: function() { return true; }}
        }));

        expect(comp.getDOMNode().querySelectorAll("h3").length).eql(0);
      });

      it("should render when not using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(loop.webapp.PromoteFirefoxView({
          helper: {isFirefox: function() { return false; }}
        }));

        expect(comp.getDOMNode().querySelectorAll("h3").length).eql(1);
      });
    });
  });

  describe("WebappHelper", function() {
    var helper;

    beforeEach(function() {
      helper = new loop.webapp.WebappHelper();
    });

    describe("#isIOS", function() {
      it("should detect iOS", function() {
        expect(helper.isIOS("iPad")).eql(true);
        expect(helper.isIOS("iPod")).eql(true);
        expect(helper.isIOS("iPhone")).eql(true);
        expect(helper.isIOS("iPhone Simulator")).eql(true);
      });

      it("shouldn't detect iOS with other platforms", function() {
        expect(helper.isIOS("MacIntel")).eql(false);
      });
    });

    describe("#isFirefox", function() {
      it("should detect Firefox", function() {
        expect(helper.isFirefox("Firefox")).eql(true);
        expect(helper.isFirefox("Gecko/Firefox")).eql(true);
        expect(helper.isFirefox("Firefox/Gecko")).eql(true);
        expect(helper.isFirefox("Gecko/Firefox/Chuck Norris")).eql(true);
      });

      it("shouldn't detect Firefox with other platforms", function() {
        expect(helper.isFirefox("Opera")).eql(false);
      });
    });
  });
});
