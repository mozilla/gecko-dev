# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os

import mozpack.path as mozpath

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mozbuild.util import (
    FileAvoidWrite,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

SUCCESS = '''
You should be ready to build with Gradle and import into IntelliJ!  Test with

    ./mach gradle build

and in IntelliJ select File > Import project... and choose

    {topobjdir}/mobile/android/gradle
'''


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('gradle', category='devenv',
        description='Run gradle.',
        conditions=[conditions.is_android])
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def gradle(self, args):
        # Avoid logging the command
        self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        code = self.gradle_install(quiet=True)
        if code:
            return code

        return self.run_process(['./gradlew'] + args,
            pass_thru=True, # Allow user to run gradle interactively.
            ensure_exit_code=False, # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topobjdir, 'mobile', 'android', 'gradle'))

    @Command('gradle-install', category='devenv',
        description='Install gradle environment.',
        conditions=[conditions.is_android])
    def gradle_install(self, quiet=False):
        import mozpack.manifests
        m = mozpack.manifests.InstallManifest()

        def srcdir(dst, src):
            m.add_symlink(os.path.join(self.topsrcdir, src), dst)

        def objdir(dst, src):
            m.add_symlink(os.path.join(self.topobjdir, src), dst)

        srcdir('build.gradle', 'mobile/android/gradle/build.gradle')
        srcdir('settings.gradle', 'mobile/android/gradle/settings.gradle')

        m.add_pattern_copy(os.path.join(self.topsrcdir, 'mobile/android/gradle/gradle/wrapper'), '**', 'gradle/wrapper')
        m.add_copy(os.path.join(self.topsrcdir, 'mobile/android/gradle/gradlew'), 'gradlew')

        defines = {
            'topsrcdir': self.topsrcdir,
            'topobjdir': self.topobjdir,
            'ANDROID_SDK_ROOT': self.substs['ANDROID_SDK_ROOT'],
        }
        m.add_preprocess(os.path.join(self.topsrcdir, 'mobile/android/gradle/gradle.properties.in'),
            'gradle.properties',
            defines=defines,
            deps=os.path.join(self.topobjdir, 'mobile/android/gradle/.deps/gradle.properties.pp'))
        m.add_preprocess(os.path.join(self.topsrcdir, 'mobile/android/gradle/local.properties.in'),
            'local.properties',
            defines=defines,
            deps=os.path.join(self.topobjdir, 'mobile/android/gradle/.deps/local.properties.pp'))

        srcdir('branding/build.gradle', 'mobile/android/gradle/branding/build.gradle')
        srcdir('branding/src/main/AndroidManifest.xml', 'mobile/android/gradle/branding/AndroidManifest.xml')
        srcdir('branding/src/main/res', os.path.join(self.substs['MOZ_BRANDING_DIRECTORY'], 'res'))

        srcdir('preprocessed_code/build.gradle', 'mobile/android/gradle/preprocessed_code/build.gradle')
        srcdir('preprocessed_code/src/main/AndroidManifest.xml', 'mobile/android/gradle/preprocessed_code/AndroidManifest.xml')
        objdir('preprocessed_code/src/main/java', 'mobile/android/base/generated/preprocessed')

        srcdir('preprocessed_resources/build.gradle', 'mobile/android/gradle/preprocessed_resources/build.gradle')
        srcdir('preprocessed_resources/src/main/AndroidManifest.xml', 'mobile/android/gradle/preprocessed_resources/AndroidManifest.xml')
        objdir('preprocessed_resources/src/main/res', 'mobile/android/base/res')

        srcdir('thirdparty/build.gradle', 'mobile/android/gradle/thirdparty/build.gradle')
        srcdir('thirdparty/src/main/AndroidManifest.xml', 'mobile/android/gradle/thirdparty/AndroidManifest.xml')
        srcdir('thirdparty/src/main/java', 'mobile/android/thirdparty')

        srcdir('thirdparty_adjust_sdk/build.gradle', 'mobile/android/gradle/thirdparty_adjust_sdk/build.gradle')
        srcdir('thirdparty_adjust_sdk/src/main/AndroidManifest.xml', 'mobile/android/gradle/thirdparty_adjust_sdk/AndroidManifest.xml')

        srcdir('omnijar/build.gradle', 'mobile/android/gradle/omnijar/build.gradle')
        srcdir('omnijar/src/main/java/locales', 'mobile/android/locales')
        srcdir('omnijar/src/main/java/chrome', 'mobile/android/chrome')
        srcdir('omnijar/src/main/java/components', 'mobile/android/components')
        srcdir('omnijar/src/main/java/modules', 'mobile/android/modules')
        srcdir('omnijar/src/main/java/themes', 'mobile/android/themes')

        srcdir('app/build.gradle', 'mobile/android/gradle/app/build.gradle')
        objdir('app/src/main/AndroidManifest.xml', 'mobile/android/base/AndroidManifest.xml')
        objdir('app/src/androidTest/AndroidManifest.xml', 'build/mobile/robocop/AndroidManifest.xml')
        srcdir('app/src/androidTest/res', 'build/mobile/robocop/res')
        srcdir('app/src/androidTest/assets', 'mobile/android/tests/browser/robocop/assets')
        objdir('app/src/debug/assets', 'dist/fennec/assets')
        objdir('app/src/debug/jniLibs', 'dist/fennec/lib')
        # Test code.
        srcdir('app/src/robocop_harness/org/mozilla/gecko', 'build/mobile/robocop')
        srcdir('app/src/robocop/org/mozilla/gecko/tests', 'mobile/android/tests/browser/robocop')
        srcdir('app/src/background/org/mozilla/gecko', 'mobile/android/tests/background/junit3/src')
        srcdir('app/src/browser', 'mobile/android/tests/browser/junit3/src')
        srcdir('app/src/javaaddons', 'mobile/android/tests/javaaddons/src')
        # Test libraries.
        srcdir('app/libs', 'build/mobile/robocop')

        srcdir('base/build.gradle', 'mobile/android/gradle/base/build.gradle')
        srcdir('base/lint.xml', 'mobile/android/gradle/base/lint.xml')
        srcdir('base/src/main/AndroidManifest.xml', 'mobile/android/gradle/base/AndroidManifest.xml')
        srcdir('base/src/main/java/org/mozilla/gecko', 'mobile/android/base')
        srcdir('base/src/main/java/org/mozilla/mozstumbler', 'mobile/android/stumbler/java/org/mozilla/mozstumbler')
        srcdir('base/src/main/java/org/mozilla/search', 'mobile/android/search/java/org/mozilla/search')
        srcdir('base/src/main/java/org/mozilla/javaaddons', 'mobile/android/javaaddons/java/org/mozilla/javaaddons')
        srcdir('base/src/main/res', 'mobile/android/base/resources')
        srcdir('base/src/crashreporter/res', 'mobile/android/base/crashreporter/res')

        manifest_path = os.path.join(self.topobjdir, 'mobile', 'android', 'gradle.manifest')
        with FileAvoidWrite(manifest_path) as f:
            m.write(fileobj=f)

        self.virtualenv_manager.ensure()
        code = self.run_process([
                self.virtualenv_manager.python_path,
                os.path.join(self.topsrcdir, 'python/mozbuild/mozbuild/action/process_install_manifest.py'),
                '--no-remove',
                '--no-remove-all-directory-symlinks',
                '--no-remove-empty-directories',
                os.path.join(self.topobjdir, 'mobile', 'android', 'gradle'),
                manifest_path],
            pass_thru=True, # Allow user to run gradle interactively.
            ensure_exit_code=False, # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir, 'mobile', 'android'))

        if not quiet:
            if not code:
                print(SUCCESS.format(topobjdir=self.topobjdir))

        return code


@CommandProvider
class PackageFrontend(MachCommandBase):
    """Fetch and install binary artifacts from Mozilla automation."""

    @Command('artifact', category='post-build',
        description='Use pre-built artifacts to build Fennec.',
        conditions=[
            conditions.is_android,  # mobile/android only for now.
            conditions.is_hg,  # mercurial only for now.
        ])
    def artifact(self):
        '''Download, cache, and install pre-built binary artifacts to build Fennec.

        Invoke |mach artifact| before each |mach package| to freshen your installed
        binary libraries.  That is, package using

        mach artifact install && mach package

        to download, cache, and install binary artifacts from Mozilla automation,
        replacing whatever may be in your object directory.  Use |mach artifact last|
        to see what binary artifacts were last used.

        Never build libxul again!
        '''
        pass

    def _make_artifacts(self, tree=None, job=None):
        self.log_manager.terminal_handler.setLevel(logging.INFO)

        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('pylru==1.0.9')
        self.virtualenv_manager.install_pip_package('taskcluster==0.0.16')

        state_dir = self._mach_context.state_dir
        cache_dir = os.path.join(state_dir, 'package-frontend')

        import which
        hg = which.which('hg')

        # Absolutely must come after the virtualenv is populated!
        from mozbuild.artifacts import Artifacts
        artifacts = Artifacts(tree, job, log=self.log, cache_dir=cache_dir, hg=hg)
        return artifacts

    @SubCommand('artifact', 'install',
        'Install a good pre-built artifact.')
    @CommandArgument('--tree', metavar='TREE', type=str,
        help='Firefox tree.',
        default='fx-team')  # TODO: switch to central as this stabilizes.
    @CommandArgument('--job', metavar='JOB', choices=['android-api-11'],
        help='Build job.',
        default='android-api-11')  # TODO: fish job from build configuration.
    @CommandArgument('source', metavar='SRC', nargs='?', type=str,
        help='Where to fetch and install artifacts from.  Can be omitted, in '
            'which case the current hg repository is inspected; an hg revision; '
            'a remote URL; or a local file.',
        default=None)
    def artifact_install(self, source=None, tree=None, job=None):
        artifacts = self._make_artifacts(tree=tree, job=job)
        return artifacts.install_from(source, self.distdir)

    @SubCommand('artifact', 'last',
        'Print the last pre-built artifact installed.')
    @CommandArgument('--tree', metavar='TREE', type=str,
        help='Firefox tree.',
        default='fx-team')
    @CommandArgument('--job', metavar='JOB', type=str,
        help='Build job.',
        default='android-api-11')
    def artifact_print_last(self, tree=None, job=None):
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.print_last()
        return 0

    @SubCommand('artifact', 'print-cache',
        'Print local artifact cache for debugging.')
    @CommandArgument('--tree', metavar='TREE', type=str,
        help='Firefox tree.',
        default='fx-team')
    @CommandArgument('--job', metavar='JOB', type=str,
        help='Build job.',
        default='android-api-11')
    def artifact_print_cache(self, tree=None, job=None):
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.print_cache()
        return 0

    @SubCommand('artifact', 'clear-cache',
        'Delete local artifacts and reset local artifact cache.')
    @CommandArgument('--tree', metavar='TREE', type=str,
        help='Firefox tree.',
        default='fx-team')
    @CommandArgument('--job', metavar='JOB', type=str,
        help='Build job.',
        default='android-api-11')
    def artifact_clear_cache(self, tree=None, job=None):
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.clear_cache()
        return 0
