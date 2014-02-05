# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import os

import mozpack.path as mozpath

from .base import MozbuildObject
from .util import DefaultOnReadDict


def rewrite_test_base(test, new_base):
    """Rewrite paths in a test to be under a new base path.

    This is useful for running tests from a separate location from where they
    were defined.
    """
    test['here'] = mozpath.join(new_base, test['dir_relpath'])
    test['path'] = mozpath.join(new_base, test['file_relpath'])

    return test


class TestMetadata(object):
    """Holds information about tests.

    This class provides an API to query tests active in the build
    configuration.
    """

    def __init__(self, filename=None):
        self._tests_by_path = DefaultOnReadDict({}, global_default=[])
        self._tests_by_flavor = DefaultOnReadDict({}, global_default=set())
        self._test_dirs = set()

        if filename:
            with open(filename, 'rt') as fh:
                d = json.load(fh)

                for path, tests in d.items():
                    for metadata in tests:
                        self._tests_by_path[path].append(metadata)
                        self._test_dirs.add(os.path.dirname(path))

                        flavor = metadata.get('flavor')
                        self._tests_by_flavor[flavor].add(path)

    def tests_with_flavor(self, flavor):
        """Obtain all tests having the specified flavor.

        This is a generator of dicts describing each test.
        """

        for path in sorted(self._tests_by_flavor.get(flavor, [])):
            yield self._tests_by_path[path]

    def resolve_tests(self, path=None, flavor=None, under_path=None):
        """Resolve tests from an identifier.

        This is a generator of dicts describing each test.

        If ``path`` is a known test file, the tests associated with that file
        are returned. Files can be specified by full path (relative to main
        directory), or as a file fragment. The lookup simply tests whether
        the string is in the path of a test file.

        If ``path`` is a directory, the tests in that directory are returned.

        If ``under_path`` is a string, it will be used to filter out tests that
        aren't in the specified path prefix relative to topsrcdir or the
        test's installed dir.

        If ``flavor`` is a string, it will be used to filter returned tests
        to only be the flavor specified. A flavor is something like
        ``xpcshell``.
        """
        def fltr(tests):
            for test in tests:
                if flavor and test.get('flavor') != flavor:
                    continue

                if under_path \
                    and not test['file_relpath'].startswith(under_path):
                    continue

                # Make a copy so modifications don't change the source.
                yield dict(test)

        path = mozpath.normpath(path) if path else None

        if path in self._test_dirs:
            candidates = []
            for p, tests in sorted(self._tests_by_path.items()):
                if not p.startswith(path):
                    continue

                candidates.extend(tests)

            for test in fltr(candidates):
                yield test

            return

        # Do file lookup.
        candidates = []
        for p, tests in sorted(self._tests_by_path.items()):
            if path is None or path in p:
                candidates.extend(tests)

        for test in fltr(candidates):
            yield test


class TestResolver(MozbuildObject):
    """Helper to resolve tests from the current environment to test files."""

    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        self._tests = TestMetadata(filename=os.path.join(self.topobjdir,
            'all-tests.json'))
        self._test_rewrites = {
            'a11y': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'a11y'),
            'browser-chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'browser'),
            'chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'chrome'),
            'mochitest': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'tests'),
            'xpcshell': os.path.join(self.topobjdir, '_tests', 'xpcshell'),
        }

    def resolve_tests(self, cwd=None, **kwargs):
        """Resolve tests in the context of the current environment.

        This is a more intelligent version of TestMetadata.resolve_tests().

        This function provides additional massaging and filtering of low-level
        results.

        Paths in returned tests are automatically translated to the paths in
        the _tests directory under the object directory.

        If cwd is defined, we will limit our results to tests under the
        directory specified. The directory should be defined as an absolute
        path under topsrcdir or topobjdir for it to work properly.
        """
        rewrite_base = None

        if cwd:
            norm_cwd = mozpath.normpath(cwd)
            norm_srcdir = mozpath.normpath(self.topsrcdir)
            norm_objdir = mozpath.normpath(self.topobjdir)

            reldir = None

            if norm_cwd.startswith(norm_objdir):
                reldir = norm_cwd[len(norm_objdir)+1:]
            elif norm_cwd.startswith(norm_srcdir):
                reldir = norm_cwd[len(norm_srcdir)+1:]

            result = self._tests.resolve_tests(under_path=reldir,
                **kwargs)

        else:
            result = self._tests.resolve_tests(**kwargs)

        for test in result:
            rewrite_base = self._test_rewrites.get(test['flavor'], None)

            if rewrite_base:
                yield rewrite_test_base(test, rewrite_base)
            else:
                yield test
