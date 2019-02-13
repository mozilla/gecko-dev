# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette.marionette_test import MarionetteTestCase, skip_if_b2g
from marionette_driver.keys import Keys
from marionette_driver.errors import ElementNotVisibleException


class TestTyping(MarionetteTestCase):

    def testShouldFireKeyPressEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("a")
        result = self.marionette.find_element("id", "result")
        self.assertTrue("press:" in result.text)

    def testShouldFireKeyDownEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("I")
        result = self.marionette.find_element("id", "result")
        self.assertTrue("down" in result.text)

    def testShouldFireKeyUpEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("a")
        result = self.marionette.find_element("id", "result")
        self.assertTrue("up:" in result.text)

    def testShouldTypeLowerCaseLetters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("abc def")
        self.assertEqual(keyReporter.get_attribute("value"), "abc def")

    def testShouldBeAbleToTypeCapitalLetters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("ABC DEF")
        self.assertEqual(keyReporter.get_attribute("value"), "ABC DEF")

    @skip_if_b2g
    def testCutAndPasteShortcuts(self):
        # Test that modifier keys work via copy/paste shortcuts.
        if self.marionette.session_capabilities['platformName'] == 'DARWIN':
            mod_key = Keys.META
        else:
            mod_key = Keys.CONTROL

        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        self.assertEqual(keyReporter.get_attribute("value"), "")
        keyReporter.send_keys("zyxwvutsr")
        self.assertEqual(keyReporter.get_attribute("value"), "zyxwvutsr")

        # Select all and cut.
        keyReporter.send_keys(mod_key, 'a')
        keyReporter.send_keys(mod_key, 'x')
        self.assertEqual(keyReporter.get_attribute("value"), "")

        self.marionette.set_context("chrome")
        url_bar = self.marionette.find_element("id", "urlbar")

        # Clear and paste.
        url_bar.send_keys(mod_key, 'a')
        url_bar.send_keys(Keys.BACK_SPACE)

        self.assertEqual(url_bar.get_attribute("value"), "")
        url_bar.send_keys(mod_key, 'v')
        self.assertEqual(url_bar.get_attribute("value"), "zyxwvutsr")

    def testShouldBeAbleToTypeQuoteMarks(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("\"")
        self.assertEqual(keyReporter.get_attribute("value"), "\"")

    def testShouldBeAbleToTypeTheAtCharacter(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("@")
        self.assertEqual(keyReporter.get_attribute("value"), "@")

    def testShouldBeAbleToMixUpperAndLowerCaseLetters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys("me@eXample.com")
        self.assertEqual(keyReporter.get_attribute("value"), "me@eXample.com")

    def testArrowKeysShouldNotBePrintable(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element("id", "keyReporter")
        keyReporter.send_keys(Keys.ARROW_LEFT)
        self.assertEqual(keyReporter.get_attribute("value"), "")

    def testWillSimulateAKeyUpWhenEnteringTextIntoInputElements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyUp")
        element.send_keys("I like cheese")
        result = self.marionette.find_element("id", "result")
        self.assertEqual(result.text, "I like cheese")

    def testWillSimulateAKeyDownWhenEnteringTextIntoInputElements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyDown")
        element.send_keys("I like cheese")
        result = self.marionette.find_element("id", "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testWillSimulateAKeyPressWhenEnteringTextIntoInputElements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyPress")
        element.send_keys("I like cheese")
        result = self.marionette.find_element("id", "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testWillSimulateAKeyUpWhenEnteringTextIntoTextAreas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyUpArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element("id", "result")
        self.assertEqual(result.text, "I like cheese")

    def testWillSimulateAKeyDownWhenEnteringTextIntoTextAreas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyDownArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element("id", "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testWillSimulateAKeyPressWhenEnteringTextIntoTextAreas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyPressArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element("id", "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testShouldReportKeyCodeOfArrowKeysUpDownEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element("id", "result")
        element = self.marionette.find_element("id", "keyReporter")
        element.send_keys(Keys.ARROW_DOWN)
        self.assertTrue("down: 40" in result.text.strip())
        self.assertTrue("up: 40" in result.text.strip())

        element.send_keys(Keys.ARROW_UP)
        self.assertTrue("down: 38" in  result.text.strip())
        self.assertTrue("up: 38" in result.text.strip())

        element.send_keys(Keys.ARROW_LEFT)
        self.assertTrue("down: 37" in result.text.strip())
        self.assertTrue("up: 37" in result.text.strip())

        element.send_keys(Keys.ARROW_RIGHT)
        self.assertTrue("down: 39" in result.text.strip())
        self.assertTrue("up: 39" in result.text.strip())

        #  And leave no rubbish/printable keys in the "keyReporter"
        self.assertEqual(element.get_attribute("value"), "")

    ''' Disabled. Reenable in Bug 1068728
    def testNumericShiftKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element("id", "result")
        element = self.marionette.find_element("id", "keyReporter")
        numericShiftsEtc = "~!@#$%^&*()_+{}:i\"<>?|END~"
        element.send_keys(numericShiftsEtc)
        self.assertEqual(element.get_attribute("value"), numericShiftsEtc)
        self.assertTrue(" up: 16" in result.text.strip())
    '''

    def testLowerCaseAlphaKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyReporter")
        lowerAlphas = "abcdefghijklmnopqrstuvwxyz"
        element.send_keys(lowerAlphas)
        self.assertEqual(element.get_attribute("value"), lowerAlphas)

    ''' Disabled. Reenable in Bug 1068735
    def testUppercaseAlphaKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element("id", "result")
        element = self.marionette.find_element("id", "keyReporter")
        upperAlphas = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        element.send_keys(upperAlphas)
        self.assertEqual(element.get_attribute("value"), upperAlphas)
        self.assertTrue(" up: 16" in result.text.strip())
    '''

    ''' Disabled. Reenable in Bug 1068726
    def testAllPrintableKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element("id", "result")
        element = self.marionette.find_element("id", "keyReporter")
        allPrintable = "!\"#$%&'()*+,-./0123456789:<=>?@ ABCDEFGHIJKLMNOPQRSTUVWXYZ [\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        element.send_keys(allPrintable)

        self.assertTrue(element.get_attribute("value"), allPrintable)
        self.assertTrue(" up: 16" in result.text.strip())
    '''

    ''' Disabled. Reenable in Bug 1068733
    def testSpecialSpaceKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyReporter")
        element.send_keys("abcd" + Keys.SPACE + "fgh" + Keys.SPACE + "ij")
        self.assertEqual(element.get_attribute("value"), "abcd fgh ij")
    '''

    def testShouldTypeAnInteger(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "keyReporter")
        element.send_keys(1234)
        self.assertEqual(element.get_attribute("value"), "1234")

    def testShouldSendKeysToElementsWithoutTheValueAttribute(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        # If we don't get an error below we are good
        self.marionette.find_element('tag name', 'body').send_keys('foo')

    def testShouldThrowElementNotVisibleWhenInputHidden(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        not_displayed = self.marionette.find_element('id', 'notDisplayed')
        self.assertRaises(ElementNotVisibleException, not_displayed.send_keys, 'foo')
