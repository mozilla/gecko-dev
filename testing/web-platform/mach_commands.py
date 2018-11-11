# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the web-platform-tests test runner with mach.

from __future__ import absolute_import, unicode_literals, print_function

import os
import sys

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandProvider,
    Command,
)

from mach_commands_base import WebPlatformTestsRunner, create_parser_wpt


class WebPlatformTestsRunnerSetup(MozbuildObject):
    default_log_type = "mach"

    def kwargs_common(self, kwargs):
        build_path = os.path.join(self.topobjdir, 'build')
        here = os.path.split(__file__)[0]
        tests_src_path = os.path.join(here, "tests")
        if build_path not in sys.path:
            sys.path.append(build_path)

        if kwargs["product"] == "fennec":
            # Note that this import may fail in non-fennec trees
            from mozrunner.devices.android_device import verify_android_device, grant_runtime_permissions
            verify_android_device(self, install=True, verbose=False, xre=True)

            # package_name may be non-fennec in the future
            package_name = kwargs["package_name"]
            if not package_name:
                package_name = self.substs["ANDROID_PACKAGE_NAME"]

            grant_runtime_permissions(self, package_name, kwargs["device_serial"])
            if kwargs["certutil_binary"] is None:
                kwargs["certutil_binary"] = os.path.join(os.environ.get('MOZ_HOST_BIN'), "certutil")

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topobjdir, '_tests', 'web-platform', 'wptrunner.local.ini')

        if kwargs["prefs_root"] is None:
            kwargs["prefs_root"] = os.path.join(self.topsrcdir, 'testing', 'profiles')

        if kwargs["stackfix_dir"] is None:
            kwargs["stackfix_dir"] = self.bindir

        if kwargs["exclude"] is None and kwargs["include"] is None and not sys.platform.startswith("linux"):
            kwargs["exclude"] = ["css"]

        if kwargs["ssl_type"] in (None, "pregenerated"):
            cert_root = os.path.join(tests_src_path, "tools", "certs")
            if kwargs["ca_cert_path"] is None:
                kwargs["ca_cert_path"] = os.path.join(cert_root, "cacert.pem")

            if kwargs["host_key_path"] is None:
                kwargs["host_key_path"] = os.path.join(cert_root, "web-platform.test.key")

            if kwargs["host_cert_path"] is None:
                kwargs["host_cert_path"] = os.path.join(cert_root, "web-platform.test.pem")

        if kwargs["log_mach_screenshot"] is None:
            kwargs["log_mach_screenshot"] = True

        kwargs["capture_stdio"] = True

        return kwargs

    def kwargs_firefox(self, kwargs):
        import mozinfo
        from wptrunner import wptcommandline
        kwargs = self.kwargs_common(kwargs)

        if kwargs["binary"] is None:
            kwargs["binary"] = self.get_binary_path()

        if kwargs["certutil_binary"] is None:
            kwargs["certutil_binary"] = self.get_binary_path('certutil')

        if kwargs["webdriver_binary"] is None:
            kwargs["webdriver_binary"] = self.get_binary_path("geckodriver", validate_exists=False)


        if mozinfo.info["os"] == "win" and mozinfo.info["os_version"] == "6.1":
            # On Windows 7 --install-fonts fails, so fall back to a Firefox-specific codepath
            self.setup_fonts_firefox()
        else:
            kwargs["install_fonts"] = True

        kwargs = wptcommandline.check_args(kwargs)

        return kwargs

    def kwargs_wptrun(self, kwargs):
        from wptrunner import wptcommandline
        here = os.path.join(self.topsrcdir, 'testing', 'web-platform')

        kwargs["tests_root"] = os.path.join(here, "tests")

        sys.path.insert(0, kwargs["tests_root"])

        if kwargs["metadata_root"] is None:
            metadir = os.path.join(here, "products", kwargs["product"])
            if not os.path.exists(metadir):
                os.makedirs(metadir)
            kwargs["metadata_root"] = metadir

        src_manifest = os.path.join(here, "meta", "MANIFEST.json")
        dest_manifest = os.path.join(kwargs["metadata_root"], "MANIFEST.json")

        if not os.path.exists(dest_manifest) and os.path.exists(src_manifest):
            with open(src_manifest) as src, open(dest_manifest, "w") as dest:
                dest.write(src.read())

        from tools.wpt import run

        # Add additional kwargs consumed by the run frontend. Currently we don't
        # have a way to set these through mach
        kwargs["channel"] = None
        kwargs["prompt"] = True
        kwargs["install_browser"] = False

        try:
            kwargs = run.setup_wptrunner(run.virtualenv.Virtualenv(self.virtualenv_manager.virtualenv_root),
                                         **kwargs)
        except run.WptrunError as e:
            print(e.message, file=sys.stderr)
            sys.exit(1)

        return kwargs

    def setup_fonts_firefox(self):
        # Ensure the Ahem font is available
        if not sys.platform.startswith("darwin"):
            font_path = os.path.join(os.path.dirname(self.get_binary_path()), "fonts")
        else:
            font_path = os.path.join(os.path.dirname(self.get_binary_path()), os.pardir, "Resources", "res", "fonts")
        ahem_src = os.path.join(self.topsrcdir, "testing", "web-platform", "tests", "fonts", "Ahem.ttf")
        ahem_dest = os.path.join(font_path, "Ahem.ttf")
        if not os.path.exists(ahem_dest) and os.path.exists(ahem_src):
            with open(ahem_src, "rb") as src, open(ahem_dest, "wb") as dest:
                dest.write(src.read())



