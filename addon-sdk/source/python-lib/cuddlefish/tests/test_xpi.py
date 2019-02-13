# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
import zipfile
import pprint
import shutil

import simplejson as json
from cuddlefish import xpi, packaging, manifest, buildJID
from cuddlefish.tests import test_packaging
from test_linker import up

import xml.etree.ElementTree as ElementTree

xpi_template_path = os.path.join(test_packaging.static_files_path,
                                 'xpi-template')

fake_manifest = '<RDF><!-- Extension metadata is here. --></RDF>'


class Bug588119Tests(unittest.TestCase):
    def makexpi(self, pkg_name):
        self.xpiname = "%s.xpi" % pkg_name
        create_xpi(self.xpiname, pkg_name, 'bug-588119-files')
        self.xpi = zipfile.ZipFile(self.xpiname, 'r')
        options = self.xpi.read('harness-options.json')
        self.xpi_harness_options = json.loads(options)

    def setUp(self):
        self.xpiname = None
        self.xpi = None

    def tearDown(self):
        if self.xpi:
            self.xpi.close()
        if self.xpiname and os.path.exists(self.xpiname):
            os.remove(self.xpiname)

    def testPackageWithImplicitIcon(self):
        self.makexpi('implicit-icon')
        assert 'icon.png' in self.xpi.namelist()

    def testPackageWithImplicitIcon64(self):
        self.makexpi('implicit-icon')
        assert 'icon64.png' in self.xpi.namelist()

    def testPackageWithExplicitIcon(self):
        self.makexpi('explicit-icon')
        assert 'icon.png' in self.xpi.namelist()

    def testPackageWithExplicitIcon64(self):
        self.makexpi('explicit-icon')
        assert 'icon64.png' in self.xpi.namelist()

    def testPackageWithNoIcon(self):
        self.makexpi('no-icon')
        assert 'icon.png' not in self.xpi.namelist()

    def testIconPathNotInHarnessOptions(self):
        self.makexpi('implicit-icon')
        assert 'icon' not in self.xpi_harness_options

    def testIcon64PathNotInHarnessOptions(self):
        self.makexpi('implicit-icon')
        assert 'icon64' not in self.xpi_harness_options

class ExtraHarnessOptions(unittest.TestCase):
    def setUp(self):
        self.xpiname = None
        self.xpi = None

    def tearDown(self):
        if self.xpi:
            self.xpi.close()
        if self.xpiname and os.path.exists(self.xpiname):
            os.remove(self.xpiname)

    def testOptions(self):
        pkg_name = "extra-options"
        self.xpiname = "%s.xpi" % pkg_name
        create_xpi(self.xpiname, pkg_name, "bug-669274-files",
                   extra_harness_options={"builderVersion": "futuristic"})
        self.xpi = zipfile.ZipFile(self.xpiname, 'r')
        options = self.xpi.read('harness-options.json')
        hopts = json.loads(options)
        self.failUnless("builderVersion" in hopts)
        self.failUnlessEqual(hopts["builderVersion"], "futuristic")

    def testBadOptionName(self):
        pkg_name = "extra-options"
        self.xpiname = "%s.xpi" % pkg_name
        self.failUnlessRaises(xpi.HarnessOptionAlreadyDefinedError,
                              create_xpi,
                              self.xpiname, pkg_name, "bug-669274-files",
                              extra_harness_options={"main": "already in use"})

