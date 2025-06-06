# This Source Code Form is subject to the terms of Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import unittest
from os import path
from pathlib import Path

import mozunit
from glean_parser import metrics, parser, util

TELEMETRY_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(TELEMETRY_ROOT_PATH)
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
from mozparsers import parse_events, parse_histograms, parse_scalars

FOG_ROOT_PATH = path.abspath(path.join(TELEMETRY_ROOT_PATH, path.pardir, "glean"))
sys.path.append(FOG_ROOT_PATH)
import metrics_index

sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
from run_glean_parser import GIFFT_TYPES

MIRROR_TYPES = {
    metric_type: [
        probe_type
        for probe_type in GIFFT_TYPES.keys()
        if metric_type in GIFFT_TYPES[probe_type]
    ]
    for (probe_type, metric_types) in GIFFT_TYPES.items()
    for metric_type in metric_types
}

# Event probes for which we permit the weaker event compatiblity checks:
# only ensuring that all the metric's extra keys are present in the probe,
# not ensuring that all the probe's extra keys are defined in the metric.
WEAKER_EVENT_COMPATIBILITY_PROBES = [
    "security.ui.protectionspopup#click",
    "intl.ui.browserLanguage#action",
    "privacy.ui.fpp#click",
    "slow_script_warning#shown",
    "address#address_form",
    "pwmgr#mgmt_interaction",
    "relay_integration#popup_option",
    "relay_integration#mask_panel",
    "security.ui.certerror#click",
    "security.ui.certerror#load",
]

# Event probes for which we permit there to be no mirror.
# Only included here are those with combinations of method+object that are unused.
UNMIRRORED_EVENT_ALLOWLIST = [
    "intl.ui.browserLanguage#action",
    "pwmgr#mgmt_interaction",
    "pwmgr#open_management",
]

# Histograms permitted to be not mirrored, pending bug 1949494.
BUG_1949494_ALLOWLIST = [
    "GC_REASON_2",
    "GC_IS_COMPARTMENTAL",
    "GC_ZONE_COUNT",
    "GC_ZONES_COLLECTED",
    "GC_MS",
    "GC_BUDGET_MS_2",
    "GC_BUDGET_WAS_INCREASED",
    "GC_SLICE_WAS_LONG",
    "GC_ANIMATION_MS",
    "GC_MAX_PAUSE_MS_2",
    "GC_PREPARE_MS",
    "GC_MARK_MS",
    "GC_SWEEP_MS",
    "GC_COMPACT_MS",
    "GC_MARK_ROOTS_US",
    "GC_MARK_GRAY_MS_2",
    "GC_MARK_WEAK_MS",
    "GC_SLICE_MS",
    "GC_SLOW_PHASE",
    "GC_SLOW_TASK",
    "GC_MMU_50",
    "GC_RESET",
    "GC_RESET_REASON",
    "GC_NON_INCREMENTAL",
    "GC_NON_INCREMENTAL_REASON",
    "GC_MINOR_REASON",
    "GC_MINOR_REASON_LONG",
    "GC_MINOR_US",
    "GC_NURSERY_BYTES_2",
    "GC_PRETENURE_COUNT_2",
    "GC_BUDGET_OVERRUN",
    "GC_NURSERY_PROMOTION_RATE",
    "GC_TENURED_SURVIVAL_RATE",
    "GC_MARK_RATE_2",
    "GC_TIME_BETWEEN_S",
    "GC_TIME_BETWEEN_SLICES_MS",
    "GC_SLICE_COUNT",
    "GC_EFFECTIVENESS",
    "GC_PARALLEL_MARK",
    "GC_PARALLEL_MARK_SPEEDUP",
    "GC_PARALLEL_MARK_UTILIZATION",
    "GC_PARALLEL_MARK_INTERRUPTIONS",
    "GC_TASK_START_DELAY_US",
    "DESERIALIZE_BYTES",
    "DESERIALIZE_ITEMS",
    "DESERIALIZE_US",
]

# This import can error, but in that case we want the test to fail anyway.
from mozbuild.base import MozbuildObject

build = MozbuildObject.from_environment()


# Generator to yield metrics.
def mirroring_metrics(objs):
    for category, metric_objs in objs.value.items():
        for metric in metric_objs.values():
            if (
                hasattr(metric, "telemetry_mirror")
                and metric.telemetry_mirror is not None
            ):
                assert (
                    metric.type in MIRROR_TYPES.keys()
                ), f"{metric.type} is not a GIFFT-supported type."
                yield metric


