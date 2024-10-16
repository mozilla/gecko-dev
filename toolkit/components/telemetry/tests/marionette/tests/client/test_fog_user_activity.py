# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_testcase import FOGTestCase


class BaselineRidealongFilter:
    """Expecting a `baseline` ping and one more ping"""

    def __init__(self, other_ping):
        self.expected_pings = ["baseline", other_ping]

    def __call__(self, ping):
        doc_type = ping["request_url"]["doc_type"]
        return doc_type in self.expected_pings


DauReportFilter = BaselineRidealongFilter("dau-reporting")


class TestClientActivity(FOGTestCase):
    """
    Tests for client activity and FOG's scheduling of the "baseline" ping.
    For every `baseline` ping we also expect a `dau-reporting` ping.
    """

    def test_user_activity(self):
        # First test that restarting the browser sends a "active" ping
        [ping0, dau_ping0] = self.wait_for_pings(
            self.restart_browser, DauReportFilter, 2, ping_server=self.fog_ping_server
        )
        self.assertEqual("active", ping0["payload"]["ping_info"]["reason"])
        self.assertEqual("active", dau_ping0["payload"]["ping_info"]["reason"])

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            zero_prefs_script = """\
            Services.prefs.setIntPref("telemetry.fog.test.inactivity_limit", 0);
            Services.prefs.setIntPref("telemetry.fog.test.activity_limit", 0);
            """
            self.marionette.execute_script(zero_prefs_script)

        def user_active(active, marionette):
            script = "Services.obs.notifyObservers(null, 'user-interaction-{}active')".format(
                "" if active else "in"
            )
            with marionette.using_context(marionette.CONTEXT_CHROME):
                marionette.execute_script(script)

        [ping1, dau_ping1] = self.wait_for_pings(
            lambda: user_active(True, self.marionette),
            DauReportFilter,
            2,
            ping_server=self.fog_ping_server,
        )

        [ping2, dau_ping2] = self.wait_for_pings(
            lambda: user_active(False, self.marionette),
            DauReportFilter,
            2,
            ping_server=self.fog_ping_server,
        )

        self.assertEqual("baseline", ping1["request_url"]["doc_type"])
        self.assertEqual("dau-reporting", dau_ping1["request_url"]["doc_type"])
        self.assertEqual("active", ping1["payload"]["ping_info"]["reason"])
        self.assertEqual("active", dau_ping1["payload"]["ping_info"]["reason"])

        self.assertEqual("baseline", ping2["request_url"]["doc_type"])
        self.assertEqual("dau-reporting", dau_ping2["request_url"]["doc_type"])
        self.assertEqual("inactive", ping2["payload"]["ping_info"]["reason"])
        self.assertEqual(
            "inactive", dau_ping2["payload"]["ping_info"]["reason"], dau_ping2
        )