class SmallXPI(unittest.TestCase):
    def setUp(self):
        self.root = up(os.path.abspath(__file__), 4)
    def get_linker_files_dir(self, name):
        return os.path.join(up(os.path.abspath(__file__)), "linker-files", name)
    def get_pkg(self, name):
        d = self.get_linker_files_dir(name)
        return packaging.get_config_in_dir(d)

    def get_basedir(self):
        return os.path.join(".test_tmp", self.id())
    def make_basedir(self):
        basedir = self.get_basedir()
        if os.path.isdir(basedir):
            here = os.path.abspath(os.getcwd())
            assert os.path.abspath(basedir).startswith(here) # safety
            shutil.rmtree(basedir)
        os.makedirs(basedir)
        return basedir

    def test_contents(self):
        target_cfg = self.get_pkg("three")
        package_path = [self.get_linker_files_dir("three-deps")]
        pkg_cfg = packaging.build_config(self.root, target_cfg,
                                         packagepath=package_path)
        deps = packaging.get_deps_for_targets(pkg_cfg,
                                              [target_cfg.name, "addon-sdk"])
        addon_sdk_dir = pkg_cfg.packages["addon-sdk"].lib[0]
        m = manifest.build_manifest(target_cfg, pkg_cfg, deps, scan_tests=False)
        used_files = list(m.get_used_files(True))
        here = up(os.path.abspath(__file__))
        def absify(*parts):
            fn = os.path.join(here, "linker-files", *parts)
            return os.path.abspath(fn)
        expected = [absify(*parts) for parts in
                    [("three", "lib", "main.js"),
                     ("three-deps", "three-a", "lib", "main.js"),
                     ("three-deps", "three-a", "lib", "subdir", "subfile.js"),
                     ("three", "data", "msg.txt"),
                     ("three", "data", "subdir", "submsg.txt"),
                     ("three-deps", "three-b", "lib", "main.js"),
                     ("three-deps", "three-c", "lib", "main.js"),
                     ("three-deps", "three-c", "lib", "sub", "foo.js")
                     ]]

        add_addon_sdk= lambda path: os.path.join(addon_sdk_dir, path)
        expected.extend([add_addon_sdk(module) for module in [
            os.path.join("sdk", "self.js"),
            os.path.join("sdk", "core", "promise.js"),
            os.path.join("sdk", "net", "url.js"),
            os.path.join("sdk", "util", "object.js"),
            os.path.join("sdk", "util", "array.js"),
            os.path.join("sdk", "preferences", "service.js")
            ]])

        missing = set(expected) - set(used_files)
        extra = set(used_files) - set(expected)
        self.failUnlessEqual(list(missing), [])
        self.failUnlessEqual(list(extra), [])
        used_deps = m.get_used_packages()

        build = packaging.generate_build_for_target(pkg_cfg, target_cfg.name,
                                                    used_deps,
                                                    include_tests=False)
        options = {'main': target_cfg.main}
        options.update(build)
        basedir = self.make_basedir()
        xpi_name = os.path.join(basedir, "contents.xpi")
        xpi.build_xpi(template_root_dir=xpi_template_path,
                      manifest=fake_manifest,
                      xpi_path=xpi_name,
                      harness_options=options,
                      limit_to=used_files)
        x = zipfile.ZipFile(xpi_name, "r")
        names = x.namelist()
        expected = ["components/",
                    "components/harness.js",
                    # the real template also has 'bootstrap.js', but the fake
                    # one in tests/static-files/xpi-template doesn't
                    "harness-options.json",
                    "install.rdf",
                    "resources/",
                    "resources/addon-sdk/",
                    "resources/addon-sdk/lib/",
                    "resources/addon-sdk/lib/sdk/",
                    "resources/addon-sdk/lib/sdk/self.js",
                    "resources/addon-sdk/lib/sdk/core/",
                    "resources/addon-sdk/lib/sdk/util/",
                    "resources/addon-sdk/lib/sdk/net/",
                    "resources/addon-sdk/lib/sdk/core/promise.js",
                    "resources/addon-sdk/lib/sdk/util/object.js",
                    "resources/addon-sdk/lib/sdk/util/array.js",
                    "resources/addon-sdk/lib/sdk/net/url.js",
                    "resources/addon-sdk/lib/sdk/preferences/",
                    "resources/addon-sdk/lib/sdk/preferences/service.js",
                    "resources/three/",
                    "resources/three/lib/",
                    "resources/three/lib/main.js",
                    "resources/three/data/",
                    "resources/three/data/msg.txt",
                    "resources/three/data/subdir/",
                    "resources/three/data/subdir/submsg.txt",
                    "resources/three-a/",
                    "resources/three-a/lib/",
                    "resources/three-a/lib/main.js",
                    "resources/three-a/lib/subdir/",
                    "resources/three-a/lib/subdir/subfile.js",
                    "resources/three-b/",
                    "resources/three-b/lib/",
                    "resources/three-b/lib/main.js",
                    "resources/three-c/",
                    "resources/three-c/lib/",
                    "resources/three-c/lib/main.js",
                    "resources/three-c/lib/sub/",
                    "resources/three-c/lib/sub/foo.js",
                    # notably absent: three-a/lib/unused.js
                    "locale/",
                    "locale/fr-FR.json",
                    "locales.json",
                    ]
        # showing deltas makes failures easier to investigate
        missing = set(expected) - set(names)
        extra = set(names) - set(expected)
        self.failUnlessEqual((list(missing), list(extra)), ([], []))
        self.failUnlessEqual(sorted(names), sorted(expected))

        # check locale files
        localedata = json.loads(x.read("locales.json"))
        self.failUnlessEqual(sorted(localedata["locales"]), sorted(["fr-FR"]))
        content = x.read("locale/fr-FR.json")
        locales = json.loads(content)
        # Locale files are merged into one.
        # Conflicts are silently resolved by taking last package translation,
        # so that we get "No" translation from three-c instead of three-b one.
        self.failUnlessEqual(locales, json.loads(u'''
          {
            "No": "Nein",
            "one": "un",
            "What?": "Quoi?",
            "Yes": "Oui",
            "plural": {
              "other": "other",
              "one": "one"
            },
            "uft8_value": "\u00e9"
          }'''))


