# Using Firefox on Glean to Record Legacy Telemetry Scalars

To record Legacy Telemetry scalars using Glean APIs, you need these two things:

1. A Glean metric definition to generate a Glean API
    * This will go in a `metrics.yaml` file [likely in your component][new-metrics-yaml]
    * The specific Glean metric you will use depends on how you record your data.
      We have prepared [this guide][migrate-scalars] to help you out.
    * If you already know of a Legacy Telemetry scalar definition similar to what you want,
      you can use `./mach gifft <Legacy Telemetry scalar name like browser.backup.enabled>`
      to generate a Glean metric definition from that Legacy Telemetry scalar definition.
2. A Legacy Telemetry scalar definition, for the [Glean Interface For Firefox Telemetry][gifft] to mirror to
    * Place it in `toolkit/components/telemetry/Scalars.yaml`
    * Be sure to add the `telemetry_mirror` property to the Glean metric
      definition from step 1. The value to use for `telemetry_mirror` is
      [a straightforward conjugation of the scalar's category and name][legacy-enum-name].

Now build Firefox.

To record data to your new scalar,
use [the Glean API for the Glean metric you defined in Step 1][glean-metrics-docs].

To test your new scalar, use the Glean `testGetValue()` API.

Your Legacy Telemetry scalar will appear in `about:telemetry`
when your code is triggered as confirmation this is all working as you expect.

## Artifact Build Support

Firefox on Glean supports artifact builds,
so you can instrument and test your Glean instrumentation with an artifact build.
However, mirroring requires use of the Legacy Telemetry's C++ enums,
which means that testing the Legacy Telemetry scalar or seeing its value in `about:telemetry`
[requires a full, compiled build][artifact-support-gifft].

[new-metrics-yaml]: ./new_definitions_file.md#where-do-i-define-new-metrics-and-pings
[migrate-scalars]: ./migration.md#scalars
[gifft]: ./gifft.md
[legacy-enum-name]: ./gifft.md#the-telemetry_mirror-property-in-metricsyaml
[glean-metrics-docs]: https://mozilla.github.io/glean/book/reference/metrics/
[artifact-support-gifft]: ./gifft.md#artifact-build-support
