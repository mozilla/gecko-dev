# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class TestLastUsedSidebarPanel(MarionetteTestCase):
    def setUp(self):
        super().setUp()
        self.marionette.set_context("chrome")
        # Add the sidebar button widget at the start of the horizontal tabstrip
        self.marionette.execute_script(
            "CustomizableUI.addWidgetToArea('sidebar-button', CustomizableUI.AREA_NAVBAR, 0)"
        )

    def tearDown(self):
        try:
            # Make sure subsequent tests get a clean profile
            self.marionette.restart(in_app=False, clean=True)
        finally:
            super().tearDown()

    def click_toolbar_button(self):
        # Click the button to show the launcher
        self.marionette.execute_script(
            """
            const window = BrowserWindowTracker.getTopWindow();
            return window.document.getElementById("sidebar-button").click()
            """
        )

    def is_sidebar_panel_visible(self):
        hidden = self.marionette.execute_script(
            """
            const window = BrowserWindowTracker.getTopWindow();
            return window.document.getElementById("sidebar-box").hidden;
            """
        )
        return not hidden

    def get_current_sidebar_id(self):
        return self.marionette.execute_script(
            """
            const window = BrowserWindowTracker.getTopWindow();
            return window.document.getElementById("sidebar-box").getAttribute("sidebarcommand");
            """
        )

    def test_panel_reopened_legacy(self):
        self.assertFalse(
            self.is_sidebar_panel_visible(), "The sidebar panel is not initially shown"
        )
        self.assertEqual(
            self.get_current_sidebar_id(), None, "The sidebar panel has no current ID"
        )

        # Open the history sidebar
        self.marionette.execute_script(
            """
            const window = BrowserWindowTracker.getTopWindow();
            return window.SidebarController.toggle("viewHistorySidebar");
            """
        )

        Wait(self.marionette).until(
            lambda _: self.get_current_sidebar_id() == "viewHistorySidebar",
            message="The history sidebar is visible",
        )

        # Click the toolbar button to close the panel
        self.click_toolbar_button()

        self.assertFalse(
            self.is_sidebar_panel_visible(), "The sidebar panel is now closed"
        )

        self.marionette.restart(clean=False, in_app=True)
        self.marionette.set_context("chrome")

        self.assertFalse(
            self.is_sidebar_panel_visible(), "The sidebar panel is initially closed"
        )
        self.click_toolbar_button()

        Wait(self.marionette).until(
            lambda _: self.get_current_sidebar_id() == "viewHistorySidebar",
            message="The history sidebar is visible",
        )
