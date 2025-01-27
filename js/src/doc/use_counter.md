Adding Use Counter Telemetry to the JS Engine
==============================================

[Use Counters](../dom/use-counters.rst) are used to collect data about the execution
of Firefox. In SpiderMonkey we can use use counters to highlight when certain VM
features are used or to measure how often certain scenarios are encountered.

Because use-counters are intended to find the frequency of a feature's use, a page
reports only _if_ a feature was used, not how many times. E.g. if you used a feature
a million times on a page, and loaded that page once, but you were the only person
who ever used that feature then telemetry would report only one use, not a million.

To add a SpiderMonkey Use Counter we have a couple extra steps compared to the
rest of Firefox.

## Adding Telemetry

1. Add an entry to `FOR_EACH_JS_USE_COUNTER` in `js/public/friend/UsageStatistics.h`.
   This will add an entry to the `JSUseCounter` enum used in the next step.

2. Call `JSRuntime::setUseCounter(JSObject*, JSUseCounter)` where you would like to
   report some use counter telemetry. The object passed is used to determine the
   global to which this usage will be attributed. A good default choice would be to
   pass `cx->global()` here.

   The UseCounter machinery is relatively efficient, and so avoiding double
   submission of counters is not necessary. Nevertheless, it is not free, so be
   cautious about telemetry on very hot paths.

3. Add an entry to `dom/base/UseCounter.conf`.  Use a custom entry in the
   JavaScript feature usage section. _Note: the first character after `JS_` should
   be lowercase. See Bug 1934649._

5. With a browser-building mozconfig active, run `mach gen-use-counter-metrics`. This
   will update `dom/base/use_counter_metrics.yaml`. Note the emails will be incorrect
   until Bug 1899418 is fixed.

6. Update the switch in `js/xpconnect/src/XPCJSRuntime.cpp` in function
   `SetUseCounterCallback`. Essentially this function redirects from the JS types to
   the DOM constants defined via use_counter_metrics.yaml.

At this point you should be able to build the browser and the shell.

## Testing Telemetry

You should write tests for your telemetry. You can do that in the shell using the
`getUseCounterResults()` test helper function.

You can then further test your browser telemetry works interactively using
`about:glean`. Visit a page which will trigger the event you're interested in,
then visit `about:glean`, and open the DevTools console.

In the console you can access these use counters from the `Glean` object:

```
Glean.useCounterDoc.jsOptimizeGetIteratorFuse.testGetValue()
```

Will for example print the current ping value for the JS Optimize Get Iterator Fuse
use counter.


## Landing Telemetry

When reviewing telemetry, follow the instructions automatically added by the data
classification robot. Typically our use counter telemetry is deeply non-identifiable
and thus easily "Category 1 Technical Data", but it is the responsibility of the
developer and reviewer to be sure.
