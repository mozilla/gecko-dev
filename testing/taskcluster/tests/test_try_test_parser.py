import unittest
import mozunit
from taskcluster_graph.try_test_parser import parse_test_opts

class TryTestParserTest(unittest.TestCase):

    def test_parse_opts_valid(self):
        self.assertEquals(
            parse_test_opts('all[Mulet Linux]'),
            [{ 'test': 'all', 'platforms': ['Mulet Linux'] }]
        )

        self.assertEquals(
            parse_test_opts('all[Amazing, Foobar woot,yeah]'),
            [{ 'test': 'all', 'platforms': ['Amazing', 'Foobar woot', 'yeah'] }]
        )

        self.assertEquals(
            parse_test_opts('a,b, c'),
            [
                { 'test': 'a' },
                { 'test': 'b' },
                { 'test': 'c' },
            ]
        )
        self.assertEquals(
            parse_test_opts('woot, bar[b], baz, qux[ z ],a'),
            [
                { 'test': 'woot' },
                { 'test': 'bar', 'platforms': ['b'] },
                { 'test': 'baz' },
                { 'test': 'qux', 'platforms': ['z'] },
                { 'test': 'a' }
            ]
        )

        self.assertEquals(
            parse_test_opts('mochitest-3[Ubuntu,10.6,10.8,Windows XP,Windows 7,Windows 8]'),
            [
                {
                    'test': 'mochitest-3',
                    'platforms': [
                        'Ubuntu', '10.6', '10.8', 'Windows XP', 'Windows 7', 'Windows 8'
                    ]
                }
            ]
        )

        self.assertEquals(
            parse_test_opts(''),
            []
        )

if __name__ == '__main__':
    mozunit.main()
