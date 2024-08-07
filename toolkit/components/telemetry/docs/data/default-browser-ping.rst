======================
"default-browser" ping
======================

This opt-out ping is sent from the Default Browser Agent, which is a Windows-only program that registers itself during Firefox installation with the Windows scheduled tasks system so that it runs automatically every 24 hours, whether Firefox is running or not. The scheduled task gathers the data for this ping and then sends it by handing it off to :doc:`../internals/pingsender`.

Even though this ping is generated by a binary separate from Firefox itself, opting out of telemetry does disable it; the pref value is copied to the registry so that the default browser agent can read it without needing to work with profiles. Relevant policies are consulted as well. The agent also has its own pref, ``default-agent.enabled``, which if set to false disables all agent functionality, including generating this ping.

Each installation of Firefox has its own copy of the agent and its own scheduled task, so one ping will be sent every day for each installation on a given machine. This is needed because the default browser setting is per-user, and different installations may have been created by different users. If multiple operating system-level users are all using one copy of Firefox, only one scheduled task will have been created and only one ping will be sent, even though the users might have different default browser settings.

The namespace this ping is in is called ``default-browser-agent``.

For more information about the default browser agent itself, see :doc:`its documentation </toolkit/mozapps/defaultagent/default-browser-agent/index>`.

Structure
=========

Since this ping is sent from an external binary, it's structured as its own ping document type and not in the standard Firefox telemetry format. It's also missing lots of data that would normally be present; for instance, there is no ``clientId`` or ``profileGroupId``, because the agent does not load any profile and so has no way to find any, and no environment block because the agent doesn't contain the telemetry library code to build it.

Here's the format of the ping data, with example values for each property:

.. code-block:: js

    {
      build_channel: <string>, // ex. "nightly", or "beta", or "release"
      version: <string>, // ex. "72.0.2"
      os_version: <string>, // ex. 10.0.18363.592
      previous_os_version: <string>, // ex. 10.0.18363.591
      os_locale: <string>, // ex. en-US
      default_browser: <string>, // ex. "firefox"
      previous_default_browser: <string>, // ex. "edge"
      default_pdf_viewer_raw: <string>, // ex. "firefox"
      notification_type: <string>, // ex. "initial" or "followup"
      notification_shown: <string>, // ex. "shown", or "not-shown", or "error"
      notification_action: <string>, // ex. "no-action" or "make-firefox-default-button"
      previous_notification_action: <string>, // Same possible values as notification_action
    }

``build_channel``
-----------------
The Firefox channel.

``version``
-----------
The Firefox version.

``os_version``
--------------
The current Windows version number. Below Windows 10, this is in the format ``[major].[minor].[build]``; for Windows 10, the format is ``10.0.[build].[UBR]``.

``previous_os_version``
-----------------------
The Windows OS version before it was changed to the current setting. The possible values are the same as for ``os_version``.

The OS does not keep track of the previous OS version, so the agent records this information itself. That means that it will be inaccurate until the first time the default is changed after the agent task begins running. Before then, the value of ``previous_os_version`` will be the same as ``os_version``.

This value is updated every time the Default Agent runs, so when the default browser is first changed the values for ``os_version`` and ``previous_os_version`` will be different. But on subsequent executions of the Default Agent, the two values will be the same.

``os_locale``
-------------
The locale that the user has selected for the operating system (NOT for Firefox).

``default_browser``
-------------------
Which browser is currently set as the system default web browser. This is simply a string with the name of the browser; the possible values include "firefox", "chrome", "edge", "edge-chrome", "ie", "opera", and "brave".

``previous_default_browser``
----------------------------
Which browser was set as the system default before it was changed to the current setting. The possible values are the same as for ``default_browser``.

The OS does not keep track of previous default settings, so the agent records this information itself. That means that it will be inaccurate until the first time the default is changed after the agent task begins running. Before then, the value of ``previous_default_browser`` will be the same as ``default_browser``.

This value is updated every time the Default Browser Agent runs, so when the default browser is first changed the values for ``default_browser`` and ``previous_default_browser`` will be different. But on subsequent executions of the Default Browser Agent, the two values will be the same.

``default_pdf_viewer_raw``
--------------------------
Which pdf viewer is currently set as the system default. This is simply a string with the name of the pdf viewer.

``notification_type``
---------------------
Which notification type was shown. There are currently two types of notifications, "initial" and "followup". The initial notification is shown first and has a "Remind me later" button. The followup notification is only shown if the "Remind me later" button is clicked and has a "Never ask again" button instead of the "Remind me later" button. Note that the value of ``notification_shown`` should be consulted to determine whether the notification type specified was actually shown.

``notification_shown``
----------------------
Whether a notification was shown or not. Possible value include "shown", "not-shown", and "error".

``notification_action``
-----------------------
The action that the user took in response to the notification. Possible values currently include "dismissed-by-timeout", "dismissed-to-action-center", "dismissed-by-button", "dismissed-by-application-hidden", "remind-me-later", "make-firefox-default-button", "toast-clicked", "no-action".

Many of the values correspond to buttons on the notification and should be pretty self explanatory, but a few are less so. The action "no-action" will be used if and only if the value of ``notification_shown`` is not "shown" to indicate that no action was taken because no notification was displayed. The action "dismissed-to-action-center" will be used if the user clicks the arrow in the top right corner of the notification to dismiss it to the action center. The action "dismissed-by-application-hidden" is provided because that is a method of dismissal that the notification API could give but, in practice, should never be seen. The action "dismissed-by-timeout" indicates that the user did not interact with the notification and it timed out.

``previous_notification_action``
--------------------------------
The action that the user took in response to the previous notification. Possible values are the same as those of ``notification_action``.

If no notification has ever been shown, this will be "no-action". If ``notification_shown`` is "shown", this will be the action that was taken on the notification before the one that was just shown (or "no-action" if there was no previous notification). Otherwise, this will be the action that the user took the last time a notification was shown.

Note that because this feature was added later, there may be people in configurations that might seem impossible, like having the combination of ``notification_type`` being "followup" with a ``previous_notification_action`` of "no-action", because the first notification action was taken before we started storing that value.
