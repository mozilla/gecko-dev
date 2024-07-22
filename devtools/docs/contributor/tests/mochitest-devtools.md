# Automated tests: DevTools mochitests

To run the whole suite of browser mochitests for DevTools (sit back and relax):

```bash
./mach mochitest --subsuite devtools --tag devtools
```
To run a specific tool's suite of browser mochitests:

```bash
./mach mochitest devtools/client/<tool>
```

For example, run all of the debugger browser mochitests:

```bash
./mach mochitest devtools/client/debugger
```
To run a specific DevTools mochitest:

```bash
./mach mochitest devtools/client/path/to/the/test_you_want_to_run.js
```
Note that the mochitests *must* have focus while running. The tests run in the browser which looks like someone is magically testing your code by hand. If the browser loses focus, the tests will stop and fail after some time. (Again, sit back and relax)

In case you'd like to run the mochitests without having to care about focus and be able to touch your computer while running:

```bash
./mach mochitest --headless devtools/client/<tool>
```

You can also run just a single test:

```bash
./mach mochitest --headless devtools/client/path/to/the/test_you_want_to_run.js
```

## Tracing JavaScript

You can log all lines being executed in the mochitest script by using DEBUG_STEP env variable.
This will help you:
 * if the test is stuck on some asynchronous waiting code, on which line it is waiting,
 * visualize the test script execution compared to various logs and assertion logs.

Note that it will only work with Mochitests importing `devtools/client/shared/test/shared-head.js` module,
which is used by most DevTools browser mochitests.

This way:
```bash
DEBUG_STEP=true ./mach mochitest browser_devtools_test.js
```
or that other way:
```bash
./mach mochitest browser_devtools_test.js --setenv DEBUG_STEP=true
```
This will log the following lines:
```
[STEP] browser_target_command_detach.js @ 19:15   ::   const tab = ↦ await addTab(TEST_URL);
```
which tells that test script at line 19 and column 15 is about to be executed.
The '↦' highlights the precise execution's column.

Instead of passing true, you may pass a duration in milliseconds where each test line will pause for a given amount of time.
Be careful when using this feature as it will pause the event loop on each test line and allow another other event to be processed.
This will cause the test to run in a unreal way that wouldn't happen otherwise.

```bash
DEBUG_STEP=250 ./mach mochitest browser_devtools_test.js
```
Each line of the mochitest script will pause for 1/4 of seconds.

Last, but not least, this feature can be used on try via:
```bash
./mach mochitest try fuzzy devtools/test/folder/ --env DEBUG_STEP=true
```

Once you found a problematic line, or want to know more about what happens on a particular line of your mochitest,
you can then use the DEBUG_TRACE_LINE env variable.
It expect a line number of the mochitest file running and it will trace all JavaScript code ran from that line of code.

```bash
DEBUG_TRACE_LINE=42 ./mach mochitest browser_devtools_test.js
```
This will log something like this:
```
 0:17.14 GECKO(94170)  [STEP] chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/head.js
 0:17.14 GECKO(94170)  [STEP] ───────────────────────────────────────────────────────────────────────────────────
 0:17.14 GECKO(94170)  [STEP] 73:36   | async function openNewTabAndConsole↦ (url, clearJstermHistory = true, hostId) {
 0:17.15 GECKO(94170)  [STEP] 74:19   |   const toolbox = ↦ await openNewTabAndToolbox(url, "webconsole", hostId);
 0:17.15 GECKO(94170)  [STEP]
 0:17.15 GECKO(94170)  [STEP] chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js
 0:17.15 GECKO(94170)  [STEP] ──────────────────────────────────────────────────────────────────────────────
 0:17.15 GECKO(94170)  [STEP] 1273:36 | async function openNewTabAndToolbox↦ (url, toolId, hostType) {
 0:17.15 GECKO(94170)  [STEP] 1274:15 |   const tab = ↦ await addTab(url);
 0:17.15 GECKO(94170)  [STEP] 531:22  | async function addTab↦ (url, options = {}) {
 0:17.15 GECKO(94170)  [STEP] 532:3   |   ↦ info("Adding a new tab with URL: " + url);
```
where you can see the execution flow between function to functions being called, but also the progress within a function call.
Similarly to DEBUG_STEP, '↦' symbols highlights the precise execution location.

If this helper isn't enough. You can also spawn the tracer from any place manually from any privileged codebase, by using the following snippet:
```js
const { JSTracer } = ChromeUtils.importESModule(
  "resource://devtools/server/tracer/tracer.sys.mjs",
  { global: "contextual" }
);
// You have to at least pass an empty object to startTracing,
// otherwise, all the attributes at optional.
JSTracer.startTracing({
  // If you want to log a custom string before each trace
  prefix: "[my log]",

  // Only if you want to restrict to a specific global,
  // otherwise it will trace the current global automatically.
  global: window,

  // If you are about to call code from another global(s),
  // this will trace code from all active globals in the current thread.
  // (use only if needed)
  traceAllGlobals: true,

  // Only if you want to step within function execution (this adds lots of additional traces!)
  traceSteps: true,

  // If you want to restrict traces to one JS file
  filterFrameSourceUrl: "foo.js",

  // If you want to avoid logging nested trace above a given threshold
  maxDepth: 10,

  // If you want the tracer to automatically stop after having logged a given amount of traces
  maxRecords: 10,

  // If you want to log all DOM events fired on the traced global(s)
  traceDOMEvents: true,

  // If you want to log all DOM Mutations happening in the traced global(s)
  traceDOMMutations: ["add", "attributes", "delete"],
});

[...run some JS code...]

JSTracer.stopTracing();
```
