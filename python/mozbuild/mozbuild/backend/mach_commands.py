# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import logging
import os
import subprocess
import sys

import mozpack.path as mozpath
from mach.decorators import Command, CommandArgument
from mozfile import which

from mozbuild.util import cpu_count


@Command(
    "ide",
    category="devenv",
    description="Generate a project and launch an IDE.",
    virtualenv_name="ide",
)
@CommandArgument("ide", choices=["eclipse", "visualstudio", "vscode"])
@CommandArgument(
    "--no-interactive",
    default=False,
    action="store_true",
    help="Just generate the configuration",
)
@CommandArgument("args", nargs=argparse.REMAINDER)
def run(command_context, ide, no_interactive, args):
    interactive = not no_interactive

    if ide == "eclipse" and not which("eclipse"):
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Eclipse CDT 8.4 or later must be installed in your PATH.",
        )
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Download: http://www.eclipse.org/cdt/downloads.php",
        )
        return 1

    if ide == "vscode":
        result = subprocess.run([sys.executable, "mach", "configure"])
        if result.returncode:
            return result.returncode

        # First install what we can through install manifests.
        # Then build the rest of the build dependencies by running the full
        # export target, because we can't do anything better.
        result = subprocess.run(
            [sys.executable, "mach", "build", "pre-export", "export", "pre-compile"]
        )
        if result.returncode:
            return result.returncode
    else:
        # Here we refresh the whole build. 'build export' is sufficient here and is
        # probably more correct but it's also nice having a single target to get a fully
        # built and indexed project (gives a easy target to use before go out to lunch).
        result = subprocess.run([sys.executable, "mach", "build"])
        if result.returncode:
            return result.returncode

    backend = None
    if ide == "eclipse":
        backend = "CppEclipse"
    elif ide == "visualstudio":
        backend = "VisualStudio"
    elif ide == "vscode":
        if not command_context.config_environment.is_artifact_build:
            backend = "Clangd"

    if backend:
        # Generate or refresh the IDE backend.
        result = subprocess.run(
            [sys.executable, "mach", "build-backend", "-b", backend]
        )
        if result.returncode:
            return result.returncode

    if ide == "eclipse":
        eclipse_workspace_dir = get_eclipse_workspace_path(command_context)
        subprocess.check_call(["eclipse", "-data", eclipse_workspace_dir])
    elif ide == "visualstudio":
        visual_studio_workspace_dir = get_visualstudio_workspace_path(command_context)
        subprocess.call(["explorer.exe", visual_studio_workspace_dir])
    elif ide == "vscode":
        return setup_vscode(command_context, interactive)


def get_eclipse_workspace_path(command_context):
    from mozbuild.backend.cpp_eclipse import CppEclipseBackend

    return CppEclipseBackend.get_workspace_path(
        command_context.topsrcdir, command_context.topobjdir
    )


def get_visualstudio_workspace_path(command_context):
    return os.path.normpath(
        os.path.join(command_context.topobjdir, "msvc", "mozilla.sln")
    )


