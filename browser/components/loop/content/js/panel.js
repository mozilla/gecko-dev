/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop:true, React */

var loop = loop || {};
loop.panel = (function(_, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views,
      // aliasing translation function as __ for concision
      __ = mozL10n.get;

  /**
   * Panel router.
   * @type {loop.desktopRouter.DesktopRouter}
   */
  var router;

  /**
   * Do not disturb panel subview.
   */
  var DoNotDisturb = React.createClass({displayName: 'DoNotDisturb',
    mixins: [sharedViews.ReactL10nMixin],

    getInitialState: function() {
      return {doNotDisturb: navigator.mozLoop.doNotDisturb};
    },

    handleCheckboxChange: function() {
      // Note: side effect!
      navigator.mozLoop.doNotDisturb = !navigator.mozLoop.doNotDisturb;
      this.setState({doNotDisturb: navigator.mozLoop.doNotDisturb});
    },

    render: function() {
      var status = this.state.doNotDisturb ? 'Unavailable' : 'Available';
      // XXX https://github.com/facebook/react/issues/310 for === htmlFor
      return (
        React.DOM.div(null, 
          React.DOM.input( {type:"checkbox", checked:this.state.doNotDisturb,
                 id:"dnd-component", onChange:this.handleCheckboxChange} ),
          React.DOM.label( {htmlFor:"dnd-component"}, status)
        )
      );
    }
  });

  var PanelContainer = React.createClass({displayName: 'PanelContainer',
    mixins: [sharedViews.ReactL10nMixin],

    propTypes: {
      summary: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        React.DOM.div( {className:"share generate-url"}, 
          React.DOM.div( {className:"description"}, 
            React.DOM.p(null, this.props.summary)
          ),
          React.DOM.div(null, this.props.children)
        )
      );
    }
  });

  var CallUrlResult = React.createClass({displayName: 'CallUrlResult',
    mixins: [sharedViews.ReactL10nMixin],

    propTypes: {
      callUrl: React.PropTypes.string.isRequired,
      retry: React.PropTypes.func.isRequired
    },

    handleButtonClick: function() {
      this.props.retry();
    },

    render: function() {
      // XXX setting elem value from a state (in the callUrl input)
      // makes it immutable ie read only but that is fine in our case.
      // readOnly attr will suppress a warning regarding this issue
      // from the react lib.
      return (
        PanelContainer( {summary:__("share_link_url")}, 
          React.DOM.input( {value:this.props.callUrl, readOnly:"true"} ),
          React.DOM.button( {className:"btn btn-success", 'data-l10n-id':"new_url",
                  onClick:this.handleButtonClick} )
        )
      );
    }
  });

  var CallUrlForm = React.createClass({displayName: 'CallUrlForm',
    mixins: [sharedViews.ReactL10nMixin],

    getInitialState: function() {
      return {pending: false, callUrl: false};
    },

    retry: function() {
      this.setState({pending: false, callUrl: false});
    },

    handleFormSubmit: function(event) {
      event.preventDefault();

      var callback = function(err, callUrlData) {
        this.setState({pending: false});
        if (err) {
          this.notifier.errorL10n("unable_retrieve_url");
          return;
        }
        this.onCallUrlReceived(callUrlData);
      }.bind(this);

      var nickname = this.refs.caller.getDOMNode().value;
      this.props.client.requestCallUrl(nickname, callback);
      this.setState({pending: true});
    },

    onCallUrlReceived: function(response) {
      this.setState({callUrl: response.call_url});
    },

    render: function() {
      // If we have a call url, render result
      if (this.state.callUrl) {
        return (
          CallUrlResult( {callUrl:this.state.callUrl, retry:this.retry})
        );
      }

      // If we don't display the form
      var cx = React.addons.classSet;
      return (
        PanelContainer( {summary:__("get_link_to_share")}, 
          React.DOM.form( {className:"invite", onSubmit:this.handleFormSubmit}, 
            React.DOM.input( {type:"text", name:"caller", ref:"caller", required:"required",
                   className:cx({'pending': this.state.pending}),
                   'data-l10n-id':"caller"} ),
            React.DOM.button( {type:"submit", className:"get-url btn btn-success",
                    'data-l10n-id':"get_a_call_url"} )
          )
        )
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({displayName: 'PanelView',

    mixins: [sharedViews.ReactL10nMixin],

    events: {
      "keyup input[name=caller]": "changeButtonState",
      "click a.go-back": "goBack"
    },

    componentWillMount: function() {
      this.notifier = this.props.notifier;
      this.client = new loop.shared.Client({
        baseServerUrl: navigator.mozLoop.serverUrl
      });
    },

    changeButtonState: function() {
      var enabled = !!this.$("input[name=caller]").val();
      if (enabled) {
        this.$(".get-url").removeClass("disabled")
            .removeAttr("disabled", "disabled");
      } else {
        this.$(".get-url").addClass("disabled").attr("disabled", "disabled");
      }
    },

    render: function() {
      return (
        React.DOM.div(null, 
          CallUrlForm( {client:this.client, notifier:this.notifier} ),
          DoNotDisturb(null )
        )
      );
    }
  });

  var PanelRouter = loop.desktopRouter.DesktopRouter.extend({
    /**
     * DOM document object.
     * @type {HTMLDocument}
     */
    document: undefined,

    routes: {
      "": "home"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.document) {
        throw new Error("missing required document");
      }
      this.document = options.document;

      this._registerVisibilityChangeEvent();

      this.on("panel:open panel:closed", this.reset, this);
    },

    /**
     * Register the DOM visibility API event for the whole document, and trigger
     * appropriate events accordingly:
     *
     * - `panel:opened` when the panel is open
     * - `panel:closed` when the panel is closed
     *
     * @link  http://www.w3.org/TR/page-visibility/
     */
    _registerVisibilityChangeEvent: function() {
      this.document.addEventListener("visibilitychange", function(event) {
        this.trigger(event.currentTarget.hidden ? "panel:closed"
                                                : "panel:open");
      }.bind(this));
    },

    /**
     * Default entry point.
     */
    home: function() {
      this.reset();
    },

    /**
     * Resets this router to its initial state.
     */
    reset: function() {
      // purge pending notifications
      this._notifier.clear();
      // reset home view
      this.loadReactComponent(PanelView( {notifier:this._notifier} ));
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    router = new PanelRouter({
      document: document,
      notifier: new sharedViews.NotificationListView({el: "#messages"})
    });
    Backbone.history.start();

    // Notify the window that we've finished initalization and initial layout
    var evtObject = document.createEvent('Event');
    evtObject.initEvent('loopPanelInitialized', true, false);
    window.dispatchEvent(evtObject);
  }

  return {
    init: init,
    PanelView: PanelView,
    PanelRouter: PanelRouter
  };
})(_, document.mozL10n);