class WebPlatformTestsUpdater(MozbuildObject):
    """Update web platform tests."""
    def setup_logging(self, **kwargs):
        import update
        return update.setup_logging(kwargs, {"mach": sys.stdout})

    def run_update(self, logger, **kwargs):
        import update
        from update import updatecommandline

        if kwargs["config"] is None:
            kwargs["config"] = os.path.join(self.topobjdir, '_tests', 'web-platform', 'wptrunner.local.ini')
        if kwargs["product"] is None:
            kwargs["product"] = "firefox"

        kwargs["store_state"] = False

        kwargs = updatecommandline.check_args(kwargs)

        try:
            update.run_update(logger, **kwargs)
        except Exception:
            import pdb
            import traceback
            traceback.print_exc()
#            pdb.post_mortem()


class WebPlatformTestsCreator(MozbuildObject):
    template_prefix = """<!doctype html>
%(documentElement)s<meta charset=utf-8>
"""
    template_long_timeout = "<meta name=timeout content=long>\n"

    template_body_th = """<title></title>
<script src=/resources/testharness.js></script>
<script src=/resources/testharnessreport.js></script>
<script>

</script>
"""

    template_body_reftest = """<title></title>
<link rel=%(match)s href=%(ref)s>
"""

    template_body_reftest_wait = """<script src="/common/reftest-wait.js"></script>
"""

    def rel_path(self, path):
        if path is None:
            return

        abs_path = os.path.normpath(os.path.abspath(path))
        return os.path.relpath(abs_path, self.topsrcdir)

    def rel_url(self, rel_path):
        upstream_path = os.path.join("testing", "web-platform", "tests")
        local_path = os.path.join("testing", "web-platform", "mozilla", "tests")

        if rel_path.startswith(upstream_path):
            return rel_path[len(upstream_path):].replace(os.path.sep, "/")
        elif rel_path.startswith(local_path):
            return "/_mozilla" + rel_path[len(local_path):].replace(os.path.sep, "/")
        else:
            return None

    def run_create(self, context, **kwargs):
        import subprocess

        path = self.rel_path(kwargs["path"])
        ref_path = self.rel_path(kwargs["ref"])

        if kwargs["ref"]:
            kwargs["reftest"] = True

        if self.rel_url(path) is None:
            print("""Test path %s is not in wpt directories:
testing/web-platform/tests for tests that may be shared
testing/web-platform/mozilla/tests for Gecko-only tests""" % path)
            return 1

        if ref_path and self.rel_url(ref_path) is None:
            print("""Reference path %s is not in wpt directories:
testing/web-platform/tests for tests that may be shared
            testing/web-platform/mozilla/tests for Gecko-only tests""" % ref_path)
            return 1

        if os.path.exists(path) and not kwargs["overwrite"]:
            print("Test path already exists, pass --overwrite to replace")
            return 1

        if kwargs["mismatch"] and not kwargs["reftest"]:
            print("--mismatch only makes sense for a reftest")
            return 1

        if kwargs["wait"] and not kwargs["reftest"]:
            print("--wait only makes sense for a reftest")
            return 1

        args = {"documentElement": "<html class=reftest-wait>\n" if kwargs["wait"] else ""}
        template = self.template_prefix % args
        if kwargs["long_timeout"]:
            template += self.template_long_timeout

        if kwargs["reftest"]:
            args = {"match": "match" if not kwargs["mismatch"] else "mismatch",
                    "ref": self.rel_url(ref_path) if kwargs["ref"] else '""'}
            template += self.template_body_reftest % args
            if kwargs["wait"]:
                template += self.template_body_reftest_wait
        else:
            template += self.template_body_th
        try:
            os.makedirs(os.path.dirname(path))
        except OSError:
            pass
        with open(path, "w") as f:
            f.write(template)

        ref_path = kwargs["ref"]
        if ref_path and not os.path.exists(ref_path):
            with open(ref_path, "w") as f:
                f.write(self.template_prefix % {"documentElement": ""})

        if kwargs["no_editor"]:
            editor = None
        elif kwargs["editor"]:
            editor = kwargs["editor"]
        elif "VISUAL" in os.environ:
            editor = os.environ["VISUAL"]
        elif "EDITOR" in os.environ:
            editor = os.environ["EDITOR"]
        else:
            editor = None

        proc = None
        if editor:
            if ref_path:
                path = "%s %s" % (path, ref_path)
            proc = subprocess.Popen("%s %s" % (editor, path), shell=True)

        if proc:
            proc.wait()