def setup_vscode(command_context, interactive):
    from mozbuild.backend.clangd import find_vscode_cmd

    # Check if platform has VSCode installed
    if interactive:
        vscode_cmd = find_vscode_cmd()
        if vscode_cmd is None:
            choice = prompt_bool(
                "VSCode cannot be found, and may not be installed. Proceed?"
            )
            if not choice:
                return 1

    vscode_settings = mozpath.join(
        command_context.topsrcdir, ".vscode", "settings.json"
    )

    new_settings = {}
    artifact_prefix = ""
    if command_context.config_environment.is_artifact_build:
        artifact_prefix = (
            "\nArtifact build configured: Skipping clang and rust setup. "
            "If you later switch to a full build, please re-run this command."
        )
    else:
        new_settings = setup_clangd_rust_in_vscode(command_context)

    relobjdir = mozpath.relpath(command_context.topobjdir, command_context.topsrcdir)

    # Add file associations.
    new_settings = {
        **new_settings,
        "files.associations": {
            "*.jsm": "javascript",
            "*.sjs": "javascript",
        },
        "files.exclude": {"obj-*": True, relobjdir: True},
        "files.watcherExclude": {"obj-*": True, relobjdir: True},
    }
    # These are added separately because vscode doesn't override user settings
    # otherwise which leads to the wrong auto-formatting.
    prettier_languages = [
        "javascript",
        "javascriptreact",
        "typescript",
        "typescriptreact",
        "json",
        "jsonc",
        "html",
        "css",
    ]
    for lang in prettier_languages:
        new_settings[f"[{lang}]"] = {
            "editor.defaultFormatter": "esbenp.prettier-vscode",
            "editor.formatOnSave": True,
        }

    # Add matchers for autolinking bugs and revisions in the terminal.
    new_settings = {
        **new_settings,
        "terminalLinks.matchers": [
            {
                "regex": "\\b[Bb]ug\\s*(\\d+)\\b",
                "uri": "https://bugzilla.mozilla.org/show_bug.cgi?id=$1",
            },
            {
                "regex": "\\b(D\\d+)\\b",
                "uri": "https://phabricator.services.mozilla.com/$1",
            },
        ],
    }

    import difflib

    try:
        import json5 as json

        dump_extra = {"quote_keys": True, "trailing_commas": False}
    except ImportError:
        import json

        dump_extra = {}

    # Load the existing .vscode/settings.json file, to check if if needs to
    # be created or updated.
    try:
        with open(vscode_settings) as fh:
            old_settings_str = fh.read()
    except FileNotFoundError:
        print(f"Configuration for {vscode_settings} will be created.{artifact_prefix}")
        old_settings_str = None

    if old_settings_str is None:
        # No old settings exist
        with open(vscode_settings, "w") as fh:
            json.dump(new_settings, fh, indent=4, **dump_extra)
    else:
        # Merge our new settings with the existing settings, and check if we
        # need to make changes. Only prompt & write out the updated config
        # file if settings actually changed.
        try:
            old_settings = json.loads(old_settings_str)
            prompt_prefix = ""
        except ValueError:
            old_settings = {}
            prompt_prefix = (
                "\n**WARNING**: Parsing of existing settings file failed. "
                "Existing settings will be lost!"
            )

        # If we've got an old section with the formatting configuration, remove it
        # so that we effectively "upgrade" the user to include json from the new
        # settings. The user is presented with the diffs so should spot any issues.
        deprecated = [
            "[javascript][javascriptreact][typescript][typescriptreact]",
            "[javascript][javascriptreact][typescript][typescriptreact][json]",
            "[javascript][javascriptreact][typescript][typescriptreact][json][html]",
            "[javascript][javascriptreact][typescript][typescriptreact][json][jsonc][html]",
        ]
        for entry in deprecated:
            if entry in old_settings:
                old_settings.pop(entry)

        settings = {**old_settings, **new_settings}

        if old_settings != settings:
            # Prompt the user with a diff of the changes we're going to make
            new_settings_str = json.dumps(settings, indent=4, **dump_extra)
            if interactive:
                print(
                    "\nThe following modifications to {settings} will occur:\n{diff}".format(
                        settings=vscode_settings,
                        diff="".join(
                            difflib.unified_diff(
                                old_settings_str.splitlines(keepends=True),
                                new_settings_str.splitlines(keepends=True),
                                "a/.vscode/settings.json",
                                "b/.vscode/settings.json",
                                n=30,
                            )
                        ),
                    )
                )
                choice = prompt_bool(
                    f"{artifact_prefix}{prompt_prefix}\nProceed with modifications to {vscode_settings}?"
                )
                if not choice:
                    return 1

            with open(vscode_settings, "w") as fh:
                fh.write(new_settings_str)

    if not interactive:
        return 0

    # Open vscode with new configuration, or ask the user to do so if the
    # binary was not found.
    if vscode_cmd is None:
        print(
            f"Please open VS Code manually and load directory: {command_context.topsrcdir}"
        )
        return 0

    rc = subprocess.call(vscode_cmd + [command_context.topsrcdir])

    if rc != 0:
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Unable to open VS Code. Please open VS Code manually and load "
            f"directory: {command_context.topsrcdir}",
        )
        return rc

    return 0


