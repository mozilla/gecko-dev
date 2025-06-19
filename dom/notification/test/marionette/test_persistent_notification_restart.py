# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Test for bug 1881043: verify that persistent notifications are still
# persisted when restarting the browser.

from marionette_harness import MarionetteTestCase


class PersistentNotificationRestartTestCase(MarionetteTestCase):
    def setUp(self):
        super().setUp()

        install_url = self.marionette.absolute_url(
            "serviceworker/install_serviceworker.html"
        )
        self.marionette.navigate(install_url)

        self.marionette.set_permission({"name": "notifications"}, "granted")
        self.marionette.execute_script(
            """
            return (async () => {
                const reg = await navigator.serviceWorker.ready;
                const notifications = await reg.getNotifications();
                for (const n of notifications) {
                    n.close();
                }
                await reg.showNotification("foo");
            })();
            """
        )

    def tearDown(self):
        self.marionette.restart(in_app=False, clean=True)
        super().tearDown()

    def test_stored_notification_after_restart(self):
        self.assertTrue(self.is_notification_stored)
        self.marionette.restart()
        self.assertTrue(self.is_notification_stored)

    @property
    def is_notification_stored(self):
        return self.marionette.execute_script(
            """
            return (async () => {
                const reg = await navigator.serviceWorker.getRegistration();
                const notifications = await reg.getNotifications();
                return notifications.length > 0;
            })()
            """
        )
