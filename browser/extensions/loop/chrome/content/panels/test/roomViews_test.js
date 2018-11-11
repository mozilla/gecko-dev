/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.roomViews", function() {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedViews = loop.shared.views;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;

  var sandbox,
      dispatcher,
      roomStore,
      activeRoomStore,
      remoteCursorStore,
      view;
  var clock, fakeWindow, requestStubs;
  var favicon = "data:image/x-icon;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();

    LoopMochaUtils.stubLoopRequest(requestStubs = {
      GetAudioBlob: sinon.spy(function() {
        return new Blob([new ArrayBuffer(10)], { type: "audio/ogg" });
      }),
      GetLoopPref: sinon.stub(),
      GetSelectedTabMetadata: sinon.stub().returns({
        favicon: favicon,
        previews: [],
        title: ""
      }),
      OpenURL: sinon.stub(),
      "Rooms:Get": sinon.stub().returns({
        roomToken: "fakeToken",
        roomName: "fakeName",
        decryptedContext: {
          roomName: "fakeName",
          urls: []
        }
      }),
      "Rooms:Update": sinon.stub().returns(null),
      TelemetryAddValue: sinon.stub(),
      SetLoopPref: sandbox.stub(),
      GetDoNotDisturb: function() { return false; }
    });

    dispatcher = new loop.Dispatcher();

    clock = sandbox.useFakeTimers();

    fakeWindow = {
      close: sinon.stub(),
      document: {},
      addEventListener: function() {},
      removeEventListener: function() {},
      setTimeout: function(callback) { callback(); }
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    // XXX These stubs should be hoisted in a common file
    // Bug 1040968
    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });

    activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
      sdkDriver: {}
    });
    roomStore = new loop.store.RoomStore(dispatcher, {
      constants: {},
      activeRoomStore: activeRoomStore
    });
    remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
      sdkDriver: {}
    });
    var textChatStore = new loop.store.TextChatStore(dispatcher, {
      sdkDriver: {}
    });

    loop.store.StoreMixin.register({
      textChatStore: textChatStore
    });

    sandbox.stub(dispatcher, "dispatch");
  });

  afterEach(function() {
    sandbox.restore();
    clock.restore();
    loop.shared.mixins.setRootObject(window);
    LoopMochaUtils.restore();
    view = null;
  });

  describe("ActiveRoomStoreMixin", function() {
    it("should merge initial state", function() {
      var TestView = React.createClass({
        mixins: [loop.roomViews.ActiveRoomStoreMixin],
        getInitialState: function() {
          return { foo: "bar" };
        },
        render: function() { return React.DOM.div(); }
      });

      var testView = TestUtils.renderIntoDocument(
        React.createElement(TestView, {
          roomStore: roomStore
        }));

      var expectedState = _.extend({ foo: "bar", savingContext: false },
        activeRoomStore.getInitialStoreState());

      expect(testView.state).eql(expectedState);
    });

    it("should listen to store changes", function() {
      var TestView = React.createClass({
        mixins: [loop.roomViews.ActiveRoomStoreMixin],
        render: function() { return React.DOM.div(); }
      });
      var testView = TestUtils.renderIntoDocument(
        React.createElement(TestView, {
          roomStore: roomStore
        }));

      activeRoomStore.setStoreState({ roomState: ROOM_STATES.READY });

      expect(testView.state.roomState).eql(ROOM_STATES.READY);
    });
  });

  describe("RoomFailureView", function() {
    var fakeAudio;

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        failureReason: props && props.failureReason || FAILURE_DETAILS.UNKNOWN
      });
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.RoomFailureView, props));
    }

    beforeEach(function() {
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);
    });

    it("should render the FailureInfoView", function() {
      view = mountTestComponent();

      TestUtils.findRenderedComponentWithType(view,
        loop.roomViews.FailureInfoView);
    });

    it("should dispatch a JoinRoom action when the rejoin call button is pressed", function() {
      view = mountTestComponent();

      var rejoinBtn = view.getDOMNode().querySelector(".btn-rejoin");

      React.addons.TestUtils.Simulate.click(rejoinBtn);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.JoinRoom());
    });

    it("should render retry button when an ice failure is dispatched", function() {
      view = mountTestComponent({
        failureReason: FAILURE_DETAILS.ICE_FAILED
      });

      var retryBtn = view.getDOMNode().querySelector(".btn-rejoin");

      expect(retryBtn.textContent).eql("retry_call_button");
    });

    it("should play a failure sound, once", function() {
      view = mountTestComponent();

      sinon.assert.calledOnce(requestStubs.GetAudioBlob);
      sinon.assert.calledWithExactly(requestStubs.GetAudioBlob, "failure");
      sinon.assert.calledOnce(fakeAudio.play);
      expect(fakeAudio.loop).to.equal(false);
    });
  });

  describe("DesktopRoomInvitationView", function() {
    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        facebookEnabled: false,
        roomData: { roomUrl: "http://invalid" },
        savingContext: false,
        show: true,
        showEditContext: false
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomInvitationView, props));
    }

    it("should dispatch an EmailRoomUrl with no description" +
       " for rooms without context when the email button is pressed",
      function() {
        view = mountTestComponent();

        var emailBtn = view.getDOMNode().querySelector(".btn-email");

        React.addons.TestUtils.Simulate.click(emailBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.EmailRoomUrl({
            roomUrl: "http://invalid",
            roomDescription: undefined,
            from: "conversation"
          }));
      });

    it("should dispatch an EmailRoomUrl with a domain name description for rooms with context",
      function() {
        var url = "http://invalid";
        view = mountTestComponent({
          roomData: {
            roomUrl: url,
            roomContextUrls: [{ location: "http://www.mozilla.com/" }]
          }
        });

        var emailBtn = view.getDOMNode().querySelector(".btn-email");

        React.addons.TestUtils.Simulate.click(emailBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.EmailRoomUrl({
            roomUrl: url,
            roomDescription: "www.mozilla.com",
            from: "conversation"
          }));
      });

    it("should not display the Facebook Share button when it is disabled in prefs",
      function() {
        view = mountTestComponent({
          facebookEnabled: false
        });

        expect(view.getDOMNode().querySelectorAll(".btn-facebook"))
          .to.have.length.of(0);
      });

    it("should display the Facebook Share button only when it is enabled in prefs",
      function() {
        view = mountTestComponent({
          facebookEnabled: true
        });

        expect(view.getDOMNode().querySelectorAll(".btn-facebook"))
          .to.have.length.of(1);
      });

    it("should dispatch a FacebookShareRoomUrl action when the facebook button is clicked",
      function() {
        var url = "http://invalid";
        view = mountTestComponent({
          facebookEnabled: true,
          roomData: {
            roomUrl: url
          }
        });

        var facebookBtn = view.getDOMNode().querySelector(".btn-facebook");

        React.addons.TestUtils.Simulate.click(facebookBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.FacebookShareRoomUrl({
            from: "conversation",
            roomUrl: url
          }));
    });

    describe("Copy Button", function() {
      beforeEach(function() {
        view = mountTestComponent();
      });

      it("should dispatch a CopyRoomUrl action when the copy button is pressed", function() {
        var copyBtn = view.getDOMNode().querySelector(".btn-copy");
        React.addons.TestUtils.Simulate.click(copyBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch, new sharedActions.CopyRoomUrl({
          roomUrl: "http://invalid",
          from: "conversation"
        }));
      });

      it("should change the text when the url has been copied", function() {
        var copyBtn = view.getDOMNode().querySelector(".btn-copy");
        React.addons.TestUtils.Simulate.click(copyBtn);

        expect(copyBtn.textContent).eql("invite_copied_link_button");
      });

      it("should keep the text for a while after the url has been copied", function() {
        var copyBtn = view.getDOMNode().querySelector(".btn-copy");
        React.addons.TestUtils.Simulate.click(copyBtn);
        clock.tick(loop.roomViews.DesktopRoomInvitationView.TRIGGERED_RESET_DELAY / 2);

        expect(copyBtn.textContent).eql("invite_copied_link_button");
      });

      it("should reset the text a bit after the url has been copied", function() {
        var copyBtn = view.getDOMNode().querySelector(".btn-copy");
        React.addons.TestUtils.Simulate.click(copyBtn);
        clock.tick(loop.roomViews.DesktopRoomInvitationView.TRIGGERED_RESET_DELAY);

        expect(copyBtn.textContent).eql("invite_copy_link_button");
      });

      it("should reset the text after the url has been copied then mouse over another button", function() {
        var copyBtn = view.getDOMNode().querySelector(".btn-copy");
        React.addons.TestUtils.Simulate.click(copyBtn);
        var emailBtn = view.getDOMNode().querySelector(".btn-email");
        React.addons.TestUtils.Simulate.mouseOver(emailBtn);

        expect(copyBtn.textContent).eql("invite_copy_link_button");
      });
    });
  });

  describe("DesktopRoomConversationView", function() {
    var onCallTerminatedStub;

    beforeEach(function() {
      LoopMochaUtils.stubLoopRequest({
        GetLoopPref: function(prefName) {
          if (prefName === "contextInConversations.enabled") {
            return true;
          }
          return "test";
        }
      });
      onCallTerminatedStub = sandbox.stub();

      activeRoomStore.setStoreState({ roomUrl: "http://invalid " });
    });

    function mountTestComponent(props) {
      props = _.extend({
        chatWindowDetached: false,
        cursorStore: remoteCursorStore,
        dispatcher: dispatcher,
        facebookEnabled: false,
        roomStore: roomStore,
        onCallTerminated: onCallTerminatedStub
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomConversationView, props));
    }

    it("should NOT show the context menu on right click", function() {
      var prevent = sandbox.stub();
      view = mountTestComponent();
      TestUtils.Simulate.contextMenu(
        view.getDOMNode(),
        { preventDefault: prevent }
      );
      sinon.assert.calledOnce(prevent);
    });

    it("should dispatch a setMute action when the audio mute button is pressed",
      function() {
        view = mountTestComponent();

        view.setState({ audioMuted: true });

        var muteBtn = view.getDOMNode().querySelector(".btn-mute-audio");

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", true));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "audio"));
      });

    it("should dispatch a setMute action when the video mute button is pressed",
      function() {
        view = mountTestComponent();

        view.setState({ videoMuted: false });

        var muteBtn = view.getDOMNode().querySelector(".btn-mute-video");

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", false));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "video"));
      });

    it("should set the mute button as mute off", function() {
      view = mountTestComponent();

      view.setState({ videoMuted: false });

      var muteBtn = view.getDOMNode().querySelector(".btn-mute-video");

      expect(muteBtn.classList.contains("muted")).eql(false);
    });

    it("should set the mute button as mute on", function() {
      view = mountTestComponent();

      view.setState({ audioMuted: true });

      var muteBtn = view.getDOMNode().querySelector(".btn-mute-audio");

      expect(muteBtn.classList.contains("muted")).eql(true);
    });

    it("should dispatch a `SetMute` action when the mute button is pressed", function() {
      view = mountTestComponent();

      var muteBtn = view.getDOMNode().querySelector(".btn-mute-video");

      React.addons.TestUtils.Simulate.click(muteBtn);

      sinon.assert.calledWithMatch(dispatcher.dispatch,
        sinon.match.hasOwn("name", "setMute"));
    });

    describe("#componentWillUpdate", function() {
      function expectActionDispatched() {
        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          sinon.match.instanceOf(sharedActions.SetupStreamElements));
      }

      it("should dispatch a `SetupStreamElements` action when the MEDIA_WAIT state is entered", function() {
          activeRoomStore.setStoreState({ roomState: ROOM_STATES.READY });
          mountTestComponent();

          activeRoomStore.setStoreState({ roomState: ROOM_STATES.MEDIA_WAIT });

          expectActionDispatched();
        });

      it("should dispatch a `SetupStreamElements` action on MEDIA_WAIT state is re-entered", function() {
          activeRoomStore.setStoreState({ roomState: ROOM_STATES.ENDED });
          mountTestComponent();

          activeRoomStore.setStoreState({ roomState: ROOM_STATES.MEDIA_WAIT });

          expectActionDispatched();
        });

      it("should dispatch a `StartBrowserShare` action when the SESSION_CONNECTED state is entered", function() {
        activeRoomStore.setStoreState({ roomState: ROOM_STATES.READY });
        mountTestComponent();

        activeRoomStore.setStoreState({ roomState: ROOM_STATES.SESSION_CONNECTED });

        expectActionDispatched();
      });
    });

    describe("#render", function() {
      it("should set document.title to store.serverData.roomName", function() {
        mountTestComponent();

        activeRoomStore.setStoreState({ roomName: "fakeName" });

        expect(fakeWindow.document.title).to.equal("fakeName");
      });

      it("should render the RoomFailureView if the roomState is `FAILED`",
        function() {
          activeRoomStore.setStoreState({
            failureReason: FAILURE_DETAILS.UNKNOWN,
            roomState: ROOM_STATES.FAILED
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.RoomFailureView);
        });

      it("should render the RoomFailureView if the roomState is `FULL`",
        function() {
          activeRoomStore.setStoreState({
            failureReason: FAILURE_DETAILS.UNKNOWN,
            roomState: ROOM_STATES.FULL
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.RoomFailureView);
        });

      it("should render the DesktopRoomInvitationView if roomState is `JOINED`",
        function() {
          activeRoomStore.setStoreState({ roomState: ROOM_STATES.JOINED });

          view = mountTestComponent();

          expect(TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomInvitationView).getDOMNode()).to.not.eql(null);
        });

      it("should render the DesktopRoomInvitationView if roomState is `JOINED` with just owner",
        function() {
          activeRoomStore.setStoreState({
            participants: [{ owner: true }],
            roomState: ROOM_STATES.JOINED
          });

          view = mountTestComponent();

          expect(TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomInvitationView).getDOMNode()).to.not.eql(null);
        });

      it("should render the DesktopRoomConversationView if roomState is `JOINED` with remote participant",
        function() {
          activeRoomStore.setStoreState({
            participants: [{}],
            roomState: ROOM_STATES.JOINED
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomConversationView);
          expect(TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomInvitationView).getDOMNode()).to.eql(null);
        });

      it("should render the DesktopRoomConversationView if roomState is `JOINED` with participants",
        function() {
          activeRoomStore.setStoreState({
            participants: [{ owner: true }, {}],
            roomState: ROOM_STATES.JOINED
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomConversationView);
          expect(TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomInvitationView).getDOMNode()).to.eql(null);
        });

      it("should render the DesktopRoomConversationView if roomState is `HAS_PARTICIPANTS`",
        function() {
          activeRoomStore.setStoreState({ roomState: ROOM_STATES.HAS_PARTICIPANTS });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomConversationView);
          expect(TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomInvitationView).getDOMNode()).to.eql(null);
        });

      it("should call onCallTerminated when the call ended", function() {
        activeRoomStore.setStoreState({
          roomState: ROOM_STATES.ENDED,
          used: true
        });

        view = mountTestComponent();
        // Force a state change so that it triggers componentDidUpdate
        view.setState({ foo: "bar" });

        sinon.assert.calledOnce(onCallTerminatedStub);
      });

      it("should display loading spinner when localSrcMediaElement is null",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.MEDIA_WAIT,
             localSrcMediaElement: null
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".local .loading-stream"))
               .not.eql(null);
         });

      it("should not display a loading spinner when local stream available",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.MEDIA_WAIT,
             localSrcMediaElement: { fake: "video" }
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".local .loading-stream"))
               .eql(null);
         });

      it("should display loading spinner when remote stream is not available",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.HAS_PARTICIPANTS,
             remoteSrcMediaElement: null
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".remote .loading-stream"))
               .not.eql(null);
         });

      it("should not display a loading spinner when remote stream available",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.HAS_PARTICIPANTS,
             remoteSrcMediaElement: { fake: "video" }
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".remote .loading-stream"))
               .eql(null);
         });

      it("should display an avatar for remote video when the room has participants but video is not enabled",
        function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            mediaConnected: true,
            remoteVideoEnabled: false
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view, sharedViews.AvatarView);
        });

      it("should display the remote video when there are participants and video is enabled", function() {
        activeRoomStore.setStoreState({
          roomState: ROOM_STATES.HAS_PARTICIPANTS,
          mediaConnected: true,
          remoteVideoEnabled: true,
          remoteSrcMediaElement: { fake: 1 }
        });

        view = mountTestComponent();

        expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
      });

      it("should display an avatar for local video when the stream is muted", function() {
        activeRoomStore.setStoreState({
          videoMuted: true
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view, sharedViews.AvatarView);
      });

      it("should display the local video when the stream is enabled", function() {
        activeRoomStore.setStoreState({
          localSrcMediaElement: { fake: 1 },
          videoMuted: false
        });

        view = mountTestComponent();

        expect(view.getDOMNode().querySelector(".local video")).not.eql(null);
      });

      describe("Room name priority", function() {
        beforeEach(function() {
          activeRoomStore.setStoreState({
            participants: [{}],
            roomState: ROOM_STATES.JOINED,
            roomName: "fakeName",
            roomContextUrls: [
              {
                description: "Website title",
                location: "https://fakeurl.com"
              }
            ]
          });
        });

        it("should use room name by default", function() {
          view = mountTestComponent();
          expect(fakeWindow.document.title).to.equal("fakeName");
        });

        it("should use context title when there's no room title", function() {
          activeRoomStore.setStoreState({ roomName: null });

          view = mountTestComponent();
          expect(fakeWindow.document.title).to.equal("Website title");
        });

        it("should use website url when there's no room title nor website", function() {
          activeRoomStore.setStoreState({
            roomName: null,
            roomContextUrls: [
                {
                  location: "https://fakeurl.com"
                }
              ]
          });
          view = mountTestComponent();
          expect(fakeWindow.document.title).to.equal("https://fakeurl.com");
        });
      });
    });
  });

  describe("SocialShareDropdown", function() {
    var fakeProvider;

    beforeEach(function() {
      fakeProvider = {
        name: "foo",
        origin: "https://foo",
        iconURL: "http://example.com/foo.png"
      };
    });

    afterEach(function() {
      fakeProvider = null;
    });

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        show: true
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.SocialShareDropdown, props));
    }

    describe("#render", function() {
      it("should show no contents when the Social Providers have not been fetched yet", function() {
        view = mountTestComponent();

        expect(view.getDOMNode()).to.eql(null);
      });

      it("should show an empty list when no Social Providers are available", function() {
        view = mountTestComponent({
          socialShareProviders: []
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".icon-add-share-service")).to.not.eql(null);
        expect(node.querySelectorAll(".dropdown-menu-item").length).to.eql(1);
      });

      it("should show a list of available Social Providers", function() {
        view = mountTestComponent({
          socialShareProviders: [fakeProvider]
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".icon-add-share-service")).to.not.eql(null);
        expect(node.querySelector(".dropdown-menu-separator")).to.not.eql(null);

        var dropdownNodes = node.querySelectorAll(".dropdown-menu-item");
        expect(dropdownNodes.length).to.eql(2);
        expect(dropdownNodes[1].querySelector("img").src).to.eql(fakeProvider.iconURL);
        expect(dropdownNodes[1].querySelector("span").textContent)
          .to.eql(fakeProvider.name);
      });
    });

    describe("#handleAddServiceClick", function() {
      it("should dispatch an action when the 'add provider' item is clicked", function() {
        view = mountTestComponent({
          socialShareProviders: []
        });

        var addItem = view.getDOMNode().querySelector(".dropdown-menu-item:first-child");
        React.addons.TestUtils.Simulate.click(addItem);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.AddSocialShareProvider());
      });
    });

    describe("#handleProviderClick", function() {
      it("should dispatch an action when a provider item is clicked", function() {
        view = mountTestComponent({
          roomUrl: "http://example.com",
          socialShareProviders: [fakeProvider]
        });

        var providerItem = view.getDOMNode().querySelector(".dropdown-menu-item:last-child");
        React.addons.TestUtils.Simulate.click(providerItem);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ShareRoomUrl({
            provider: fakeProvider,
            roomUrl: "http://example.com",
            previews: []
          }));
      });
    });
  });
});
