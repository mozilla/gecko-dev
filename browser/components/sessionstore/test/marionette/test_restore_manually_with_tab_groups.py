# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from urllib.parse import quote

# add this directory to the path
sys.path.append(os.path.dirname(__file__))

from marionette_driver import Wait, errors
from session_store_test_case import SessionStoreTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestSessionRestoreWithTabGroups(SessionStoreTestCase):
    def setUp(self):
        super(TestSessionRestoreWithTabGroups, self).setUp(
            startup_page=1,
            include_private=False,
            restore_on_demand=True,
            test_windows=set(
                [
                    (
                        inline("""<div">lorem</div>"""),
                        inline("""<div">ipsum</div>"""),
                        inline("""<div">dolor</div>"""),
                        inline("""<div">sit</div>"""),
                        inline("""<div">amet</div>"""),
                    ),
                ]
            ),
        )

    def test_no_restore_with_quit(self):
        self.wait_for_windows(
            self.all_windows, "Not all requested windows have been opened"
        )

        self.marionette.execute_async_script(
            """
            let resolve = arguments[0];
            gBrowser.addTabGroup([gBrowser.tabs[3], gBrowser.tabs[4]], { id: "group-left-open-1", label: "open-1" });
            gBrowser.addTabGroup([gBrowser.tabs[2]], { id: "group-left-open-2", label: "open-2" });
            let closedGroup = gBrowser.addTabGroup([gBrowser.tabs[1]], { id:"group-closed", label: "closed" });
            gBrowser.removeTabGroup(closedGroup);

            let { TabStateFlusher } = ChromeUtils.importESModule("resource:///modules/sessionstore/TabStateFlusher.sys.mjs");
            TabStateFlusher.flushWindow(gBrowser.ownerGlobal).then(resolve);
        """
        )

        self.marionette.quit()
        self.marionette.start_session()
        self.marionette.set_context("chrome")
        self.assertEqual(
            self.marionette.execute_script(
                """
                let closedGroupIds = SessionStore.getWindowState(gBrowser.ownerGlobal).windows[0].closedGroups.map(g => g.id);
                return !(closedGroupIds.includes("group-left-open-1") && closedGroupIds.includes("group-left-open-2"));
                """
            ),
            True,
            "Groups that were left open are NOT in closedGroups",
        )

        self.assertEqual(
            self.marionette.execute_script(
                "return SessionStore.getWindowState(gBrowser.ownerGlobal).windows[0].closedGroups.length"
            ),
            1,
            msg="There is one closed group in the window",
        )

        self.assertEqual(
            self.marionette.execute_script(
                "return SessionStore.getWindowState(gBrowser.ownerGlobal).windows[0].closedGroups[0].id"
            ),
            "group-closed",
            msg="Correct group appears in closedGroups",
        )

        self.assertEqual(
            self.marionette.execute_script("return SessionStore.savedGroups.length"),
            2,
            msg="There are two saved groups",
        )

        self.assertEqual(
            self.marionette.execute_script(
                "return SessionStore.savedGroups.map(g => g.id)"
            ),
            ["group-left-open-1", "group-left-open-2"],
            msg="The open groups from last session are now saved",
        )

        self.assertEqual(
            self.marionette.execute_script(
                "return SessionStore.getWindowState(gBrowser.ownerGlobal).windows[0].closedGroups[0].tabs.length"
            ),
            1,
            msg="Closed group has 1 tab",
        )

        self.assertEqual(
            self.marionette.execute_script(
                "return SessionStore.savedGroups.map(g => g.tabs.length)"
            ),
            [2, 1],
            msg="Saved groups have 1 and 2 tabs respectively",
        )

        self.marionette.execute_script(
            """
            SessionStore.restoreLastSession();
        """
        )
        self.wait_for_tabcount(4, "Waiting for 4 tabs")

        self.assertEqual(
            self.marionette.execute_script("return gBrowser.tabs.length"),
            4,
            msg="4 tabs was restored",
        )
        self.assertEqual(
            self.marionette.execute_script("return gBrowser.tabGroups.length"),
            2,
            msg="2 tab groups were restored",
        )

        self.assertEqual(
            self.marionette.execute_script("return SessionStore.savedGroups?.length"),
            0,
            msg="We have no saved tab groups",
        )

    def wait_for_tabcount(self, expected_tabcount, message, timeout=5):
        current_tabcount = None

        def check(_):
            nonlocal current_tabcount
            current_tabcount = self.marionette.execute_script(
                "return gBrowser.tabs.length;"
            )
            return current_tabcount == expected_tabcount

        try:
            wait = Wait(self.marionette, timeout=timeout, interval=0.1)
            wait.until(check, message=message)
        except errors.TimeoutException as e:
            # Update the message to include the most recent list of windows
            message = (
                f"{e.message}. Expected {expected_tabcount}, got {current_tabcount}."
            )
            raise errors.TimeoutException(message)
