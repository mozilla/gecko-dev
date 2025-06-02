import xml.etree.ElementTree as ET

import requests
from marionette_driver import expected
from marionette_driver.by import By
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase


class TestBackgroundUpdate(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.about_fx_url = "chrome://browser/content/aboutDialog.xhtml"

    def test_background_update_is_applied(self):
        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.set_pref("remote.system-access-check.enabled", False)
        self.marionette.set_pref("app.update.log", True)
        self.marionette.set_pref("remote.log.level", "Trace")
        self.marionette.set_pref("app.update.interval", 5)
        self.marionette.navigate(self.about_fx_url)

        self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        update_url = self.marionette.execute_async_script(
            """
            (async function() {
                let { UpdateUtils } = ChromeUtils.importESModule(
                    "resource://gre/modules/UpdateUtils.sys.mjs"
                );
                let url = await UpdateUtils.formatUpdateURL(Services.appinfo.updateURL);
                return url;
            })().then(arguments[0]);
            """
        )

        response = requests.get(update_url)
        if response.status_code != 200:
            raise Exception(
                f"Tried to fetch update.xml but got response code {response.status_code}"
            )

        # Get the target version
        root = ET.fromstring(response.text)
        target_ver = root[0].get("appVersion")

        # Wait for the background update to be ready by checking for popup
        Wait(self.marionette, timeout=100).until(
            lambda _: self.marionette.find_elements(By.ID, "appMenu-notification-popup")
        )

        # Dismiss the popup
        self.marionette.find_element(By.ID, "PanelUI-menu-button").click()
        self.marionette.find_element(By.ID, "PanelUI-menu-button").click()

        # Check that there is a green badge on hamburger menu
        Wait(self.marionette, timeout=100).until(
            lambda _: self.marionette.find_element(
                By.ID, "PanelUI-menu-button"
            ).get_attribute("badge-status")
            == "update-available"
        )

        # Click the update button in hamburger menu to download the update
        self.marionette.find_element(By.ID, "PanelUI-menu-button").click()
        self.marionette.find_element(By.ID, "appMenu-update-banner").click()

        # Make sure that the download is finished
        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)
        Wait(self.marionette, timeout=200).until(
            expected.element_displayed(By.ID, "updateButton")
        )
        initial_ver = self.marionette.find_element(By.ID, "version").text

        # Restart normally
        self.marionette.restart()

        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.set_pref("remote.system-access-check.enabled", False)
        self.marionette.set_pref("app.update.log", True)
        self.marionette.set_pref("remote.log.level", "Trace")
        self.marionette.navigate(self.about_fx_url)
        Wait(self.marionette, timeout=100).until(
            expected.element_displayed(By.ID, "noUpdatesFound")
        )

        # Mini smoke test
        print(f"Updated from {initial_ver} to {target_ver}")
        assert target_ver in self.marionette.find_element(By.ID, "version").text
        assert len(self.marionette.window_handles) == 1
        self.marionette.open("tab")
        Wait(self.marionette, timeout=20).until(
            lambda _: len(self.marionette.window_handles) == 2
        )
        self.marionette.close()
        Wait(self.marionette, timeout=20).until(
            lambda _: len(self.marionette.window_handles) == 1
        )

    def tearDown(self):
        MarionetteTestCase.tearDown(self)
