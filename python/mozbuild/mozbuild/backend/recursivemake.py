# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import logging
import os
import re
import types

from collections import (
    defaultdict,
    namedtuple,
)
from StringIO import StringIO

from reftest import ReftestManifest

from mozpack.copier import FilePurger
from mozpack.manifests import (
    InstallManifest,
)
import mozpack.path as mozpath

from .common import CommonBackend
from ..frontend.data import (
    AndroidEclipseProjectData,
    BrandingFiles,
    ConfigFileSubstitution,
    ContextDerived,
    ContextWrapped,
    Defines,
    DistFiles,
    DirectoryTraversal,
    Exports,
    ExternalLibrary,
    FinalTargetFiles,
    GeneratedFile,
    GeneratedInclude,
    GeneratedSources,
    HostLibrary,
    HostProgram,
    HostSimpleProgram,
    HostSources,
    InstallationTarget,
    JARManifest,
    JavaJarData,
    JavaScriptModules,
    JsPreferenceFile,
    Library,
    LocalInclude,
    PerSourceFlag,
    Program,
    Resources,
    SharedLibrary,
    SimpleProgram,
    Sources,
    StaticLibrary,
    TestHarnessFiles,
    TestManifest,
    VariablePassthru,
    XPIDLFile,
)
from ..util import (
    ensureParentDir,
    FileAvoidWrite,
)
from ..makeutil import Makefile

MOZBUILD_VARIABLES = [
    'ANDROID_GENERATED_RESFILES',
    'ANDROID_RES_DIRS',
    'ASFLAGS',
    'CMSRCS',
    'CMMSRCS',
    'CPP_UNIT_TESTS',
    'DIRS',
    'DIST_INSTALL',
    'EXTRA_DSO_LDOPTS',
    'EXTRA_JS_MODULES',
    'EXTRA_PP_COMPONENTS',
    'EXTRA_PP_JS_MODULES',
    'FORCE_SHARED_LIB',
    'FORCE_STATIC_LIB',
    'FINAL_LIBRARY',
    'HOST_CSRCS',
    'HOST_CMMSRCS',
    'HOST_EXTRA_LIBS',
    'HOST_LIBRARY_NAME',
    'HOST_PROGRAM',
    'HOST_SIMPLE_PROGRAMS',
    'IS_COMPONENT',
    'JAR_MANIFEST',
    'JAVA_JAR_TARGETS',
    'LD_VERSION_SCRIPT',
    'LIBRARY_NAME',
    'LIBS',
    'MAKE_FRAMEWORK',
    'MODULE',
    'NO_DIST_INSTALL',
    'NO_EXPAND_LIBS',
    'NO_INTERFACES_MANIFEST',
    'NO_JS_MANIFEST',
    'OS_LIBS',
    'PARALLEL_DIRS',
    'PREF_JS_EXPORTS',
    'PROGRAM',
    'PYTHON_UNIT_TESTS',
    'RESOURCE_FILES',
    'SDK_HEADERS',
    'SDK_LIBRARY',
    'SHARED_LIBRARY_LIBS',
    'SHARED_LIBRARY_NAME',
    'SIMPLE_PROGRAMS',
    'SONAME',
    'STATIC_LIBRARY_NAME',
    'TEST_DIRS',
    'TOOL_DIRS',
    # XXX config/Makefile.in specifies this in a make invocation
    #'USE_EXTENSION_MANIFEST',
    'XPCSHELL_TESTS',
    'XPIDL_MODULE',
]

DEPRECATED_VARIABLES = [
    'ANDROID_RESFILES',
    'EXPORT_LIBRARY',
    'EXTRA_LIBS',
    'HOST_LIBS',
    'LIBXUL_LIBRARY',
    'MOCHITEST_A11Y_FILES',
    'MOCHITEST_BROWSER_FILES',
    'MOCHITEST_BROWSER_FILES_PARTS',
    'MOCHITEST_CHROME_FILES',
    'MOCHITEST_FILES',
    'MOCHITEST_FILES_PARTS',
    'MOCHITEST_METRO_FILES',
    'MOCHITEST_ROBOCOP_FILES',
    'MOZ_CHROME_FILE_FORMAT',
    'SHORT_LIBNAME',
    'TESTING_JS_MODULES',
    'TESTING_JS_MODULE_DIR',
]

class BackendMakeFile(object):
    """Represents a generated backend.mk file.

    This is both a wrapper around a file handle as well as a container that
    holds accumulated state.

    It's worth taking a moment to explain the make dependencies. The
    generated backend.mk as well as the Makefile.in (if it exists) are in the
    GLOBAL_DEPS list. This means that if one of them changes, all targets
    in that Makefile are invalidated. backend.mk also depends on all of its
    input files.

    It's worth considering the effect of file mtimes on build behavior.

    Since we perform an "all or none" traversal of moz.build files (the whole
    tree is scanned as opposed to individual files), if we were to blindly
    write backend.mk files, the net effect of updating a single mozbuild file
    in the tree is all backend.mk files have new mtimes. This would in turn
    invalidate all make targets across the whole tree! This would effectively
    undermine incremental builds as any mozbuild change would cause the entire
    tree to rebuild!

    The solution is to not update the mtimes of backend.mk files unless they
    actually change. We use FileAvoidWrite to accomplish this.
    """

    def __init__(self, srcdir, objdir, environment, topsrcdir, topobjdir):
        self.topsrcdir = topsrcdir
        self.srcdir = srcdir
        self.objdir = objdir
        self.relobjdir = mozpath.relpath(objdir, topobjdir)
        self.environment = environment
        self.name = mozpath.join(objdir, 'backend.mk')

        # XPIDLFiles attached to this file.
        self.idls = []
        self.xpt_name = None

        self.fh = FileAvoidWrite(self.name, capture_diff=True)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    def write_once(self, buf):
        if '\n' + buf not in self.fh.getvalue():
            self.write(buf)

    # For compatibility with makeutil.Makefile
    def add_statement(self, stmt):
        self.write('%s\n' % stmt)

    def close(self):
        if self.xpt_name:
            self.fh.write('XPT_NAME := %s\n' % self.xpt_name)

            # We just recompile all xpidls because it's easier and less error
            # prone.
            self.fh.write('NONRECURSIVE_TARGETS += export\n')
            self.fh.write('NONRECURSIVE_TARGETS_export += xpidl\n')
            self.fh.write('NONRECURSIVE_TARGETS_export_xpidl_DIRECTORY = '
                '$(DEPTH)/xpcom/xpidl\n')
            self.fh.write('NONRECURSIVE_TARGETS_export_xpidl_TARGETS += '
                'export\n')

        return self.fh.close()

    @property
    def diff(self):
        return self.fh.diff


