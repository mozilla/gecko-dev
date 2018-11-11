/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.ConversationAppStore", function() {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var sandbox, activeRoomStore, dispatcher, roomUsed;

  beforeEach(function() {
    roomUsed = false;
    activeRoomStore = {
      getStoreState: function() { return { used: roomUsed }; }
    };
    sandbox = LoopMochaUtils.createSandbox();
    LoopMochaUtils.stubLoopRequest({
      GetLoopPref: function() {}
    });
    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");
  });

  afterEach(function() {
    sandbox.restore();
    LoopMochaUtils.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if the activeRoomStore is missing", function() {
      expect(function() {
        new loop.store.ConversationAppStore({
          dispatcher: dispatcher,
          feedbackPeriod: 1,
          feedbackTimestamp: 1
        });
      }).to.Throw(/activeRoomStore/);
    });

    it("should throw an error if the dispatcher is missing", function() {
      expect(function() {
        new loop.store.ConversationAppStore({
          activeRoomStore: activeRoomStore,
          feedbackPeriod: 1,
          feedbackTimestamp: 1
        });
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if feedbackPeriod is missing", function() {
      expect(function() {
        new loop.store.ConversationAppStore({
          activeRoomStore: activeRoomStore,
          dispatcher: dispatcher,
          feedbackTimestamp: 1
        });
      }).to.Throw(/feedbackPeriod/);
    });

    it("should throw an error if feedbackTimestamp is missing", function() {
      expect(function() {
        new loop.store.ConversationAppStore({
          activeRoomStore: activeRoomStore,
          dispatcher: dispatcher,
          feedbackPeriod: 1
        });
      }).to.Throw(/feedbackTimestamp/);
    });

    it("should start listening to events on the window object", function() {
      var fakeWindow = {
        addEventListener: sinon.stub()
      };

      var store = new loop.store.ConversationAppStore({
        activeRoomStore: activeRoomStore,
        dispatcher: dispatcher,
        feedbackPeriod: 1,
        feedbackTimestamp: 1,
        rootObject: fakeWindow
      });

      var eventNames = Object.getOwnPropertyNames(store._eventHandlers);
      sinon.assert.callCount(fakeWindow.addEventListener, eventNames.length);
      eventNames.forEach(function(eventName) {
        sinon.assert.calledWith(fakeWindow.addEventListener, eventName,
          store._eventHandlers[eventName]);
      });
    });
  });

  describe("#getWindowData", function() {
    var fakeWindowData, fakeGetWindowData, store, getLoopPrefStub;
    var setLoopPrefStub;

    beforeEach(function() {
      fakeWindowData = {
        type: "room",
        roomToken: "123456"
      };

      fakeGetWindowData = {
        windowId: "42"
      };

      getLoopPrefStub = sandbox.stub();
      setLoopPrefStub = sandbox.stub();

      loop.storedRequests = {
        "GetConversationWindowData|42": fakeWindowData
      };
      LoopMochaUtils.stubLoopRequest({
        GetLoopPref: getLoopPrefStub,
        SetLoopPref: setLoopPrefStub
      });

      store = new loop.store.ConversationAppStore({
        activeRoomStore: activeRoomStore,
        dispatcher: dispatcher,
        feedbackPeriod: 42,
        feedbackTimestamp: 42
      });
    });

    afterEach(function() {
      sandbox.restore();
    });

    it("should fetch the window type from the Loop API", function() {
      store.getWindowData(new sharedActions.GetWindowData(fakeGetWindowData));

      expect(store.getStoreState().windowType).eql("room");
    });

    it("should have the feedback period in initial state", function() {
      // Expect ms.
      expect(store.getInitialStoreState().feedbackPeriod).to.eql(42 * 1000);
    });

    it("should have the dateLastSeen in initial state", function() {
      // Expect ms.
      expect(store.getInitialStoreState().feedbackTimestamp).to.eql(42 * 1000);
    });

    it("should set showFeedbackForm to true when action is triggered", function() {
      var showFeedbackFormStub = sandbox.stub(store, "showFeedbackForm");

      store.showFeedbackForm(new sharedActions.ShowFeedbackForm());

      sinon.assert.calledOnce(showFeedbackFormStub);
    });

    it("should set feedback timestamp on ShowFeedbackForm action", function() {
      var clock = sandbox.useFakeTimers();
      // Make sure we round down the value.
      clock.tick(1001);
      store.showFeedbackForm(new sharedActions.ShowFeedbackForm());

      sinon.assert.calledOnce(setLoopPrefStub);
      sinon.assert.calledWithExactly(setLoopPrefStub,
                                     "feedback.dateLastSeenSec", 1);
    });

    it("should dispatch a SetupWindowData action with the data from the Loop API",
      function() {
        store.getWindowData(new sharedActions.GetWindowData(fakeGetWindowData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetupWindowData(_.extend({
            windowId: fakeGetWindowData.windowId
          }, fakeWindowData)));
      });
  });

  describe("Window object event handlers", function() {
    var store, fakeWindow;

    beforeEach(function() {
      fakeWindow = {
        addEventListener: sinon.stub(),
        removeEventListener: sinon.stub()
      };

      LoopMochaUtils.stubLoopRequest({
        GetLoopPref: function() {}
      });

      store = new loop.store.ConversationAppStore({
        activeRoomStore: activeRoomStore,
        dispatcher: dispatcher,
        feedbackPeriod: 1,
        feedbackTimestamp: 1,
        rootObject: fakeWindow
      });
    });

    describe("#unloadHandler", function() {
      it("should dispatch a 'WindowUnload' action when invoked", function() {
        store.unloadHandler();

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.WindowUnload());
      });

      it("should remove all registered event handlers from the window object", function() {
        var eventHandlers = store._eventHandlers;
        var eventNames = Object.getOwnPropertyNames(eventHandlers);

        store.unloadHandler();

        sinon.assert.callCount(fakeWindow.removeEventListener, eventNames.length);
        expect(store._eventHandlers).to.eql(null);
        eventNames.forEach(function(eventName) {
          sinon.assert.calledWith(fakeWindow.removeEventListener, eventName,
            eventHandlers[eventName]);
        });
      });
    });

    describe("#LoopHangupNowHandler", function() {
      beforeEach(function() {
        sandbox.stub(loop.shared.mixins.WindowCloseMixin, "closeWindow");
      });

      it("should dispatch the correct action when a room was used", function() {
        store.setStoreState({ windowType: "room" });
        roomUsed = true;

        store.LoopHangupNowHandler();

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.LeaveRoom());
        sinon.assert.notCalled(loop.shared.mixins.WindowCloseMixin.closeWindow);
      });

      it("should close the window when a room was used and it showed feedback", function() {
        store.setStoreState({
          showFeedbackForm: true,
          windowType: "room"
        });
        roomUsed = true;

        store.LoopHangupNowHandler();

        sinon.assert.notCalled(dispatcher.dispatch);
        sinon.assert.calledOnce(loop.shared.mixins.WindowCloseMixin.closeWindow);
      });

      it("should close the window when a room was not used", function() {
        store.setStoreState({ windowType: "room" });

        store.LoopHangupNowHandler();

        sinon.assert.notCalled(dispatcher.dispatch);
        sinon.assert.calledOnce(loop.shared.mixins.WindowCloseMixin.closeWindow);
      });

      it("should close the window for all other window types", function() {
        store.setStoreState({ windowType: "foobar" });

        store.LoopHangupNowHandler();

        sinon.assert.notCalled(dispatcher.dispatch);
        sinon.assert.calledOnce(loop.shared.mixins.WindowCloseMixin.closeWindow);
      });
    });

    describe("#socialFrameAttachedHandler", function() {
      it("should update the store correctly to reflect the attached state", function() {
        store.setStoreState({ chatWindowDetached: true });

        store.socialFrameAttachedHandler();

        expect(store.getStoreState().chatWindowDetached).to.eql(false);
      });
    });

    describe("#socialFrameDetachedHandler", function() {
      it("should update the store correctly to reflect the detached state", function() {
        store.socialFrameDetachedHandler();

        expect(store.getStoreState().chatWindowDetached).to.eql(true);
      });
    });

    describe("#ToggleBrowserSharingHandler", function() {
      it("should dispatch the correct action", function() {
        store.ToggleBrowserSharingHandler({ detail: false });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.ToggleBrowserSharing({
          enabled: true
        }));
      });
    });
  });
});