def document_dir(name):
    if name in ['packages', 'xpi-template']:
        dirname = os.path.join(test_packaging.static_files_path, name)
        document_dir_files(dirname)
    elif name == 'xpi-output':
        create_xpi('test-xpi.xpi')
        document_zip_file('test-xpi.xpi')
        os.remove('test-xpi.xpi')
    else:
        raise Exception('unknown dir: %s' % name)

def normpath(path):
    """
    Make a platform-specific relative path use '/' as a separator.
    """

    return path.replace(os.path.sep, '/')

def document_zip_file(path):
    zip = zipfile.ZipFile(path, 'r')
    for name in sorted(zip.namelist()):
        contents = zip.read(name)
        lines = contents.splitlines()
        if len(lines) == 1 and name.endswith('.json') and len(lines[0]) > 75:
            # Ideally we would json-decode this, but it results
            # in an annoying 'u' before every string literal,
            # since json decoding makes all strings unicode.
            contents = eval(contents)
            contents = pprint.pformat(contents)
            lines = contents.splitlines()
        contents = "\n  ".join(lines)
        print "%s:\n  %s" % (normpath(name), contents)
    zip.close()

def document_dir_files(path):
    filename_contents_tuples = []
    for dirpath, dirnames, filenames in os.walk(path):
        relpath = dirpath[len(path)+1:]
        for filename in filenames:
            abspath = os.path.join(dirpath, filename)
            contents = open(abspath, 'r').read()
            contents = "\n  ".join(contents.splitlines())
            relfilename = os.path.join(relpath, filename)
            filename_contents_tuples.append((normpath(relfilename), contents))
    filename_contents_tuples.sort()
    for filename, contents in filename_contents_tuples:
        print "%s:" % filename
        print "  %s" % contents

def create_xpi(xpiname, pkg_name='aardvark', dirname='static-files',
               extra_harness_options={}):
    configs = test_packaging.get_configs(pkg_name, dirname)
    options = {'main': configs.target_cfg.main,
               'jetpackID': buildJID(configs.target_cfg), }
    options.update(configs.build)
    xpi.build_xpi(template_root_dir=xpi_template_path,
                  manifest=fake_manifest,
                  xpi_path=xpiname,
                  harness_options=options,
                  extra_harness_options=extra_harness_options)

if __name__ == '__main__':
    unittest.main()