class RecursiveMakeTraversal(object):
    """
    Helper class to keep track of how the "traditional" recursive make backend
    recurses subdirectories. This is useful until all adhoc rules are removed
    from Makefiles.

    Each directory may have one or more types of subdirectories:
        - (normal) dirs
        - tests
    """
    SubDirectoryCategories = ['dirs', 'tests']
    SubDirectoriesTuple = namedtuple('SubDirectories', SubDirectoryCategories)
    class SubDirectories(SubDirectoriesTuple):
        def __new__(self):
            return RecursiveMakeTraversal.SubDirectoriesTuple.__new__(self, [], [])

    def __init__(self):
        self._traversal = {}

    def add(self, dir, dirs=[], tests=[]):
        """
        Adds a directory to traversal, registering its subdirectories,
        sorted by categories. If the directory was already added to
        traversal, adds the new subdirectories to the already known lists.
        """
        subdirs = self._traversal.setdefault(dir, self.SubDirectories())
        for key, value in (('dirs', dirs), ('tests', tests)):
            assert(key in self.SubDirectoryCategories)
            getattr(subdirs, key).extend(value)

    @staticmethod
    def default_filter(current, subdirs):
        """
        Default filter for use with compute_dependencies and traverse.
        """
        return current, [], subdirs.dirs + subdirs.tests

    def call_filter(self, current, filter):
        """
        Helper function to call a filter from compute_dependencies and
        traverse.
        """
        return filter(current, self._traversal.get(current,
            self.SubDirectories()))

    def compute_dependencies(self, filter=None):
        """
        Compute make dependencies corresponding to the registered directory
        traversal.

        filter is a function with the following signature:
            def filter(current, subdirs)
        where current is the directory being traversed, and subdirs the
        SubDirectories instance corresponding to it.
        The filter function returns a tuple (filtered_current, filtered_parallel,
        filtered_dirs) where filtered_current is either current or None if
        the current directory is to be skipped, and filtered_parallel and
        filtered_dirs are lists of parallel directories and sequential
        directories, which can be rearranged from whatever is given in the
        SubDirectories members.

        The default filter corresponds to a default recursive traversal.
        """
        filter = filter or self.default_filter

        deps = {}

        def recurse(start_node, prev_nodes=None):
            current, parallel, sequential = self.call_filter(start_node, filter)
            if current is not None:
                if start_node != '':
                    deps[start_node] = prev_nodes
                prev_nodes = (start_node,)
            if not start_node in self._traversal:
                return prev_nodes
            parallel_nodes = []
            for node in parallel:
                nodes = recurse(node, prev_nodes)
                if nodes and nodes != ('',):
                    parallel_nodes.extend(nodes)
            if parallel_nodes:
                prev_nodes = tuple(parallel_nodes)
            for dir in sequential:
                prev_nodes = recurse(dir, prev_nodes)
            return prev_nodes

        return recurse(''), deps

    def traverse(self, start, filter=None):
        """
        Iterate over the filtered subdirectories, following the traditional
        make traversal order.
        """
        if filter is None:
            filter = self.default_filter

        current, parallel, sequential = self.call_filter(start, filter)
        if current is not None:
            yield start
        if not start in self._traversal:
            return
        for node in parallel:
            for n in self.traverse(node, filter):
                yield n
        for dir in sequential:
            for d in self.traverse(dir, filter):
                yield d

    def get_subdirs(self, dir):
        """
        Returns all direct subdirectories under the given directory.
        """
        return self._traversal.get(dir, self.SubDirectories())


