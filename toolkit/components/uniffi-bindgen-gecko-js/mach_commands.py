# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import distutils.ccompiler
import os
import subprocess

from mach.decorators import Command, SubCommand

CPP_PATH = "toolkit/components/uniffi-js/UniFFIGeneratedScaffolding.cpp"
JS_DIR = "toolkit/components/uniffi-bindgen-gecko-js/components/generated"
FIXTURE_JS_DIR = "toolkit/components/uniffi-bindgen-gecko-js/fixtures/generated"


def build_gkrust_uniffi_library(command_context, package_name):
    uniffi_root = crate_root(command_context)
    print("Building gkrust-uniffi-components")
    cmdline = [
        "cargo",
        "build",
        "--release",
        "--manifest-path",
        os.path.join(command_context.topsrcdir, "Cargo.toml"),
        "--package",
        package_name,
    ]
    subprocess.check_call(cmdline, cwd=uniffi_root)
    print()
    ccompiler = distutils.ccompiler.new_compiler()
    return ccompiler.find_library_file(
        [os.path.join(command_context.topsrcdir, "target", "release")],
        package_name.replace("-", "_"),
    )


def build_uniffi_bindgen_gecko_js(command_context):
    uniffi_root = crate_root(command_context)
    print("Building uniffi-bindgen-gecko-js")
    cmdline = [
        "cargo",
        "build",
        "--release",
        "--manifest-path",
        os.path.join(command_context.topsrcdir, "Cargo.toml"),
        "--package",
        "uniffi-bindgen-gecko-js",
    ]
    subprocess.check_call(cmdline, cwd=uniffi_root)
    print()
    return os.path.join(
        command_context.topsrcdir, "target", "release", "uniffi-bindgen-gecko-js"
    )


@Command(
    "uniffi",
    category="devenv",
    description="Generate JS bindings using uniffi-bindgen-gecko-js",
)
def uniffi(command_context, *runargs, **lintargs):
    """Run uniffi."""
    command_context._sub_mach(["help", "uniffi"])
    return 1


@SubCommand(
    "uniffi",
    "generate",
    description="Generate/regenerate bindings",
)
def generate_command(command_context):
    library_path = build_gkrust_uniffi_library(
        command_context, "gkrust-uniffi-components"
    )
    fixtures_library_path = build_gkrust_uniffi_library(
        command_context, "gkrust-uniffi-fixtures"
    )
    binary_path = build_uniffi_bindgen_gecko_js(command_context)
    cmdline = [
        binary_path,
        "--library-path",
        library_path,
        "--fixtures-library-path",
        fixtures_library_path,
        "--js-dir",
        JS_DIR,
        "--fixture-js-dir",
        FIXTURE_JS_DIR,
        "--cpp-path",
        CPP_PATH,
    ]
    subprocess.check_call(cmdline, cwd=command_context.topsrcdir)
    return 0


def crate_root(command_context):
    return os.path.join(
        command_context.topsrcdir, "toolkit", "components", "uniffi-bindgen-gecko-js"
    )
