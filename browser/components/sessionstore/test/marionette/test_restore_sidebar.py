# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 0.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/0.0/.

import os
import sys

# add this directory to the path
sys.path.append(os.path.dirname(__file__))

from session_store_test_case import SessionStoreTestCase


def inline(title):
    return "data:text/html;charset=utf-8,<html><head><title>{}</title></head><body></body></html>".format(
        title
    )


class TestSessionRestore(SessionStoreTestCase):
    """
    Test that the sidebar and its attributes are restored on reopening of window.
    """

    def setUp(self):
        super(TestSessionRestore, self).setUp(
            startup_page=1,
            include_private=False,
            restore_on_demand=True,
            test_windows=set(
                [
                    (
                        inline("lorem ipsom"),
                        inline("dolor"),
                    ),
                ]
            ),
        )

    def test_restore_sidebar_open(self):
        self.assertEqual(
            len(self.marionette.chrome_window_handles),
            1,
            msg="Should have 1 window open.",
        )
        self.marionette.execute_script(
            """
            let window = BrowserWindowTracker.getTopWindow()
            window.SidebarController.show("viewHistorySidebar");
            let sidebarBox = window.document.getElementById("sidebar-box")
            sidebarBox.style.width = "100px";
            """
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                let window = BrowserWindowTracker.getTopWindow()
                return !window.document.getElementById("sidebar-box").hidden;
                """
            ),
            True,
            "Sidebar is open before window is closed.",
        )

        self.marionette.restart()
        self.marionette.set_context("chrome")

        self.assertEqual(
            len(self.marionette.chrome_window_handles),
            1,
            msg="Windows from last session have been restored.",
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                let window = BrowserWindowTracker.getTopWindow()
                return !window.document.getElementById("sidebar-box").hidden;
                """
            ),
            True,
            "Sidebar has been restored.",
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                let window = BrowserWindowTracker.getTopWindow()
                return window.document.getElementById("sidebar-box").style.width;
                """
            ),
            "100px",
            "Sidebar width been restored.",
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                const lazy = {};
                ChromeUtils.defineESModuleGetters(lazy, {
                    SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
                });
                let state = SessionStore.getCurrentState();
                return state.windows[0].sidebar.command;
                """
            ),
            "viewHistorySidebar",
            "Correct sidebar category has been restored.",
        )

    def test_restore_sidebar_closed(self):
        self.marionette.execute_script(
            """
            let window = BrowserWindowTracker.getTopWindow()
            window.SidebarController.show("viewHistorySidebar");
            let sidebarBox = window.document.getElementById("sidebar-box")
            sidebarBox.style.width = "100px";
            window.SidebarController.toggle();
            """
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                let window = BrowserWindowTracker.getTopWindow()
                return window.document.getElementById("sidebar-box").hidden;
                """
            ),
            True,
            "Sidebar is hidden before window is closed.",
        )

        self.marionette.restart()
        self.marionette.set_context("chrome")

        self.assertEqual(
            len(self.marionette.chrome_window_handles),
            1,
            msg="Windows from last session have been restored.",
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                let window = BrowserWindowTracker.getTopWindow()
                return window.document.getElementById("sidebar-box").hidden;
                """
            ),
            True,
            "Sidebar is hidden on session restore.",
        )

        self.assertEqual(
            self.marionette.execute_script(
                """
                let window = BrowserWindowTracker.getTopWindow()
                return window.document.getElementById("sidebar-box").style.width;
                """
            ),
            "100px",
            "Sidebar width has been restored.",
        )

    def test_restore_for_always_show(self):
        self.marionette.execute_script(
            """
            Services.prefs.setBoolPref("sidebar.revamp", true);
            Services.prefs.setBoolPref("sidebar.animation.enabled", false);
            Services.prefs.setStringPref("sidebar.visibility", "always-show");
            """
        )
        self.marionette.restart()
        self.marionette.set_context("chrome")

        self.assertEqual(
            len(self.marionette.chrome_window_handles),
            1,
            msg="Should have 1 window open.",
        )
        self.assertTrue(
            self.marionette.execute_script(
                """
                const window = BrowserWindowTracker.getTopWindow();
                window.SidebarController.toolbarButton.click();
                return window.SidebarController.sidebarMain.expanded;
                """
            ),
            "Sidebar is expanded before window is closed.",
        )

        self.marionette.restart()

        self.marionette.execute_async_script(
            """
            let resolve = arguments[0];
            let { BrowserInitState } = ChromeUtils.importESModule("resource:///modules/BrowserGlue.sys.mjs");
            BrowserInitState.startupIdleTaskPromise.then(resolve);
            """
        )

        self.assertTrue(
            self.marionette.execute_script(
                """
                const window = BrowserWindowTracker.getTopWindow();
                return window.SidebarController.sidebarMain.expanded;
                """
            ),
            "Sidebar expanded state has been restored.",
        )

    def test_restore_for_hide_sidebar(self):
        self.marionette.execute_script(
            """
            Services.prefs.setBoolPref("sidebar.revamp", true);
            Services.prefs.setStringPref("sidebar.visibility", "hide-sidebar");
            """
        )
        self.marionette.restart()
        self.marionette.set_context("chrome")

        self.assertEqual(
            len(self.marionette.chrome_window_handles),
            1,
            msg="Should have 1 window open.",
        )
        self.assertFalse(
            self.marionette.execute_script(
                """
                const window = BrowserWindowTracker.getTopWindow();
                window.SidebarController.toolbarButton.click();
                return window.SidebarController.sidebarContainer.hidden;
                """
            ),
            "Sidebar is visible before window is closed.",
        )

        self.marionette.restart()

        self.assertFalse(
            self.marionette.execute_script(
                """
                const window = BrowserWindowTracker.getTopWindow();
                return window.SidebarController.sidebarContainer.hidden;
                """
            ),
            "Sidebar visibility state has been restored.",
        )
