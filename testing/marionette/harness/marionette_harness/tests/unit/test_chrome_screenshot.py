# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import os
import sys

import mozinfo

from marionette_driver import By
from marionette_driver.errors import NoSuchWindowException
from marionette_harness import WindowManagerMixin

# add this directory to the path
sys.path.append(os.path.dirname(__file__))

from test_screenshot import inline, ScreenCaptureTestCase


class TestScreenCaptureChrome(WindowManagerMixin, ScreenCaptureTestCase):
    def setUp(self):
        super(TestScreenCaptureChrome, self).setUp()
        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        super(TestScreenCaptureChrome, self).tearDown()

    @property
    def window_dimensions(self):
        return tuple(
            self.marionette.execute_script(
                """
            let el = document.documentElement;
            let rect = el.getBoundingClientRect();
            return [rect.width, rect.height];
            """
            )
        )

    def open_dialog(self):
        return self.open_chrome_window(
            "chrome://remote/content/marionette/test_dialog.xhtml"
        )

    def test_capture_different_context(self):
        """Check that screenshots in content and chrome are different."""
        with self.marionette.using_context("content"):
            screenshot_content = self.marionette.screenshot()
        screenshot_chrome = self.marionette.screenshot()
        self.assertNotEqual(screenshot_content, screenshot_chrome)

    def test_capture_element(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        # Ensure we only capture the element
        el = self.marionette.find_element(By.ID, "test-list")
        screenshot_element = self.marionette.screenshot(element=el)
        self.assertEqual(
            self.scale(self.get_element_dimensions(el)),
            self.get_image_dimensions(screenshot_element),
        )

        # Ensure we do not capture the full window
        screenshot_dialog = self.marionette.screenshot()
        self.assertNotEqual(screenshot_dialog, screenshot_element)

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_capture_full_area(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        root_dimensions = self.scale(self.get_element_dimensions(self.document_element))

        # self.marionette.set_window_rect(width=100, height=100)
        # A full capture is not the outer dimensions of the window,
        # but instead the bounding box of the window's root node (documentElement).
        screenshot_full = self.marionette.screenshot()
        screenshot_root = self.marionette.screenshot(element=self.document_element)

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

        self.assert_png(screenshot_full)
        self.assert_png(screenshot_root)
        self.assertEqual(root_dimensions, self.get_image_dimensions(screenshot_full))
        self.assertEqual(screenshot_root, screenshot_full)

    def test_capture_window_already_closed(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)
        self.marionette.close_chrome_window()

        self.assertRaises(NoSuchWindowException, self.marionette.screenshot)
        self.marionette.switch_to_window(self.start_window)

    def test_formats(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        self.assert_formats()

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_format_unknown(self):
        with self.assertRaises(ValueError):
            self.marionette.screenshot(format="cheese")
