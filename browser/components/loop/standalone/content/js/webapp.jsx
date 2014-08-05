/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */
/* jshint newcap:false */

var loop = loop || {};
loop.webapp = (function($, _, OT, webL10n) {
  "use strict";

  loop.config = loop.config || {};
  loop.config.serverUrl = loop.config.serverUrl || "http://localhost:5000";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      baseServerUrl = loop.config.serverUrl,
      __ = webL10n.get;

  /**
   * App router.
   * @type {loop.webapp.WebappRouter}
   */
  var router;

  /**
   * Homepage view.
   */
  var HomeView = sharedViews.BaseView.extend({
    template: _.template('<p data-l10n-id="welcome"></p>')
  });

  /**
   * Firefox promotion interstitial. Will display only to non-Firefox users.
   */
  var PromoteFirefoxView = React.createClass({
    propTypes: {
      helper: React.PropTypes.object.isRequired
    },

    render: function() {
      if (this.props.helper.isFirefox(navigator.userAgent)) {
        return <div />;
      }
      return (
        <div className="promote-firefox">
          <h3>{__("promote_firefox_hello_heading")}</h3>
          <p>
            <a className="btn btn-large btn-success"
               href="https://www.mozilla.org/firefox/">
              {__("get_firefox_button")}
            </a>
          </p>
        </div>
      );
    }
  });

  /**
   * Expired call URL view.
   */
  var CallUrlExpiredView = React.createClass({
    propTypes: {
      helper: React.PropTypes.object.isRequired
    },

    render: function() {
      /* jshint ignore:start */
      return (
        <div className="expired-url-info">
          <div className="info-panel">
            <div className="firefox-logo" />
            <h1>{__("call_url_unavailable_notification_heading")}</h1>
            <h4>{__("call_url_unavailable_notification_message")}</h4>
          </div>
          <PromoteFirefoxView helper={this.props.helper} />
        </div>
      );
      /* jshint ignore:end */
    }
  });

  var ConversationHeader = React.createClass({
    render: function() {
      var cx = React.addons.classSet;
      var conversationUrl = location.href;

      var urlCreationDateClasses = cx({
        "light-color-font": true,
        "call-url-date": true, /* Used as a handler in the tests */
        /*hidden until date is available*/
        "hide": !this.props.urlCreationDateString.length
      });

      var callUrlCreationDateString = __("call_url_creation_date_label", {
        "call_url_creation_date": this.props.urlCreationDateString
      });

      return (
        /* jshint ignore:start */
        <header className="container-box">
          <h1 className="light-weight-font">
            <strong>{__("brandShortname")}</strong> {__("clientShortname")}
          </h1>
          <div className="loop-logo" title="Firefox WebRTC! logo"></div>
          <h3 className="call-url">
            {conversationUrl}
          </h3>
          <h4 className={urlCreationDateClasses} >
            {callUrlCreationDateString}
          </h4>
        </header>
        /* jshint ignore:end */
      );
    }
  });

  var ConversationFooter = React.createClass({
    render: function() {
      return (
        <div className="footer container-box">
          <div title="Mozilla Logo" className="footer-logo"></div>
        </div>
      );
    }
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var ConversationFormView = React.createClass({
    /**
     * Constructor.
     *
     * Required options:
     * - {loop.shared.model.ConversationModel}    model    Conversation model.
     * - {loop.shared.views.NotificationListView} notifier Notifier component.
     *
     */

    getInitialState: function() {
      return {
        urlCreationDateString: '',
        disableCallButton: false
      };
    },

    propTypes: {
      model: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                                       .isRequired,
      // XXX Check more tightly here when we start injecting window.loop.*
      notifier: React.PropTypes.object.isRequired,
      client: React.PropTypes.object.isRequired
    },

    componentDidMount: function() {
      this.props.model.listenTo(this.props.model, "session:error",
                                this._onSessionError);
      this.props.client.requestCallUrlInfo(this.props.model.get("loopToken"),
                                           this._setConversationTimestamp);
      // XXX DOM element does not exist before React view gets instantiated
      // We should turn the notifier into a react component
      this.props.notifier.$el = $("#messages");
    },

    _onSessionError: function(error) {
      console.error(error);
      this.props.notifier.errorL10n("unable_retrieve_call_info");
    },

    /**
     * Initiates the call.
     */
    _initiate: function() {
      this.props.model.initiate({
        client: new loop.StandaloneClient({
          baseServerUrl: baseServerUrl
        }),
        outgoing: true,
        // For now, we assume both audio and video as there is no
        // other option to select.
        callType: "audio-video",
        loopServer: loop.config.serverUrl
      });

      this.setState({disableCallButton: true});
    },

    _setConversationTimestamp: function(err, callUrlInfo) {
      if (err) {
        this.props.notifier.errorL10n("unable_retrieve_call_info");
      } else {
        var date = (new Date(callUrlInfo.urlCreationDate * 1000));
        var options = {year: "numeric", month: "long", day: "numeric"};
        var timestamp = date.toLocaleDateString(navigator.language, options);

        this.setState({urlCreationDateString: timestamp});
      }
    },

    render: function() {
      var tos_link_name = __("terms_of_use_link_text");
      var privacy_notice_name = __("privacy_notice_link_text");

      var tosHTML = __("legal_text_and_links", {
        "terms_of_use_url": "<a target=_blank href='" +
          "https://accounts.firefox.com/legal/terms'>" + tos_link_name + "</a>",
        "privacy_notice_url": "<a target=_blank href='" +
          "https://www.mozilla.org/privacy/'>" + privacy_notice_name + "</a>"
      });

      var callButtonClasses = "btn btn-success btn-large " +
                              loop.shared.utils.getTargetPlatform();

      return (
        /* jshint ignore:start */
        <div className="container">
          <div className="container-box">

            <ConversationHeader
              urlCreationDateString={this.state.urlCreationDateString} />

            <p className="large-font light-weight-font">
              {__("initiate_call_button_label")}
            </p>

            <div id="messages"></div>

            <div className="button-group">
              <div className="flex-padding-1"></div>
              <button ref="submitButton" onClick={this._initiate}
                className={callButtonClasses}
                disabled={this.state.disableCallButton}>
                {__("initiate_call_button")}
                <i className="icon icon-video"></i>
              </button>
              <div className="flex-padding-1"></div>
            </div>

            <p className="terms-service"
               dangerouslySetInnerHTML={{__html: tosHTML}}></p>
          </div>

          <ConversationFooter />
        </div>
        /* jshint ignore:end */
      );
    }
  });

  /**
   * Webapp Router.
   */
  var WebappRouter = loop.shared.router.BaseConversationRouter.extend({
    routes: {
      "":                    "home",
      "unsupportedDevice":   "unsupportedDevice",
      "unsupportedBrowser":  "unsupportedBrowser",
      "call/expired":        "expired",
      "call/ongoing/:token": "loadConversation",
      "call/:token":         "initiate"
    },

    initialize: function(options) {
      this.helper = options.helper;
      if (!this.helper) {
        throw new Error("WebappRouter requires an helper object");
      }

      // Load default view
      this.loadView(new HomeView());

      this.listenTo(this._conversation, "timeout", this._onTimeout);
      this.listenTo(this._conversation, "session:expired",
                    this._onSessionExpired);
    },

    _onSessionExpired: function() {
      this.navigate("/call/expired", {trigger: true});
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.startCall}
     */
    startCall: function() {
      if (!this._conversation.get("loopToken")) {
        this._notifier.errorL10n("missing_conversation_info");
        this.navigate("home", {trigger: true});
      } else {
        this.navigate("call/ongoing/" + this._conversation.get("loopToken"), {
          trigger: true
        });
      }
    },

    /**
     * @override {loop.shared.router.BaseConversationRouter.endCall}
     */
    endCall: function() {
      var route = "home";
      if (this._conversation.get("loopToken")) {
        route = "call/" + this._conversation.get("loopToken");
      }
      this.navigate(route, {trigger: true});
    },

    _onTimeout: function() {
      this._notifier.errorL10n("call_timeout_notification_text");
    },

    /**
     * Default entry point.
     */
    home: function() {
      this.loadView(new HomeView());
    },

    unsupportedDevice: function() {
      this.loadView(new sharedViews.UnsupportedDeviceView());
    },

    unsupportedBrowser: function() {
      this.loadView(new sharedViews.UnsupportedBrowserView());
    },

    expired: function() {
      this.loadReactComponent(CallUrlExpiredView({helper: this.helper}));
    },

    /**
     * Loads conversation launcher view, setting the received conversation token
     * to the current conversation model. If a session is currently established,
     * terminates it first.
     *
     * @param  {String} loopToken Loop conversation token.
     */
    initiate: function(loopToken) {
      // Check if a session is ongoing; if so, terminate it
      if (this._conversation.get("ongoing")) {
        this._conversation.endSession();
      }
      this._conversation.set("loopToken", loopToken);
      this.loadReactComponent(ConversationFormView({
        model: this._conversation,
        notifier: this._notifier,
        client: new loop.StandaloneClient({
          baseServerUrl: loop.config.serverUrl
        })
      }));
    },

    /**
     * Loads conversation establishment view.
     *
     */
    loadConversation: function(loopToken) {
      if (!this._conversation.isSessionReady()) {
        // User has loaded this url directly, actually setup the call.
        return this.navigate("call/" + loopToken, {trigger: true});
      }
      this.loadReactComponent(sharedViews.ConversationView({
        sdk: OT,
        model: this._conversation
      }));
    }
  });

  /**
   * Local helpers.
   */
  function WebappHelper() {
    this._iOSRegex = /^(iPad|iPhone|iPod)/;
  }

  WebappHelper.prototype = {
    isFirefox: function(platform) {
      return platform.indexOf("Firefox") !== -1;
    },

    isIOS: function(platform) {
      return this._iOSRegex.test(platform);
    }
  };

  /**
   * App initialization.
   */
  function init() {
    var helper = new WebappHelper();
    router = new WebappRouter({
      helper: helper,
      notifier: new sharedViews.NotificationListView({el: "#messages"}),
      conversation: new sharedModels.ConversationModel({}, {
        sdk: OT,
        pendingCallTimeout: loop.config.pendingCallTimeout
      })
    });
    Backbone.history.start();
    if (helper.isIOS(navigator.platform)) {
      router.navigate("unsupportedDevice", {trigger: true});
    } else if (!OT.checkSystemRequirements()) {
      router.navigate("unsupportedBrowser", {trigger: true});
    }
    // Set the 'lang' and 'dir' attributes to <html> when the page is translated
    document.documentElement.lang = document.webL10n.getLanguage();
    document.documentElement.dir = document.webL10n.getDirection();
  }

  return {
    baseServerUrl: baseServerUrl,
    CallUrlExpiredView: CallUrlExpiredView,
    ConversationFormView: ConversationFormView,
    HomeView: HomeView,
    init: init,
    PromoteFirefoxView: PromoteFirefoxView,
    WebappHelper: WebappHelper,
    WebappRouter: WebappRouter
  };
})(jQuery, _, window.OT, document.webL10n);
