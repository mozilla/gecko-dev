from marionette_driver import expected
from marionette_driver.by import By
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase


class TestApplyUpdate(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.about_fx_url = "chrome://browser/content/aboutDialog.xhtml"

    def test_update_is_applied(self):
        # self.marionette.quit()
        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.set_pref("app.update.log", True)
        self.marionette.navigate(self.about_fx_url)

        wait = Wait(self.marionette)
        wait_long = Wait(self.marionette, timeout=200)

        wait.until(expected.element_displayed(By.ID, "downloadAndInstallButton"))
        self.marionette.find_element(By.ID, "downloadAndInstallButton").click()
        wait_long.until(expected.element_displayed(By.ID, "updateButton"))
        self.marionette.restart()

        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.navigate(self.about_fx_url)
        wait_long.until(expected.element_displayed(By.ID, "noUpdatesFound"))

    def tearDown(self):
        MarionetteTestCase.tearDown(self)
