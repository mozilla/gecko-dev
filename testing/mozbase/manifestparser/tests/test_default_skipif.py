#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
from manifestparser import ManifestParser

here = os.path.dirname(os.path.abspath(__file__))

class TestDefaultSkipif(unittest.TestCase):
    """test applying a skip-if condition in [DEFAULT] and || with the value for the test"""


    def test_defaults(self):

        default = os.path.join(here, 'default-skipif.ini')
        parser = ManifestParser(manifests=(default,))
        for test in parser.tests:
            if test['name'] == 'test1':
                self.assertEqual(test['skip-if'], "(os == 'win' && debug ) || (debug)")
            elif test['name'] == 'test2':
                self.assertEqual(test['skip-if'], "(os == 'win' && debug ) || (os == 'linux')")
            elif test['name'] == 'test3':
                self.assertEqual(test['skip-if'], "(os == 'win' && debug ) || (os == 'win')")
            elif test['name'] == 'test4':
                self.assertEqual(test['skip-if'], "(os == 'win' && debug ) || (os == 'win' && debug)")
            elif test['name'] == 'test5':
                self.assertEqual(test['skip-if'], "os == 'win' && debug # a pesky comment")
            elif test['name'] == 'test6':
                self.assertEqual(test['skip-if'], "(os == 'win' && debug ) || (debug )")

if __name__ == '__main__':
    unittest.main()
