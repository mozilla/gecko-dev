# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_ping_filters import (
    FOG_DELETION_REQUEST_PING,
    FOGDocTypePingFilter,
)
from telemetry_harness.fog_testcase import FOGTestCase

BASELINE = FOGDocTypePingFilter("baseline")
FOG_USAGE_REPORTING = FOGDocTypePingFilter("usage-reporting")
FOG_USAGE_DELETION_REQUEST_PING = FOGDocTypePingFilter("usage-deletion-request")
CANARY_USAGE_PROFILE_ID = "beefbeef-beef-beef-beef-beeefbeefbee"


class TestUsageReporting(FOGTestCase):
    """Tests for FOG usage-reporting ping and usage-id cycling."""

    def disable_usage_reporting(self):
        """Disable usage reporting in the current browser."""
        self.marionette.instance.profile.set_persistent_preferences(
            {"datareporting.usage.uploadEnabled": False}
        )
        self.marionette.set_pref("datareporting.usage.uploadEnabled", False)

    def enable_usage_reporting(self):
        """Enable usage reporting in the current browser."""
        self.marionette.instance.profile.set_persistent_preferences(
            {"datareporting.usage.uploadEnabled": True}
        )
        self.marionette.set_pref("datareporting.usage.uploadEnabled", True)

    def test_deletion_request(self):
        """
        Test the "usage-reporting" ping behaviour and usage-id cycling when disabling telemetry in general.

        We do not expect a "usage-deletion-request" ping.  The "deletion-request" ping should not include the usage-id.
        """

        ping1 = self.wait_for_ping(
            self.restart_browser,
            FOG_USAGE_REPORTING,
            ping_server=self.fog_ping_server,
        )

        self.assertNotIn("ping_info", ping1["payload"])
        self.assertNotIn("client_info", ping1["payload"])

        metrics = ping1["payload"]["metrics"]
        self.assertNotIn("legacy.telemetry.client_id", metrics["uuid"])
        self.assertNotIn("legacy.telemetry.profile_group_id", metrics["uuid"])

        self.assertIn("usage.profile_id", metrics["uuid"])
        usage_id1 = metrics["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(usage_id1)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, usage_id1)

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
            FOG_USAGE_REPORTING,
            ping_server=self.fog_ping_server,
        )

        self.assertIn("usage.profile_id", ping3["payload"]["metrics"]["uuid"])
        usage_id2 = ping3["payload"]["metrics"]["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(usage_id2)

        self.assertNotEqual(usage_id1, usage_id2)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, usage_id2)

    def test_usage_deletion_request(self):
        """
        Test the "usage-reporting" ping behaviour and usage-id cycling when disabling telemetry.

        We expect a "usage-deletion-request" ping, and it should include the usage-id.
        """

        ping1 = self.wait_for_ping(
            self.restart_browser,
            FOG_USAGE_REPORTING,
            ping_server=self.fog_ping_server,
        )

        self.assertNotIn("ping_info", ping1["payload"])
        self.assertNotIn("client_info", ping1["payload"])

        metrics = ping1["payload"]["metrics"]
        self.assertNotIn("legacy.telemetry.client_id", metrics["uuid"])
        self.assertNotIn("legacy.telemetry.profile_group_id", metrics["uuid"])

        self.assertIn("usage.profile_id", metrics["uuid"])
        usage_id1 = metrics["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(usage_id1)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, usage_id1)

        # `usage-deletion-request` ping will have the `usage.profile_id`.
        #
        # N.b.: the `usage-deletion-request` ping has `include_info_sections:
        # false`, so no `payload.ping_info.reason` is available for inspection.
        # We still set the reason in case details change.
        ping2 = self.wait_for_ping(
            self.disable_usage_reporting,
            FOG_USAGE_DELETION_REQUEST_PING,
            ping_server=self.fog_ping_server,
        )
        metrics = ping2["payload"]["metrics"]
        self.assertIn("usage.profile_id", metrics["uuid"])
        usage_id2 = metrics["uuid"]["usage.profile_id"]
        self.assertEqual(usage_id1, usage_id2)

        self.enable_usage_reporting()
        ping3 = self.wait_for_ping(
            self.restart_browser,
            FOG_USAGE_REPORTING,
            ping_server=self.fog_ping_server,
        )

        self.assertIn("usage.profile_id", ping3["payload"]["metrics"]["uuid"])
        usage_id3 = ping3["payload"]["metrics"]["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(usage_id3)

        self.assertNotEqual(usage_id1, usage_id3)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, usage_id3)

    def test_enabled_state_after_restart(self):
        """
        Test that the "usage-reporting" ping remains enabled and the usage ID remains fixed when restarting the browser.
        """

        self.disable_usage_reporting()
        # Not guaranteed to send a "usage-reporting" ping.
        self.enable_usage_reporting()

        # But restarting should send a "usage-reporting ping".
        ping1 = self.wait_for_ping(
            self.restart_browser,
            FOG_USAGE_REPORTING,
            ping_server=self.fog_ping_server,
        )

        metrics = ping1["payload"]["metrics"]
        self.assertIn("usage.profile_id", metrics["uuid"])
        usage_id1 = metrics["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(usage_id1)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, usage_id1)

        # Restarting again should maintain enabled state.
        ping2 = self.wait_for_ping(
            self.restart_browser,
            FOG_USAGE_REPORTING,
            ping_server=self.fog_ping_server,
        )

        metrics = ping2["payload"]["metrics"]
        self.assertIn("usage.profile_id", metrics["uuid"])
        usage_id2 = metrics["uuid"]["usage.profile_id"]

        self.assertEqual(usage_id1, usage_id2)

    def test_disabled_state_after_restart(self):
        """
        Test that the "usage-reporting" ping remains disabled and the usage ID remains null when restarting the browser.
        """

        self.enable_usage_reporting()

        # Disabling should send a "usage-deletion-request".
        ping1 = self.wait_for_ping(
            self.disable_usage_reporting,
            FOG_USAGE_DELETION_REQUEST_PING,
            ping_server=self.fog_ping_server,
        )
        metrics = ping1["payload"]["metrics"]
        self.assertIn("usage.profile_id", metrics["uuid"])
        usage_id1 = metrics["uuid"]["usage.profile_id"]
        self.assertIsValidUUID(usage_id1)
        self.assertNotEqual(CANARY_USAGE_PROFILE_ID, usage_id1)

        current_num_pings = len(self.fog_ping_server.pings)

        # It's not easy to wait for the _absence_ of a ping.  So restart and
        # wait for a _different_ ping, then verify we didn't get any additional
        # "usage-reporting" pings.
        _ping2 = self.wait_for_ping(
            self.restart_browser,
            BASELINE,
            ping_server=self.fog_ping_server,
        )

        self.assertIs(
            len(self.fog_ping_server.pings) > current_num_pings,
            True,
            "Expected at least 'baseline' ping",
        )
        for ping in self.fog_ping_server.pings[(current_num_pings + 1) :]:
            self.assertIs(
                FOG_USAGE_REPORTING(ping), False, "Expected no 'usage-reporting' pings"
            )