def create_parser_update():
    from update import updatecommandline
    return updatecommandline.create_parser()


def create_parser_create():
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--no-editor", action="store_true",
                   help="Don't try to open the test in an editor")
    p.add_argument("-e", "--editor", action="store", help="Editor to use")
    p.add_argument("--long-timeout", action="store_true",
                   help="Test should be given a long timeout (typically 60s rather than 10s, but varies depending on environment)")
    p.add_argument("--overwrite", action="store_true",
                   help="Allow overwriting an existing test file")
    p.add_argument("-r", "--reftest", action="store_true",
                   help="Create a reftest rather than a testharness (js) test"),
    p.add_argument("-m", "--reference", dest="ref", help="Path to the reference file")
    p.add_argument("--mismatch", action="store_true",
                   help="Create a mismatch reftest")
    p.add_argument("--wait", action="store_true",
                   help="Create a reftest that waits until takeScreenshot() is called")
    p.add_argument("path", action="store", help="Path to the test file")
    return p


def create_parser_manifest_update():
    import manifestupdate
    return manifestupdate.create_parser()


@CommandProvider
class MachCommands(MachCommandBase):
    def setup(self):
        self._activate_virtualenv()

    @Command("web-platform-tests",
             category="testing",
             conditions=[conditions.is_firefox_or_android],
             parser=create_parser_wpt)
    def run_web_platform_tests(self, **params):
        self.setup()
        if conditions.is_android(self) and params["product"] != "fennec":
            if params["product"] is None:
                params["product"] = "fennec"
            else:
                raise ValueError("Must specify --product=fennec in Android environment.")
        if "test_objects" in params:
            for item in params["test_objects"]:
                params["include"].append(item["name"])
            del params["test_objects"]

        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        wpt_runner = WebPlatformTestsRunner(wpt_setup)

        logger = wpt_runner.setup_logging(**params)

        return wpt_runner.run(logger, **params)

    @Command("wpt",
             category="testing",
             conditions=[conditions.is_firefox_or_android],
             parser=create_parser_wpt)
    def run_wpt(self, **params):
        return self.run_web_platform_tests(**params)

    @Command("web-platform-tests-update",
             category="testing",
             parser=create_parser_update)
    def update_web_platform_tests(self, **params):
        self.setup()
        self.virtualenv_manager.install_pip_package('html5lib==1.0.1')
        self.virtualenv_manager.install_pip_package('ujson')
        self.virtualenv_manager.install_pip_package('requests')

        wpt_updater = self._spawn(WebPlatformTestsUpdater)
        logger = wpt_updater.setup_logging(**params)
        return wpt_updater.run_update(logger, **params)

    @Command("wpt-update",
             category="testing",
             parser=create_parser_update)
    def update_wpt(self, **params):
        return self.update_web_platform_tests(**params)

    @Command("web-platform-tests-create",
             category="testing",
             parser=create_parser_create)
    def create_web_platform_test(self, **params):
        self.setup()
        wpt_creator = self._spawn(WebPlatformTestsCreator)
        wpt_creator.run_create(self._mach_context, **params)

    @Command("wpt-create",
             category="testing",
             parser=create_parser_create)
    def create_wpt(self, **params):
        return self.create_web_platform_test(**params)

    @Command("wpt-manifest-update",
             category="testing",
             parser=create_parser_manifest_update)
    def wpt_manifest_update(self, **params):
        self.setup()
        wpt_setup = self._spawn(WebPlatformTestsRunnerSetup)
        wpt_runner = WebPlatformTestsRunner(wpt_setup)
        logger = wpt_runner.setup_logging(**params)
        return 0 if wpt_runner.update_manifest(logger, **params) else 1
