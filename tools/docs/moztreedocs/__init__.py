# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os

from mozbuild.base import MozbuildObject
from mozbuild.frontend.reader import BuildReader
from mozbuild.util import memoize
from mozpack.copier import FileCopier
from mozpack.files import FileFinder
from mozpack.manifests import InstallManifest

import sphinx
import sphinx.apidoc

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

MAIN_DOC_PATH = os.path.join(build.topsrcdir, 'tools', 'docs')

logger = sphinx.util.logging.getLogger(__name__)


@memoize
def read_build_config(docdir):
    """Read the active build config and return the relevant doc paths.

    The return value is cached so re-generating with the same docdir won't
    invoke the build system a second time."""
    trees = {}
    python_package_dirs = set()

    is_main = docdir == MAIN_DOC_PATH
    relevant_mozbuild_path = None if is_main else docdir

    # Reading the Sphinx variables doesn't require a full build context.
    # Only define the parts we need.
    class fakeconfig(object):
        topsrcdir = build.topsrcdir

    variables = ('SPHINX_TREES', 'SPHINX_PYTHON_PACKAGE_DIRS')
    reader = BuildReader(fakeconfig())
    result = reader.find_variables_from_ast(variables, path=relevant_mozbuild_path)
    for path, name, key, value in result:
        reldir = os.path.dirname(path)

        if name == 'SPHINX_TREES':
            # If we're building a subtree, only process that specific subtree.
            absdir = os.path.join(build.topsrcdir, reldir, value)
            if not is_main and absdir not in (docdir, MAIN_DOC_PATH):
                continue

            assert key
            if key.startswith('/'):
                key = key[1:]
            else:
                key = os.path.normpath(os.path.join(reldir, key))

            if key in trees:
                raise Exception('%s has already been registered as a destination.' % key)
            trees[key] = os.path.join(reldir, value)

        if name == 'SPHINX_PYTHON_PACKAGE_DIRS':
            python_package_dirs.add(os.path.join(reldir, value))

    return trees, python_package_dirs


class _SphinxManager(object):
    """Manages the generation of Sphinx documentation for the tree."""

    def __init__(self, topsrcdir, main_path):
        self.topsrcdir = topsrcdir
        self.conf_py_path = os.path.join(main_path, 'conf.py')
        self.index_path = os.path.join(main_path, 'index.rst')

        # Instance variables that get set in self.generate_docs()
        self.staging_dir = None
        self.trees = None
        self.python_package_dirs = None

    def generate_docs(self, app):
        """Generate/stage documentation."""
        self.staging_dir = os.path.join(app.outdir, '_staging')

        logger.info('Reading Sphinx metadata from build configuration')
        self.trees, self.python_package_dirs = read_build_config(app.srcdir)

        logger.info('Staging static documentation')
        self._synchronize_docs()
        logger.info('Generating Python API documentation')
        self._generate_python_api_docs()

    def _generate_python_api_docs(self):
        """Generate Python API doc files."""
        out_dir = os.path.join(self.staging_dir, 'python')
        base_args = ['--no-toc', '-o', out_dir]

        for p in sorted(self.python_package_dirs):
            full = os.path.join(self.topsrcdir, p)

            finder = FileFinder(full)
            dirs = {os.path.dirname(f[0]) for f in finder.find('**')}

            excludes = {d for d in dirs if d.endswith('test')}

            args = list(base_args)
            args.append(full)
            args.extend(excludes)

            sphinx.ext.apidoc.main(argv=args)

    def _synchronize_docs(self):
        m = InstallManifest()

        m.add_link(self.conf_py_path, 'conf.py')

        for dest, source in sorted(self.trees.items()):
            source_dir = os.path.join(self.topsrcdir, source)
            for root, dirs, files in os.walk(source_dir):
                for f in files:
                    source_path = os.path.join(root, f)
                    rel_source = source_path[len(source_dir) + 1:]

                    m.add_link(source_path, os.path.join(dest, rel_source))

        copier = FileCopier()
        m.populate_registry(copier)
        copier.copy(self.staging_dir)

        with open(self.index_path, 'rb') as fh:
            data = fh.read()

        def is_toplevel(key):
            """Whether the tree is nested under the toplevel index, or is
            nested under another tree's index.
            """
            for k in self.trees:
                if k == key:
                    continue
                if key.startswith(k):
                    return False
            return True

        toplevel_trees = {k: v for k, v in self.trees.items() if is_toplevel(k)}
        indexes = ['%s/index' % p for p in sorted(toplevel_trees.keys())]
        indexes = '\n   '.join(indexes)

        packages = [os.path.basename(p) for p in self.python_package_dirs]
        packages = ['python/%s' % p for p in packages]
        packages = '\n   '.join(sorted(packages))
        data = data.format(indexes=indexes, python_packages=packages)

        with open(os.path.join(self.staging_dir, 'index.rst'), 'wb') as fh:
            fh.write(data)


manager = _SphinxManager(build.topsrcdir, MAIN_DOC_PATH)