# Events are compatible if their extra keys are compatible.
def ensure_compatible_event(metric, probe):
    # There is a pattern where Telemetry event definitions will have extra
    # keys that are only used by _some_ of the method+object pairs.
    # We only permit that pattern for old definitions that rely on it.
    if probe.identifier in WEAKER_EVENT_COMPATIBILITY_PROBES:
        for key in metric.allowed_extra_keys:
            # `event` metrics may have a `value` extra for mapping to a
            # mirror's value parameter.
            if key == "value":
                continue
            assert (
                key in probe.extra_keys
            ), f"Key {key} not in mirrored event probe {probe.identifier}. Be sure to add it."
    else:
        assert (
            metric.allowed_extra_keys == probe.extra_keys
            or metric.allowed_extra_keys == sorted(probe.extra_keys + ["value"])
        ), f"Metric {metric.identifier()}'s extra keys {metric.allowed_extra_keys} are not the same as probe {probe.identifier}'s extras {probe.extra_keys}."


# Histograms are compatible with metrics if they are
#  * keyed if the metric is labeled_*
#  * of a suitable `kind` (e.g. "linear", "exponential", or "enumerated")
def ensure_compatible_histogram(metric, probe):
    if metric.type == "counter":
        assert (
            probe.kind() == "count"
        ), f"Metric {metric.identifier()} is a `counter` mapping to a histogram, but {probe.name()} isn't a 'count' Histogram (is '{probe.kind()}')."
        return
    elif metric.type == "labeled_counter":
        if probe.kind() == "boolean":
            assert metric.ordered_labels == [
                "false",
                "true",
            ], f"Metric {metric.identifier()} is a `labeled_counter` mapping to a boolean histogram, but it doesn't have labels ['false', 'true'] (has {metric.ordered_labels} instead)."
        elif probe.kind() == "count":
            assert (
                probe.keyed()
            ), f"Metric {metric.identifier()} is a `labeled_counter` mapping to un-keyed 'count' histogram {probe.name()}."
        elif probe.kind() == "categorical":
            assert (
                metric.ordered_labels == probe.labels()
            ), f"Metric {metric.identifier()} is a `labeled_counter` mapping to categorical histogram {probe.name()}, but the labels don't match."
        else:
            assert (
                False
            ), f"Metric {metric.identifier()} is a `labeled_counter` mapping to a histogram, but {probe.name()} isn't a 'boolean, keyed 'count', or 'categorical' Histogram (is '{probe.kind()}')."
        return

    assert probe.kind() in [
        "linear",
        "exponential",
        "enumerated",
    ], f"Histogram {probe.name()}'s kind is not mirror-compatible."

    # We cannot assert that all enumerated hgrams are custom distributions
    # (some are e.g. timing_distributions), nor that all custom distributions
    # mirror to enumerated hgrams (some map to linear/exponential).
    # But in the case of a custom mapping to an enumerated, we check buckets.
    if probe.kind() == "enumerated" and metric.type in (
        "custom_distribution",
        "labeled_custom_distribution",
    ):
        n_values_plus_one = probe._n_buckets
        assert (
            metric.range_min == 0
            and metric.histogram_type == metrics.HistogramType.linear
            and metric.bucket_count == n_values_plus_one
        ), f"Metric {metric.identifier()} mapping to enumerated histogram {probe.name()} must have a range that starts at 0 (is {metric.range_min}), must have `linear` bucket allocation (is {metric.histogram_type}), and must have one more bucket than the probe's n_values (is {metric.bucket_count}, should be {n_values_plus_one})."
    assert (
        hasattr(metric, "labeled") and metric.labeled
    ) == probe.keyed(), f"Metric {metric.identifier()}'s labeledness must match mirrored histogram probe {probe.name()}'s keyedness."


# Scalars are compatible with metrics if they are
#  * keyed when necessary (e.g. when the metric is labeled_* or complex)
#  * of a compatible `kind` (e.g. `uint` for `counter` or `quantity`)
def ensure_compatible_scalar(metric, probe):
    mirror_should_be_keyed = (
        hasattr(metric, "labeled") and metric.labeled
    ) or metric.type in ["string_list", "rate"]
    assert (
        mirror_should_be_keyed == probe.keyed
    ), f"Metric {metric.identifier()}'s type ({metric.type}) must have appropriate keyedness in the mirrored scalar probe {probe.label}."

    TYPE_MAP = {
        "boolean": "boolean",
        "labeled_boolean": "boolean",
        "counter": "uint",
        "labeled_counter": "uint",
        "string": "string",
        "string_list": "boolean",
        "timespan": "uint",
        "uuid": "string",
        "url": "string",
        "datetime": "string",
        "quantity": "uint",
        "labeled_quantity": "uint",
        "rate": "uint",
    }
    assert (
        TYPE_MAP[metric.type] == probe.kind
    ), f"Metric {metric.identifier()}'s type ({metric.type}) requires a mirror probe scalar of kind '{TYPE_MAP[metric.type]}' which doesn't match mirrored scalar probe {probe.label}'s kind ({probe.kind})"


