/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.panel", function() {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;

  var sandbox, notifications;
  var fakeXHR, fakeWindow, fakeMozLoop;
  var requests = [];

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };

    fakeWindow = {
      close: sandbox.stub(),
      addEventListener: function() {},
      document: { addEventListener: function(){} },
      setTimeout: function(callback) { callback(); }
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    notifications = new loop.shared.models.NotificationCollection();

    fakeMozLoop = navigator.mozLoop = {
      doNotDisturb: true,
      fxAEnabled: true,
      getStrings: function() {
        return JSON.stringify({textContent: "fakeText"});
      },
      get locale() {
        return "en-US";
      },
      setLoopPref: sandbox.stub(),
      getLoopPref: function (prefName) {
        switch (prefName) {
          case "contextInConversations.enabled":
            return true;
          default:
            return "unseen";
        }
      },
      getPluralForm: function() {
        return "fakeText";
      },
      contacts: {
        getAll: function(callback) {
          callback(null, []);
        },
        on: sandbox.stub()
      },
      rooms: {
        getAll: function(version, callback) {
          callback(null, []);
        },
        on: sandbox.stub()
      },
      confirm: sandbox.stub(),
      hasEncryptionKey: true,
      logInToFxA: sandbox.stub(),
      logOutFromFxA: sandbox.stub(),
      notifyUITour: sandbox.stub(),
      openURL: sandbox.stub(),
      getSelectedTabMetadata: sandbox.stub()
    };

    document.mozL10n.initialize(navigator.mozLoop);
  });

  afterEach(function() {
    delete navigator.mozLoop;
    loop.shared.mixins.setRootObject(window);
    sandbox.restore();
  });

  describe("#init", function() {
    beforeEach(function() {
      sandbox.stub(React, "render");
      sandbox.stub(document.mozL10n, "initialize");
      sandbox.stub(document.mozL10n, "get").returns("Fake title");
    });

    it("should initalize L10n", function() {
      loop.panel.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);
      sinon.assert.calledWithExactly(document.mozL10n.initialize,
        navigator.mozLoop);
    });

    it("should render the panel view", function() {
      loop.panel.init();

      sinon.assert.calledOnce(React.render);
      sinon.assert.calledWith(React.render,
        sinon.match(function(value) {
          return TestUtils.isCompositeComponentElement(value,
            loop.panel.PanelView);
      }));
    });

    it("should dispatch an loopPanelInitialized", function(done) {
      function listener() {
        done();
      }

      window.addEventListener("loopPanelInitialized", listener);

      loop.panel.init();

      window.removeEventListener("loopPanelInitialized", listener);
    });
  });

  describe("loop.panel.AvailabilityDropdown", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(
        React.createElement(loop.panel.AvailabilityDropdown));
    });

    describe("doNotDisturb preference change", function() {
      beforeEach(function() {
        navigator.mozLoop.doNotDisturb = true;
      });

      it("should toggle the value of mozLoop.doNotDisturb", function() {
        var availableMenuOption = view.getDOMNode()
                                    .querySelector(".dnd-make-available");

        TestUtils.Simulate.click(availableMenuOption);

        expect(navigator.mozLoop.doNotDisturb).eql(false);
      });

      it("should toggle the dropdown menu", function() {
        var availableMenuOption = view.getDOMNode()
                                    .querySelector(".dnd-status span");

        TestUtils.Simulate.click(availableMenuOption);

        expect(view.state.showMenu).eql(true);
      });
    });
  });

  describe("loop.panel.PanelView", function() {
    var fakeClient, dispatcher, roomStore, callUrlData;

    beforeEach(function() {
      callUrlData = {
        callUrl: "http://call.invalid/",
        expiresAt: 1000
      };

      fakeClient = {
        requestCallUrl: function(_, cb) {
          cb(null, callUrlData);
        }
      };

      dispatcher = new loop.Dispatcher();
      roomStore = new loop.store.RoomStore(dispatcher, {
        mozLoop: fakeMozLoop
      });
    });

    function createTestPanelView() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.PanelView, {
          notifications: notifications,
          client: fakeClient,
          showTabButtons: true,
          mozLoop: fakeMozLoop,
          dispatcher: dispatcher,
          roomStore: roomStore
        }));
    }

    it("should hide the account entry when FxA is not enabled", function() {
      navigator.mozLoop.userProfile = {email: "test@example.com"};
      navigator.mozLoop.fxAEnabled = false;

      var view = TestUtils.renderIntoDocument(
        React.createElement(loop.panel.SettingsDropdown, {
          mozLoop: fakeMozLoop
        }));

      expect(view.getDOMNode().querySelectorAll(".icon-account"))
        .to.have.length.of(0);
    });

    describe("TabView", function() {
      var view, callTab, roomsTab, contactsTab;

      beforeEach(function() {
        navigator.mozLoop.getLoopPref = function(pref) {
          if (pref === "gettingStarted.seen") {
            return true;
          }
        };

        view = createTestPanelView();

        [roomsTab, contactsTab] =
          TestUtils.scryRenderedDOMComponentsWithClass(view, "tab");
      });

      it("should select contacts tab when clicking tab button", function() {
        TestUtils.Simulate.click(
          view.getDOMNode().querySelector("li[data-tab-name=\"contacts\"]"));

        expect(contactsTab.getDOMNode().classList.contains("selected"))
          .to.be.true;
      });

      it("should select rooms tab when clicking tab button", function() {
        TestUtils.Simulate.click(
          view.getDOMNode().querySelector("li[data-tab-name=\"rooms\"]"));

        expect(roomsTab.getDOMNode().classList.contains("selected"))
          .to.be.true;
      });
    });

    describe("AuthLink", function() {

      beforeEach(function() {
        navigator.mozLoop.calls = { clearCallInProgress: function() {} };
      });

      afterEach(function() {
        delete navigator.mozLoop.logInToFxA;
        delete navigator.mozLoop.calls;
        navigator.mozLoop.fxAEnabled = true;
      });

      it("should trigger the FxA sign in/up process when clicking the link",
        function() {
          navigator.mozLoop.loggedInToFxA = false;
          navigator.mozLoop.logInToFxA = sandbox.stub();

          var view = createTestPanelView();

          TestUtils.Simulate.click(
            view.getDOMNode().querySelector(".signin-link a"));

          sinon.assert.calledOnce(navigator.mozLoop.logInToFxA);
        });

      it("should close the panel after clicking the link",
        function() {
          navigator.mozLoop.loggedInToFxA = false;
          navigator.mozLoop.logInToFxA = sandbox.stub();

          var view = createTestPanelView();

          TestUtils.Simulate.click(
            view.getDOMNode().querySelector(".signin-link a"));

          sinon.assert.calledOnce(fakeWindow.close);
        });

      it("should be hidden if FxA is not enabled",
        function() {
          navigator.mozLoop.fxAEnabled = false;
          var view = TestUtils.renderIntoDocument(
            React.createElement(loop.panel.AuthLink));
          expect(view.getDOMNode()).to.be.null;
      });
    });

    describe("SettingsDropdown", function() {
      function mountTestComponent() {
        return TestUtils.renderIntoDocument(
          React.createElement(loop.panel.SettingsDropdown, {
            mozLoop: fakeMozLoop
          }));
      }

      beforeEach(function() {
        navigator.mozLoop.logInToFxA = sandbox.stub();
        navigator.mozLoop.logOutFromFxA = sandbox.stub();
        navigator.mozLoop.openFxASettings = sandbox.stub();
      });

      afterEach(function() {
        navigator.mozLoop.fxAEnabled = true;
      });

      it("should show a signin entry when user is not authenticated",
        function() {
          navigator.mozLoop.loggedInToFxA = false;

          var view = mountTestComponent();

          expect(view.getDOMNode().querySelectorAll(".icon-signout"))
            .to.have.length.of(0);
          expect(view.getDOMNode().querySelectorAll(".icon-signin"))
            .to.have.length.of(1);
        });

      it("should show a signout entry when user is authenticated", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};

        var view = mountTestComponent();

        expect(view.getDOMNode().querySelectorAll(".icon-signout"))
          .to.have.length.of(1);
        expect(view.getDOMNode().querySelectorAll(".icon-signin"))
          .to.have.length.of(0);
      });

      it("should show an account entry when user is authenticated", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};

        var view = mountTestComponent();

        expect(view.getDOMNode().querySelectorAll(".icon-account"))
          .to.have.length.of(1);
      });

      it("should open the FxA settings when the account entry is clicked", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};

        var view = mountTestComponent();

        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".icon-account"));

        sinon.assert.calledOnce(navigator.mozLoop.openFxASettings);
      });

      it("should hide any account entry when user is not authenticated",
        function() {
          navigator.mozLoop.loggedInToFxA = false;

          var view = mountTestComponent();

          expect(view.getDOMNode().querySelectorAll(".icon-account"))
            .to.have.length.of(0);
        });

      it("should sign in the user on click when unauthenticated", function() {
        navigator.mozLoop.loggedInToFxA = false;
        var view = mountTestComponent();

        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".icon-signin"));

        sinon.assert.calledOnce(navigator.mozLoop.logInToFxA);
      });

      it("should sign out the user on click when authenticated", function() {
        navigator.mozLoop.userProfile = {email: "test@example.com"};
        var view = mountTestComponent();

        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".icon-signout"));

        sinon.assert.calledOnce(navigator.mozLoop.logOutFromFxA);
      });
    });

    describe("Help", function() {
      var view, supportUrl;

      function mountTestComponent() {
        return TestUtils.renderIntoDocument(
          React.createElement(loop.panel.SettingsDropdown, {
            mozLoop: fakeMozLoop
          }));
      }

      beforeEach(function() {
        supportUrl = "https://example.com";
        navigator.mozLoop.getLoopPref = function(pref) {
          if (pref === "support_url") {
            return supportUrl;
          }

          return "unseen";
        };
      });

      it("should open a tab to the support page", function() {
        view = mountTestComponent();

        TestUtils.Simulate
          .click(view.getDOMNode().querySelector(".icon-help"));

        sinon.assert.calledOnce(fakeMozLoop.openURL);
        sinon.assert.calledWithExactly(fakeMozLoop.openURL, supportUrl);
      });

      it("should close the panel", function() {
        view = mountTestComponent();

        TestUtils.Simulate
          .click(view.getDOMNode().querySelector(".icon-help"));

        sinon.assert.calledOnce(fakeWindow.close);
      });
    });

    describe("#render", function() {
      it("should render a ToSView", function() {
        var view = createTestPanelView();

        TestUtils.findRenderedComponentWithType(view, loop.panel.ToSView);
      });

      it("should not render a ToSView when the view has been 'seen'", function() {
        navigator.mozLoop.getLoopPref = function() {
          return "seen";
        };
        var view = createTestPanelView();

        try {
          TestUtils.findRenderedComponentWithType(view, loop.panel.ToSView);
          sinon.assert.fail("Should not find the ToSView if it has been 'seen'");
        } catch (ex) {}
      });

      it("should render a GettingStarted view", function() {
        navigator.mozLoop.getLoopPref = function(pref) {
          return false;
        };
        var view = createTestPanelView();

        TestUtils.findRenderedComponentWithType(view, loop.panel.GettingStartedView);
      });

      it("should not render a GettingStartedView when the view has been seen", function() {
        navigator.mozLoop.getLoopPref = function() {
          return true;
        };
        var view = createTestPanelView();

        try {
          TestUtils.findRenderedComponentWithType(view, loop.panel.GettingStartedView);
          sinon.assert.fail("Should not find the GettingStartedView if it has been seen");
        } catch (ex) {}
      });

      it("should render a SignInRequestView when mozLoop.hasEncryptionKey is false", function() {
        fakeMozLoop.hasEncryptionKey = false;

        var view = createTestPanelView();

        TestUtils.findRenderedComponentWithType(view, loop.panel.SignInRequestView);
      });

      it("should render a SignInRequestView when mozLoop.hasEncryptionKey is true", function() {
        var view = createTestPanelView();

        try {
          TestUtils.findRenderedComponentWithType(view, loop.panel.SignInRequestView);
          sinon.assert.fail("Should not find the GettingStartedView if it has been seen");
        } catch (ex) {}
      });
    });
  });

  describe("loop.panel.RoomEntry", function() {
    var dispatcher, roomData;

    beforeEach(function() {
      dispatcher = new loop.Dispatcher();
      roomData = {
        roomToken: "QzBbvGmIZWU",
        roomUrl: "http://sample/QzBbvGmIZWU",
        decryptedContext: {
          roomName: "Second Room Name"
        },
        maxSize: 2,
        participants: [
          { displayName: "Alexis", account: "alexis@example.com",
            roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb" },
          { displayName: "Adam",
            roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7" }
        ],
        ctime: 1405517418
      };
    });

    function mountRoomEntry(props) {
      props = _.extend({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.RoomEntry, props));
    }

    describe("Copy button", function() {
      var roomEntry, copyButton;

      beforeEach(function() {
        roomEntry = mountRoomEntry({
          deleteRoom: sandbox.stub(),
          room: new loop.store.Room(roomData)
        });
        copyButton = roomEntry.getDOMNode().querySelector("button.copy-link");
      });

      it("should not display a copy button by default", function() {
        expect(copyButton).to.not.equal(null);
      });

      it("should copy the URL when the click event fires", function() {
        sandbox.stub(dispatcher, "dispatch");

        TestUtils.Simulate.click(copyButton);

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.CopyRoomUrl({
          roomUrl: roomData.roomUrl,
          from: "panel"
        }));
      });

      it("should set state.urlCopied when the click event fires", function() {
        TestUtils.Simulate.click(copyButton);

        expect(roomEntry.state.urlCopied).to.equal(true);
      });

      it("should switch to displaying a check icon when the URL has been copied",
        function() {
          TestUtils.Simulate.click(copyButton);

          expect(copyButton.classList.contains("checked")).eql(true);
        });

      it("should not display a check icon after mouse leaves the entry",
        function() {
          var roomNode = roomEntry.getDOMNode();
          TestUtils.Simulate.click(copyButton);

          TestUtils.SimulateNative.mouseOut(roomNode);

          expect(copyButton.classList.contains("checked")).eql(false);
        });
    });

    describe("Delete button click", function() {
      var roomEntry, deleteButton;

      beforeEach(function() {
        roomEntry = mountRoomEntry({
          room: new loop.store.Room(roomData)
        });
        deleteButton = roomEntry.getDOMNode().querySelector("button.delete-link");
      });

      it("should not display a delete button by default", function() {
        expect(deleteButton).to.not.equal(null);
      });

      it("should dispatch a delete action when confirmation is granted", function() {
        sandbox.stub(dispatcher, "dispatch");

        navigator.mozLoop.confirm.callsArgWith(1, null, true);
        TestUtils.Simulate.click(deleteButton);

        sinon.assert.calledOnce(navigator.mozLoop.confirm);
        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DeleteRoom({roomToken: roomData.roomToken}));
      });

      it("should not dispatch an action when the confirmation is cancelled", function() {
        sandbox.stub(dispatcher, "dispatch");

        navigator.mozLoop.confirm.callsArgWith(1, null, false);
        TestUtils.Simulate.click(deleteButton);

        sinon.assert.calledOnce(navigator.mozLoop.confirm);
        sinon.assert.notCalled(dispatcher.dispatch);
      });
    });

    describe("Context Indicator", function() {
      var roomEntry;

      function mountEntryForContext() {
        return mountRoomEntry({
          room: new loop.store.Room(roomData)
        });
      }

      it("should not display a context indicator if the room doesn't have any", function() {
        roomEntry = mountEntryForContext();

        expect(roomEntry.getDOMNode().querySelector(".room-entry-context-item")).eql(null);
      });

      it("should a context indicator if the room specifies context", function() {
        roomData.decryptedContext.urls = [{
          description: "invalid entry",
          location: "http://invalid",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }];

        roomEntry = mountEntryForContext();

        expect(roomEntry.getDOMNode().querySelector(".room-entry-context-item")).not.eql(null);
      });

      it("should call mozLoop.openURL to open a new url", function() {
        roomData.decryptedContext.urls = [{
          description: "invalid entry",
          location: "http://invalid/",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }];

        roomEntry = mountEntryForContext();

        TestUtils.Simulate.click(roomEntry.getDOMNode().querySelector("a"));

        sinon.assert.calledOnce(fakeMozLoop.openURL);
        sinon.assert.calledWithExactly(fakeMozLoop.openURL, "http://invalid/");
      });

      it("should call close the panel after opening a url", function() {
        roomData.decryptedContext.urls = [{
          description: "invalid entry",
          location: "http://invalid/",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }];

        roomEntry = mountEntryForContext();

        TestUtils.Simulate.click(roomEntry.getDOMNode().querySelector("a"));

        sinon.assert.calledOnce(fakeWindow.close);
      });
    });

    describe("Room Entry click", function() {
      var roomEntry, roomEntryNode;

      beforeEach(function() {
        sandbox.stub(dispatcher, "dispatch");

        roomEntry = mountRoomEntry({
          dispatcher: dispatcher,
          room: new loop.store.Room(roomData)
        });
        roomEntryNode = roomEntry.getDOMNode();
      });

      it("should dispatch an OpenRoom action", function() {
        TestUtils.Simulate.click(roomEntryNode);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.OpenRoom({roomToken: roomData.roomToken}));
      });

      it("should call window.close", function() {
        TestUtils.Simulate.click(roomEntryNode);

        sinon.assert.calledOnce(fakeWindow.close);
      });
    });

    describe("Room name updated", function() {
      it("should update room name", function() {
        var roomEntry = mountRoomEntry({
          dispatcher: dispatcher,
          room: new loop.store.Room(roomData)
        });
        var updatedRoom = new loop.store.Room(_.extend({}, roomData, {
          decryptedContext: {
            roomName: "New room name"
          },
          ctime: new Date().getTime()
        }));

        roomEntry.setProps({room: updatedRoom});

        expect(
          roomEntry.getDOMNode().textContent)
        .eql("New room name");
      });
    });
  });

  describe("loop.panel.RoomList", function() {
    var roomStore, dispatcher, fakeEmail, dispatch;

    beforeEach(function() {
      fakeEmail = "fakeEmail@example.com";
      dispatcher = new loop.Dispatcher();
      roomStore = new loop.store.RoomStore(dispatcher, {
        mozLoop: navigator.mozLoop
      });
      roomStore.setStoreState({
        pendingCreation: false,
        pendingInitialRetrieval: false,
        rooms: [],
        error: undefined
      });
      dispatch = sandbox.stub(dispatcher, "dispatch");
    });

    function createTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.RoomList, {
          store: roomStore,
          dispatcher: dispatcher,
          userDisplayName: fakeEmail,
          mozLoop: fakeMozLoop
        }));
    }

    it("should dispatch a GetAllRooms action on mount", function() {
      createTestComponent();

      sinon.assert.calledOnce(dispatch);
      sinon.assert.calledWithExactly(dispatch, new sharedActions.GetAllRooms());
    });

    it("should close the panel once a room is created and there is no error", function() {
      var view = createTestComponent();

      roomStore.setStoreState({pendingCreation: true});

      sinon.assert.notCalled(fakeWindow.close);

      roomStore.setStoreState({pendingCreation: false});

      sinon.assert.calledOnce(fakeWindow.close);
    });
  });

  describe("loop.panel.NewRoomView", function() {
    var roomStore, dispatcher, fakeEmail, dispatch;

    beforeEach(function() {
      fakeEmail = "fakeEmail@example.com";
      dispatcher = new loop.Dispatcher();
      roomStore = new loop.store.RoomStore(dispatcher, {
        mozLoop: navigator.mozLoop
      });
      roomStore.setStoreState({
        pendingCreation: false,
        pendingInitialRetrieval: false,
        rooms: [],
        error: undefined
      });
      dispatch = sandbox.stub(dispatcher, "dispatch");
    });

    function createTestComponent(pendingOperation) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.NewRoomView, {
          dispatcher: dispatcher,
          mozLoop: fakeMozLoop,
          pendingOperation: pendingOperation,
          userDisplayName: fakeEmail
        }));
    }

    it("should dispatch a CreateRoom action when clicking on the Start a " +
       "conversation button",
      function() {
        navigator.mozLoop.userProfile = {email: fakeEmail};
        var view = createTestComponent(false);

        TestUtils.Simulate.click(view.getDOMNode().querySelector(".new-room-button"));

        sinon.assert.calledWith(dispatch, new sharedActions.CreateRoom({
          nameTemplate: "fakeText",
          roomOwner: fakeEmail
        }));
      });

    it("should dispatch a CreateRoom action with context when clicking on the " +
       "Start a conversation button", function() {
      fakeMozLoop.userProfile = {email: fakeEmail};
      var favicon = "data:image/x-icon;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "http://invalid.com",
          description: "fakeSite",
          favicon: favicon,
          previews: ["fakeimage.png"]
        });
      };

      var view = createTestComponent(false);

      // Simulate being visible
      view.onDocumentVisible();

      var node = view.getDOMNode();

      // Select the checkbox
      TestUtils.Simulate.click(node.querySelector(".checkbox-wrapper"));

      TestUtils.Simulate.click(node.querySelector(".new-room-button"));

      sinon.assert.calledWith(dispatch, new sharedActions.CreateRoom({
        nameTemplate: "fakeText",
        roomOwner: fakeEmail,
        urls: [{
          location: "http://invalid.com",
          description: "fakeSite",
          thumbnail: favicon
        }]
      }));
    });

    it("should disable the create button when pendingOperation is true",
      function() {
        var view = createTestComponent(true);

        var buttonNode = view.getDOMNode().querySelector(".new-room-button[disabled]");
        expect(buttonNode).to.not.equal(null);
      });

    it("should show context information when a URL is available", function() {
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "https://www.example.com",
          description: "fake description",
          previews: [""]
        });
      };

      var view = createTestComponent(false);

      // Simulate being visible
      view.onDocumentVisible();

      var contextContent = view.getDOMNode().querySelector(".context-content");
      expect(contextContent).to.not.equal(null);
    });

    it("should cancel the checkbox when a new URL is available", function() {
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "https://www.example.com",
          description: "fake description",
          previews: [""]
        });
      };

      var view = createTestComponent(false);

      view.setState({ checked: true });

      // Simulate being visible
      view.onDocumentVisible();

      expect(view.state.checked).eql(false);
    });

    it("should show a default favicon when none is available", function() {
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "https://www.example.com",
          description: "fake description",
          previews: [""]
        });
      };

      var view = createTestComponent(false);

      // Simulate being visible
      view.onDocumentVisible();

      var previewImage = view.getDOMNode().querySelector(".context-preview");
      expect(previewImage.src).to.match(/loop\/shared\/img\/icons-16x16.svg#globe$/);
    });

    it("should not show context information when a URL is unavailable", function() {
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "",
          description: "fake description",
          previews: [""]
        });
      };

      var view = createTestComponent(false);

      view.onDocumentVisible();

      var contextInfo = view.getDOMNode().querySelector(".context");
      expect(contextInfo.classList.contains("hide")).to.equal(true);
    });

    it("should show only the hostname of the url", function() {
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "https://www.example.com:1234",
          description: "fake description",
          previews: [""]
        });
      };

      var view = createTestComponent(false);

      // Simulate being visible
      view.onDocumentVisible();

      var contextHostname = view.getDOMNode().querySelector(".context-url");
      expect(contextHostname.textContent).eql("www.example.com");
    });

    it("should show the favicon when available", function() {
      var favicon = "data:image/x-icon;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";
      fakeMozLoop.getSelectedTabMetadata = function (callback) {
        callback({
          url: "https://www.example.com:1234",
          description: "fake description",
          favicon: favicon,
          previews: ["foo.gif"]
        });
      };

      var view = createTestComponent(false);

      // Simulate being visible.
      view.onDocumentVisible();

      var contextPreview = view.getDOMNode().querySelector(".context-preview");
      expect(contextPreview.src).eql(favicon);
    });
  });

  describe("loop.panel.ToSView", function() {

    it("should render when the value of loop.seenToS is not set", function() {
      navigator.mozLoop.getLoopPref = function(key) {
        return {
          "gettingStarted.seen": true,
          "seenToS": "unseen"
        }[key];
      };

      var view = TestUtils.renderIntoDocument(
        React.createElement(loop.panel.ToSView));

      TestUtils.findRenderedDOMComponentWithClass(view, "terms-service");
    });

    it("should not render when the value of loop.seenToS is set to 'seen'", function() {
        navigator.mozLoop.getLoopPref = function(key) {
          return {
            "gettingStarted.seen": true,
            "seenToS": "seen"
          }[key];
        };

        var view = TestUtils.renderIntoDocument(
          React.createElement(loop.panel.ToSView));

        expect(function() {
          TestUtils.findRenderedDOMComponentWithClass(view, "terms-service");
        }).to.Throw(/not find/);
    });

    it("should render when the value of loop.gettingStarted.seen is false",
       function() {
         navigator.mozLoop.getLoopPref = function(key) {
           return {
             "gettingStarted.seen": false,
             "seenToS": "seen"
           }[key];
         };
         var view = TestUtils.renderIntoDocument(
           React.createElement(loop.panel.ToSView));

         TestUtils.findRenderedDOMComponentWithClass(view, "terms-service");
       });

    it("should render the telefonica logo after the first time use",
       function() {
         navigator.mozLoop.getLoopPref = function(key) {
           return {
             "gettingStarted.seen": false,
             "seenToS": "unseen",
             "showPartnerLogo": false
           }[key];
         };

         var view = TestUtils.renderIntoDocument(
           React.createElement(loop.panel.ToSView));

         expect(view.getDOMNode().querySelector(".powered-by")).eql(null);
       });

  });

  describe("loop.panel.SignInRequestView", function() {
    var view;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.panel.SignInRequestView, {
          mozLoop: fakeMozLoop
        }));
    }

    it("should call login with forced re-authentication when sign-in is clicked", function() {
      view = mountTestComponent();

      TestUtils.Simulate.click(view.getDOMNode().querySelector("button"));

      sinon.assert.calledOnce(fakeMozLoop.logInToFxA);
      sinon.assert.calledWithExactly(fakeMozLoop.logInToFxA, true);
    });

    it("should logout when use as guest is clicked", function() {
      view = mountTestComponent();

      TestUtils.Simulate.click(view.getDOMNode().querySelector("a"));

      sinon.assert.calledOnce(fakeMozLoop.logOutFromFxA);
    });
  });
});
