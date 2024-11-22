# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_ping_filters import (
    FOG_DELETION_REQUEST_PING,
    FOGDocTypePingFilter,
)
from telemetry_harness.fog_testcase import FOGTestCase

FOG_DAU_REPORTING = FOGDocTypePingFilter("usage-reporting")
CANARY_USAGE_PROFILE_ID = "beefbeef-beef-beef-beef-beeefbeefbee"


class TestDauReporting(FOGTestCase):
    """Tests for FOG usage-reporting ping and dau-id cycling."""

    def test_dau_reporting(self):
        """
        Test the "usage-reporting" ping behaviour and dau-id cycling when disabling telemetry.
        """

        ping1 = self.wait_for_ping(
            self.restart_browser,
            FOG_DAU_REPORTING,
            ping_server=self.fog_ping_server,
        )

        self.assertNotIn("ping_info", ping1["payload"])
        self.assertNotIn("client_info", ping1["payload"])

        metrics = ping1["payload"]["metrics"]
        self.assertNotIn("legacy.telemetry.client_id", metrics["uuid"])
        self.assertNotIn("legacy.telemetry.profile_group_id", metrics["uuid"])

        self.assertIn("usage.profile_id", metrics["uuid"])
        dau_id1 = metrics["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(dau_id1)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, dau_id1)

        # Regular `deletion-request` ping won't have the `usage.profile_id`.
        # We just wait for it to know it happened.
        _ping2 = self.wait_for_ping(
            self.disable_telemetry,
            FOG_DELETION_REQUEST_PING,
            ping_server=self.fog_ping_server,
        )

        self.enable_telemetry()
        ping3 = self.wait_for_ping(
            self.restart_browser,
            FOG_DAU_REPORTING,
            ping_server=self.fog_ping_server,
        )

        self.assertIn("usage.profile_id", ping3["payload"]["metrics"]["uuid"])
        dau_id2 = ping3["payload"]["metrics"]["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(dau_id2)

        self.assertNotEqual(dau_id1, dau_id2)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, dau_id2)
