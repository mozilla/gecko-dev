# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase

class SimpletestSanityTest(MarionetteTestCase):

    callFinish = "return finish();"

    def test_is(self):
        def runtests():
            sentFail1 = "is(true, false, 'isTest1', TEST_UNEXPECTED_FAIL, TEST_PASS);" + self.callFinish
            sentFail2 = "is(true, false, 'isTest2', TEST_UNEXPECTED_FAIL, TEST_PASS);" + self.callFinish
            sentPass1 = "is(true, true, 'isTest3');" + self.callFinish
            sentPass2 = "is(true, true, 'isTest4');" + self.callFinish

            self.assertEqual(1, len(self.marionette.execute_script(sentFail1)["failures"]))
            self.assertEqual(0, self.marionette.execute_script(sentFail2)["passed"])
            self.assertEqual(1, self.marionette.execute_script(sentPass1)["passed"])
            self.assertEqual(0, len(self.marionette.execute_script(sentPass2)["failures"]))

            self.marionette.set_script_timeout(1000)
            self.assertEqual(1, len(self.marionette.execute_async_script(sentFail1)["failures"]))
            self.assertEqual(0, self.marionette.execute_async_script(sentFail2)["passed"])
            self.assertEqual(1, self.marionette.execute_async_script(sentPass1)["passed"])
            self.assertEqual(0, len(self.marionette.execute_async_script(sentPass2)["failures"]))

        self.marionette.set_context("content")
        runtests()
        self.marionette.set_context("chrome")
        runtests()

    def test_isnot(self):
        def runtests():
           sentFail1 = "isnot(true, true, 'isnotTest3', TEST_UNEXPECTED_FAIL, TEST_PASS);" + self.callFinish
           sentFail2 = "isnot(true, true, 'isnotTest4', TEST_UNEXPECTED_FAIL, TEST_PASS);" + self.callFinish
           sentPass1 = "isnot(true, false, 'isnotTest1');" + self.callFinish
           sentPass2 = "isnot(true, false, 'isnotTest2');" + self.callFinish

           self.assertEqual(1, len(self.marionette.execute_script(sentFail1)["failures"]));
           self.assertEqual(0, self.marionette.execute_script(sentFail2)["passed"]);
           self.assertEqual(0, len(self.marionette.execute_script(sentPass1)["failures"]));
           self.assertEqual(1, self.marionette.execute_script(sentPass2)["passed"]);

           self.marionette.set_script_timeout(1000)
           self.assertEqual(1, len(self.marionette.execute_async_script(sentFail1)["failures"]));
           self.assertEqual(0, self.marionette.execute_async_script(sentFail2)["passed"]);
           self.assertEqual(0, len(self.marionette.execute_async_script(sentPass1)["failures"]));
           self.assertEqual(1, self.marionette.execute_async_script(sentPass2)["passed"]);

        self.marionette.set_context("content")
        runtests()
        self.marionette.set_context("chrome")
        runtests()

    def test_ok(self):
        def runtests():
            sentFail1 = "ok(1==2, 'testOk1', TEST_UNEXPECTED_FAIL, TEST_PASS);" + self.callFinish
            sentFail2 = "ok(1==2, 'testOk2', TEST_UNEXPECTED_FAIL, TEST_PASS);" + self.callFinish
            sentPass1 = "ok(1==1, 'testOk3');" + self.callFinish
            sentPass2 = "ok(1==1, 'testOk4');" + self.callFinish

            self.assertEqual(1, len(self.marionette.execute_script(sentFail1)["failures"]));
            self.assertEqual(0, self.marionette.execute_script(sentFail2)["passed"]);
            self.assertEqual(0, len(self.marionette.execute_script(sentPass1)["failures"]));
            self.assertEqual(1, self.marionette.execute_script(sentPass2)["passed"]);

            self.marionette.set_script_timeout(1000)
            self.assertEqual(1, len(self.marionette.execute_async_script(sentFail1)["failures"]));
            self.assertEqual(0, self.marionette.execute_async_script(sentFail2)["passed"]);
            self.assertEqual(0, len(self.marionette.execute_async_script(sentPass1)["failures"]));
            self.assertEqual(1, self.marionette.execute_async_script(sentPass2)["passed"]);

        self.marionette.set_context("content")
        runtests()
        self.marionette.set_context("chrome")
        runtests()

    def test_todo(self):
        def runtests():
            sentFail1 = "todo(1==1, 'testTodo1', TEST_UNEXPECTED_PASS, TEST_KNOWN_FAIL);" + self.callFinish
            sentFail2 = "todo(1==1, 'testTodo2', TEST_UNEXPECTED_PASS, TEST_KNOWN_FAIL);" + self.callFinish
            sentPass1 = "todo(1==2, 'testTodo3');" + self.callFinish
            sentPass2 = "todo(1==2, 'testTodo4');" + self.callFinish

            self.assertEqual(1, len(self.marionette.execute_script(sentFail1)["unexpectedSuccesses"]));
            self.assertEqual(0, len(self.marionette.execute_script(sentFail2)["expectedFailures"]));
            self.assertEqual(0, len(self.marionette.execute_script(sentPass1)["unexpectedSuccesses"]));
            self.assertEqual(1, len(self.marionette.execute_script(sentPass2)["expectedFailures"]));

            self.marionette.set_script_timeout(1000)
            self.assertEqual(1, len(self.marionette.execute_async_script(sentFail1)["unexpectedSuccesses"]));
            self.assertEqual(0, len(self.marionette.execute_async_script(sentFail2)["expectedFailures"]));
            self.assertEqual(0, len(self.marionette.execute_async_script(sentPass1)["unexpectedSuccesses"]));
            self.assertEqual(1, len(self.marionette.execute_async_script(sentPass2)["expectedFailures"]));

        self.marionette.set_context("content")
        runtests()
        self.marionette.set_context("chrome")
        runtests()