class TestTelemetryMirrors(unittest.TestCase):
    def test_compatible_mirrors(self):
        """Glean metrics can be mirrored via the `telemetry_mirror` property to
        Telemetry probes. Ensure the mirror is compatible with the metric."""

        # Step 1, parse all Glean metrics and Telemetry probes:
        metrics_yamls = [Path(build.topsrcdir, x) for x in metrics_index.metrics_yamls]
        # Accept any value of expires.
        parser_options = {
            "allow_reserved": True,
            "custom_is_expired": lambda expires: False,
            "custom_validate_expires": lambda expires: True,
        }
        objs = parser.parse_objects(metrics_yamls, parser_options)
        assert not util.report_validation_errors(objs)

        hgrams = list(
            parse_histograms.from_files(
                [path.join(TELEMETRY_ROOT_PATH, "Histograms.json")]
            )
        )

        scalars = list(
            parse_scalars.load_scalars(path.join(TELEMETRY_ROOT_PATH, "Scalars.yaml"))
        )

        events = list(
            parse_events.load_events(
                path.join(TELEMETRY_ROOT_PATH, "Events.yaml"), True
            )
        )

        # Step 2: For every mirroring Glean metric, assert its mirror Telemetry
        # probe is compatible.
        for metric in mirroring_metrics(objs):
            mirror = metric.telemetry_mirror.split("#")[-1]
            found = False
            for probe_type in MIRROR_TYPES[metric.type]:
                if probe_type == "Event":
                    for event in events:
                        for enum in event.enum_labels:
                            event_id = event.category_cpp + "_" + enum
                            if event_id == mirror:
                                found = True
                                ensure_compatible_event(metric, event)
                                break
                        if found:
                            break
                elif probe_type == "Histogram":
                    # To mirror to a Histogram if you also mirror to another type,
                    # you must prefix your mirror with "h#"
                    if len(
                        MIRROR_TYPES[metric.type]
                    ) > 1 and not metric.telemetry_mirror.startswith("h#"):
                        continue
                    for hgram in hgrams:
                        if hgram.name() == mirror:
                            found = True
                            ensure_compatible_histogram(metric, hgram)
                            break
                elif probe_type == "Scalar":
                    for scalar in scalars:
                        if scalar.enum_label == mirror:
                            found = True
                            ensure_compatible_scalar(metric, scalar)
                            break
                else:
                    assert (
                        False
                    ), f"mirror probe type {MIRROR_TYPES[metric.type]} isn't recognized."
            assert (
                found
            ), f"Mirror {metric.telemetry_mirror} not found for metric {metric.identifier()}"

        # Step 3: Forbid unmirrored-to probes
        for event in events:
            for enum in event.enum_labels:
                event_id = event.category_cpp + "_" + enum
                if event.identifier in UNMIRRORED_EVENT_ALLOWLIST:
                    # Some combinations of object+method are never used,
                    # but are nevertheless possible.
                    continue
                if event.category in ("telemetry.test", "telemetry.test.second"):
                    continue
                assert any(
                    metric.telemetry_mirror == event_id
                    for metric in mirroring_metrics(objs)
                ), f"No mirror metric found for event probe {event.identifier}."

        for hgram in hgrams:
            if hgram.keyed() and hgram.kind() in ("categorical", "boolean"):
                continue  # bug 1960567
            if hgram.name() in BUG_1949494_ALLOWLIST:
                continue  # bug 1949494
            if hgram.name().startswith("TELEMETRY_TEST_"):
                continue
            assert any(
                metric.telemetry_mirror == hgram.name()
                or metric.telemetry_mirror == "h#" + hgram.name()
                for metric in mirroring_metrics(objs)
            ), f"No mirror metric found for histogram probe {hgram.name()}."

        for scalar in scalars:
            if scalar.label == "mathml.doc_count":
                continue  # bug 1962732
            if scalar.category in ("telemetry", "telemetry.discarded"):
                # Internal Scalars for use inside the Telemetry component.
                continue
            if scalar.category == "telemetry.test":
                continue
            assert any(
                metric.telemetry_mirror == scalar.enum_label
                for metric in mirroring_metrics(objs)
            ), f"No mirror metric found for scalar probe {scalar.label}."


if __name__ == "__main__":
    mozunit.main()
