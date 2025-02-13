# FOG Identifiers (Metric Ids, Submetric Ids, Ping Ids, ...)

The Glean Ecosystem identifies metrics by their
[base identifier](https://searchfox.org/glean/rev/3df4f3423aede68d17289f59bc726e287e6dc49d/glean-core/src/common_metric_data.rs#123),
a combination of the metric's category and name.
The Glean SDK further identifies "submetrics"
(the metric instances that are created when you call `.get(aLabel)` on a labeled metric)
by a combination of the base identifier and its label.

Glean identifies pings merely by their names.

FOG needs to identify instances of
* metrics
* submetrics
* [runtime-registered](jog) metrics and submetrics
* pings

across foreign function interfaces (to support [JS and C++](code_organization))
and processes ([for ipc](ipc)).

For efficiency's sake, FOG uses numerical IDs.
This document explains how those IDs work to identify their objects.

## Identifying a FOG Metric

Depending on the language, the process, and how a metric was defined,
the way you identify a specific FOG metric instance is different.

### Rust: FOG's `MetricId`, `BaseMetricId`, and `SubMetricId`

To identify a specific FOG metric instance in Rust you will have either a
`BaseMetricId` for metric instances corresponding to definitions in `metrics.yaml`,
or a `SubMetricId` for submetric metric instances built on-demand when you call `get(aLabel)`
on a labeled metric.
FOG supplies `enum MetricId` which encapsulates whichever is appropriate
and supplies methods for discrimination.
(See {searchfox}`toolkit/components/glean/api/src/private/metric_getter.rs` for details.)

Both IDs have the same format:
```
   3                   2                   1                   0
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| unused  | | |            FOG metric instance ID               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* 32 bits wide (`u32`)
* Top 5 bits are unused by Rust, to align with JS.
* Bit 26 is `1` if the FOG metric instance was created from a definition
  [specified at runtime](jog)
  and thus can be found stored in `factory::__jog_metric_maps`, or
  `0` if the FOG metric instance was created from a definition in a `metrics.yaml`
  file read at compile time and can be found statically stored in the
  `metrics::__glean_metric_maps`
* Bit 25 is `1` if the FOG metric instance was created in response to a labeled metric having
  `get(aLabel)` called on it and thus can be found in
  `metrics::__glean_metric_maps::submetric_maps`, `0` otherwise.
    * It is impossible for both bit 25 and bit 26 to be `1`
      as all submetric instancess are created and stored identically
      regardless of whether their parent metric instance was defined at compile-
      or run-time.
* Lower 25 bits are for identifying a specific FOG metric instance.
    * For FOG submetric instances, the values are assigned per-process in order of access.
      As a result, _they are not stable between processes_
      and _they do not have the same lower bits as their parent labeled metric_.
      (In fact, they couldn't, as we'd then need to allocate bits for the
      [4096](https://mozilla.github.io/glean/book/reference/metrics/labeled_counters.html#labels)
      possible submetrics.)
      See `NEXT_LABELED_SUBMETRIC_ID` for details on how these are assigned,
      and `LABELED_METRICS_TO_IDS` for how the parent labeled metric maps to its submetric ids.
      Both are in
      {searchfox}`toolkit/components/glean/build_scripts/glean_parser_ext/templates/rust.jinja2`.
    * For FOG metric instances created from a definition in a `metrics.yaml`
      file read at compile time, the values are assigned during codegen.
      See {searchfox}`toolkit/components/glean/build_scripts/` for details.
    * For FOG metric instances created from a definition specified at runtime,
      the values are assigned at registration in order of registration.
      See `NEXT_METRIC_ID` in
      {searchfox}`toolkit/components/glean/build_scripts/glean_parser_ext/templates/jog_factory.jinja2`
      for details.


```{admonition} Zeroes are Reserved
The values `0`, `2**25`, `2**26`, and `2**25 + 2**26`
(`0` with every combination of the signal bits set to `0` or `1`)
are *reserved*.
They may be used to signal "no metric found" or "no metric created"
kinds of situations.
```

As a result of this format there is quite a bit of unreachable space in these ids.
We don't expect more than 10k metric definitions in any one product at any one time,
so we're not (at time of writing) worried.

If we run out of space for defined metrics, the build will fail due to one of several `static_assert`s.

If we create more than `2**25 - 1` submetrics in a given run of Firefox Desktop,
things will go very weird at runtime. But that would require more than 33M submetrics,
so we consider this almost impossible as it would require more than
3355 labels for each and every one of the suspected 10k upper limit of defined metrics.

### C++: Just a `uint32_t`, actually

FOG C++ metric instances are the thinnest possible piece of behaviour layered over storage of a single metric id.
Their job is to speak Firefox-Desktop-dialect C++ on the outside
({searchfox}`toolkit/components/glean/bindings/private/`),
and immediately call the `firefox-on-glean` crate's FFI
({searchfox}`toolkit/components/glean/api/src/ffi`)
on the inside.

To call the FFI, the FOG C++ metric instance needs to be able to identify the specific
FOG Rust metric instance that will need to perform the operation.
To do this it stores the FOG Rust metric instance's metric id.

The C++ layer doesn't know or care about whether the metric id is
* assigned during codegen from a metrics definition in a `metrics.yaml` file
* assigned from a metrics definition specified at runtime
    * Though the C++ API doesn't support runtime-defined metrics,
      seeing as it's a compiled language, it *does* have to support JS'
      support of runtime-defined metrics.
* assigned upon first use because it's a submetric instance created in response to calling
  `.get(aLabel)` on a labeled metric

As a result, it doesn't put any new spin on what Rust defines.

```{admonition} GIFFT Cares, though
Though the C++ layer doesn't care about what the metric id's deal is,
[GIFFT](../user/gifft) has to in order to e.g. properly map a
`labeled_counter`'s `counter` submetric's operations to a
keyed `uint` Scalar's subscalar with the correct key.
```

### JS: `metric_entry_t`, `category_entry_t`, and `uint32_t`

(Despite this being for JS, you'll note that all these are C++ types.
This is because the FOG JS API is implemented in C++, for our sins.)

The FOG JS API, unlike the FOG Rust and C++ APIs,
does not statically name the identifiers of all defined metrics,
assigning them their metric ids as parameters to their constructors.
This is due to us not being able to find a suitably performant way to do this.
(e.g. codegenning `webidl`s was not regarded favourably
[at the time](https://bugzilla.mozilla.org/show_bug.cgi?id=1635238)).

Instead, it uses `NamedGetter` and performs string matches on
category and metric names to build FOG JS metric instances.
(See {searchfox}`toolkit/components/glean/bindings/` for details.)

FOG JS metric instances themselves contain FOG C++ metric instances
to which they delegate as much work as possible.
As mentioned in the section on C++,
these FOG C++ metric instances are thin wrappers storing a `uint32_t`
that maps to exactly one specific FOG Rust metric instance.

Where the complication kicks in is in how we build a JS FOG metric instance from the `NamedGetter`.

It boils down to needing to link:
* the (JS conjugation of the) Glean metric's base identifier,
    * (so when someone calls `Glean.someCategory.aMetric`
      we can locate the rest of the information)
* the Glean metric's type
    * (so we locate or create a JS object satisfying the correct webidl interface)
* the FOG C++ (and, thus, the Rust) metric id
    * (so the FOG JS metric instance's FOG C++ metric instance's id is correct
      when it tries to run code over the FFI)

This is the `metric_entry_t` which is laid out as follows:
```
       6                   5                   4               3
 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Metric String Table Index                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   3                   2                   1                   0
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   type  | | |            FOG metric instance ID               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* 64 bits wide (`uint64_t` with a `typdef`)
* Top 32 bits are an index into a string table of all metrics' base identifiers
  (in JS conjugation)
    * We use `perfecthash` to make it fast to go from `Glean.someCategory.aMetric`
      to "Here's the `metric_entry_t` for that",
      but `perfecthash` will always return a value even if the supplied string doesn't match,
      so we need to check the string on retrieval.
    * We also use the entries and string table to supply a list of supported names,
      allowing tab completion of metric names in privileged devtools consoles.
* Bits 31 to 27 (5 bits) denote the Glean metric type
* Bits 26, 25, and the 25 bits below are all exactly as are used to identify FOG Rust
  (and, by extension, C++) metric instances.
    * (It isn't necessary for FOG JS' `metric_entry_t` to contain FOG Rust's metric id,
      but it is convenient.)

We have a similar system to support categories since
`Glean.someCategory` itself needs to return a JS object.
It isn't nearly as complicated as they're all the same type
and none of them need to correspond to any instance in C++ or Rust.
We do this with `category_entry_t` which is laid out as follows:
```
   3                   2                   1                   0
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Category String Table Index                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* 32 bits wide (`uint32_t` with a `typedef`)
* All 32 bits are an index into a string table of all metrics' category names
  (in JS conjugation)

```{admonition} We maybe should have a stronger MetricId type in FOG JS
Unfortunately, due to both the FOG Rust/C++ metric ids being 32 bits wide,
and the lower 32 bits of the JS `metric_entry_t` being useful for constructing
FOG JS metric instances, in the FOG JS impl, a `uint32_t aMetricId` could mean either
a) The equivalent of the FOG Rust/C++ metric id, suitable for use in FFI, or
b) That *plus* 5 bits denoting the Glean metric type.
So **be careful**.
```

## Identifying a FOG Ping: `ping_entry_t` and `u32`/`uint32_t`

Luckily, Glean Pings are far less numerous and far less varied than Glean Metrics.

Unluckily, they need to support a similar level of complication when it comes to
supporting string tables for the FOG JS API and supporting [runtime registration](jog).

So to identify a FOG Ping instance in Rust or C++ we use a ping id which is:
```
   3                   2                   1                   0
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           unused              | |    FOG Ping instance ID     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* 32 bits wide (`u32` or `uint32_t`)
* The top 16 bits are unused, to align with JS
* Bit 15 is the `RUNTIME_PING_BIT` which is `1`
  if the FOG ping instance was created from a definition supplied [at runtime](jog),
  and `0` if it corresponds to a definition supplied at compile time in a `pings.yaml`.
* The bottom 15 bits identify a specific, named ping.

```{admonition} Zeroes are Reserved for Pings As Well
The values `0` and `2**15` (`0` with or without the signal bit set)
are *reserved*.
They may be used to signal "no ping found" or "no ping created"
kinds of situations.
```

To identify a FOG Ping instance in JS we use `ping_entry_t` which is:
```
   3                   2                   1                   0
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Ping String Table Index    | |    FOG Ping instance ID     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* 32 bits wide (`uint32_t` with a `typedef`)
* The top 16 bits are an index into the ping string table
* Bit 15 and the bottom 15 bits are exactly as are used in Rust or C++ to identify a specific Ping instance

```{admonition} We maybe should have a stronger PingId type in FOG JS
Unfortunately, due to all of the ping ids and `ping_entry_t` being 32 bits wide,
in the FOG JS impl a `uint32_t aPingId` could mean anything.
It's likely to not be a full `ping_entry_t` unless it uses that naming,
but that's just a `typedef`.
So **be careful**.
```
