# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path

import mozunit
import yaml

DESKTOP_METRICS_FILE = Path(__file__).parents[2].joinpath("metrics.yaml").absolute()
MOBILE_METRICS_FILE = (
    Path(__file__)
    .parents[5]
    .joinpath("mobile", "android", "fenix", "app", "metrics.yaml")
    .absolute()
)


def get_relevant_metrics_from_yaml_file(filename: str):
    metrics = set()

    for group_key, group_value in yaml.safe_load(open(filename)).items():
        if type(group_value) is not dict:
            continue

        for metric_key, metric_value in group_value.items():
            if "broken-site-report" in metric_value.get("send_in_pings", []):
                metrics.add(".".join([group_key, metric_key]))

    return metrics


def test_mobile_metrics_match_desktop():
    desktop_metrics = get_relevant_metrics_from_yaml_file(DESKTOP_METRICS_FILE)
    mobile_metrics = get_relevant_metrics_from_yaml_file(MOBILE_METRICS_FILE)

    assert desktop_metrics == mobile_metrics


if __name__ == "__main__":
    mozunit.main()
