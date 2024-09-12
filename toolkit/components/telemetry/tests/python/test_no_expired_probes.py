# This Source Code Form is subject to the terms of Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import unittest
from os import path

import mozunit

TELEMETRY_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(TELEMETRY_ROOT_PATH)
# The parsers live in a subdirectory of "build_scripts", account for that.
# NOTE: if the parsers are moved, this logic will need to be updated.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
from mozparsers import parse_events, parse_histograms, parse_scalars


def is_expired(app_version_major, probe_expiration):
    if probe_expiration in ["never", "default"]:
        return False

    if int(app_version_major) < int(probe_expiration.split(".", 1)[0]):
        return False

    return True


class TestNoExpiredProbes(unittest.TestCase):
    def test_no_expired_probes(self):
        """Ensure there are no expired histograms, events, or scalars
        in Histograms.json, Events.yaml, or Scalars.yaml, respectively."""

        with open("browser/config/version.txt", "r") as version_file:
            app_version = version_file.read().strip()

        app_version_major = app_version.split(".", 1)[0]

        hgrams = parse_histograms.from_files(
            [path.join(TELEMETRY_ROOT_PATH, "Histograms.json")]
        )
        for hgram in hgrams:
            if hgram.name().startswith("TELEMETRY_TEST_"):
                # We ignore test histograms which are permitted to be expired.
                continue

            self.assertFalse(
                is_expired(app_version_major, hgram.expiration()),
                f"Histogram {hgram.name()} is expired (expiration: {hgram.expiration()}).",
            )

        scalars = parse_scalars.load_scalars(
            path.join(TELEMETRY_ROOT_PATH, "Scalars.yaml")
        )
        for scalar in scalars:
            if scalar.category == "telemetry.test":
                # We ignore test scalars which are permitted to be expired.
                continue

            self.assertFalse(
                is_expired(app_version_major, scalar.expires),
                f"Scalar {scalar.label} is expired (expires: {scalar.expires}).",
            )

        events = parse_events.load_events(
            path.join(TELEMETRY_ROOT_PATH, "Events.yaml"), True
        )
        for event in events:
            if event.category == "telemetry.test":
                # We ignore test events which are permitted to be expired.
                continue

            self.assertFalse(
                is_expired(app_version_major, event.expiry_version),
                f"Event definition {event.identifier} is expired (expiry_version: {event.expiry_version}).",
            )


if __name__ == "__main__":
    mozunit.main()
