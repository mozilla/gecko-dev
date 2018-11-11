# This Source Code Form is subject to the terms of Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import yaml
import mozunit
import sys
import unittest
from os import path

TELEMETRY_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TELEMETRY_ROOT_PATH)
# The parsers live in a subdirectory of "build_scripts", account for that.
# NOTE: if the parsers are moved, this logic will need to be updated.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
from mozparsers.shared_telemetry_utils import ParserError
from mozparsers import parse_scalars


def load_scalar(scalar):
    """Parse the passed Scalar and return a dictionary

    :param scalar: Scalar as YAML string
    :returns: Parsed Scalar dictionary
    """
    return yaml.safe_load(scalar)


class TestParser(unittest.TestCase):
    def test_valid_email_address(self):
        SAMPLE_SCALAR_VALID_ADDRESSES = """
description: A nice one-line description.
expires: never
record_in_processes:
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com
  - test02@mozilla.com
bug_numbers:
  - 12345
"""
        scalar = load_scalar(SAMPLE_SCALAR_VALID_ADDRESSES)
        sclr = parse_scalars.ScalarType("CATEGORY",
                                        "PROVE",
                                        scalar,
                                        strict_type_checks=True)
        ParserError.exit_func()

        self.assertEqual(sclr.notification_emails, ["test01@mozilla.com", "test02@mozilla.com"])

    def test_invalid_email_address(self):
        SAMPLE_SCALAR_INVALID_ADDRESSES = """
description: A nice one-line description.
expires: never
record_in_processes:
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com, test02@mozilla.com
bug_numbers:
  - 12345
"""
        scalar = load_scalar(SAMPLE_SCALAR_INVALID_ADDRESSES)
        parse_scalars.ScalarType("CATEGORY",
                                 "PROVE",
                                 scalar,
                                 strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_multistore_default(self):
        SAMPLE_SCALAR = """
description: A nice one-line description.
expires: never
record_in_processes:
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com
bug_numbers:
  - 12345
"""
        scalar = load_scalar(SAMPLE_SCALAR)
        sclr = parse_scalars.ScalarType("CATEGORY",
                                        "PROVE",
                                        scalar,
                                        strict_type_checks=True)
        ParserError.exit_func()

        self.assertEqual(sclr.record_into_store, ["main"])

    def test_multistore_extended(self):
        SAMPLE_SCALAR = """
description: A nice one-line description.
expires: never
record_in_processes:
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com
bug_numbers:
  - 12345
record_into_store:
    - main
    - sync
"""
        scalar = load_scalar(SAMPLE_SCALAR)
        sclr = parse_scalars.ScalarType("CATEGORY",
                                        "PROVE",
                                        scalar,
                                        strict_type_checks=True)
        ParserError.exit_func()

        self.assertEqual(sclr.record_into_store, ["main", "sync"])

    def test_multistore_empty(self):
        SAMPLE_SCALAR = """
description: A nice one-line description.
expires: never
record_in_processes:
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com
bug_numbers:
  - 12345
record_into_store: []
"""
        scalar = load_scalar(SAMPLE_SCALAR)
        parse_scalars.ScalarType("CATEGORY",
                                 "PROVE",
                                 scalar,
                                 strict_type_checks=True)
        self.assertRaises(SystemExit, ParserError.exit_func)


if __name__ == '__main__':
    mozunit.main()
