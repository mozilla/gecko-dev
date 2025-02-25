import os
import re
import unittest
from tempfile import NamedTemporaryFile

import mozunit

from mozbuild.configure.confvars import ConfVarsSyntaxError, parse


def TemporaryConfVars():
    return NamedTemporaryFile("wt", delete=False)


class TestContext(unittest.TestCase):

    def loads(self, *lines):
        with NamedTemporaryFile("wt", delete=False) as ntf:
            ntf.writelines(lines)
        try:
            confvars = parse(ntf.name)
        finally:
            os.remove(ntf.name)
        return confvars

    def test_parse_empty_file(self):
        confvars = self.loads("# comment\n")
        self.assertEqual(confvars, {})

    def test_parse_simple_assignment(self):
        confvars = self.loads("a=b\n")
        self.assertEqual(confvars, {"a": "b"})

    def test_parse_simple_assignment_with_equal_in_value(self):
        confvars = self.loads("a='='\n", "b==")
        self.assertEqual(confvars, {"a": "=", "b": "="})

    def test_parse_simple_assignment_with_sharp_in_value(self):
        confvars = self.loads("a='#'\n")
        self.assertEqual(confvars, {"a": "#"})

    def test_parse_simple_assignment_with_trailing_spaces(self):
        confvars = self.loads("a1=1\t\n", "\n", "a2=2\n", "a3=3 \n", "a4=4")

        self.assertEqual(
            confvars,
            {
                "a1": "1",
                "a2": "2",
                "a3": "3",
                "a4": "4",
            },
        )

    def test_parse_trailing_comment(self):
        confvars = self.loads("a=b#comment\n")
        self.assertEqual(confvars, {"a": "b"})

    def test_parse_invalid_assign_in_trailing_comment(self):
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("a#=comment\n")
        self.assertTrue(
            re.match("Expecting key=value format \\(.*, line 1\\)", str(cm.exception))
        )

    def test_parse_quoted_assignment(self):
        confvars = self.loads("a='b'\n" "b=' c'\n" 'c=" \'c"\n')
        self.assertEqual(confvars, {"a": "b", "b": " c", "c": " 'c"})

    def test_parse_invalid_assignment(self):
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("a#comment\n")
        self.assertTrue(
            re.match("Expecting key=value format \\(.*, line 1\\)", str(cm.exception))
        )

    def test_parse_empty_value(self):
        confvars = self.loads("a=\n")
        self.assertEqual(confvars, {"a": ""})

    def test_parse_invalid_value(self):
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("#comment\na='er\n")
        self.assertTrue(
            re.match(
                "Unterminated quoted string \\(.*, line 2\\)",
                str(cm.exception),
            )
        )
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("a= er\n")
        self.assertTrue(
            re.match(
                "Expecting no spaces between '=' and 'er' \\(.*, line 1\\)",
                str(cm.exception),
            )
        )

    def test_parse_invalid_char(self):
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("a=$\n")
        self.assertTrue(
            re.match(
                "Unquoted, non-escaped special character '\\$' \\(.*, line 1\\)",
                str(cm.exception),
            )
        )

    def test_parse_invalid_key(self):
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads(" a=1\n")
        self.assertTrue(
            re.match(
                "Expecting no spaces around 'a' \\(.*, line 1\\)",
                str(cm.exception),
            )
        )
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("a =1\n")
        self.assertTrue(
            re.match(
                "Expecting no spaces around 'a' \\(.*, line 1\\)",
                str(cm.exception),
            )
        )

    def test_parse_redundant_key(self):
        with self.assertRaises(ConfVarsSyntaxError) as cm:
            self.loads("a=1\na=2\n")
        self.assertTrue(
            re.match(
                "Invalid redefinition for 'a' \\(.*, line 2\\)",
                str(cm.exception),
            )
        )


if __name__ == "__main__":
    mozunit.main()
