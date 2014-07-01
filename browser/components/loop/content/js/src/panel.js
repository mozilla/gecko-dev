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
  var DoNotDisturb = React.createClass({
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
        <div>
          <input type="checkbox" checked={this.state.doNotDisturb}
                 id="dnd-component" onChange={this.handleCheckboxChange} />
          <label htmlFor="dnd-component">{status}</label>
        </div>
      );
    }
  });

  var InviteForm = React.createClass({
    mixins: [sharedViews.ReactL10nMixin],

    getInitialState: function() {
      return {
        pending: false,
        callUrl: false
      };
    },

    handleFormSubmit: function(e) {
      var callback = function(err, callUrlData) {
        this.clearPending();
        if (err) {
          this.notifier.errorL10n("unable_retrieve_url");
          this.setState({pending: false});
          return;
        }
        this.onCallUrlReceived(callUrlData);
      }.bind(this);

      var nickname = this.refs.caller.getDOMNode().value;
      this.props.client.requestCallUrl(nickname, callback);
      this.setState({pending: true});
    },

    onCallUrlReceived: function(response) {
      this.setState({callUrl: response.call_url, pending: false});
    },

    render: function() {
      var cx = React.addons.classSet;
      return (

        // XXX setting elem value from a state (in the callUrl input)
        // makes it immutable ie read only but that is fine in our case.
        // readOnly attr will suppress a warning regarding this issue
        // from the react lib

        <form className="invite" onSubmit={this.handleFormSubmit}>
          <input type="text" name="caller" ref="caller" required="required"
                 className={cx({'pending': this.state.pending,
                                'hide': !this.state.pending})}
                 data-l10n-id="caller" />
          <input value={this.state.callUrl}
                 className={cx({'hide': this.state.callUrl})}
                 readOnly="true" />
          <button type="submit" className="get-url btn btn-success"
                  data-l10n-id="get_a_call_url" />
        </form>
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({

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

    goBack: function(event) {
      event.preventDefault();
      this.$(".action .result").hide();
      this.$(".action .invite").show();
      this.$(".description p").text(__("get_link_to_share"));
      this.changeButtonState();
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
        <div className="share generate-url">
          <div className="description">
            <p data-l10n-id="get_link_to_share"></p>
          </div>
          <div className="action">
            <InviteForm client={this.client} notifier={this.notifier} />
            <DoNotDisturb />
          </div>
        </div>
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
      this.loadReactComponent(<PanelView notifier={this._notifier} />);
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
