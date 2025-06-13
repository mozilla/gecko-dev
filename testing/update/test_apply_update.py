import xml.etree.ElementTree as ET
from os import environ
from pathlib import Path

import requests
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
        self.marionette.set_pref("app.update.log", True)
        self.marionette.set_pref("remote.log.level", "Trace")
        self.marionette.set_pref("remote.system-access-check.enabled", False)
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

        response = requests.get(f"{update_url}?force=1")
        response.raise_for_status()

        # Get the target version
        root = ET.fromstring(response.text)
        target_ver = root[0].get("appVersion")

        if environ.get("UPLOAD_DIR"):
            version_info_log = Path(
                environ.get("UPLOAD_DIR"), environ.get("VERSION_LOG_FILENAME")
            )
            if version_info_log.is_file():
                with version_info_log.open("a") as fh:
                    fh.write(f"Target version: {target_ver}\n")

        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)
        initial_ver = self.marionette.find_element(By.ID, "version").text

        Wait(self.marionette, timeout=10).until(
            expected.element_displayed(By.ID, "downloadAndInstallButton")
        )
        self.marionette.find_element(By.ID, "downloadAndInstallButton").click()

        Wait(self.marionette, timeout=240).until(
            expected.element_displayed(By.ID, "updateButton")
        )

        self.marionette.restart(
            callback=lambda: self.marionette.find_element(By.ID, "updateButton").click()
        )

        self.marionette.set_pref("app.update.disabledForTesting", False)
        self.marionette.set_pref("app.update.log", True)
        self.marionette.set_pref("remote.log.level", "Trace")
        self.marionette.set_pref("remote.system-access-check.enabled", False)
        self.marionette.navigate(self.about_fx_url)
        Wait(self.marionette, timeout=240).until(
            expected.element_displayed(By.ID, "noUpdatesFound")
        )

        # Mini smoke test
        try:
            print(f"Updated from {initial_ver} to {target_ver}")
        except UnicodeEncodeError:
            print(f"Updated to {target_ver}")
        version_text = self.marionette.find_element(By.ID, "version").text
        assert target_ver in version_text
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
