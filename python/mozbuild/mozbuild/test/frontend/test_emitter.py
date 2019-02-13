# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from mozunit import main

from mozbuild.frontend.data import (
    BrandingFiles,
    ConfigFileSubstitution,
    Defines,
    DistFiles,
    DirectoryTraversal,
    Exports,
    GeneratedFile,
    GeneratedInclude,
    GeneratedSources,
    HostSources,
    IPDLFile,
    JARManifest,
    JsPreferenceFile,
    LocalInclude,
    Program,
    ReaderSummary,
    Resources,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestHarnessFiles,
    TestManifest,
    UnifiedSources,
    VariablePassthru,
)
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import (
    BuildReader,
    BuildReaderError,
    SandboxValidationError,
)

from mozbuild.test.common import MockConfig

import mozpack.path as mozpath


data_path = mozpath.abspath(mozpath.dirname(__file__))
data_path = mozpath.join(data_path, 'data')


class TestEmitterBasic(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop('MOZ_OBJDIR', None)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

    def reader(self, name):
        config = MockConfig(mozpath.join(data_path, name), extra_substs=dict(
            ENABLE_TESTS='1',
            BIN_SUFFIX='.prog',
            OS_TARGET='WINNT',
        ))

        return BuildReader(config)

    def read_topsrcdir(self, reader, filter_common=True):
        emitter = TreeMetadataEmitter(reader.config)
        def ack(obj):
            obj.ack()
            return obj

        objs = list(ack(o) for o in emitter.emit(reader.read_topsrcdir()))
        self.assertGreater(len(objs), 0)
        self.assertIsInstance(objs[-1], ReaderSummary)

        filtered = []
        for obj in objs:
            if filter_common and isinstance(obj, DirectoryTraversal):
                continue

            # Always filter ReaderSummary because it's asserted above.
            if isinstance(obj, ReaderSummary):
                continue

            filtered.append(obj)

        return filtered

    def test_dirs_traversal_simple(self):
        reader = self.reader('traversal-simple')
        objs = self.read_topsrcdir(reader, filter_common=False)
        self.assertEqual(len(objs), 4)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)
            self.assertEqual(o.test_dirs, [])
            self.assertTrue(os.path.isabs(o.context_main_path))
            self.assertEqual(len(o.context_all_paths), 1)

        reldirs = [o.relativedir for o in objs]
        self.assertEqual(reldirs, ['', 'foo', 'foo/biz', 'bar'])

        self.assertEqual(objs[3].affected_tiers, {'misc'})

        dirs = [[d.full_path for d in o.dirs] for o in objs]
        self.assertEqual(dirs, [
            [
                mozpath.join(reader.config.topsrcdir, 'foo'),
                mozpath.join(reader.config.topsrcdir, 'bar')
            ], [
                mozpath.join(reader.config.topsrcdir, 'foo', 'biz')
            ], [], []])

    def test_traversal_all_vars(self):
        reader = self.reader('traversal-all-vars')
        objs = self.read_topsrcdir(reader, filter_common=False)
        self.assertEqual(len(objs), 3)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)

        reldirs = set([o.relativedir for o in objs])
        self.assertEqual(reldirs, set(['', 'regular', 'test']))

        for o in objs:
            reldir = o.relativedir

            if reldir == '':
                self.assertEqual([d.full_path for d in o.dirs], [
                    mozpath.join(reader.config.topsrcdir, 'regular')])
                self.assertEqual([d.full_path for d in o.test_dirs], [
                    mozpath.join(reader.config.topsrcdir, 'test')])

    def test_config_file_substitution(self):
        reader = self.reader('config-file-substitution')
        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 2)

        self.assertIsInstance(objs[0], ConfigFileSubstitution)
        self.assertIsInstance(objs[1], ConfigFileSubstitution)

        topobjdir = mozpath.abspath(reader.config.topobjdir)
        self.assertEqual(objs[0].relpath, 'foo')
        self.assertEqual(mozpath.normpath(objs[0].output_path),
            mozpath.normpath(mozpath.join(topobjdir, 'foo')))
        self.assertEqual(mozpath.normpath(objs[1].output_path),
            mozpath.normpath(mozpath.join(topobjdir, 'bar')))

    def test_variable_passthru(self):
        reader = self.reader('variable-passthru')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], VariablePassthru)

        wanted = {
            'DISABLE_STL_WRAPPING': True,
            'EXTRA_COMPONENTS': ['dummy.manifest', 'fans.js', 'tans.js'],
            'EXTRA_PP_COMPONENTS': ['fans.pp.js', 'tans.pp.js'],
            'FAIL_ON_WARNINGS': True,
            'NO_DIST_INSTALL': True,
            'VISIBILITY_FLAGS': '',
            'RCFILE': 'foo.rc',
            'RESFILE': 'bar.res',
            'RCINCLUDE': 'bar.rc',
            'DEFFILE': 'baz.def',
            'USE_STATIC_LIBS': True,
            'MOZBUILD_CFLAGS': ['-fno-exceptions', '-w'],
            'MOZBUILD_CXXFLAGS': ['-fcxx-exceptions', '-include foo.h'],
            'MOZBUILD_LDFLAGS': ['-framework Foo', '-x', '-DELAYLOAD:foo.dll',
                                 '-DELAYLOAD:bar.dll'],
            'WIN32_EXE_LDFLAGS': ['-subsystem:console'],
        }

        variables = objs[0].variables
        maxDiff = self.maxDiff
        self.maxDiff = None
        self.assertEqual(wanted, variables)
        self.maxDiff = maxDiff

    def test_generated_files(self):
        reader = self.reader('generated-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        for o in objs:
            self.assertIsInstance(o, GeneratedFile)

        expected = ['bar.c', 'foo.c']
        for o, expected_filename in zip(objs, expected):
            self.assertEqual(o.output, expected_filename)
            self.assertEqual(o.script, None)
            self.assertEqual(o.method, None)
            self.assertEqual(o.inputs, [])

    def test_generated_files_method_names(self):
        reader = self.reader('generated-files-method-names')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 2)
        for o in objs:
            self.assertIsInstance(o, GeneratedFile)

        expected = ['bar.c', 'foo.c']
        expected_method_names = ['make_bar', 'main']
        for o, expected_filename, expected_method in zip(objs, expected, expected_method_names):
            self.assertEqual(o.output, expected_filename)
            self.assertEqual(o.method, expected_method)
            self.assertEqual(o.inputs, [])

    def test_generated_files_absolute_script(self):
        reader = self.reader('generated-files-absolute-script')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)

        o = objs[0]
        self.assertIsInstance(o, GeneratedFile)
        self.assertEqual(o.output, 'bar.c')
        self.assertRegexpMatches(o.script, 'script.py$')
        self.assertEqual(o.method, 'make_bar')
        self.assertEqual(o.inputs, [])

    def test_generated_files_no_script(self):
        reader = self.reader('generated-files-no-script')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Script for generating bar.c does not exist'):
            objs = self.read_topsrcdir(reader)

    def test_generated_files_no_inputs(self):
        reader = self.reader('generated-files-no-inputs')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Input for generating foo.c does not exist'):
            objs = self.read_topsrcdir(reader)

    def test_generated_files_no_python_script(self):
        reader = self.reader('generated-files-no-python-script')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Script for generating bar.c does not end in .py'):
            objs = self.read_topsrcdir(reader)

    def test_exports(self):
        reader = self.reader('exports')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], Exports)

        expected = [
            ('', ['foo.h', 'bar.h', 'baz.h']),
            ('mozilla', ['mozilla1.h', 'mozilla2.h']),
            ('mozilla/dom', ['dom1.h', 'dom2.h', 'dom3.h']),
            ('mozilla/gfx', ['gfx.h']),
            ('nspr/private', ['pprio.h', 'pprthred.h']),
            ('overwrite', ['new.h']),
            ('vpx', ['mem.h', 'mem2.h']),
        ]
        for (expect_path, expect_headers), (actual_path, actual_headers) in \
                zip(expected, [(path, list(seq)) for path, seq in objs[0].exports.walk()]):
            self.assertEqual(expect_path, actual_path)
            self.assertEqual(expect_headers, actual_headers)

    def test_test_harness_files(self):
        reader = self.reader('test-harness-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], TestHarnessFiles)

        expected = {
            'mochitest': ['runtests.py', 'utils.py'],
            'testing/mochitest': ['mochitest.py', 'mochitest.ini'],
        }

        for path, strings in objs[0].srcdir_files.iteritems():
            self.assertTrue(path in expected)
            basenames = sorted(mozpath.basename(s) for s in strings)
            self.assertEqual(sorted(expected[path]), basenames)

    def test_test_harness_files_root(self):
        reader = self.reader('test-harness-files-root')
        with self.assertRaisesRegexp(SandboxValidationError,
            'Cannot install files to the root of TEST_HARNESS_FILES'):
            objs = self.read_topsrcdir(reader)

    def test_resources(self):
        reader = self.reader('resources')
        objs = self.read_topsrcdir(reader)

        expected_defines = dict(reader.config.defines)
        expected_defines.update({
            'FOO': True,
            'BAR': 'BAZ',
        })

        self.assertEqual(len(objs), 2)
        self.assertIsInstance(objs[0], Defines)
        self.assertIsInstance(objs[1], Resources)

        self.assertEqual(objs[1].defines, expected_defines)

        resources = objs[1].resources
        self.assertEqual(resources._strings, ['foo.res', 'bar.res', 'baz.res',
                                              'foo_p.res.in', 'bar_p.res.in', 'baz_p.res.in'])
        self.assertFalse(resources['foo.res'].preprocess)
        self.assertFalse(resources['bar.res'].preprocess)
        self.assertFalse(resources['baz.res'].preprocess)
        self.assertTrue(resources['foo_p.res.in'].preprocess)
        self.assertTrue(resources['bar_p.res.in'].preprocess)
        self.assertTrue(resources['baz_p.res.in'].preprocess)

        self.assertIn('mozilla', resources._children)
        mozilla = resources._children['mozilla']
        self.assertEqual(mozilla._strings, ['mozilla1.res', 'mozilla2.res',
                                            'mozilla1_p.res.in', 'mozilla2_p.res.in'])
        self.assertFalse(mozilla['mozilla1.res'].preprocess)
        self.assertFalse(mozilla['mozilla2.res'].preprocess)
        self.assertTrue(mozilla['mozilla1_p.res.in'].preprocess)
        self.assertTrue(mozilla['mozilla2_p.res.in'].preprocess)

        self.assertIn('dom', mozilla._children)
        dom = mozilla._children['dom']
        self.assertEqual(dom._strings, ['dom1.res', 'dom2.res', 'dom3.res'])

        self.assertIn('gfx', mozilla._children)
        gfx = mozilla._children['gfx']
        self.assertEqual(gfx._strings, ['gfx.res'])

        self.assertIn('vpx', resources._children)
        vpx = resources._children['vpx']
        self.assertEqual(vpx._strings, ['mem.res', 'mem2.res'])

        self.assertIn('nspr', resources._children)
        nspr = resources._children['nspr']
        self.assertIn('private', nspr._children)
        private = nspr._children['private']
        self.assertEqual(private._strings, ['pprio.res', 'pprthred.res'])

        self.assertIn('overwrite', resources._children)
        overwrite = resources._children['overwrite']
        self.assertEqual(overwrite._strings, ['new.res'])

    def test_branding_files(self):
        reader = self.reader('branding-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], BrandingFiles)

        files = objs[0].files

        self.assertEqual(files._strings, ['app.ico', 'bar.ico', 'baz.png', 'foo.xpm'])
        self.assertEqual(files['app.ico'].source, 'test/bar.ico')

        self.assertIn('icons', files._children)
        icons = files._children['icons']

        self.assertEqual(icons._strings, ['quux.icns'])

    def test_preferences_js(self):
        reader = self.reader('js_preference_files')
        objs = self.read_topsrcdir(reader)

        prefs = [o.path for o in objs if isinstance(o, JsPreferenceFile)]

        prefsByDir = [
            'valid_val/prefs.js',
            'ww/ww.js',
            'xx/xx.js',
            'yy/yy.js',
            ]

        self.assertEqual(sorted(prefs), prefsByDir)

    def test_program(self):
        reader = self.reader('program')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        self.assertIsInstance(objs[0], Program)
        self.assertIsInstance(objs[1], SimpleProgram)
        self.assertIsInstance(objs[2], SimpleProgram)

        self.assertEqual(objs[0].program, 'test_program.prog')
        self.assertEqual(objs[1].program, 'test_program1.prog')
        self.assertEqual(objs[2].program, 'test_program2.prog')

    def test_test_manifest_missing_manifest(self):
        """A missing manifest file should result in an error."""
        reader = self.reader('test-manifest-missing-manifest')

        with self.assertRaisesRegexp(SandboxValidationError, 'IOError: Missing files'):
            self.read_topsrcdir(reader)

    def test_empty_test_manifest_rejected(self):
        """A test manifest without any entries is rejected."""
        reader = self.reader('test-manifest-empty')

        with self.assertRaisesRegexp(SandboxValidationError, 'Empty test manifest'):
            self.read_topsrcdir(reader)


    def test_test_manifest_just_support_files(self):
        """A test manifest with no tests but support-files is supported."""
        reader = self.reader('test-manifest-just-support')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 2)
        paths = sorted([k[len(o.directory)+1:] for k in o.installs.keys()])
        self.assertEqual(paths, ["foo.txt", "just-support.ini"])

    def test_test_manifest_absolute_support_files(self):
        """Support files starting with '/' are placed relative to the install root"""
        reader = self.reader('test-manifest-absolute-support')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 2)
        expected = [
            mozpath.normpath(mozpath.join(o.install_prefix, "../.well-known/foo.txt")),
            mozpath.join(o.install_prefix, "absolute-support.ini"),
        ]
        paths = sorted([v[0] for v in o.installs.values()])
        self.assertEqual(paths, expected)

    def test_test_manifest_install_to_subdir(self):
        """ """
        reader = self.reader('test-manifest-install-subdir')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 3)
        self.assertEqual(o.manifest_relpath, "subdir.ini")
        self.assertEqual(o.manifest_obj_relpath, "subdir/subdir.ini")
        expected = [
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/subdir.ini")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/support.txt")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/test_foo.html")),
        ]
        paths = sorted([v[0] for v in o.installs.values()])
        self.assertEqual(paths, expected)

    def test_test_manifest_install_includes(self):
        """Ensure that any [include:foo.ini] are copied to the objdir."""
        reader = self.reader('test-manifest-install-includes')

        objs = self.read_topsrcdir(reader)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(len(o.installs), 3)
        self.assertEqual(o.manifest_relpath, "mochitest.ini")
        self.assertEqual(o.manifest_obj_relpath, "subdir/mochitest.ini")
        expected = [
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/common.ini")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/mochitest.ini")),
            mozpath.normpath(mozpath.join(o.install_prefix, "subdir/test_foo.html")),
        ]
        paths = sorted([v[0] for v in o.installs.values()])
        self.assertEqual(paths, expected)

    def test_test_manifest_includes(self):
        """Ensure that manifest objects from the emitter list a correct manifest.
        """
        reader = self.reader('test-manifest-emitted-includes')
        [obj] = self.read_topsrcdir(reader)

        # Expected manifest leafs for our tests.
        expected_manifests = {
            'reftest1.html': 'reftest.list',
            'reftest1-ref.html': 'reftest.list',
            'reftest2.html': 'included-reftest.list',
            'reftest2-ref.html': 'included-reftest.list',
        }

        for t in obj.tests:
            self.assertTrue(t['manifest'].endswith(expected_manifests[t['name']]))


    def test_test_manifest_keys_extracted(self):
        """Ensure all metadata from test manifests is extracted."""
        reader = self.reader('test-manifest-keys-extracted')

        objs = [o for o in self.read_topsrcdir(reader)
                if isinstance(o, TestManifest)]

        self.assertEqual(len(objs), 8)

        metadata = {
            'a11y.ini': {
                'flavor': 'a11y',
                'installs': {
                    'a11y.ini': False,
                    'test_a11y.js': True,
                },
                'pattern-installs': 1,
            },
            'browser.ini': {
                'flavor': 'browser-chrome',
                'installs': {
                    'browser.ini': False,
                    'test_browser.js': True,
                    'support1': False,
                    'support2': False,
                },
            },
            'metro.ini': {
                'flavor': 'metro-chrome',
                'installs': {
                    'metro.ini': False,
                    'test_metro.js': True,
                },
            },
            'mochitest.ini': {
                'flavor': 'mochitest',
                'installs': {
                    'mochitest.ini': False,
                    'test_mochitest.js': True,
                },
                'external': {
                    'external1',
                    'external2',
                },
            },
            'chrome.ini': {
                'flavor': 'chrome',
                'installs': {
                    'chrome.ini': False,
                    'test_chrome.js': True,
                },
            },
            'xpcshell.ini': {
                'flavor': 'xpcshell',
                'dupe': True,
                'installs': {
                    'xpcshell.ini': False,
                    'test_xpcshell.js': True,
                    'head1': False,
                    'head2': False,
                    'tail1': False,
                    'tail2': False,
                },
            },
            'reftest.list': {
                'flavor': 'reftest',
                'installs': {},
            },
            'crashtest.list': {
                'flavor': 'crashtest',
                'installs': {},
            },
        }

        for o in objs:
            m = metadata[mozpath.basename(o.manifest_relpath)]

            self.assertTrue(o.path.startswith(o.directory))
            self.assertEqual(o.flavor, m['flavor'])
            self.assertEqual(o.dupe_manifest, m.get('dupe', False))

            external_normalized = set(mozpath.basename(p) for p in
                    o.external_installs)
            self.assertEqual(external_normalized, m.get('external', set()))

            self.assertEqual(len(o.installs), len(m['installs']))
            for path in o.installs.keys():
                self.assertTrue(path.startswith(o.directory))
                relpath = path[len(o.directory)+1:]

                self.assertIn(relpath, m['installs'])
                self.assertEqual(o.installs[path][1], m['installs'][relpath])

            if 'pattern-installs' in m:
                self.assertEqual(len(o.pattern_installs), m['pattern-installs'])

    def test_test_manifest_unmatched_generated(self):
        reader = self.reader('test-manifest-unmatched-generated')

        with self.assertRaisesRegexp(SandboxValidationError,
            'entry in generated-files not present elsewhere'):
            self.read_topsrcdir(reader),

    def test_test_manifest_parent_support_files_dir(self):
        """support-files referencing a file in a parent directory works."""
        reader = self.reader('test-manifest-parent-support-files-dir')

        objs = [o for o in self.read_topsrcdir(reader)
                if isinstance(o, TestManifest)]

        self.assertEqual(len(objs), 1)

        o = objs[0]

        expected = mozpath.join(o.srcdir, 'support-file.txt')
        self.assertIn(expected, o.installs)
        self.assertEqual(o.installs[expected],
            ('testing/mochitest/tests/child/support-file.txt', False))

    def test_test_manifest_missing_test_error(self):
        """Missing test files should result in error."""
        reader = self.reader('test-manifest-missing-test-file')

        with self.assertRaisesRegexp(SandboxValidationError,
            'lists test that does not exist: test_missing.html'):
            self.read_topsrcdir(reader)

    def test_test_manifest_missing_test_error_unfiltered(self):
        """Missing test files should result in error, even when the test list is not filtered."""
        reader = self.reader('test-manifest-missing-test-file-unfiltered')

        with self.assertRaisesRegexp(SandboxValidationError,
            'lists test that does not exist: missing.js'):
            self.read_topsrcdir(reader)

    def test_ipdl_sources(self):
        reader = self.reader('ipdl_sources')
        objs = self.read_topsrcdir(reader)

        ipdls = []
        for o in objs:
            if isinstance(o, IPDLFile):
                ipdls.append('%s/%s' % (o.relativedir, o.basename))

        expected = [
            'bar/bar.ipdl',
            'bar/bar2.ipdlh',
            'foo/foo.ipdl',
            'foo/foo2.ipdlh',
        ]

        self.assertEqual(ipdls, expected)

    def test_local_includes(self):
        """Test that LOCAL_INCLUDES is emitted correctly."""
        reader = self.reader('local_includes')
        objs = self.read_topsrcdir(reader)

        local_includes = [o.path for o in objs if isinstance(o, LocalInclude)]
        expected = [
            '/bar/baz',
            'foo',
        ]

        self.assertEqual(local_includes, expected)

    def test_generated_includes(self):
        """Test that GENERATED_INCLUDES is emitted correctly."""
        reader = self.reader('generated_includes')
        objs = self.read_topsrcdir(reader)

        generated_includes = [o.path for o in objs if isinstance(o, GeneratedInclude)]
        expected = [
            '/bar/baz',
            'foo',
        ]

        self.assertEqual(generated_includes, expected)

    def test_defines(self):
        reader = self.reader('defines')
        objs = self.read_topsrcdir(reader)

        defines = {}
        for o in objs:
            if isinstance(o, Defines):
                defines = o.defines

        expected = {
            'BAR': 7,
            'BAZ': '"abcd"',
            'FOO': True,
            'VALUE': 'xyz',
            'QUX': False,
        }

        self.assertEqual(defines, expected)

    def test_jar_manifests(self):
        reader = self.reader('jar-manifests')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        for obj in objs:
            self.assertIsInstance(obj, JARManifest)
            self.assertTrue(os.path.isabs(obj.path))

    def test_jar_manifests_multiple_files(self):
        with self.assertRaisesRegexp(SandboxValidationError, 'limited to one value'):
            reader = self.reader('jar-manifests-multiple-files')
            self.read_topsrcdir(reader)

    def test_xpidl_module_no_sources(self):
        """XPIDL_MODULE without XPIDL_SOURCES should be rejected."""
        with self.assertRaisesRegexp(SandboxValidationError, 'XPIDL_MODULE '
            'cannot be defined'):
            reader = self.reader('xpidl-module-no-sources')
            self.read_topsrcdir(reader)

    def test_missing_local_includes(self):
        """LOCAL_INCLUDES containing non-existent directories should be rejected."""
        with self.assertRaisesRegexp(SandboxValidationError, 'Path specified in '
            'LOCAL_INCLUDES does not exist'):
            reader = self.reader('missing-local-includes')
            self.read_topsrcdir(reader)

    def test_library_defines(self):
        """Test that LIBRARY_DEFINES is propagated properly."""
        reader = self.reader('library-defines')
        objs = self.read_topsrcdir(reader)

        libraries = [o for o in objs if isinstance(o,StaticLibrary)]
        expected = {
            'liba': '-DIN_LIBA',
            'libb': '-DIN_LIBA -DIN_LIBB',
            'libc': '-DIN_LIBA -DIN_LIBB',
            'libd': ''
        }
        defines = {}
        for lib in libraries:
            defines[lib.basename] = ' '.join(lib.defines.get_defines())
        self.assertEqual(expected, defines)

    def test_sources(self):
        """Test that SOURCES works properly."""
        reader = self.reader('sources')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 6)
        for o in objs:
            self.assertIsInstance(o, Sources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 6)

        expected = {
            '.cpp': ['a.cpp', 'b.cc', 'c.cxx'],
            '.c': ['d.c'],
            '.m': ['e.m'],
            '.mm': ['f.mm'],
            '.S': ['g.S'],
            '.s': ['h.s', 'i.asm'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])

    def test_generated_sources(self):
        """Test that GENERATED_SOURCES works properly."""
        reader = self.reader('generated-sources')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 6)

        generated_sources = [o for o in objs if isinstance(o, GeneratedSources)]
        self.assertEqual(len(generated_sources), 6)

        suffix_map = {obj.canonical_suffix: obj for obj in generated_sources}
        self.assertEqual(len(suffix_map), 6)

        expected = {
            '.cpp': ['a.cpp', 'b.cc', 'c.cxx'],
            '.c': ['d.c'],
            '.m': ['e.m'],
            '.mm': ['f.mm'],
            '.S': ['g.S'],
            '.s': ['h.s', 'i.asm'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topobjdir, f) for f in files])

    def test_host_sources(self):
        """Test that HOST_SOURCES works properly."""
        reader = self.reader('host-sources')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, HostSources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 3)

        expected = {
            '.cpp': ['a.cpp', 'b.cc', 'c.cxx'],
            '.c': ['d.c'],
            '.mm': ['e.mm', 'f.mm'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])

    def test_unified_sources(self):
        """Test that UNIFIED_SOURCES works properly."""
        reader = self.reader('unified-sources')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, UnifiedSources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 3)

        expected = {
            '.cpp': ['bar.cxx', 'foo.cpp', 'quux.cc'],
            '.mm': ['objc1.mm', 'objc2.mm'],
            '.c': ['c1.c', 'c2.c'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])
            self.assertTrue(sources.have_unified_mapping)

    def test_unified_sources_non_unified(self):
        """Test that UNIFIED_SOURCES with FILES_PER_UNIFIED_FILE=1 works properly."""
        reader = self.reader('unified-sources-non-unified')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 3)
        for o in objs:
            self.assertIsInstance(o, UnifiedSources)

        suffix_map = {obj.canonical_suffix: obj for obj in objs}
        self.assertEqual(len(suffix_map), 3)

        expected = {
            '.cpp': ['bar.cxx', 'foo.cpp', 'quux.cc'],
            '.mm': ['objc1.mm', 'objc2.mm'],
            '.c': ['c1.c', 'c2.c'],
        }
        for suffix, files in expected.items():
            sources = suffix_map[suffix]
            self.assertEqual(
                sources.files,
                [mozpath.join(reader.config.topsrcdir, f) for f in files])
            self.assertFalse(sources.have_unified_mapping)

    def test_dist_files(self):
        """Test that DIST_FILES works properly."""
        reader = self.reader('dist-files')
        objs = self.read_topsrcdir(reader)

        self.assertEqual(len(objs), 1)
        self.assertIsInstance(objs[0], DistFiles)

        self.assertEqual(len(objs[0].files), 2)

        expected = {'install.rdf', 'main.js'}
        for f in objs[0].files:
            self.assertTrue(f in expected)

    def test_missing_dist_files(self):
        """Test that DIST_FILES with missing files throws errors."""
        with self.assertRaisesRegexp(SandboxValidationError, 'File listed in '
            'DIST_FILES does not exist'):
            reader = self.reader('dist-files-missing')
            self.read_topsrcdir(reader)

if __name__ == '__main__':
    main()