def setup_clangd_rust_in_vscode(command_context):
    clangd_cc_path = mozpath.join(command_context.topobjdir, "clangd")

    # Verify if the required files are present
    clang_tools_path = mozpath.join(
        command_context._mach_context.state_dir, "clang-tools"
    )
    clang_tidy_bin = mozpath.join(clang_tools_path, "clang-tidy", "bin")

    clangd_path = mozpath.join(
        clang_tidy_bin,
        "clangd" + command_context.config_environment.substs.get("BIN_SUFFIX", ""),
    )

    if not os.path.exists(clangd_path):
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            f"Unable to locate clangd in {clang_tidy_bin}.",
        )
        rc = get_clang_tools(command_context, clang_tools_path)

        if rc != 0:
            return rc

    from mozbuild.code_analysis.utils import ClangTidyConfig

    clang_tidy_cfg = ClangTidyConfig(command_context.topsrcdir)

    # The location of the comm/ directory if we're building Thunderbird. `None`
    # if we're building Firefox.
    commtopsrcdir = command_context.substs.get("commtopsrcdir")

    if commtopsrcdir:
        # Thunderbird uses its own Rust workspace, located in comm/rust/ - we
        # set it as the main workspace to build a little further below. The
        # working directory for cargo check commands is the workspace's root.
        if sys.platform == "win32":
            cargo_check_command = [sys.executable, "../../mach"]
        else:
            cargo_check_command = ["../../mach"]
    else:
        if sys.platform == "win32":
            cargo_check_command = [sys.executable, "mach"]
        else:
            cargo_check_command = ["./mach"]

    cargo_check_command += [
        "--log-no-times",
        "cargo",
        "check",
        "-j",
        str(cpu_count() // 2),
        "--all-crates",
        "--message-format-json",
    ]

    clang_tidy = {}
    clang_tidy["Checks"] = ",".join(clang_tidy_cfg.checks)
    clang_tidy.update(clang_tidy_cfg.checks_config)

    # Write .clang-tidy yml
    import yaml

    with open(".clang-tidy", "w") as file:
        yaml.dump(clang_tidy, file)

    clangd_cfg = {
        "CompileFlags": {
            "CompilationDatabase": clangd_cc_path,
        }
    }

    with open(".clangd", "w") as file:
        yaml.dump(clangd_cfg, file)

    rust_analyzer_extra_includes = [command_context.topobjdir]

    if windows_rs_dir := command_context.config_environment.substs.get(
        "MOZ_WINDOWS_RS_DIR"
    ):
        rust_analyzer_extra_includes.append(windows_rs_dir)

    config = {
        "clangd.path": clangd_path,
        "clangd.arguments": [
            "-j",
            str(cpu_count() // 2),
            "--limit-results",
            "0",
            "--completion-style",
            "detailed",
            "--background-index",
            "--all-scopes-completion",
            "--log",
            "info",
            "--pch-storage",
            "disk",
            "--clang-tidy",
            "--header-insertion=never",
        ],
        "rust-analyzer.server.extraEnv": {
            # Point rust-analyzer at the real target directory used by our
            # build, so it can discover the files created when we run `./mach
            # cargo check`.
            "CARGO_TARGET_DIR": command_context.topobjdir,
        },
        "rust-analyzer.vfs.extraIncludes": rust_analyzer_extra_includes,
        "rust-analyzer.cargo.buildScripts.overrideCommand": cargo_check_command,
        "rust-analyzer.check.overrideCommand": cargo_check_command,
    }

    # If we're building Thunderbird, configure rust-analyzer to use its Cargo
    # workspace rather than Firefox's. `linkedProjects` disables rust-analyzer's
    # project auto-discovery, therefore setting it ensures we use the correct
    # workspace.
    if commtopsrcdir:
        config["rust-analyzer.linkedProjects"] = [
            os.path.join(commtopsrcdir, "rust", "Cargo.toml")
        ]

    return config


def get_clang_tools(command_context, clang_tools_path):
    import shutil

    if os.path.isdir(clang_tools_path):
        shutil.rmtree(clang_tools_path)

    # Create base directory where we store clang binary
    os.mkdir(clang_tools_path)

    from mozbuild.artifact_commands import artifact_toolchain

    job, _ = command_context.platform

    if job is None:
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "The current platform isn't supported. "
            "Currently only the following platforms are "
            "supported: win32/win64, linux64 and macosx64.",
        )
        return 1

    job += "-clang-tidy"

    # We want to unpack data in the clang-tidy mozbuild folder
    currentWorkingDir = os.getcwd()
    os.chdir(clang_tools_path)
    rc = artifact_toolchain(
        command_context, verbose=False, from_build=[job], no_unpack=False, retry=0
    )
    # Change back the cwd
    os.chdir(currentWorkingDir)

    return rc


def prompt_bool(prompt, limit=5):
    """Prompts the user with prompt and requires a boolean value."""
    from mach.util import strtobool

    for _ in range(limit):
        try:
            return strtobool(input(prompt + " [Y/N]\n"))
        except ValueError:
            print(
                "ERROR! Please enter a valid option! Please use any of the following:"
                " Y, N, True, False, 1, 0"
            )
    return False
