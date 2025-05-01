from marionette_driver import expected
from marionette_driver.by import By
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase


class TestApplyUpdate(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.about_fx_url = "chrome://browser/content/aboutDialog.xhtml"

    def test_update_is_applied(self):
        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.set_pref("remote.system-access-check.enabled", False)
        self.marionette.set_pref("app.update.log", True)
        self.marionette.set_pref("remote.log.level", "Trace")
        self.marionette.navigate(self.about_fx_url)
        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)

        Wait(self.marionette, timeout=10).until(
            expected.element_displayed(By.ID, "downloadAndInstallButton")
        )
        self.marionette.find_element(By.ID, "downloadAndInstallButton").click()

        Wait(self.marionette, timeout=200).until(
            expected.element_displayed(By.ID, "updateButton")
        )

        self.marionette.restart(
            callback=lambda: self.marionette.find_element(By.ID, "updateButton").click()
        )

        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.set_pref("remote.system-access-check.enabled", False)
        self.marionette.set_pref("app.update.log", True)
        self.marionette.set_pref("remote.log.level", "Trace")
        self.marionette.navigate(self.about_fx_url)
        Wait(self.marionette, timeout=200).until(
            expected.element_displayed(By.ID, "noUpdatesFound")
        )

    def tearDown(self):
        MarionetteTestCase.tearDown(self)
