Native messaging for a strictly-confined Firefox
================================================

Rationale
---------

Firefox, when packaged as a snap or flatpak, is confined in a way that the browser only has a very partial view of the host filesystem and limited capabilities.
Because of this, when an extension attempts to use the `nativeMessaging API <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging>`_, the browser cannot locate the corresponding `native manifest <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests>`_, and it cannot launch the native messaging host (native application) either.
Instead, it can use the `WebExtensions XDG desktop portal <https://github.com/flatpak/xdg-desktop-portal/pull/705>`_ (work in progress). The portal is responsible for mediating accesses to otherwise unavailable files on the host filesystem, prompting the user whether they want to allow a given extension to launch a given native application (and remembering the user's choice), and spawning the native application on behalf of the browser.
The portal is browser-agnostic, although currently its only known use is in Firefox.

Workflow
--------

When Firefox detects that it is running strictly confined, and if the value of the ``widget.use-xdg-desktop-portal.native-messaging`` preference is ≠ ``0``, it queries the existence of the WebExtensions portal on the D-Bus session bus. If the portal is not available, native messaging will not work (a generic error is reported). A value of ``1`` will enable the portal, while a value of ``2`` will try to autodetect its use.

If the portal is available, Firefox starts by creating a session (`CreateSession method <https://github.com/flatpak/xdg-desktop-portal/blob/557d3c1b22ce393358d2fecb6862566321a57983/data/org.freedesktop.portal.WebExtensions.xml#L35>`_). The resulting Session object will be used to communicate with the portal until it is closed (`Close method <https://flatpak.github.io/xdg-desktop-portal/#gdbus-method-org-freedesktop-portal-Session.Close>`_).

Firefox then calls `the GetManifest method <https://github.com/flatpak/xdg-desktop-portal/blob/557d3c1b22ce393358d2fecb6862566321a57983/data/org.freedesktop.portal.WebExtensions.xml#L83>`_ on the portal, and the portal looks up a host manifest matching the name of the native application and the extension ID, and returns the JSON manifest, which Firefox can use to do its own validation before pursuing.

Firefox then calls `the Start method <https://github.com/jhenstridge/xdg-desktop-portal/blob/557d3c1b22ce393358d2fecb6862566321a57983/data/org.freedesktop.portal.WebExtensions.xml#L99>`_ on the Session object, which creates and returns `a Request object <https://flatpak.github.io/xdg-desktop-portal/#gdbus-org.freedesktop.portal.Request>`_. The portal asynchronously spawns the native application and emits `the Response signal <https://flatpak.github.io/xdg-desktop-portal/#gdbus-signal-org-freedesktop-portal-Request.Response>`_ on the Request object.

Firefox then calls `the GetPipes method <https://github.com/jhenstridge/xdg-desktop-portal/blob/557d3c1b22ce393358d2fecb6862566321a57983/data/org.freedesktop.portal.WebExtensions.xml#L134>`_ on the portal, which returns open file descriptors for stdin, stdout and stderr of the spawned process.

From that point on, Firefox can talk to the native process exactly as it does when running unconfined (i.e. when it is responsible for launching the process itself).

Closing the session will have the portal terminate the native process cleanly.

From an end user's perspective, assuming the portal is present and in use, the only visible difference is going to be a one-time prompt for each extension requesting to launch a given native application. There is currently no GUI tool to edit the saved authorizations, but there is a CLI tool (``flatpak permissions webextensions``, whose name is confusing because it's not flatpak-specific).

Implementation details
----------------------

Some complexity that is specific to XDG desktop portals architecture is hidden away in the XPCOM interface used by Firefox to talk to the portal: the Request and Response objects aren't exposed (instead the relevant methods are asynchronous and return a Promise that resolves when the response has arrived), and the GetPipes method has been folded into the Start method.

A ``connectRunning()`` method was added to the ``Subprocess`` javascript module to wrap a process spawned externally as a ``ManagedProcess``. Interaction with a ``ManagedProcess`` object is limited to communication through its open file descriptors, the caller cannot directly ``wait()`` on the process. The ``kill()`` method there does not kill the process but allows to notify of the process termination to ensure proper freeing of the file descriptors.

Extensions with the "nativeMessaging" permission should know nothing about the underlying mechanism used to talk to native applications, so it is important that the errors thrown in this separate code path aren't distinguishable from the generic errors thrown in the usual code path where the browser is responsible for managing the lifecycle of the native applications itself.

Debugging via ``MOZ_LOG`` environment variable or ``about:logging`` can be triggered with the log module ``NativeMessagingPortal``. It will enable more verbose logs to be emitted by the Firefox side of the portal client implementation.

The ``IDL`` interface to the portal is ``nsINativeMessagingPortal``.


Future work
-----------

The WebExtensions portal isn't widely available yet in a release of the XDG desktop portals project, however an agreement in principle was reached with its maintainers, pending minor changes to the current implementation, and the goal is to land it with the next stable release, hopefully 1.19.
In the meantime, the portal has been available in Ubuntu `as a distro patch <https://launchpad.net/bugs/1968215>`_ starting with release 22.04.

The functionality is exercised with XPCShell tests that mock the portal's DBus interface. There are currently no integration tests that exercise the real portal.

Security Considerations
_______________________

Baseline
~~~~~~~~

Without confinement, the following stakeholders are relevant. The tree structure reflects the relative trust between components:

- User
   + Browser
      o Extension run by browser
   + Installer of native messaging host (native manifest and native app)
      o Native messaging host ("native app")

The browser is responsible for mediating access from the extension to native apps. This includes verifying the extension ID and user consent to the nativeMessaging permission, and verifying that the native manifest permits access to the extension. If permitted, the browser is expected to relay messages between the extension and the native app.
The native app is distrustful by default, and is only willing to accept messages from extensions that have been allowlisted in the native manifest.
The extension, the native app and its installer are commonly (but not always!) provided by the same developer. E.g. a distro may create a package for installation independently of the original NMH developer.

Installation to the user profile or even system directories require privileges. Therefore the browser and NMH installer are considered trusted by the user.

Without confinement, the native app runs with the same privileges as the browser, because it's the default behavior when an external process is launched. The trust of the browser in the native app is acceptable because the existence of a registered native messaging host implies that the user has at some point consented to write access to the user profile or at a system location. Conversely, the native app trusts the browser implicitly, because the intent to communicate with the browser was declared by the existence of the native manifest.

With confinement
~~~~~~~~~~~~~~~~

With confinement of the browser, the browser cannot solely act as a mediator to launch the native app, because of the restricted access to the filesystem and ability to launch external applications. These two tasks have been delegated to the portal.

The portal will launch external applications as specified in the native messaging manifest. To avoid sandbox escapes and privilege escalation, the host system should make sure that the native messaging manifests and referenced applications cannot be modified by the confined browser. At the time of writing, this concern was accounted for in `the portal implementation <https://github.com/flatpak/xdg-desktop-portal/pull/705#issuecomment-2262776394>`_.
