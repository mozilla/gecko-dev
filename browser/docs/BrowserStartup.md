# Browser Startup

## Invoking your code on browser startup

The first rule of running code during startup is: don't.

We take performance very seriously and ideally your component/feature should
initialize only when needed.

If you have established that you really must run code during startup,
available entrypoints are:

- registering a `browser-idle-startup` category entry for your JS module (or
  even a "best effort" user idle task, see `BrowserGlue.sys.mjs`)
- registering a `browser-window-delayed-startup` category entry for your JS
  module. **Note that this is invoked for each browser window.**
- registering a `browser-before-ui-startup` category entry if you really really
  need to. This will run code before the first browser window appears on the
  screen and make Firefox seem slow, so please don't do it unless absolutely
  necessary.

See [the category manager indirection docs](./CategoryManagerIndirection.md) for
more details on this.

Other useful points in startup are:
- `BrowserGlue`'s `_onFirstWindowLoaded` (which should be converted to use a
  category manager call instead), which fires after the first browser window's
  `browser-window-delayed-startup` call (see above).
- `BrowserGlue`'s `_scheduleBestEffortUserIdleTasks` as mentioned above.
  Note that in this case, your code **may not run at all** if the browser is
  shut down quickly.
- `BrowserGlue`'s `_onWindowsRestored`, and/or the observer service's
  `sessionstore-windows-restored` topic, and/or a category manager call that
  should replace the `BrowserGlue` list of direct calls. This fires after
  session restore has completed restoring all windows (but before all pages
  that may have been restored have necessarily loaded). Note that this is
  guaranteed to fire even if automatic session restore is not enabled.

## How do first run/first startup experiments work?

Why does synchronously reading Nimbus feature values work for
customizing display features like `about:welcome` onboarding and the
default browser prompt?  The key invariant is that the display
decisions wait for `sessionstore-windows-restored` to show
customizable UI, and therefore we just need Nimbus available to read
at that point.  This is arranged either via the `--first-startup`
flag; or, for subsequent startups, the relevant Nimbus features being
marked `isEarlyStartup: true`.  When `isEarlyStartup: true`, Nimbus
caches all its feature variables as Gecko preferences, ready to be
read during early startup.  (See [the early startup
docs](https://experimenter.info/faq/early-startup/what-do-it-do).)

Customizable display features like `about:welcome` or the default
browser prompt are used in
[`_maybeShowDefaultBrowserPrompt()`](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/components/BrowserGlue.sys.mjs#4685),
which is invoked as part of a startup idle task.  Startup idle tasks
are [scheduled in response to
`sessionstore-windows-restored`](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/components/BrowserGlue.sys.mjs#2423).

Now, why is `sessionstore-windows-restored` late enough for a
first startup experiment?  The answer is subtle.

During Firefox launch, [`final-ui-startup` is
notified](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/toolkit/xre/nsAppRunner.cpp#5764-5765),
and in response `SessionStore` is initialized.  Additionally,
Nimbus/Normandy initialization is [started but not
awaited](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/components/BrowserGlue.sys.mjs#1487).

Then the [command line is
handled](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/toolkit/xre/nsAppRunner.cpp#5775-5778).
When `--first-startup` is passed, we [spin the event loop to allow
Nimbus/Normandy time to complete its initialization and first
fetch](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/components/BrowserContentHandler.sys.mjs#677)
before continuing to process the command line.  See [the
`FirstStartup`
module](https://firefox-source-docs.mozilla.org/toolkit/modules/toolkit_modules/FirstStartup.html).
(Important caveat: `--first-startup` is only used on Windows; see [Bug
1872934](https://bugzilla.mozilla.org/show_bug.cgi?id=1872934), for
example.)

This races with `SessionStore`, which itself waits for the first
browser window to be shown -- in particular, the
`sessionstore-windows-restored` notification waits for the [first
browser window's `browser-delayed-startup-finished`
notification](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/components/sessionstore/SessionStore.sys.mjs#2147-2159).

This first `browser-delayed-startup-finished` notification **is not
guaranteed** to be after `--first-startup` has spun the event loop!
But, when launched with only `--first-startup` and flags considered
very early in `nsAppRunner.cpp` -- as [the stub installer
does](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/installer/windows/nsis/stub.nsi#1424-1456)
-- then the first window **is guaranteed** to be after the event loop
has been spun, and therefore `sessionstore-windows-restored` is after
as well.  (As a counter-example: try `firefox.exe --browser
--first-startup` and witness the [`--browser`
flag](https://searchfox.org/mozilla-central/rev/a965e3c683ecc035dee1de72bd33a8d91b1203ed/browser/components/BrowserContentHandler.sys.mjs#505-508)
creating a window before spinning the event loop, inadvertently racing
against `sessionstore-windows-restored`.)  Making this deterministic
is tracked by [Bug
1944431](https://bugzilla.mozilla.org/show_bug.cgi?id=1944431).

Together, this means that first-startup experiments will be loaded in
time to impact display features such as `about:welcome` and the
default browser prompt, and we should not have a "split brain"
scenario in which the Nimbus feature is intermittently unavailable to
the relevant display features.
