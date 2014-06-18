===================
Firefox for Android
===================

UI Telemetry
============

Fennec records UI events using a telemetry framework called UITelemetry.

Some links:

- `Project page <https://wiki.mozilla.org/Mobile/Projects/Telemetry_probes_for_Fennec_UI_elements>`_
- `Wiki page <https://wiki.mozilla.org/Mobile/Fennec/Android/UITelemetry>`_
- `User research notes <https://wiki.mozilla.org/Mobile/User_Experience/Research>`_

Sessions
========

**Sessions** are essentially scopes. They are meant to provide context to
events; this allows events to be simpler and more reusable. Sessions are
usually bound to some component of the UI, some user action with a duration, or
some transient state.

For example, a session might be begun when a user begins interacting with a
menu, and stopped when the interaction ends. Or a session might encapsulate
period of no network connectivity, the first five seconds after the browser
launched, the time spent with an active download, or a guest mode session.

Sessions implicitly record the duration of the interaction.

A simple use-case for sessions is the bookmarks panel in about:home. We start a
session when the user swipes into the panel, and stop it when they swipe away.
This bookmarks session does two things: firstly, it gives scope to any generic
event that may occur within the panel (*e.g.*, loading a URL). Secondly, it
allows us to figure out how much time users are spending in the bookmarks
panel.

To start a session, call ``Telemetry.startUISession(String sessionName)``.

``sessionName``
  The name of the session. Session names should be brief, lowercase, and should describe which UI
  component the user is interacting with. In certain cases where the UI component is dynamic, they could include an ID, essential to identifying that component. An example of this is dynamic home panels: we use session names of the format ``homepanel:<panel_id>`` to identify home panel sessions.

To stop a session, call ``Telemetry.stopUISession(String sessionName, String reason)``.

``sessionName``
  The name of the open session

``reason`` (Optional)
  A descriptive cause for ending the session. It should be brief, lowercase, and generic so it can be reused in different places. Examples reasons are:

    ``switched``
      The user transitioned to a UI element of equal level.

    ``exit``
      The user left for an entirely different element.

Events
======

Events capture key occurrences. They should be brief and simple, and should not contain sensitive or excess information. Context for events should come from the session (scope). An event can be created with four fields (via ``Telemetry.sendUIEvent``): ``action``, ``method``, ``extras``, and ``timestamp``.

``action``
  The name of the event. Should be brief and lowercase. If needed, you can make use of namespacing with a '``.``' separator. Example event names: ``panel.switch``, ``panel.enable``, ``panel.disable``, ``panel.install``.

``method`` (Optional)
  Used for user actions that can be performed in many ways. This field specifies the method by which the action was performed. For example, users can add an item to their reading list either by long-tapping the reader icon in the address bar, or from within reader mode. We would use the same event name for both user actions but specify two methods: ``addressbar`` and ``readermode``.

``extras`` (Optional)
  For extra information that may be useful in understanding the event. Make an effort to keep this brief.

``timestamp`` (Optional)
  The time at which the event occurred. If not specified, this field defaults to the current value of the realtime clock.

Versioning
========

As a we improve on our Telemetry methods, it is foreseeable that our probes will change over time. Different versions of a probe could carry different data or have different interpretations on the server-side. To make it easier for the server to handle these changes, you should add version numbers to your event and session names. An example of a versioned session is ``homepanel.1``; this is version 1 of the ``homepanel`` session. This approach should also be applied to event names, an example being: ``panel.enable.1`` and ``panel.enable.2``.


Clock
=====

Times are relative to either elapsed realtime (an arbitrary monotonically increasing clock that continues to tick when the device is asleep), or elapsed uptime (which doesn't tick when the device is in deep sleep). We default to elapsed realtime.

See the documentation in `the source <http://mxr.mozilla.org/mozilla-central/source/mobile/android/base/Telemetry.java>`_ for more details.

Dictionary
==========

Events
------
``action.1``
  Generic action, usually for tracking menu and toolbar actions.

``cancel.1``
  Cancel a state, action, etc.

``cast.1``
  Start casting a video.

``edit.1``
  Sent when the user edits a top site.

``launch.1``
  Launching (opening) an external application.
  Note: Only used in JavaScript for now.

``loadurl.1``
  Loading a URL.

``locale.browser.reset.1``
  When the user chooses "System default" in the browser locale picker.

``locale.browser.selected.1``
  When the user chooses a locale in the browser locale picker. The selected
  locale is provided as the extra.

``locale.browser.unselected.1``
  When the user chose a different locale in the browser locale picker, this
  event is fired with the previous locale as the extra. If the previous locale
  could not be determined, "unknown" is provided.

``setdefault.1``
  Set default home panel.

``pin.1``, ``unpin.1``
  Sent when the user pinned or unpinned a top site.

``policynotification.success.1:true``
  Sent when a user has accepted the data notification policy. Can be ``false``
  instead of ``true`` if an error occurs.

``sanitize.1``
  Sent when the user chooses to clear private data.

``save.1`` ``unsave.1``
  Saving or unsaving a resource (reader, bookmark, etc.) for viewing later.
  Note: Only used in JavaScript for now.

``share.1``
  Sharing content.

Methods
-------
``banner``
  Action triggered from a banner (such as HomeBanner).
  Note: Only used in JavaScript for now.

Sessions
--------

