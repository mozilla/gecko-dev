import xml.etree.ElementTree as ET
from os import environ
from pathlib import Path

import requests
from marionette_driver import expected
from marionette_driver.by import By
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase


def get_possible_target_versions(update_url):
    """If throttled to a lower target version, return both possible versions"""
    versions = []
    for n in range(2):
        response = requests.get(f"{update_url}?force={n}")
        if response.status_code != 200:
            raise Exception(
                f"Tried to fetch update.xml but got response code {response.status_code}"
            )

        # Get the target version
        root = ET.fromstring(response.text)
        versions.append(root[0].get("appVersion"))

    return list(set(versions))


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

        target_vers = get_possible_target_versions(update_url)

        if environ.get("UPLOAD_DIR"):
            version_info_log = Path(
                environ.get("UPLOAD_DIR"), environ.get("VERSION_LOG_FILENAME")
            )
            if version_info_log.is_file():
                with version_info_log.open("a") as fh:
                    fh.write(f"Target version options: {', '.join(target_vers)}\n")

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
            expected.element_displayed(By.ID, "version")
        )

        # Mini smoke test
        target_ver_verified = False
        version_text = self.marionette.find_element(By.ID, "version").text
        for target_ver in target_vers:
            if target_ver in version_text:
                target_ver_verified = True
                try:
                    print(f"Updated from {initial_ver} to {target_ver}")
                except UnicodeEncodeError:
                    print(f"Updated to {target_ver}")
        assert target_ver_verified
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
