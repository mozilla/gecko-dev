# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import json
import os
import subprocess
import unittest
from functools import lru_cache
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any, Dict, List, TypedDict

import mozunit
import yaml

FILE = Path(__file__)
TEST_DIR = FILE.parent
NIMBUS_DIR = FILE.parent.parent.parent
METRICS_PATH = NIMBUS_DIR / "metrics.yaml"
TOPSRCDIR = NIMBUS_DIR.parent.parent.parent
MACH = TOPSRCDIR / "mach"


def run_mach(args: List[str], *, env: Dict[str, str] = None):
    if os.name == "nt":
        command = ["py", str(MACH), *args]
    else:
        command = [str(MACH), *args]

    cmd_env = dict(os.environ)
    cmd_env.update(env)

    try:
        subprocess.run(
            command,
            env=cmd_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=True,
            text=True,
        )
    except Exception as e:
        print(f"Exception when running mach command: {command}")
        print(f"output:\n{e.stdout}\n")
        raise


@lru_cache(maxsize=None)
def get_metrics():
    with METRICS_PATH.open() as f:
        return yaml.safe_load(f)


class TargetingPref(TypedDict):
    pref_name: str
    metric_name: str
    type: str


class TargetingContextDump(TypedDict):
    prefs: List[TargetingPref]
    values: List[str]


@lru_cache(maxsize=None)
def dump_targeting_context() -> TargetingContextDump:
    # Because we cannot run `mach xpcshell` in CI, we also allow our callers
    # to set the DUMP_TARGETING_CONTEXT_PATH environment variable to the path
    # of an existing dump file.
    dump_targeting_context_path = os.environ.get("DUMP_TARGETING_CONTEXT_PATH")
    if dump_targeting_context_path:
        print(
            "Loading dump from DUMP_TARGETING_CONTEXT_PATH = "
            f"{dump_targeting_context_path}"
        )

        with open(dump_targeting_context_path) as f:
            return json.load(f)

    # If we're running locally, we can use `mach xpcshell`.
    with TemporaryDirectory() as temp_dir:
        dump_file = Path(temp_dir) / "dump.json"
        run_mach(
            ["xpcshell", str(TEST_DIR / "dump-targeting-context.js")],
            env={
                "DUMP_TARGETING_CONTEXT_PATH": str(dump_file),
            },
        )

        with dump_file.open() as f:
            return json.load(f)


class TargetingContextMetricTests(unittest.TestCase):
    """Tests for the nimbus_targeting_environment and nimbus_targeting_context
    Glean categories.
    """

    def _assert_metric_matches_defaults(
        self,
        category_name: str,
        metric_name: str,
        defaults: Dict[str, Any],
        metric: Dict[str, Any],
    ):
        for key in (
            "bugs",
            "data_reviews",
            "notification_emails",
            "expires",
            "send_in_pings",
        ):
            self.assertEqual(
                defaults[key],
                metric[key],
                f"value {key} should match default in {category_name}.{metric_name}",
            )

    def test_nimbus_targeting_categories_consistency(self):
        """Testing each metric in the nimbus_targeting_{environment,context}
        categories have consistent fields
        """
        metrics = get_metrics()

        targeting_environment = metrics["nimbus_targeting_environment"]
        targeting_context = metrics["nimbus_targeting_context"]

        # This is used as the defaults for each metric.
        defaults = targeting_environment["targeting_context_value"]

        for metric_name, metric in targeting_environment.items():
            if metric_name == "targeting_context_value":
                # No point comparing the metric to itself.
                continue

            self._assert_metric_matches_defaults(
                "nimbus_targeting_environment",
                metric_name,
                defaults,
                metric,
            )

        for metric_name, metric in targeting_context.items():
            self._assert_metric_matches_defaults(
                "nimbus_targeting_context",
                metric_name,
                defaults,
                metric,
            )

    def test_nimbus_targeting_context_metrics(self):
        """Testing the nimbus_targeting_context metrics are consistent with the
        Nimbus targeting context dump.
        """
        dump = dump_targeting_context()
        metrics = get_metrics()

        nimbus_targeting_context = metrics["nimbus_targeting_context"]

        for attr in dump["attrs"]:
            metric_name = attr["metric_name"]
            attr_name = attr["attr_name"]

            self.assertIn(
                metric_name,
                nimbus_targeting_context,
                f"attribute {attr_name} should appear as"
                f"nimbus_targeting_context.{metric_name}",
            )

        dumped_names = set(a["metric_name"] for a in dump["attrs"])
        for metric_name in nimbus_targeting_context:
            self.assertIn(
                metric_name,
                dumped_names,
                f"metric {metric_name} should appear in targeting context dump",
            )

    def test_nimbus_targeting_environment_attr_errors(self):
        """Testing that each attribute in the targeting context is listed as an
        option for the nimbus_targeting_environment.attr_eval_errors metric and
        that the list of labels is sorted.
        """
        attrs = dump_targeting_context()["attrs"]
        metrics = get_metrics()
        metric = metrics["nimbus_targeting_environment"]["attr_eval_errors"]
        labels = metric["labels"]

        for attr in attrs:
            attr_name = attr["attr_name"]
            self.assertIn(
                attr_name,
                labels,
                f"attribute {attr_name} should appear in "
                "nimbus_targeting_environment.attr_errors.labels",
            )

        self.assertTrue(
            labels == sorted(labels),
            "nimbus_targeting_environment.attr_errors.labels should be sorted",
        )

    def test_nimbus_targeting_environment_pref_errors(self):
        """Testing that each pref available to the targeting context is
        listed as an option for the
        nimbus_targeting_environment.pref_type_errors metric and that the list
        of labels is sorted.
        """
        dump = dump_targeting_context()
        metrics = get_metrics()
        metric = metrics["nimbus_targeting_environment"]["pref_type_errors"]
        labels = metric["labels"]

        for pref in dump["prefs"]:
            pref_name = pref["pref_name"]
            self.assertIn(
                pref_name,
                labels,
                f"pref {pref_name} should appear in "
                "nimbus_targeting_environment.pref_type_errors.labels",
            )

        self.assertTrue(
            labels == sorted(labels),
            "nimbus_targeting_environment.pref_type_errors.labels are sorted",
        )

    def test_nimbus_targeting_environment_pref_values(self):
        """Testing the nimbus_targeting_environment.prefValues metric is
        consistent with the Nimbus targeting context dump
        """
        dump = dump_targeting_context()
        metrics = get_metrics()
        metric = metrics["nimbus_targeting_environment"]["pref_values"]
        properties = metric["structure"]["properties"]

        for pref in dump["prefs"]:
            pref_name = pref["pref_name"]
            field_name = pref["field_name"]
            field_type = pref["type"]

            self.assertIn(
                field_name,
                properties,
                f"pref {pref_name} should appear as {field_name} in "
                "nimbus_targeting_environment.pref_values",
            )

            self.assertEqual(
                field_type,
                properties[field_name]["type"],
                f"pref {pref_name} should have type {field_type} in "
                "nimbus_targeting_environment.pref_values",
            )

        dumped_pref_field_names = set(p["field_name"] for p in dump["prefs"])
        for field_name in properties:
            self.assertIn(
                field_name,
                dumped_pref_field_names,
                f"field {field_name} should correspond to a pref in the "
                "targeting context dump",
            )


if __name__ == "__main__":
    mozunit.main(runwith="unittest")
