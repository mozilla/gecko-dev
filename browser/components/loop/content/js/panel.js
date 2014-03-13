/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/

var loop = loop || {};
loop.panel = (function(_, __) {
  "use strict";

  // XXX: baseApiUrl should be configurable (browser pref)
  var baseApiUrl = "http://localhost:5000",
      panelView,
      notificationCollection,
      notificationListView;

  /**
   * Panel initialisation.
   */
  function init() {
    panelView = new PanelView();
    panelView.render();
    notificationCollection = new NotificationCollection();
    notificationListView = new NotificationListView({
      collection: notificationCollection
    });
    notificationListView.render();
  }

  /**
   * Notification model.
   */
  var NotificationModel = Backbone.Model.extend({
    defaults: {
      level: "info",
      message: ""
    }
  });

  /**
   * Notification collection
   */
  var NotificationCollection = Backbone.Collection.extend({
    model: NotificationModel
  });

  /**
   * Notification view.
   *
   * XXX: removal on close btn click
   */
  var NotificationView = Backbone.View.extend({
    template: _.template($("#notification-tpl").html()),

    events: {
      "click .close": "dismiss"
    },

    dismiss: function() {
      this.$el.addClass("fade-out");
      setTimeout(function() {
        notificationCollection.remove(this.model);
        this.remove();
      }.bind(this), 500);
    },

    render: function() {
      this.$el.html(this.template(this.model.toJSON()));
      return this;
    }
  });

  /**
   * Notification list view.
   */
  var NotificationListView = Backbone.View.extend({
    el: ".share .messages",

    initialize: function(options) {
      if (!options.collection)
        throw new Error("missing required collection");

      this.collection = options.collection;
      this.listenTo(this.collection, "reset add remove", this.render);
    },

    render: function() {
      this.$el.html(this.collection.map(function(notification) {
        return new NotificationView({model: notification}).render().$el;
      }));
      return this;
    }
  });

  /**
   * Panel view.
   */
  var PanelView = Backbone.View.extend({
    el: "#default-view",

    events: {
      "click a.get-url": "getCallUrl"
    },

    getCallUrl: function(event) {
      event.preventDefault();
      requestCallUrl(function(err, callUrl) {
        if (err) {
          notificationCollection.add({
            level: "error",
            message: __("unable_retrieve_url")
          });
          return;
        }
        this.onCallUrlReceived(callUrl);
      }.bind(this));
    },

    onCallUrlReceived: function(callUrl) {
      notificationCollection.reset();
      this.$(".action .invite").hide();
      this.$(".action .result input").val(callUrl);
      this.$(".action .result").show();
      this.$(".description p").text(__("share_link_url"));
    }
  });

  function requestCallUrl(cb) {
    var endpoint = baseApiUrl + "/call-url/",
        reqData = {simplepushUrl: "xxx"};

    function validate(callUrlData) {
      if (typeof callUrlData !== "object" ||
          !callUrlData.hasOwnProperty("call_url")) {
        console.error("Invalid call url data received", callUrlData);
        throw new Error("Invalid call url data received");
      }
      return callUrlData.call_url;
    }

    var req = $.post(endpoint, reqData, function(callUrlData) {
      try {
        cb(null, validate(callUrlData));
      } catch (err) {
        cb(err);
      }
    }, "json");

    req.fail(function(xhr, type, err) {
      var error = "Unknown error.";
      if (xhr && xhr.responseJSON && xhr.responseJSON.error) {
        error = xhr.responseJSON.error;
      }
      cb(new Error("HTTP error " + xhr.status + ": " + err + "; " + error));
    });
  }

  return {
    init: init,
    requestCallUrl: requestCallUrl,
    NotificationModel: NotificationModel,
    NotificationCollection: NotificationCollection,
    NotificationView: NotificationView,
    NotificationListView: NotificationListView,
    PanelView: PanelView
  };
})(_, document.mozL10n.get);
