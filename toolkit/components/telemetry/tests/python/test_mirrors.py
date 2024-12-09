# This Source Code Form is subject to the terms of Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import unittest
from os import path
from pathlib import Path

import mozunit
from glean_parser import parser, util

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

# This import can error, but in that case we want the test to fail anyway.
from mozbuild.base import MozbuildObject

build = MozbuildObject.from_environment()


# Generator to yield metrics.
def mirroring_metrics(objs):
    for category, metrics in objs.value.items():
        for metric in metrics.values():
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
    # Alas, there is a pattern where Telemetry event definitions will have extra
    # keys that are only used by _some_ of the method+object pairs, so we can't
    # assert that the lists are the same.
    # So, instead, assert all extras allowed in the metric exist in the probe.
    for key in metric.allowed_extra_keys:
        # `event` metrics may have a `value` extra for mapping to a
        # mirror's value parameter.
        if key == "value":
            continue
        assert (
            key in probe.extra_keys
        ), f"Key {key} not in mirrored event probe {probe.identifier}. Be sure to add it."


# Histograms are compatible with metrics if they are
#  * keyed if the metric is labeled_*
#  * of a suitable `kind` (e.g. "linear" or "exponential")
def ensure_compatible_histogram(metric, probe):
    if metric.type == "counter":
        assert (
            probe.kind() == "count"
        ), f"Metric {metric.identifier()} is a `counter` mapping to a histogram, but {probe.name()} isn't a 'count' Histogram (is '{probe.kind()}')."
        return
    elif metric.type == "labeled_counter":
        assert (
            probe.kind() == "boolean"
        ), f"Metric {metric.identifier()} is a `labeled_counter` mapping to a histogram, but {probe.name()} isn't a 'boolean' Histogram (is '{probe.kind()}')."
        assert metric.ordered_labels == [
            "false",
            "true",
        ], f"Metric {metric.identifier()} is a `labeled_counter` mapping to a boolean histogram, but it doesn't have labels ['false', 'true'] (has {metric.ordered_labels} instead)."
        return

    assert probe.kind() in [
        "linear",
        "exponential",
    ], f"Histogram {probe.name()}'s kind is not mirror-compatible."
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
    ), f"Metric {metric.identifier()}'s type ({metric.type}) must have appropriate keyedness in the mirrored scalar probe {probe.name}."

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
    ), f"Metric {metric.identifier()}'s type ({metric.type}) requires a mirror probe scalar of kind '{TYPE_MAP[metric.type]}' which doesn't match mirrored scalar probe {probe.name}'s kind ({probe.kind})"


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


if __name__ == "__main__":
    mozunit.main()
