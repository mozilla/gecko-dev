# Using Firefox on Glean to Record Legacy Telemetry Events

To record Legacy Telemetry events using Glean APIs, you need these two things:

1. A Glean event definition to generate a Glean API
    * This will go in a `metrics.yaml` file [likely in your component][new-metrics-yaml]
    * If you're familiar with Telemetry events, you might like to read [this short section][glean-events-vs]
  	  about how Glean events are different.
    * The [Glean `event` metric docs][glean-event-doc] has an
  	  [example definition][sample-event-defn] you might find helpful.
    * If you already know of a Legacy Telemetry event definition similar to what you want,
      you can use `./mach gifft <Legacy Telemetry event name like readermode.view>`
      to generate a Glean `event` metric definition from that Legacy Telemetry event definition.
2. A Legacy Telemetry event definition, for the [Glean Interface For Firefox Telemetry][gifft] to mirror to
    * Use `./mach event-into-legacy <Glean event metric name like privacy.sanitize.dialog_open>` to generate this automatically.
    * Place it in `toolkit/components/telemetry/Events.yaml`
    * Be sure to add the `telemetry_mirror` property to the Glean `event`
      definition from step 1. You can follow the instructions in the output from `./mach event-into-legacy`,
      or [this guide for determining the Legacy Telemetry event's enum name manually][legacy-enum-name].

Now build Firefox.

To record your new event, use [the Glean `record(...)` API][glean-event-api].

To test your new event, use [the Glean `testGetValue()` API][glean-test-api].

Your Legacy Telemetry event will appear in `about:telemetry`
when your code is triggered as confirmation this is all working as you expect.

## Artifact Build Support

Firefox on Glean supports artifact builds,
so you can instrument and test your Glean instrumentation with an artifact build.
However, mirroring requires use of the Legacy Telemetry event's C++ enum,
which means that testing the Legacy Telemetry event or seeing its value in `about:telemetry`
[requires a full, compiled build][artifact-support-gifft].

[new-metrics-yaml]: ./new_definitions_file.md#where-do-i-define-new-metrics-and-pings
[glean-events-vs]: ./migration.md#events---use-gleans-event
[glean-event-doc]: https://mozilla.github.io/glean/book/reference/metrics/event.html
[sample-event-defn]: https://mozilla.github.io/glean/book/reference/metrics/event.html#metric-parameters
[gifft]: ./gifft.md
[legacy-enum-name]: ./gifft.md#the-telemetry_mirror-property-in-metricsyaml
[glean-event-api]: https://mozilla.github.io/glean/book/reference/metrics/event.html#recordobject
[glean-test-api]: https://mozilla.github.io/glean/book/reference/metrics/event.html#testing-api
[artifact-support-gifft]: ./gifft.md#artifact-build-support
