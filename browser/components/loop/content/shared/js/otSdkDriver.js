/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.OTSdkDriver = (function() {

  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var STREAM_PROPERTIES = loop.shared.utils.STREAM_PROPERTIES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  /**
   * This is a wrapper for the OT sdk. It is used to translate the SDK events into
   * actions, and instruct the SDK what to do as a result of actions.
   */
  var OTSdkDriver = function(options) {
    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    if (!options.sdk) {
      throw new Error("Missing option sdk");
    }

    this.dispatcher = options.dispatcher;
    this.sdk = options.sdk;

    this._useDataChannels = !!options.useDataChannels;
    this._isDesktop = !!options.isDesktop;

    if (this._isDesktop) {
      if (!options.mozLoop) {
        throw new Error("Missing option mozLoop");
      }
      this.mozLoop = options.mozLoop;
    }

    this.connections = {};

    // Metrics object to keep track of the number of connections we have
    // and the amount of streams.
    this._metrics = {
      connections: 0,
      sendStreams: 0,
      recvStreams: 0
    };

    this.dispatcher.register(this, [
      "setupStreamElements",
      "setMute"
    ]);

    // Set loop.debug.twoWayMediaTelemetry to true in the browser
    // by changing the hidden pref loop.debug.twoWayMediaTelemetry using
    // about:config, or use
    //
    // localStorage.setItem("debug.twoWayMediaTelemetry", true);
    this._debugTwoWayMediaTelemetry =
      loop.shared.utils.getBoolPreference("debug.twoWayMediaTelemetry");

    /**
     * XXX This is a workaround for desktop machines that do not have a
     * camera installed. As we don't yet have device enumeration, when
     * we do, this can be removed (bug 1138851), and the sdk should handle it.
     */
    if (this._isDesktop && !window.MediaStreamTrack.getSources) {
      // If there's no getSources function, the sdk defines its own and caches
      // the result. So here we define the "normal" one which doesn't get cached, so
      // we can change it later.
      window.MediaStreamTrack.getSources = function(callback) {
        callback([{kind: "audio"}, {kind: "video"}]);
      };
    }
  };

  OTSdkDriver.prototype = {
    /**
     * Clones the publisher config into a new object, as the sdk modifies the
     * properties object.
     */
    get _getCopyPublisherConfig() {
      return _.extend({}, this.publisherConfig);
    },

    /**
     * Returns the required data channel settings.
     */
    get _getDataChannelSettings() {
      return {
        channels: {
          // We use a single channel for text. To make things simpler, we
          // always send on the publisher channel, and receive on the subscriber
          // channel.
          text: {}
        }
      };
    },

    /**
     * Handles the setupStreamElements action. Saves the required data and
     * kicks off the initialising of the publisher.
     *
     * @param {sharedActions.SetupStreamElements} actionData The data associated
     *   with the action. See action.js.
     */
    setupStreamElements: function(actionData) {
      this.publisherConfig = actionData.publisherConfig;

      this.sdk.on("exception", this._onOTException.bind(this));

      // At this state we init the publisher, even though we might be waiting for
      // the initial connect of the session. This saves time when setting up
      // the media.
      this._publishLocalStreams();
    },

    /**
     * Internal function to publish a local stream.
     * XXX This can be simplified when bug 1138851 is actioned.
     */
    _publishLocalStreams: function() {
      // We expect the local video to be muted automatically by the SDK. Hence
      // we don't mute it manually here.
      this._mockPublisherEl = document.createElement("div");

      this.publisher = this.sdk.initPublisher(this._mockPublisherEl,
        _.extend(this._getDataChannelSettings, this._getCopyPublisherConfig));

      this.publisher.on("streamCreated", this._onLocalStreamCreated.bind(this));
      this.publisher.on("streamDestroyed", this._onLocalStreamDestroyed.bind(this));
      this.publisher.on("accessAllowed", this._onPublishComplete.bind(this));
      this.publisher.on("accessDenied", this._onPublishDenied.bind(this));
      this.publisher.on("accessDialogOpened",
        this._onAccessDialogOpened.bind(this));
    },

    /**
     * Forces the sdk into not using video, and starts publishing again.
     * XXX This is part of the work around that will be removed by bug 1138851.
     */
    retryPublishWithoutVideo: function() {
      window.MediaStreamTrack.getSources = function(callback) {
        callback([{kind: "audio"}]);
      };
      this._publishLocalStreams();
    },

    /**
     * Handles the setMute action. Informs the published stream to mute
     * or unmute audio as appropriate.
     *
     * @param {sharedActions.SetMute} actionData The data associated with the
     *                                           action. See action.js.
     */
    setMute: function(actionData) {
      if (actionData.type === "audio") {
        this.publisher.publishAudio(actionData.enabled);
      } else {
        this.publisher.publishVideo(actionData.enabled);
      }
    },

    /**
     * Initiates a screen sharing publisher.
     *
     * options items:
     *  - {String}  videoSource    The type of screen to share. Values of 'screen',
     *                             'window', 'application' and 'browser' are
     *                             currently supported.
     *  - {mixed}   browserWindow  The unique identifier of a browser window. May
     *                             be passed when `videoSource` is 'browser'.
     *  - {Boolean} scrollWithPage Flag to signal that scrolling a page should
     *                             update the stream. May be passed when
     *                             `videoSource` is 'browser'.
     *
     * @param {Object} options Hash containing options for the SDK
     */
    startScreenShare: function(options) {
      // For browser sharing, we store the window Id so that we can avoid unnecessary
      // re-triggers.
      if (options.videoSource === "browser") {
        this._windowId = options.constraints.browserWindow;
      }

      var config = _.extend(this._getCopyPublisherConfig, options);

      this._mockScreenSharePreviewEl = document.createElement("div");

      this.screenshare = this.sdk.initPublisher(this._mockScreenSharePreviewEl,
        config);
      this.screenshare.on("accessAllowed", this._onScreenShareGranted.bind(this));
      this.screenshare.on("accessDenied", this._onScreenShareDenied.bind(this));
      this.screenshare.on("streamCreated", this._onScreenShareStreamCreated.bind(this));

      this._noteSharingState(options.videoSource, true);
    },

    /**
     * Initiates switching the browser window that is being shared.
     *
     * @param {Integer} windowId  The windowId of the browser.
     */
    switchAcquiredWindow: function(windowId) {
      if (windowId === this._windowId) {
        return;
      }

      this._windowId = windowId;
      this.screenshare._.switchAcquiredWindow(windowId);
    },

    /**
     * Ends an active screenshare session. Return `true` when an active screen-
     * sharing session was ended or `false` when no session is active.
     *
     * @returns {Boolean}
     */
    endScreenShare: function() {
      if (!this.screenshare) {
        return false;
      }

      this._notifyMetricsEvent("Publisher.streamDestroyed");

      this.session.unpublish(this.screenshare);
      this.screenshare.off("accessAllowed accessDenied streamCreated");
      this.screenshare.destroy();
      delete this.screenshare;
      delete this._mockScreenSharePreviewEl;
      this._noteSharingState(this._windowId ? "browser" : "window", false);
      delete this._windowId;
      return true;
    },

    /**
     * Connects a session for the SDK, listening to the required events.
     *
     * sessionData items:
     * - sessionId: The OT session ID
     * - apiKey: The OT API key
     * - sessionToken: The token for the OT session
     * - sendTwoWayMediaTelemetry: boolean should we send telemetry on length
     *                             of media sessions.  Callers should ensure
     *                             that this is only set for one side of the
     *                             session so that things don't get
     *                             double-counted.
     *
     * @param {Object} sessionData The session data for setting up the OT session.
     */
    connectSession: function(sessionData) {
      this.session = this.sdk.initSession(sessionData.sessionId);

      this._sendTwoWayMediaTelemetry = !!sessionData.sendTwoWayMediaTelemetry;
      this._setTwoWayMediaStartTime(this.CONNECTION_START_TIME_UNINITIALIZED);

      this.session.on("sessionDisconnected",
        this._onSessionDisconnected.bind(this));
      this.session.on("connectionCreated", this._onConnectionCreated.bind(this));
      this.session.on("connectionDestroyed",
        this._onConnectionDestroyed.bind(this));
      this.session.on("streamCreated", this._onRemoteStreamCreated.bind(this));
      this.session.on("streamDestroyed", this._onRemoteStreamDestroyed.bind(this));
      this.session.on("streamPropertyChanged", this._onStreamPropertyChanged.bind(this));
      this.session.on("signal:readyForDataChannel", this._onReadyForDataChannel.bind(this));

      // This starts the actual session connection.
      this.session.connect(sessionData.apiKey, sessionData.sessionToken,
        this._onSessionConnectionCompleted.bind(this));
    },

    /**
     * Disconnects the sdk session.
     */
    disconnectSession: function() {
      this.endScreenShare();

      this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({
        available: false
      }));

      if (this.session) {
        this.session.off("sessionDisconnected streamCreated streamDestroyed connectionCreated connectionDestroyed streamPropertyChanged");
        this.session.disconnect();
        delete this.session;

        this._notifyMetricsEvent("Session.connectionDestroyed", "local");
      }
      if (this.publisher) {
        this.publisher.off("accessAllowed accessDenied accessDialogOpened streamCreated");
        this.publisher.destroy();
        delete this.publisher;
      }

      this._noteConnectionLengthIfNeeded(this._getTwoWayMediaStartTime(), performance.now());

      // Also, tidy these variables ready for next time.
      delete this._sessionConnected;
      delete this._publisherReady;
      delete this._publishedLocalStream;
      delete this._subscribedRemoteStream;
      delete this._mockPublisherEl;
      delete this._publisherChannel;
      delete this._subscriberChannel;
      this.connections = {};
      this._setTwoWayMediaStartTime(this.CONNECTION_START_TIME_UNINITIALIZED);
    },

    /**
     * Oust all users from an ongoing session. This is typically done when a room
     * owner deletes the room.
     *
     * @param {Function} callback Function to be invoked once all connections are
     *                            ousted
     */
    forceDisconnectAll: function(callback) {
      if (!this._sessionConnected) {
        callback();
        return;
      }

      var connectionNames = Object.keys(this.connections);
      if (connectionNames.length === 0) {
        callback();
        return;
      }
      var disconnectCount = 0;
      connectionNames.forEach(function(id) {
        var connection = this.connections[id];
        this.session.forceDisconnect(connection, function() {
          // When all connections have disconnected, call the callback, since
          // we're done.
          if (++disconnectCount === connectionNames.length) {
            callback();
          }
        });
      }, this);
    },

    /**
     * Called once the session has finished connecting.
     *
     * @param {Error} error An OT error object, null if there was no error.
     */
    _onSessionConnectionCompleted: function(error) {
      if (error) {
        console.error("Failed to complete connection", error);
        // We log this here before the connection failure to ensure the metrics
        // event gets to the server before the leave action occurs. Otherwise
        // the server won't log the metrics event because the user is no longer
        // in the room.
        this._notifyMetricsEvent("sdk.exception." + error.code);
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: FAILURE_DETAILS.COULD_NOT_CONNECT
        }));
        return;
      }

      this.dispatcher.dispatch(new sharedActions.ConnectedToSdkServers());
      this._sessionConnected = true;
      this._maybePublishLocalStream();
    },

    /**
     * Handles the connection event for a peer's connection being dropped.
     *
     * @param {ConnectionEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     */
    _onConnectionDestroyed: function(event) {
      var connection = event.connection;
      if (connection && (connection.id in this.connections)) {
        delete this.connections[connection.id];
      }

      this._notifyMetricsEvent("Session.connectionDestroyed", "peer");

      this._noteConnectionLengthIfNeeded(this._getTwoWayMediaStartTime(), performance.now());

      this.dispatcher.dispatch(new sharedActions.RemotePeerDisconnected({
        peerHungup: event.reason === "clientDisconnected"
      }));
    },

    /**
     * Handles the session event for the connection for this client being
     * destroyed.
     *
     * @param {SessionDisconnectEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/SessionDisconnectEvent.html
     */
    _onSessionDisconnected: function(event) {
      var reason;
      switch (event.reason) {
        case "networkDisconnected":
          reason = FAILURE_DETAILS.NETWORK_DISCONNECTED;
          break;
        case "forceDisconnected":
          reason = FAILURE_DETAILS.EXPIRED_OR_INVALID;
          break;
        default:
          // Other cases don't need to be handled.
          return;
      }

      this._noteConnectionLengthIfNeeded(this._getTwoWayMediaStartTime(),
        performance.now());
      this._notifyMetricsEvent("Session." + event.reason);
      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
        reason: reason
      }));
    },

    /**
     * Handles the connection event for a newly connecting peer.
     *
     * @param {ConnectionEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     */
    _onConnectionCreated: function(event) {
      var connection = event.connection;
      if (this.session.connection.id === connection.id) {
        // This is the connection event for our connection.
        this._notifyMetricsEvent("Session.connectionCreated", "local");
        return;
      }
      this.connections[connection.id] = connection;
      this._notifyMetricsEvent("Session.connectionCreated", "peer");
      this.dispatcher.dispatch(new sharedActions.RemotePeerConnected());
    },

    /**
     * Works out the current connection state based on the streams being
     * sent or received.
     */
    _getConnectionState: function() {
      if (this._metrics.sendStreams) {
        return this._metrics.recvStreams ? "sendrecv" : "sending";
      }

      if (this._metrics.recvStreams) {
        return "receiving";
      }

      return "starting";
    },

    /**
     * Notifies of a metrics related event for tracking call setup purposes.
     * See https://wiki.mozilla.org/Loop/Architecture/Rooms#Updating_Session_State
     *
     * @param {String} eventName  The name of the event for the update.
     * @param {String} clientType Used for connection created/destoryed. Indicates
     *                            if it is for the "peer" or the "local" client.
     */
    _notifyMetricsEvent: function(eventName, clientType) {
      if (!eventName) {
        return;
      }

      var state;

      // We intentionally don't bounds check these, in case there's an error
      // somewhere, if there is, we'll see it in the server metrics and are more
      // likely to investigate.
      switch (eventName) {
        case "Session.connectionCreated":
          this._metrics.connections++;
          if (clientType === "local") {
            state = "waiting";
          }
          break;
        case "Session.connectionDestroyed":
          this._metrics.connections--;
          if (clientType === "local") {
            // Don't log this, as the server doesn't accept it after
            // the room has been left.
            return;
          } else if (!this._metrics.connections) {
            state = "waiting";
          }
          break;
        case "Publisher.streamCreated":
          this._metrics.sendStreams++;
          break;
        case "Publisher.streamDestroyed":
          this._metrics.sendStreams--;
          break;
        case "Session.streamCreated":
          this._metrics.recvStreams++;
          break;
        case "Session.streamDestroyed":
          this._metrics.recvStreams--;
          break;
        case "Session.networkDisconnected":
        case "Session.forceDisconnected":
          break;
        default:
          if (eventName.indexOf("sdk.exception") === -1) {
            console.error("Unexpected event name", eventName);
            return;
          }
      }
      if (!state) {
        state = this._getConnectionState();
      }

      this.dispatcher.dispatch(new sharedActions.ConnectionStatus({
        event: eventName,
        state: state,
        connections: this._metrics.connections,
        sendStreams: this._metrics.sendStreams,
        recvStreams: this._metrics.recvStreams
      }));
    },

    /**
     * Handles when a remote screen share is created, subscribing to
     * the stream, and notifying the stores that a share is being
     * received.
     *
     * @param {Stream} stream The SDK Stream:
     * https://tokbox.com/opentok/libraries/client/js/reference/Stream.html
     */
    _handleRemoteScreenShareCreated: function(stream) {
      // Let the stores know first so they can update the display.
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({
        receiving: true
      }));

      // There's no audio for screen shares so we don't need to worry about mute.
      this._mockScreenShareEl = document.createElement("div");
      this.session.subscribe(stream, this._mockScreenShareEl,
        this._getCopyPublisherConfig,
        this._onScreenShareSubscribeCompleted.bind(this));
    },

    /**
     * Handles the event when the remote stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamCreated: function(event) {
      this._notifyMetricsEvent("Session.streamCreated");

      if (event.stream[STREAM_PROPERTIES.HAS_VIDEO]) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({
          isLocal: false,
          videoType: event.stream.videoType,
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS]
        }));
      }

      if (event.stream.videoType === "screen") {
        this._handleRemoteScreenShareCreated(event.stream);
        return;
      }

      // Setting up the subscribe might want to be before the VideoDimensionsChange
      // dispatch. If so, we might also want to consider moving the dispatch to
      // _onSubscribeCompleted. However, this seems to work fine at the moment,
      // so we haven't felt the need to move it.

      // XXX This mock element currently handles playing audio for the session.
      // We might want to consider making the react tree responsible for playing
      // the audio, so that the incoming audio could be disable/tracked easly from
      // the UI (bug 1171896).
      this._mockSubscribeEl = document.createElement("div");

      this.subscriber = this.session.subscribe(event.stream,
        this._mockSubscribeEl, this._getCopyPublisherConfig,
        this._onSubscribeCompleted.bind(this));
    },

    /**
     * This method is passed as the "completionHandler" parameter to the SDK's
     * Session.subscribe.
     *
     * @param err {(null|Error)} - null on success, an Error object otherwise
     * @param sdkSubscriberObject {OT.Subscriber} - undocumented; returned on success
     * @param subscriberVideo {HTMLVideoElement} - used for unit testing
     */
    _onSubscribeCompleted: function(err, sdkSubscriberObject, subscriberVideo) {
      // XXX test for and handle errors better (bug 1172140)
      if (err) {
        console.log("subscribe error:", err);
        return;
      }

      var sdkSubscriberVideo = subscriberVideo ? subscriberVideo :
        this._mockSubscribeEl.querySelector("video");
      if (!sdkSubscriberVideo) {
        console.error("sdkSubscriberVideo unexpectedly falsy!");
      }

      sdkSubscriberObject.on("videoEnabled", this._onVideoEnabled.bind(this));
      sdkSubscriberObject.on("videoDisabled", this._onVideoDisabled.bind(this));

      // XXX for some reason, the SDK deliberately suppresses sending the
      // videoEnabled event after subscribe, in spite of docs claiming
      // otherwise, so we do it ourselves.
      if (sdkSubscriberObject.stream.hasVideo) {
        this.dispatcher.dispatch(new sharedActions.RemoteVideoEnabled({
          srcVideoObject: sdkSubscriberVideo}));
      }

      this._subscribedRemoteStream = true;
      if (this._checkAllStreamsConnected()) {
        this._setTwoWayMediaStartTime(performance.now());
        this.dispatcher.dispatch(new sharedActions.MediaConnected());
      }

      this._setupDataChannelIfNeeded(sdkSubscriberObject.stream.connection);
    },

    /**
     * This method is passed as the "completionHandler" parameter to the SDK's
     * Session.subscribe.
     *
     * @param err {(null|Error)} - null on success, an Error object otherwise
     * @param sdkSubscriberObject {OT.Subscriber} - undocumented; returned on success
     * @param subscriberVideo {HTMLVideoElement} - used for unit testing
     */
    _onScreenShareSubscribeCompleted: function(err, sdkSubscriberObject, subscriberVideo) {
      // XXX test for and handle errors better
      if (err) {
        console.log("subscribe error:", err);
        return;
      }

      var sdkSubscriberVideo = subscriberVideo ? subscriberVideo :
        this._mockScreenShareEl.querySelector("video");

      // XXX no idea why this is necessary in addition to the dispatch in
      // _handleRemoteScreenShareCreated.  Maybe these should be separate
      // actions.  But even so, this shouldn't be necessary....
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({
        receiving: true, srcVideoObject: sdkSubscriberVideo
      }));

    },

    /**
     * Once a remote stream has been subscribed to, this triggers the data
     * channel set-up routines. A data channel cannot be requested before this
     * time as the peer connection is not set up.
     *
     * @param {OT.Connection} connection The OT connection class object.paul
     * sched
     *
     */
    _setupDataChannelIfNeeded: function(connection) {
      if (this._useDataChannels) {
        this.session.signal({
          type: "readyForDataChannel",
          to: connection
        }, function(signalError) {
          if (signalError) {
            console.error(signalError);
          }
        });
      }
    },

    /**
     * Handles receiving the signal that the other end of the connection
     * has subscribed to the stream and we're ready to setup the data channel.
     *
     * We get data channels for both the publisher and subscriber on reception
     * of the signal, as it means that a) the remote client is setup for data
     * channels, and b) that subscribing of streams has definitely completed
     * for both clients.
     *
     * @param {OT.SignalEvent} event Details of the signal received.
     */
    _onReadyForDataChannel: function(event) {
      // If we don't want data channels, just ignore the message. We haven't
      // send the other side a message, so it won't display anything.
      if (!this._useDataChannels) {
        return;
      }

      // This won't work until a subscriber exists for this publisher
      this.publisher._.getDataChannel("text", {}, function(err, channel) {
        if (err) {
          console.error(err);
          return;
        }

        this._publisherChannel = channel;

        channel.on({
          close: function(e) {
            // XXX We probably want to dispatch and handle this somehow.
            console.log("Published data channel closed!");
          }
        });

        this._checkDataChannelsAvailable();
      }.bind(this));

      this.subscriber._.getDataChannel("text", {}, function(err, channel) {
        // Sends will queue until the channel is fully open.
        if (err) {
          console.error(err);
          return;
        }

        channel.on({
          message: function(ev) {
            try {
              var message = JSON.parse(ev.data);
              /* Append the timestamp. This is the time that gets shown. */
              message.receivedTimestamp = (new Date()).toISOString();

              this.dispatcher.dispatch(
                new sharedActions.ReceivedTextChatMessage(message));
            } catch (ex) {
              console.error("Failed to process incoming chat message", ex);
            }
          }.bind(this),

          close: function(e) {
            // XXX We probably want to dispatch and handle this somehow.
            console.log("Subscribed data channel closed!");
          }
        });

        this._subscriberChannel = channel;
        this._checkDataChannelsAvailable();
      }.bind(this));
    },

    /**
     * Checks to see if all channels have been obtained, and if so it dispatches
     * a notification to the stores to inform them.
     */
    _checkDataChannelsAvailable: function() {
      if (this._publisherChannel && this._subscriberChannel) {
        this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({
          available: true
        }));
      }
    },

    /**
     * Sends a text chat message on the data channel.
     *
     * @param {String} message The message to send.
     */
    sendTextChatMessage: function(message) {
      this._publisherChannel.send(JSON.stringify(message));
    },

    /**
     * Handles the event when the local stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onLocalStreamCreated: function(event) {
      this._notifyMetricsEvent("Publisher.streamCreated");

      if (event.stream[STREAM_PROPERTIES.HAS_VIDEO]) {

        var sdkLocalVideo = this._mockPublisherEl.querySelector("video");

        this.dispatcher.dispatch(new sharedActions.LocalVideoEnabled(
              {srcVideoObject: sdkLocalVideo}));

        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({
          isLocal: true,
          videoType: event.stream.videoType,
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS]
        }));
      }
    },

    /**
     * Implementation detail, may be set to one of the CONNECTION_START_TIME
     * constants, or a positive integer in milliseconds.
     *
     * @private
     */
    __twoWayMediaStartTime: undefined,

    /**
     * Used as a guard to make sure we don't inadvertently use an
     * uninitialized value.
     */
    CONNECTION_START_TIME_UNINITIALIZED: -1,

    /**
     * Use as a guard to ensure that we don't note any bidirectional sessions
     * twice.
     */
    CONNECTION_START_TIME_ALREADY_NOTED: -2,

    /**
     * Set and get the start time of the two-way media connection.  These
     * are done as wrapper functions so that we can log sets to make manual
     * verification of various telemetry scenarios possible.  The get API is
     * analogous in order to follow the principle of least surprise for
     * people consuming this code.
     *
     * If this._sendTwoWayMediaTelemetry is not true, returns immediately
     * without making any changes, since this data is not used, and it makes
     * reading the logs confusing for manual verification of both ends of the
     * call in the same browser, which is a case we care about.
     *
     * @param start  start time in milliseconds, as returned by
     *               performance.now()
     * @private
     */
    _setTwoWayMediaStartTime: function(start) {
      if (!this._sendTwoWayMediaTelemetry) {
        return;
      }

      this.__twoWayMediaStartTime = start;
      if (this._debugTwoWayMediaTelemetry) {
        console.log("Loop Telemetry: noted two-way connection start, " +
                    "start time in ms:", start);
      }
    },
    _getTwoWayMediaStartTime: function() {
      return this.__twoWayMediaStartTime;
    },

    /**
     * Handles the event when the remote stream is destroyed.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamDestroyed: function(event) {
      this._notifyMetricsEvent("Session.streamDestroyed");

      if (event.stream.videoType !== "screen") {
        this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({
          available: false
        }));
        delete this._subscriberChannel;
        delete this._mockSubscribeEl;
        return;
      }

      // All we need to do is notify the store we're no longer receiving,
      // the sdk should do the rest.
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({
        receiving: false
      }));

      delete this._mockScreenShareEl;
    },

    /**
     * Handles the event when the remote stream is destroyed.
     */
    _onLocalStreamDestroyed: function() {
      this._notifyMetricsEvent("Publisher.streamDestroyed");
      this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({
        available: false
      }));
      delete this._publisherChannel;
      delete this._mockPublisherEl;
    },

    /**
     * Called from the sdk when the media access dialog is opened.
     * Prevents the default action, to prevent the SDK's "allow access"
     * dialog from being shown.
     *
     * @param {OT.Event} event
     */
    _onAccessDialogOpened: function(event) {
      event.preventDefault();
    },

    /**
     * Handles the publishing being complete.
     *
     * @param {OT.Event} event
     */
    _onPublishComplete: function(event) {
      event.preventDefault();
      this._publisherReady = true;

      this.dispatcher.dispatch(new sharedActions.GotMediaPermission());

      this._maybePublishLocalStream();
    },

    /**
     * Handles publishing of media being denied.
     *
     * @param {OT.Event} event
     */
    _onPublishDenied: function(event) {
      // This prevents the SDK's "access denied" dialog showing.
      event.preventDefault();

      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.MEDIA_DENIED
      }));

      delete this._mockPublisherEl;
    },

    _onOTException: function(event) {
      if (event.code === OT.ExceptionCodes.UNABLE_TO_PUBLISH &&
          event.message === "GetUserMedia") {
        // We free up the publisher here in case the store wants to try
        // grabbing the media again.
        if (this.publisher) {
          this.publisher.off("accessAllowed accessDenied accessDialogOpened streamCreated");
          this.publisher.destroy();
          delete this.publisher;
          delete this._mockPublisherEl;
        }
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA
        }));
      } else {
        this._notifyMetricsEvent("sdk.exception." + event.code);
      }
    },

    /**
     * Handles publishing of property changes to a stream.
     */
    _onStreamPropertyChanged: function(event) {
      if (event.changedProperty == STREAM_PROPERTIES.VIDEO_DIMENSIONS) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({
          isLocal: event.stream.connection.id == this.session.connection.id,
          videoType: event.stream.videoType,
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS]
        }));
      }
    },

    /**
     * Handle the (remote) VideoEnabled event from the subscriber object
     * by dispatching an action with the (hidden) video element from
     * which to copy the stream when attaching it to visible video element
     * that the views control directly.
     *
     * @param event {OT.VideoEnabledChangedEvent} from the SDK
     *
     * @see https://tokbox.com/opentok/libraries/client/js/reference/VideoEnabledChangedEvent.html
     * @private
     */
    _onVideoEnabled: function(event) {
      var sdkSubscriberVideo = this._mockSubscribeEl.querySelector("video");
      if (!sdkSubscriberVideo) {
        console.error("sdkSubscriberVideo unexpectedly falsy!");
      }

      this.dispatcher.dispatch(
        new sharedActions.RemoteVideoEnabled(
          {srcVideoObject: sdkSubscriberVideo}));
    },

    /**
     * Handle the SDK disabling of remote video by dispatching the
     * appropriate event.
     *
     * @param event {OT.VideoEnabledChangedEvent) from the SDK
     *
     * @see https://tokbox.com/opentok/libraries/client/js/reference/VideoEnabledChangedEvent.html
     * @private
     */
    _onVideoDisabled: function(event) {
      this.dispatcher.dispatch(
        new sharedActions.RemoteVideoDisabled());
    },

    /**
     * Publishes the local stream if the session is connected
     * and the publisher is ready.
     */
    _maybePublishLocalStream: function() {
      if (this._sessionConnected && this._publisherReady) {
        // We are clear to publish the stream to the session.
        this.session.publish(this.publisher);

        // Now record the fact, and check if we've got all media yet.
        this._publishedLocalStream = true;
        if (this._checkAllStreamsConnected()) {
          this._setTwoWayMediaStartTime(performance.now());
          this.dispatcher.dispatch(new sharedActions.MediaConnected());
        }
      }
    },

    /**
     * Used to check if both local and remote streams are available
     * and send an action if they are.
     */
    _checkAllStreamsConnected: function() {
      return this._publishedLocalStream &&
        this._subscribedRemoteStream;
    },

    /**
     * Called when a screenshare is complete, publishes it to the session.
     */
    _onScreenShareGranted: function() {
      this.session.publish(this.screenshare);
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.ACTIVE
      }));
    },

    /**
     * Called when a screenshare is denied. Notifies the other stores.
     */
    _onScreenShareDenied: function() {
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.INACTIVE
      }));
      delete this._mockScreenSharePreviewEl;
    },

    /**
     * Called when a screenshare stream is published.
     */
    _onScreenShareStreamCreated: function() {
      this._notifyMetricsEvent("Publisher.streamCreated");
    },

    /*
     * XXX all of the bi-directional media connection telemetry stuff in this
     * file, (much, but not all, of it is below) should be hoisted into its
     * own object for maintainability and clarity, also in part because this
     * stuff only wants to run one side of the connection, not both (tracked
     * by bug 1145237).
     */

    /**
     * A hook exposed only for the use of the functional tests so that
     * they can check that the bi-directional media count is being updated
     * correctly.
     *
     * @type number
     * @private
     */
    _connectionLengthNotedCalls: 0,

    /**
     * Wrapper for adding a keyed value that also updates
     * connectionLengthNoted calls and sets the twoWayMediaStartTime to
     * this.CONNECTION_START_TIME_ALREADY_NOTED.
     *
     * @param {number} callLengthSeconds  the call length in seconds
     * @private
     */
    _noteConnectionLength: function(callLengthSeconds) {
      var buckets = this.mozLoop.TWO_WAY_MEDIA_CONN_LENGTH;

      var bucket = buckets.SHORTER_THAN_10S;
      if (callLengthSeconds >= 10 && callLengthSeconds <= 30) {
        bucket = buckets.BETWEEN_10S_AND_30S;
      } else if (callLengthSeconds > 30 && callLengthSeconds <= 300) {
        bucket = buckets.BETWEEN_30S_AND_5M;
      } else if (callLengthSeconds > 300) {
        bucket = buckets.MORE_THAN_5M;
      }

      this.mozLoop.telemetryAddValue("LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1", bucket);
      this._setTwoWayMediaStartTime(this.CONNECTION_START_TIME_ALREADY_NOTED);

      this._connectionLengthNotedCalls++;
      if (this._debugTwoWayMediaTelemetry) {
        console.log("Loop Telemetry: noted two-way media connection " +
          "in bucket: ", bucket);
      }
    },

    /**
     * Note connection length if it's valid (the startTime has been initialized
     * and is not later than endTime) and not yet already noted.  If
     * this._sendTwoWayMediaTelemetry is not true, we return immediately.
     *
     * @param {number} startTime  in milliseconds
     * @param {number} endTime  in milliseconds
     * @private
     */
    _noteConnectionLengthIfNeeded: function(startTime, endTime) {
      if (!this._sendTwoWayMediaTelemetry) {
        return;
      }

      if (startTime == this.CONNECTION_START_TIME_ALREADY_NOTED ||
          startTime == this.CONNECTION_START_TIME_UNINITIALIZED ||
          startTime > endTime) {
        if (this._debugTwoWayMediaTelemetry) {
          console.log("_noteConnectionLengthIfNeeded called with " +
            " invalid params, either the calls were never" +
            " connected or there is a bug; startTime:", startTime,
            "endTime:", endTime);
        }
        return;
      }

      var callLengthSeconds = (endTime - startTime) / 1000;
      this._noteConnectionLength(callLengthSeconds);
    },

    /**
     * If set to true, make it easy to test/verify 2-way media connection
     * telemetry code operation by viewing the logs.
     */
    _debugTwoWayMediaTelemetry: false,

    /**
     * Note the sharing state. If this.mozLoop is not defined, we're assumed to
     * be running in the standalone client and return immediately.
     *
     * @param  {String}  type    Type of sharing that was flipped. May be 'window'
     *                           or 'tab'.
     * @param  {Boolean} enabled Flag that tells us if the feature was flipped on
     *                           or off.
     * @private
     */
    _noteSharingState: function(type, enabled) {
      if (!this.mozLoop) {
        return;
      }

      var bucket = this.mozLoop.SHARING_STATE_CHANGE[type.toUpperCase() + "_" +
        (enabled ? "ENABLED" : "DISABLED")];
      if (typeof bucket === "undefined") {
        console.error("No sharing state bucket found for '" + type + "'");
        return;
      }

      this.mozLoop.telemetryAddValue("LOOP_SHARING_STATE_CHANGE_1", bucket);
    }
  };

  return OTSdkDriver;

})();
