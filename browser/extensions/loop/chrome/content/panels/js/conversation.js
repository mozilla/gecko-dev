/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.conversation = function (mozL10n) {
  "use strict";

  var sharedMixins = loop.shared.mixins;
  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;

  var DesktopRoomConversationView = loop.roomViews.DesktopRoomConversationView;
  var FeedbackView = loop.feedbackViews.FeedbackView;
  var RoomFailureView = loop.roomViews.RoomFailureView;

  /**
   * Master controller view for handling if incoming or outgoing calls are
   * in progress, and hence, which view to display.
   */
  var AppControllerView = React.createClass({
    displayName: "AppControllerView",

    mixins: [Backbone.Events, loop.store.StoreMixin("conversationAppStore"), sharedMixins.DocumentTitleMixin, sharedMixins.WindowCloseMixin],

    propTypes: {
      cursorStore: React.PropTypes.instanceOf(loop.store.RemoteCursorStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore)
    },

    componentWillMount: function () {
      this.listenTo(this.props.cursorStore, "change:remoteCursorPosition", this._onRemoteCursorChange);
    },

    _onRemoteCursorChange: function () {
      return loop.request("AddRemoteCursorOverlay", this.props.cursorStore.getStoreState("remoteCursorPosition"));
    },

    getInitialState: function () {
      return this.getStoreState();
    },

    _renderFeedbackForm: function () {
      this.setTitle(mozL10n.get("conversation_has_ended"));

      return React.createElement(FeedbackView, {
        onAfterFeedbackReceived: this.closeWindow });
    },

    /**
     * We only show the feedback for once every 6 months, otherwise close
     * the window.
     */
    handleCallTerminated: function () {
      var delta = new Date() - new Date(this.state.feedbackTimestamp);

      // Show timestamp if feedback period (6 months) passed.
      // 0 is default value for pref. Always show feedback form on first use.
      if (this.state.feedbackTimestamp === 0 || delta >= this.state.feedbackPeriod) {
        this.props.dispatcher.dispatch(new sharedActions.ShowFeedbackForm());
        return;
      }

      this.closeWindow();
    },

    render: function () {
      if (this.state.showFeedbackForm) {
        return this._renderFeedbackForm();
      }

      switch (this.state.windowType) {
        case "room":
          {
            return React.createElement(DesktopRoomConversationView, {
              chatWindowDetached: this.state.chatWindowDetached,
              cursorStore: this.props.cursorStore,
              dispatcher: this.props.dispatcher,
              facebookEnabled: this.state.facebookEnabled,
              onCallTerminated: this.handleCallTerminated,
              roomStore: this.props.roomStore });
          }
        case "failed":
          {
            return React.createElement(RoomFailureView, {
              dispatcher: this.props.dispatcher,
              failureReason: FAILURE_DETAILS.UNKNOWN });
          }
        default:
          {
            // If we don't have a windowType, we don't know what we are yet,
            // so don't display anything.
            return null;
          }
      }
    }
  });

  /**
   * Conversation initialisation.
   */
  function init() {
    // Obtain the windowId and pass it through
    var locationHash = loop.shared.utils.locationData().hash;
    var windowId;

    var hash = locationHash.match(/#(.*)/);
    if (hash) {
      windowId = hash[1];
    }

    var requests = [["GetAllConstants"], ["GetAllStrings"], ["GetLocale"], ["GetLoopPref", "ot.guid"], ["GetLoopPref", "feedback.periodSec"], ["GetLoopPref", "feedback.dateLastSeenSec"], ["GetLoopPref", "facebook.enabled"]];
    var prefetch = [["GetConversationWindowData", windowId]];

    return loop.requestMulti.apply(null, requests.concat(prefetch)).then(function (results) {
      // `requestIdx` is keyed off the order of the `requests` and `prefetch`
      // arrays. Be careful to update both when making changes.
      var requestIdx = 0;
      var constants = results[requestIdx];
      // Do the initial L10n setup, we do this before anything
      // else to ensure the L10n environment is setup correctly.
      var stringBundle = results[++requestIdx];
      var locale = results[++requestIdx];
      mozL10n.initialize({
        locale: locale,
        getStrings: function (key) {
          if (!(key in stringBundle)) {
            console.error("No string found for key: ", key);
            return "{ textContent: '' }";
          }

          return JSON.stringify({ textContent: stringBundle[key] });
        }
      });

      // Plug in an alternate client ID mechanism, as localStorage and cookies
      // don't work in the conversation window
      var currGuid = results[++requestIdx];
      window.OT.overrideGuidStorage({
        get: function (callback) {
          callback(null, currGuid);
        },
        set: function (guid, callback) {
          // See nsIPrefBranch
          var PREF_STRING = 32;
          currGuid = guid;
          loop.request("SetLoopPref", "ot.guid", guid, PREF_STRING);
          callback(null);
        }
      });

      var dispatcher = new loop.Dispatcher();
      var sdkDriver = new loop.OTSdkDriver({
        constants: constants,
        isDesktop: true,
        useDataChannels: true,
        dispatcher: dispatcher,
        sdk: OT
      });

      // expose for functional tests
      loop.conversation._sdkDriver = sdkDriver;

      // Create the stores.
      var activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
        isDesktop: true,
        sdkDriver: sdkDriver
      });
      var conversationAppStore = new loop.store.ConversationAppStore({
        activeRoomStore: activeRoomStore,
        dispatcher: dispatcher,
        feedbackPeriod: results[++requestIdx],
        feedbackTimestamp: results[++requestIdx],
        facebookEnabled: results[++requestIdx]
      });

      prefetch.forEach(function (req) {
        req.shift();
        loop.storeRequest(req, results[++requestIdx]);
      });

      var roomStore = new loop.store.RoomStore(dispatcher, {
        activeRoomStore: activeRoomStore,
        constants: constants
      });
      var textChatStore = new loop.store.TextChatStore(dispatcher, {
        sdkDriver: sdkDriver
      });
      var remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
        sdkDriver: sdkDriver
      });

      loop.store.StoreMixin.register({
        conversationAppStore: conversationAppStore,
        remoteCursorStore: remoteCursorStore,
        textChatStore: textChatStore
      });

      React.render(React.createElement(AppControllerView, {
        cursorStore: remoteCursorStore,
        dispatcher: dispatcher,
        roomStore: roomStore }), document.querySelector("#main"));

      document.documentElement.setAttribute("lang", mozL10n.language.code);
      document.documentElement.setAttribute("dir", mozL10n.language.direction);
      document.body.setAttribute("platform", loop.shared.utils.getPlatform());

      dispatcher.dispatch(new sharedActions.GetWindowData({
        windowId: windowId
      }));

      loop.request("TelemetryAddValue", "LOOP_MAU", constants.LOOP_MAU_TYPE.OPEN_CONVERSATION);
    });
  }

  return {
    AppControllerView: AppControllerView,
    init: init,

    /**
     * Exposed for the use of functional tests to be able to check
     * metric-related execution as the call sequence progresses.
     *
     * @type loop.OTSdkDriver
     */
    _sdkDriver: null
  };
}(document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.conversation.init);