class RecursiveMakeBackend(CommonBackend):
    """Backend that integrates with the existing recursive make build system.

    This backend facilitates the transition from Makefile.in to moz.build
    files.

    This backend performs Makefile.in -> Makefile conversion. It also writes
    out .mk files containing content derived from moz.build files. Both are
    consumed by the recursive make builder.

    This backend may eventually evolve to write out non-recursive make files.
    However, as long as there are Makefile.in files in the tree, we are tied to
    recursive make and thus will need this backend.
    """

    def _init(self):
        CommonBackend._init(self)

        self._backend_files = {}
        self._idl_dirs = set()

        def detailed(summary):
            s = '{:d} total backend files; ' \
                '{:d} created; {:d} updated; {:d} unchanged; ' \
                '{:d} deleted; {:d} -> {:d} Makefile'.format(
                summary.created_count + summary.updated_count +
                summary.unchanged_count,
                summary.created_count,
                summary.updated_count,
                summary.unchanged_count,
                summary.deleted_count,
                summary.makefile_in_count,
                summary.makefile_out_count)

            return s

        # This is a little kludgy and could be improved with a better API.
        self.summary.backend_detailed_summary = types.MethodType(detailed,
            self.summary)
        self.summary.makefile_in_count = 0
        self.summary.makefile_out_count = 0

        self._test_manifests = {}

        self.backend_input_files.add(mozpath.join(self.environment.topobjdir,
            'config', 'autoconf.mk'))

        self._install_manifests = {
            k: InstallManifest() for k in [
                'dist_bin',
                'dist_branding',
                'dist_idl',
                'dist_include',
                'dist_public',
                'dist_private',
                'dist_sdk',
                'dist_xpi-stage',
                'tests',
                'xpidl',
            ]}

        self._traversal = RecursiveMakeTraversal()
        self._compile_graph = defaultdict(set)

        self._may_skip = {
            'export': set(),
            'libs': set(),
        }
        self._no_skip = {
            'misc': set(),
            'tools': set(),
        }

    def _get_backend_file_for(self, obj):
        if obj.objdir not in self._backend_files:
            self._backend_files[obj.objdir] = \
                BackendMakeFile(obj.srcdir, obj.objdir, obj.config,
                    obj.topsrcdir, self.environment.topobjdir)
        return self._backend_files[obj.objdir]

    def consume_object(self, obj):
        """Write out build files necessary to build with recursive make."""

        if not isinstance(obj, ContextDerived):
            return

        backend_file = self._get_backend_file_for(obj)

        CommonBackend.consume_object(self, obj)

        # CommonBackend handles XPIDLFile and TestManifest, but we want to do
        # some extra things for them.
        if isinstance(obj, XPIDLFile):
            backend_file.idls.append(obj)
            backend_file.xpt_name = '%s.xpt' % obj.module
            self._idl_dirs.add(obj.relobjdir)

        elif isinstance(obj, TestManifest):
            self._process_test_manifest(obj, backend_file)

        # If CommonBackend acknowledged the object, we're done with it.
        if obj._ack:
            return

        if isinstance(obj, DirectoryTraversal):
            self._process_directory_traversal(obj, backend_file)
        elif isinstance(obj, ConfigFileSubstitution):
            # Other ConfigFileSubstitution should have been acked by
            # CommonBackend.
            assert os.path.basename(obj.output_path) == 'Makefile'
            self._create_makefile(obj)
        elif isinstance(obj, (Sources, GeneratedSources)):
            suffix_map = {
                '.s': 'ASFILES',
                '.c': 'CSRCS',
                '.m': 'CMSRCS',
                '.mm': 'CMMSRCS',
                '.cpp': 'CPPSRCS',
                '.rs': 'RSSRCS',
                '.S': 'SSRCS',
            }
            variables = [suffix_map[obj.canonical_suffix]]
            if isinstance(obj, GeneratedSources):
                variables.append('GARBAGE')
                base = backend_file.objdir
            else:
                base = backend_file.srcdir
            for f in sorted(obj.files):
                f = mozpath.relpath(f, base)
                for var in variables:
                    backend_file.write('%s += %s\n' % (var, f))
        elif isinstance(obj, HostSources):
            suffix_map = {
                '.c': 'HOST_CSRCS',
                '.mm': 'HOST_CMMSRCS',
                '.cpp': 'HOST_CPPSRCS',
            }
            var = suffix_map[obj.canonical_suffix]
            for f in sorted(obj.files):
                backend_file.write('%s += %s\n' % (
                    var, mozpath.relpath(f, backend_file.srcdir)))
        elif isinstance(obj, VariablePassthru):
            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if isinstance(v, list):
                    for item in v:
                        backend_file.write('%s += %s\n' % (k, item))
                elif isinstance(v, bool):
                    if v:
                        backend_file.write('%s := 1\n' % k)
                else:
                    backend_file.write('%s := %s\n' % (k, v))

        elif isinstance(obj, Defines):
            self._process_defines(obj, backend_file)

        elif isinstance(obj, Exports):
            self._process_exports(obj, obj.exports, backend_file)

        elif isinstance(obj, GeneratedFile):
            backend_file.write('GENERATED_FILES += %s\n' % obj.output)
            if obj.script:
                backend_file.write("""{output}: {script}{inputs}
\t$(call py_action,file_generate,{script} {method} {output}{inputs})

""".format(output=obj.output,
           inputs=' ' + ' '.join(obj.inputs) if obj.inputs else '',
           script=obj.script,
           method=obj.method))

        elif isinstance(obj, TestHarnessFiles):
            self._process_test_harness_files(obj, backend_file)

        elif isinstance(obj, Resources):
            self._process_resources(obj, obj.resources, backend_file)

        elif isinstance(obj, BrandingFiles):
            self._process_branding_files(obj, obj.files, backend_file)

        elif isinstance(obj, JsPreferenceFile):
            if obj.path.startswith('/'):
                backend_file.write('PREF_JS_EXPORTS += $(topsrcdir)%s\n' % obj.path)
            else:
                backend_file.write('PREF_JS_EXPORTS += $(srcdir)/%s\n' % obj.path)

        elif isinstance(obj, JARManifest):
            backend_file.write('JAR_MANIFEST := %s\n' % obj.path)

        elif isinstance(obj, Program):
            self._process_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostProgram):
            self._process_host_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, SimpleProgram):
            self._process_simple_program(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostSimpleProgram):
            self._process_host_simple_program(obj.program, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, LocalInclude):
            self._process_local_include(obj.path, backend_file)

        elif isinstance(obj, GeneratedInclude):
            self._process_generated_include(obj.path, backend_file)

        elif isinstance(obj, PerSourceFlag):
            self._process_per_source_flag(obj, backend_file)

        elif isinstance(obj, InstallationTarget):
            self._process_installation_target(obj, backend_file)

        elif isinstance(obj, JavaScriptModules):
            self._process_javascript_modules(obj, backend_file)

        elif isinstance(obj, ContextWrapped):
            # Process a rich build system object from the front-end
            # as-is.  Please follow precedent and handle CamelCaseData
            # in a function named _process_camel_case_data.  At some
            # point in the future, this unwrapping process may be
            # automated.
            if isinstance(obj.wrapped, JavaJarData):
                self._process_java_jar_data(obj.wrapped, backend_file)
            elif isinstance(obj.wrapped, AndroidEclipseProjectData):
                self._process_android_eclipse_project_data(obj.wrapped, backend_file)
            else:
                return

        elif isinstance(obj, SharedLibrary):
            self._process_shared_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, StaticLibrary):
            self._process_static_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, HostLibrary):
            self._process_host_library(obj, backend_file)
            self._process_linked_libraries(obj, backend_file)

        elif isinstance(obj, FinalTargetFiles):
            self._process_final_target_files(obj, obj.files, obj.target)

        elif isinstance(obj, DistFiles):
            # We'd like to install these via manifests as preprocessed files.
            # But they currently depend on non-standard flags being added via
            # some Makefiles, so for now we just pass them through to the
            # underlying Makefile.in.
            for f in obj.files:
                backend_file.write('DIST_FILES += %s\n' % f)

        else:
            return
        obj.ack()

    def _fill_root_mk(self):
        """
        Create two files, root.mk and root-deps.mk, the first containing
        convenience variables, and the other dependency definitions for a
        hopefully proper directory traversal.
        """
        for tier, skip in self._may_skip.items():
            self.log(logging.DEBUG, 'fill_root_mk', {
                'number': len(skip), 'tier': tier
                }, 'Ignoring {number} directories during {tier}')
        for tier, no_skip in self._no_skip.items():
            self.log(logging.DEBUG, 'fill_root_mk', {
                'number': len(no_skip), 'tier': tier
                }, 'Using {number} directories during {tier}')

        def should_skip(tier, dir):
            if tier in self._may_skip:
                return dir in self._may_skip[tier]
            if tier in self._no_skip:
                return dir not in self._no_skip[tier]
            return False

        # Traverse directories in parallel, and skip static dirs
        def parallel_filter(current, subdirs):
            all_subdirs = subdirs.dirs + subdirs.tests
            if should_skip(tier, current) or current.startswith('subtiers/'):
                current = None
            return current, all_subdirs, []

        # build everything in parallel, including static dirs
        # Because of bug 925236 and possible other unknown race conditions,
        # don't parallelize the libs tier.
        def libs_filter(current, subdirs):
            if should_skip('libs', current) or current.startswith('subtiers/'):
                current = None
            return current, [], subdirs.dirs + subdirs.tests

        # Because of bug 925236 and possible other unknown race conditions,
        # don't parallelize the tools tier. There aren't many directories for
        # this tier anyways.
        def tools_filter(current, subdirs):
            if should_skip('tools', current) or current.startswith('subtiers/'):
                current = None
            return current, [], subdirs.dirs + subdirs.tests

        filters = [
            ('export', parallel_filter),
            ('libs', libs_filter),
            ('misc', parallel_filter),
            ('tools', tools_filter),
        ]

        root_deps_mk = Makefile()

        # Fill the dependencies for traversal of each tier.
        for tier, filter in filters:
            main, all_deps = \
                self._traversal.compute_dependencies(filter)
            for dir, deps in all_deps.items():
                if deps is not None or (dir in self._idl_dirs \
                                        and tier == 'export'):
                    rule = root_deps_mk.create_rule(['%s/%s' % (dir, tier)])
                if deps:
                    rule.add_dependencies('%s/%s' % (d, tier) for d in deps if d)
                if dir in self._idl_dirs and tier == 'export':
                    rule.add_dependencies(['xpcom/xpidl/%s' % tier])
            rule = root_deps_mk.create_rule(['recurse_%s' % tier])
            if main:
                rule.add_dependencies('%s/%s' % (d, tier) for d in main)

        all_compile_deps = reduce(lambda x,y: x|y,
            self._compile_graph.values()) if self._compile_graph else set()
        compile_roots = set(self._compile_graph.keys()) - all_compile_deps

        rule = root_deps_mk.create_rule(['recurse_compile'])
        rule.add_dependencies(compile_roots)
        for target, deps in sorted(self._compile_graph.items()):
            if deps:
                rule = root_deps_mk.create_rule([target])
                rule.add_dependencies(deps)

        root_mk = Makefile()

        # Fill root.mk with the convenience variables.
        for tier, filter in filters:
            all_dirs = self._traversal.traverse('', filter)
            root_mk.add_statement('%s_dirs := %s' % (tier, ' '.join(all_dirs)))

        # Need a list of compile targets because we can't use pattern rules:
        # https://savannah.gnu.org/bugs/index.php?42833
        root_mk.add_statement('compile_targets := %s' % ' '.join(sorted(
            set(self._compile_graph.keys()) | all_compile_deps)))

        root_mk.add_statement('$(call include_deps,root-deps.mk)')

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'root.mk')) as root:
            root_mk.dump(root, removal_guard=False)

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'root-deps.mk')) as root_deps:
            root_deps_mk.dump(root_deps, removal_guard=False)

    def _add_unified_build_rules(self, makefile, unified_source_mapping,
                                 unified_files_makefile_variable='unified_files',
                                 include_curdir_build_rules=True):

        # In case it's a generator.
        unified_source_mapping = sorted(unified_source_mapping)

        explanation = "\n" \
            "# We build files in 'unified' mode by including several files\n" \
            "# together into a single source file.  This cuts down on\n" \
            "# compilation times and debug information size."
        makefile.add_statement(explanation)

        all_sources = ' '.join(source for source, _ in unified_source_mapping)
        makefile.add_statement('%s := %s' % (unified_files_makefile_variable,
                                             all_sources))

        if include_curdir_build_rules:
            makefile.add_statement('\n'
                '# Make sometimes gets confused between "foo" and "$(CURDIR)/foo".\n'
                '# Help it out by explicitly specifiying dependencies.')
            makefile.add_statement('all_absolute_unified_files := \\\n'
                                   '  $(addprefix $(CURDIR)/,$(%s))'
                                   % unified_files_makefile_variable)
            rule = makefile.create_rule(['$(all_absolute_unified_files)'])
            rule.add_dependencies(['$(CURDIR)/%: %'])

    def _check_blacklisted_variables(self, makefile_in, makefile_content):
        if b'EXTERNALLY_MANAGED_MAKE_FILE' in makefile_content:
            # Bypass the variable restrictions for externally managed makefiles.
            return

        for x in MOZBUILD_VARIABLES:
            if re.search(r'^[^#]*\b%s\s*[:?+]?=' % x, makefile_content, re.M):
                raise Exception('Variable %s is defined in %s. It should '
                    'only be defined in moz.build files.' % (x, makefile_in))

        for x in DEPRECATED_VARIABLES:
            if re.search(r'^[^#]*\b%s\s*[:?+]?=' % x, makefile_content, re.M):
                raise Exception('Variable %s is defined in %s. This variable '
                    'has been deprecated. It does nothing. It must be removed '
                    'in order to build.' % (x, makefile_in))

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        for objdir, backend_file in sorted(self._backend_files.items()):
            srcdir = backend_file.srcdir
            with self._write_file(fh=backend_file) as bf:
                makefile_in = mozpath.join(srcdir, 'Makefile.in')
                makefile = mozpath.join(objdir, 'Makefile')

                # If Makefile.in exists, use it as a template. Otherwise,
                # create a stub.
                stub = not os.path.exists(makefile_in)
                if not stub:
                    self.log(logging.DEBUG, 'substitute_makefile',
                        {'path': makefile}, 'Substituting makefile: {path}')
                    self.summary.makefile_in_count += 1

                    for tier, skiplist in self._may_skip.items():
                        if bf.relobjdir in skiplist:
                            skiplist.remove(bf.relobjdir)
                else:
                    self.log(logging.DEBUG, 'stub_makefile',
                        {'path': makefile}, 'Creating stub Makefile: {path}')

                obj = self.Substitution()
                obj.output_path = makefile
                obj.input_path = makefile_in
                obj.topsrcdir = backend_file.topsrcdir
                obj.topobjdir = bf.environment.topobjdir
                obj.config = bf.environment
                self._create_makefile(obj, stub=stub)
                with open(obj.output_path) as fh:
                    content = fh.read()
                    # Skip every directory but those with a Makefile
                    # containing a tools target, or XPI_PKGNAME or
                    # INSTALL_EXTENSION_ID.
                    for t in (b'XPI_PKGNAME', b'INSTALL_EXTENSION_ID',
                            b'tools'):
                        if t not in content:
                            continue
                        if t == b'tools' and not re.search('(?:^|\s)tools.*::', content, re.M):
                            continue
                        if objdir == self.environment.topobjdir:
                            continue
                        self._no_skip['tools'].add(mozpath.relpath(objdir,
                            self.environment.topobjdir))

                    # Detect any Makefile.ins that contain variables on the
                    # moz.build-only list
                    self._check_blacklisted_variables(makefile_in, content)

        self._fill_root_mk()

        # Write out a dependency file used to determine whether a config.status
        # re-run is needed.
        inputs = sorted(p.replace(os.sep, '/') for p in self.backend_input_files)

        # We need to use $(DEPTH) so the target here matches what's in
        # rules.mk. If they are different, the dependencies don't get pulled in
        # properly.
        with self._write_file('%s.pp' % self._backend_output_list_file) as backend_deps:
            backend_deps.write('$(DEPTH)/backend.%s: %s\n' %
                (self.__class__.__name__, ' '.join(inputs)))
            for path in inputs:
                backend_deps.write('%s:\n' % path)

        with open(self._backend_output_list_file, 'a'):
            pass
        os.utime(self._backend_output_list_file, None)

        # Make the master test manifest files.
        for flavor, t in self._test_manifests.items():
            install_prefix, manifests = t
            manifest_stem = mozpath.join(install_prefix, '%s.ini' % flavor)
            self._write_master_test_manifest(mozpath.join(
                self.environment.topobjdir, '_tests', manifest_stem),
                manifests)

            # Catch duplicate inserts.
            try:
                self._install_manifests['tests'].add_optional_exists(manifest_stem)
            except ValueError:
                pass

        self._write_manifests('install', self._install_manifests)

        ensureParentDir(mozpath.join(self.environment.topobjdir, 'dist', 'foo'))

    def _process_unified_sources(self, obj):
        backend_file = self._get_backend_file_for(obj)

        suffix_map = {
            '.c': 'UNIFIED_CSRCS',
            '.mm': 'UNIFIED_CMMSRCS',
            '.cpp': 'UNIFIED_CPPSRCS',
        }

        var = suffix_map[obj.canonical_suffix]
        non_unified_var = var[len('UNIFIED_'):]

        if obj.have_unified_mapping:
            self._add_unified_build_rules(backend_file,
                                          obj.unified_source_mapping,
                                          unified_files_makefile_variable=var,
                                          include_curdir_build_rules=False)
            backend_file.write('%s += $(%s)\n' % (non_unified_var, var))
        else:
            # Sorted so output is consistent and we don't bump mtimes.
            source_files = list(sorted(obj.files))

            backend_file.write('%s += %s\n' % (
                    non_unified_var, ' '.join(source_files)))

    def _process_directory_traversal(self, obj, backend_file):
        """Process a data.DirectoryTraversal instance."""
        fh = backend_file.fh

        def relativize(base, dirs):
            return (mozpath.relpath(d.translated, base) for d in dirs)

        if obj.dirs:
            fh.write('DIRS := %s\n' % ' '.join(
                relativize(backend_file.objdir, obj.dirs)))
            self._traversal.add(backend_file.relobjdir,
                dirs=relativize(self.environment.topobjdir, obj.dirs))

        if obj.test_dirs:
            fh.write('TEST_DIRS := %s\n' % ' '.join(
                relativize(backend_file.objdir, obj.test_dirs)))
            if self.environment.substs.get('ENABLE_TESTS', False):
                self._traversal.add(backend_file.relobjdir,
                    dirs=relativize(self.environment.topobjdir, obj.test_dirs))

        # The directory needs to be registered whether subdirectories have been
        # registered or not.
        self._traversal.add(backend_file.relobjdir)

        affected_tiers = set(obj.affected_tiers)

        for tier in set(self._may_skip.keys()) - affected_tiers:
            self._may_skip[tier].add(backend_file.relobjdir)

        for tier in set(self._no_skip.keys()) & affected_tiers:
            self._no_skip[tier].add(backend_file.relobjdir)

    def _walk_hierarchy(self, obj, element, namespace=''):
        """Walks the ``HierarchicalStringList`` ``element`` in the context of
        the mozbuild object ``obj`` as though by ``element.walk()``, but yield
        three-tuple containing the following:

        - ``source`` - The path to the source file named by the current string
        - ``dest``   - The relative path, including the namespace, of the
                       destination file.
        - ``flags``  - A dictionary of flags associated with the current string,
                       or None if there is no such dictionary.
        """
        for path, strings in element.walk():
            for s in strings:
                source = mozpath.normpath(mozpath.join(obj.srcdir, s))
                dest = mozpath.join(namespace, path, mozpath.basename(s))
                yield source, dest, strings.flags_for(s)

    def _process_defines(self, obj, backend_file):
        """Output the DEFINES rules to the given backend file."""
        defines = list(obj.get_defines())
        if defines:
            backend_file.write('DEFINES +=')
            for define in defines:
                backend_file.write(' %s' % define)
            backend_file.write('\n')

    def _process_exports(self, obj, exports, backend_file):
        # This may not be needed, but is present for backwards compatibility
        # with the old make rules, just in case.
        if not obj.dist_install:
            return

        for source, dest, _ in self._walk_hierarchy(obj, exports):
            self._install_manifests['dist_include'].add_symlink(source, dest)

            if not os.path.exists(source):
                raise Exception('File listed in EXPORTS does not exist: %s' % source)

    def _process_test_harness_files(self, obj, backend_file):
        for path, files in obj.srcdir_files.iteritems():
            for source in files:
                dest = '%s/%s' % (path, mozpath.basename(source))
                self._install_manifests['tests'].add_symlink(source, dest)

        for path, patterns in obj.srcdir_pattern_files.iteritems():
            for p in patterns:
                self._install_manifests['tests'].add_pattern_symlink(p[0], p[1], path)

        for path, files in obj.objdir_files.iteritems():
            prefix = 'TEST_HARNESS_%s' % path.replace('/', '_')
            backend_file.write("""
%(prefix)s_FILES := %(files)s
%(prefix)s_DEST := %(dest)s
INSTALL_TARGETS += %(prefix)s
""" % { 'prefix': prefix,
        'dest': '$(DEPTH)/_tests/%s' % path,
        'files': ' '.join(mozpath.relpath(f, backend_file.objdir)
                          for f in files) })

    def _process_resources(self, obj, resources, backend_file):
        dep_path = mozpath.join(self.environment.topobjdir, '_build_manifests', '.deps', 'install')

        # Resources need to go in the 'res' subdirectory of $(DIST)/bin, so we
        # specify a root namespace of 'res'.
        for source, dest, flags in self._walk_hierarchy(obj, resources,
                                                        namespace='res'):
            if flags and flags.preprocess:
                if dest.endswith('.in'):
                    dest = dest[:-3]
                dep_file = mozpath.join(dep_path, mozpath.basename(source) + '.pp')
                self._install_manifests['dist_bin'].add_preprocess(source, dest, dep_file, marker='%', defines=obj.defines)
            else:
                self._install_manifests['dist_bin'].add_symlink(source, dest)

            if not os.path.exists(source):
                raise Exception('File listed in RESOURCE_FILES does not exist: %s' % source)

    def _process_branding_files(self, obj, files, backend_file):
        for source, dest, flags in self._walk_hierarchy(obj, files):
            if flags and flags.source:
                source = mozpath.normpath(mozpath.join(obj.srcdir, flags.source))
            if not os.path.exists(source):
                raise Exception('File listed in BRANDING_FILES does not exist: %s' % source)

            self._install_manifests['dist_branding'].add_symlink(source, dest)

        # Also emit the necessary rules to create $(DIST)/branding during partial
        # tree builds. The locale makefiles rely on this working.
        backend_file.write('NONRECURSIVE_TARGETS += export\n')
        backend_file.write('NONRECURSIVE_TARGETS_export += branding\n')
        backend_file.write('NONRECURSIVE_TARGETS_export_branding_DIRECTORY = $(DEPTH)\n')
        backend_file.write('NONRECURSIVE_TARGETS_export_branding_TARGETS += install-dist/branding\n')

    def _process_installation_target(self, obj, backend_file):
        # A few makefiles need to be able to override the following rules via
        # make XPI_NAME=blah commands, so we default to the lazy evaluation as
        # much as possible here to avoid breaking things.
        if obj.xpiname:
            backend_file.write('XPI_NAME = %s\n' % (obj.xpiname))
        if obj.subdir:
            backend_file.write('DIST_SUBDIR = %s\n' % (obj.subdir))
        if obj.target and not obj.is_custom():
            backend_file.write('FINAL_TARGET = $(DEPTH)/%s\n' % (obj.target))
        else:
            backend_file.write('FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)$(DIST_SUBDIR:%=/%)\n')

        if not obj.enabled:
            backend_file.write('NO_DIST_INSTALL := 1\n')

    def _process_javascript_modules(self, obj, backend_file):
        if obj.flavor not in ('extra', 'extra_pp', 'testing'):
            raise Exception('Unsupported JavaScriptModules instance: %s' % obj.flavor)

        if obj.flavor == 'extra':
            for path, strings in obj.modules.walk():
                if not strings:
                    continue

                prefix = 'extra_js_%s' % path.replace('/', '_')
                backend_file.write('%s_FILES := %s\n'
                                   % (prefix, ' '.join(strings)))
                backend_file.write('%s_DEST = %s\n' %
                                   (prefix, mozpath.join('$(FINAL_TARGET)', 'modules', path)))
                backend_file.write('%s_TARGET := misc\n' % prefix)
                backend_file.write('INSTALL_TARGETS += %s\n\n' % prefix)
            return

        if obj.flavor == 'extra_pp':
            for path, strings in obj.modules.walk():
                if not strings:
                    continue

                prefix = 'extra_pp_js_%s' % path.replace('/', '_')
                backend_file.write('%s := %s\n'
                                   % (prefix, ' '.join(strings)))
                backend_file.write('%s_PATH = %s\n' %
                                   (prefix, mozpath.join('$(FINAL_TARGET)', 'modules', path)))
                backend_file.write('%s_TARGET := misc\n' % prefix)
                backend_file.write('PP_TARGETS += %s\n\n' % prefix)
            return

        if not self.environment.substs.get('ENABLE_TESTS', False):
            return

        manifest = self._install_manifests['tests']

        for source, dest, _ in self._walk_hierarchy(obj, obj.modules):
            manifest.add_symlink(source, mozpath.join('modules', dest))

    def _handle_idl_manager(self, manager):
        build_files = self._install_manifests['xpidl']

        for p in ('Makefile', 'backend.mk', '.deps/.mkdir.done'):
            build_files.add_optional_exists(p)

        for idl in manager.idls.values():
            self._install_manifests['dist_idl'].add_symlink(idl['source'],
                idl['basename'])
            self._install_manifests['dist_include'].add_optional_exists('%s.h'
                % idl['root'])

        for module in manager.modules:
            build_files.add_optional_exists(mozpath.join('.deps',
                '%s.pp' % module))

        modules = manager.modules
        xpt_modules = sorted(modules.keys())
        xpt_files = set()

        mk = Makefile()

        for module in xpt_modules:
            install_target, sources = modules[module]
            deps = sorted(sources)

            # It may seem strange to have the .idl files listed as
            # prerequisites both here and in the auto-generated .pp files.
            # It is necessary to list them here to handle the case where a
            # new .idl is added to an xpt. If we add a new .idl and nothing
            # else has changed, the new .idl won't be referenced anywhere
            # except in the command invocation. Therefore, the .xpt won't
            # be rebuilt because the dependencies say it is up to date. By
            # listing the .idls here, we ensure the make file has a
            # reference to the new .idl. Since the new .idl presumably has
            # an mtime newer than the .xpt, it will trigger xpt generation.
            xpt_path = '$(DEPTH)/%s/components/%s.xpt' % (install_target, module)
            xpt_files.add(xpt_path)
            mk.add_statement('%s_deps = %s' % (module, ' '.join(deps)))

            if install_target.startswith('dist/'):
                path = mozpath.relpath(xpt_path, '$(DEPTH)/dist')
                prefix, subpath = path.split('/', 1)
                key = 'dist_%s' % prefix

                self._install_manifests[key].add_optional_exists(subpath)

        rules = StringIO()
        mk.dump(rules, removal_guard=False)

        # Write out manifests defining interfaces
        dist_dir = mozpath.join(self.environment.topobjdir, 'dist')
        for manifest, entries in manager.interface_manifests.items():
            path = mozpath.join(self.environment.topobjdir, manifest)
            with self._write_file(path) as fh:
                for xpt in sorted(entries):
                    fh.write('interfaces %s\n' % xpt)

            if install_target.startswith('dist/'):
                path = mozpath.relpath(path, dist_dir)
                prefix, subpath = path.split('/', 1)
                key = 'dist_%s' % prefix
                self._install_manifests[key].add_optional_exists(subpath)

        chrome_manifests = [mozpath.join('$(DEPTH)', m) for m in sorted(manager.chrome_manifests)]

        # Create dependency for output header so we force regeneration if the
        # header was deleted. This ideally should not be necessary. However,
        # some processes (such as PGO at the time this was implemented) wipe
        # out dist/include without regard to our install manifests.

        obj = self.Substitution()
        obj.output_path = mozpath.join(self.environment.topobjdir, 'config',
            'makefiles', 'xpidl', 'Makefile')
        obj.input_path = mozpath.join(self.environment.topsrcdir, 'config',
            'makefiles', 'xpidl', 'Makefile.in')
        obj.topsrcdir = self.environment.topsrcdir
        obj.topobjdir = self.environment.topobjdir
        obj.config = self.environment
        self._create_makefile(obj, extra=dict(
            chrome_manifests = ' '.join(chrome_manifests),
            xpidl_rules=rules.getvalue(),
            xpidl_modules=' '.join(xpt_modules),
            xpt_files=' '.join(sorted(xpt_files)),
        ))

    def _process_program(self, program, backend_file):
        backend_file.write('PROGRAM = %s\n' % program)

    def _process_host_program(self, program, backend_file):
        backend_file.write('HOST_PROGRAM = %s\n' % program)

    def _process_simple_program(self, obj, backend_file):
        if obj.is_unit_test:
            backend_file.write('CPP_UNIT_TESTS += %s\n' % obj.program)
        else:
            backend_file.write('SIMPLE_PROGRAMS += %s\n' % obj.program)

    def _process_host_simple_program(self, program, backend_file):
        backend_file.write('HOST_SIMPLE_PROGRAMS += %s\n' % program)

    def _process_test_manifest(self, obj, backend_file):
        # Much of the logic in this function could be moved to CommonBackend.
        self.backend_input_files.add(mozpath.join(obj.topsrcdir,
            obj.manifest_relpath))

        # Don't allow files to be defined multiple times unless it is allowed.
        # We currently allow duplicates for non-test files or test files if
        # the manifest is listed as a duplicate.
        for source, (dest, is_test) in obj.installs.items():
            try:
                self._install_manifests['tests'].add_symlink(source, dest)
            except ValueError:
                if not obj.dupe_manifest and is_test:
                    raise

        for base, pattern, dest in obj.pattern_installs:
            try:
                self._install_manifests['tests'].add_pattern_symlink(base,
                    pattern, dest)
            except ValueError:
                if not obj.dupe_manifest:
                    raise

        for dest in obj.external_installs:
            try:
                self._install_manifests['tests'].add_optional_exists(dest)
            except ValueError:
                if not obj.dupe_manifest:
                    raise

        m = self._test_manifests.setdefault(obj.flavor,
            (obj.install_prefix, set()))
        m[1].add(obj.manifest_obj_relpath)

        if isinstance(obj.manifest, ReftestManifest):
            # Mark included files as part of the build backend so changes
            # result in re-config.
            self.backend_input_files |= obj.manifest.manifests

    def _process_local_include(self, local_include, backend_file):
        if local_include.startswith('/'):
            path = '$(topsrcdir)'
        else:
            path = '$(srcdir)/'
        backend_file.write('LOCAL_INCLUDES += -I%s%s\n' % (path, local_include))

    def _process_generated_include(self, generated_include, backend_file):
        if generated_include.startswith('/'):
            path = self.environment.topobjdir.replace('\\', '/')
        else:
            path = ''
        backend_file.write('LOCAL_INCLUDES += -I%s%s\n' % (path, generated_include))

    def _process_per_source_flag(self, per_source_flag, backend_file):
        for flag in per_source_flag.flags:
            backend_file.write('%s_FLAGS += %s\n' % (mozpath.basename(per_source_flag.file_name), flag))

    def _process_java_jar_data(self, jar, backend_file):
        target = jar.name
        backend_file.write('JAVA_JAR_TARGETS += %s\n' % target)
        backend_file.write('%s_DEST := %s.jar\n' % (target, jar.name))
        if jar.sources:
            backend_file.write('%s_JAVAFILES := %s\n' %
                (target, ' '.join(jar.sources)))
        if jar.generated_sources:
            backend_file.write('%s_PP_JAVAFILES := %s\n' %
                (target, ' '.join(mozpath.join('generated', f) for f in jar.generated_sources)))
        if jar.extra_jars:
            backend_file.write('%s_EXTRA_JARS := %s\n' %
                (target, ' '.join(jar.extra_jars)))
        if jar.javac_flags:
            backend_file.write('%s_JAVAC_FLAGS := %s\n' %
                (target, ' '.join(jar.javac_flags)))

    def _process_android_eclipse_project_data(self, project, backend_file):
        # We add a single target to the backend.mk corresponding to
        # the moz.build defining the Android Eclipse project. This
        # target depends on some targets to be fresh, and installs a
        # manifest generated by the Android Eclipse build backend. The
        # manifests for all projects live in $TOPOBJDIR/android_eclipse
        # and are installed into subdirectories thereof.

        project_directory = mozpath.join(self.environment.topobjdir, 'android_eclipse', project.name)
        manifest_path = mozpath.join(self.environment.topobjdir, 'android_eclipse', '%s.manifest' % project.name)

        fragment = Makefile()
        rule = fragment.create_rule(targets=['ANDROID_ECLIPSE_PROJECT_%s' % project.name])
        rule.add_dependencies(project.recursive_make_targets)
        args = ['--no-remove',
            '--no-remove-all-directory-symlinks',
            '--no-remove-empty-directories',
            project_directory,
            manifest_path]
        rule.add_commands(['$(call py_action,process_install_manifest,%s)' % ' '.join(args)])
        fragment.dump(backend_file.fh, removal_guard=False)

    def _process_shared_library(self, libdef, backend_file):
        backend_file.write_once('LIBRARY_NAME := %s\n' % libdef.basename)
        backend_file.write('FORCE_SHARED_LIB := 1\n')
        backend_file.write('IMPORT_LIBRARY := %s\n' % libdef.import_name)
        backend_file.write('SHARED_LIBRARY := %s\n' % libdef.lib_name)
        if libdef.variant == libdef.COMPONENT:
            backend_file.write('IS_COMPONENT := 1\n')
        if libdef.soname:
            backend_file.write('DSO_SONAME := %s\n' % libdef.soname)
        if libdef.is_sdk:
            backend_file.write('SDK_LIBRARY := %s\n' % libdef.import_name)

    def _process_static_library(self, libdef, backend_file):
        backend_file.write_once('LIBRARY_NAME := %s\n' % libdef.basename)
        backend_file.write('FORCE_STATIC_LIB := 1\n')
        backend_file.write('REAL_LIBRARY := %s\n' % libdef.lib_name)
        if libdef.is_sdk:
            backend_file.write('SDK_LIBRARY := %s\n' % libdef.import_name)
        if libdef.no_expand_lib:
            backend_file.write('NO_EXPAND_LIBS := 1\n')

    def _process_host_library(self, libdef, backend_file):
        backend_file.write('HOST_LIBRARY_NAME = %s\n' % libdef.basename)

    def _build_target_for_obj(self, obj):
        return '%s/%s' % (mozpath.relpath(obj.objdir,
            self.environment.topobjdir), obj.KIND)

    def _process_linked_libraries(self, obj, backend_file):
        def write_shared_and_system_libs(lib):
            for l in lib.linked_libraries:
                if isinstance(l, StaticLibrary):
                    write_shared_and_system_libs(l)
                else:
                    backend_file.write_once('SHARED_LIBS += %s/%s\n'
                        % (pretty_relpath(l), l.import_name))
            for l in lib.linked_system_libs:
                backend_file.write_once('OS_LIBS += %s\n' % l)

        def pretty_relpath(lib):
            return '$(DEPTH)/%s' % mozpath.relpath(lib.objdir, topobjdir)

        topobjdir = mozpath.normsep(obj.topobjdir)
        # This will create the node even if there aren't any linked libraries.
        build_target = self._build_target_for_obj(obj)
        self._compile_graph[build_target]

        for lib in obj.linked_libraries:
            if not isinstance(lib, ExternalLibrary):
                self._compile_graph[build_target].add(
                    self._build_target_for_obj(lib))
            relpath = pretty_relpath(lib)
            if isinstance(obj, Library):
                if isinstance(lib, StaticLibrary):
                    backend_file.write_once('STATIC_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
                    if isinstance(obj, SharedLibrary):
                        write_shared_and_system_libs(lib)
                elif isinstance(obj, SharedLibrary):
                    assert lib.variant != lib.COMPONENT
                    backend_file.write_once('SHARED_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
            elif isinstance(obj, (Program, SimpleProgram)):
                if isinstance(lib, StaticLibrary):
                    backend_file.write_once('STATIC_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
                    write_shared_and_system_libs(lib)
                else:
                    assert lib.variant != lib.COMPONENT
                    backend_file.write_once('SHARED_LIBS += %s/%s\n'
                                        % (relpath, lib.import_name))
            elif isinstance(obj, (HostLibrary, HostProgram, HostSimpleProgram)):
                assert isinstance(lib, HostLibrary)
                backend_file.write_once('HOST_LIBS += %s/%s\n'
                                   % (relpath, lib.import_name))

        for lib in obj.linked_system_libs:
            if obj.KIND == 'target':
                backend_file.write_once('OS_LIBS += %s\n' % lib)
            else:
                backend_file.write_once('HOST_EXTRA_LIBS += %s\n' % lib)

        # Process library-based defines
        self._process_defines(obj.defines, backend_file)

    def _process_final_target_files(self, obj, files, target):
        if target.startswith('dist/bin'):
            install_manifest = self._install_manifests['dist_bin']
            reltarget = mozpath.relpath(target, 'dist/bin')
        elif target.startswith('dist/xpi-stage'):
            install_manifest = self._install_manifests['dist_xpi-stage']
            reltarget = mozpath.relpath(target, 'dist/xpi-stage')
        else:
            raise Exception("Cannot install to " + target)

        for path, strings in files.walk():
            for f in strings:
                source = mozpath.normpath(os.path.join(obj.srcdir, f))
                dest = mozpath.join(reltarget, path, mozpath.basename(f))
                install_manifest.add_symlink(source, dest)

    def _write_manifests(self, dest, manifests):
        man_dir = mozpath.join(self.environment.topobjdir, '_build_manifests',
            dest)

        # We have a purger for the manifests themselves to ensure legacy
        # manifests are deleted.
        purger = FilePurger()

        for k, manifest in manifests.items():
            purger.add(k)

            with self._write_file(mozpath.join(man_dir, k)) as fh:
                manifest.write(fileobj=fh)

        purger.purge(man_dir)

    def _write_master_test_manifest(self, path, manifests):
        with self._write_file(path) as master:
            master.write(
                '; THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n\n')

            for manifest in sorted(manifests):
                master.write('[include:%s]\n' % manifest)

    class Substitution(object):
        """BaseConfigSubstitution-like class for use with _create_makefile."""
        __slots__ = (
            'input_path',
            'output_path',
            'topsrcdir',
            'topobjdir',
            'config',
        )

    def _create_makefile(self, obj, stub=False, extra=None):
        '''Creates the given makefile. Makefiles are treated the same as
        config files, but some additional header and footer is added to the
        output.

        When the stub argument is True, no source file is used, and a stub
        makefile with the default header and footer only is created.
        '''
        with self._get_preprocessor(obj) as pp:
            if extra:
                pp.context.update(extra)
            if not pp.context.get('autoconfmk', ''):
                pp.context['autoconfmk'] = 'autoconf.mk'
            pp.handleLine(b'# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n');
            pp.handleLine(b'DEPTH := @DEPTH@\n')
            pp.handleLine(b'topsrcdir := @top_srcdir@\n')
            pp.handleLine(b'srcdir := @srcdir@\n')
            pp.handleLine(b'VPATH := @srcdir@\n')
            pp.handleLine(b'relativesrcdir := @relativesrcdir@\n')
            pp.handleLine(b'include $(DEPTH)/config/@autoconfmk@\n')
            if not stub:
                pp.do_include(obj.input_path)
            # Empty line to avoid failures when last line in Makefile.in ends
            # with a backslash.
            pp.handleLine(b'\n')
            pp.handleLine(b'include $(topsrcdir)/config/recurse.mk\n')
        if not stub:
            # Adding the Makefile.in here has the desired side-effect
            # that if the Makefile.in disappears, this will force
            # moz.build traversal. This means that when we remove empty
            # Makefile.in files, the old file will get replaced with
            # the autogenerated one automatically.
            self.backend_input_files.add(obj.input_path)

        self.summary.makefile_out_count += 1

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources,
                             unified_ipdl_cppsrcs_mapping):
        # Write out a master list of all IPDL source files.
        mk = Makefile()

        mk.add_statement('ALL_IPDLSRCS := %s' % ' '.join(sorted_ipdl_sources))

        self._add_unified_build_rules(mk, unified_ipdl_cppsrcs_mapping,
                                      unified_files_makefile_variable='CPPSRCS')

        mk.add_statement('IPDLDIRS := %s' % ' '.join(sorted(set(mozpath.dirname(p)
            for p in self._ipdl_sources))))

        with self._write_file(mozpath.join(ipdl_dir, 'ipdlsrcs.mk')) as ipdls:
            mk.dump(ipdls, removal_guard=False)

    def _handle_webidl_build(self, bindings_dir, unified_source_mapping,
                             webidls, expected_build_output_files,
                             global_define_files):
        include_dir = mozpath.join(self.environment.topobjdir, 'dist',
            'include')
        for f in expected_build_output_files:
            if f.startswith(include_dir):
                self._install_manifests['dist_include'].add_optional_exists(
                    mozpath.relpath(f, include_dir))

        # We pass WebIDL info to make via a completely generated make file.
        mk = Makefile()
        mk.add_statement('nonstatic_webidl_files := %s' % ' '.join(
            sorted(webidls.all_non_static_basenames())))
        mk.add_statement('globalgen_sources := %s' % ' '.join(
            sorted(global_define_files)))
        mk.add_statement('test_sources := %s' % ' '.join(
            sorted('%sBinding.cpp' % s for s in webidls.all_test_stems())))

        # Add rules to preprocess bindings.
        # This should ideally be using PP_TARGETS. However, since the input
        # filenames match the output filenames, the existing PP_TARGETS rules
        # result in circular dependencies and other make weirdness. One
        # solution is to rename the input or output files repsectively. See
        # bug 928195 comment 129.
        for source in sorted(webidls.all_preprocessed_sources()):
            basename = os.path.basename(source)
            rule = mk.create_rule([basename])
            rule.add_dependencies([source, '$(GLOBAL_DEPS)'])
            rule.add_commands([
                # Remove the file before writing so bindings that go from
                # static to preprocessed don't end up writing to a symlink,
                # which would modify content in the source directory.
                '$(RM) $@',
                '$(call py_action,preprocessor,$(DEFINES) $(ACDEFINES) '
                    '$(XULPPFLAGS) $< -o $@)'
            ])

        self._add_unified_build_rules(mk,
            unified_source_mapping,
            unified_files_makefile_variable='unified_binding_cpp_files')

        webidls_mk = mozpath.join(bindings_dir, 'webidlsrcs.mk')
        with self._write_file(webidls_mk) as fh:
            mk.dump(fh, removal_guard=False)
