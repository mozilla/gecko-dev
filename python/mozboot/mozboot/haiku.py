# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper


class HaikuBootstrapper(BaseBootstrapper):
    def __init__(self, version, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.packages = [
            "make",
            "pkgconf",
            "rust_bin",
        ]

        self.browser_packages = [
            "dbus_glib_devel",
            "gtk3_devel",
            "llvm12",
            "nasm",
            "nodejs16",
        ]

    def pkgman_install(self, *packages):
        command = ["pkgman", "install"]
        if self.no_interactive:
            command.append("-y")

        command.extend(packages)
        self.run_as_root(command)

    def install_system_packages(self):
        self.pkgman_install(*self.packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        self.pkgman_install(*self.browser_packages)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def ensure_clang_static_analysis_package(self):
        # TODO: we don't ship clang base static analysis for this platform
        pass

    def ensure_stylo_packages(self):
        # Clang / llvm already installed as browser package
        self.pkgman_install("cbindgen")

    def ensure_nasm_packages(self):
        # installed via install_browser_packages
        pass

    def ensure_node_packages(self):
        self.pkgman_install("npm")
