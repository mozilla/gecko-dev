#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Builds the Bergamot translations engine for integration with Firefox.

If you wish to test the Bergamot engine locally, then uncomment the .wasm line in
the toolkit/components/translations/jar.mn after building the file. Just make sure
not to check the code change in.
"""

import argparse
import os
import platform
import shutil
import subprocess
from collections import namedtuple

import yaml

DIR_PATH = os.path.realpath(os.path.dirname(__file__))
THIRD_PARTY_PATH = os.path.join(DIR_PATH, "thirdparty")
MOZ_YAML_PATH = os.path.join(DIR_PATH, "moz.yaml")
REPO_PATH = os.path.join(THIRD_PARTY_PATH, "translations")
INFERENCE_PATH = os.path.join(REPO_PATH, "inference")
BUILD_PATH = os.path.join(INFERENCE_PATH, "build-wasm")
JS_PATH = os.path.join(BUILD_PATH, "bergamot-translator.js")
FINAL_JS_PATH = os.path.join(DIR_PATH, "bergamot-translator.js")
ROOT_PATH = os.path.join(DIR_PATH, "../../../..")

parser = argparse.ArgumentParser(
    description=__doc__,
    # Preserves whitespace in the help text.
    formatter_class=argparse.RawTextHelpFormatter,
)
parser.add_argument(
    "--clobber", action="store_true", help="Clobber the build artifacts"
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="Build with debug symbols, useful for profiling",
)

ArgNamespace = namedtuple("ArgNamespace", ["clobber", "debug"])


def git_clone_update(name: str, repo_url: str, revision: str):
    if not os.path.exists(REPO_PATH):
        print(f"\n📥 Clone the {name} repo into {REPO_PATH}\n")
        subprocess.check_call(
            ["git", "clone", repo_url, REPO_PATH],
            cwd=THIRD_PARTY_PATH,
        )

    def run(command):
        return subprocess.check_call(command, cwd=REPO_PATH)

    local_head = subprocess.check_output(
        ["git", "rev-parse", "HEAD"],
        cwd=REPO_PATH,
        text=True,
    ).strip()

    if local_head != revision:
        print(f"The head ({local_head}) and revision ({revision}) don't match.")
        print(f"\n🔎 Fetching revision {revision} from {name}.\n")
        run(["git", "fetch", "--recurse-submodules", "origin", revision])

        print(f"🛒 Checking out the revision {revision}")
        run(["git", "checkout", revision])


def maybe_remove_repo_path():
    """
    Removes the REPO_PATH if it exists, handling files, directories, and symlinks.
    """
    if not os.path.exists(REPO_PATH):
        return

    if os.path.islink(REPO_PATH) or os.path.isfile(REPO_PATH):
        os.remove(REPO_PATH)
    elif os.path.isdir(REPO_PATH):
        shutil.rmtree(REPO_PATH)

    print(f"\n🗑  Remove existing path: {REPO_PATH}")


def fetch_bergamot_source():
    """
    Fetches the Bergamot source code either from a specified path via the
    MOZILLA_TRANSLATIONS_PATH environment variable or by cloning the repository
    as defined in the moz.yaml file.

    Returns:
        str: The path to the Bergamot repository.
    """
    moz_translations_env = os.getenv("MOZILLA_TRANSLATIONS_PATH")

    maybe_remove_repo_path()

    if moz_translations_env:
        print(f"\n🛠️  MOZILLA_TRANSLATIONS_PATH is set to: {moz_translations_env}")

        moz_translations_env = os.path.abspath(moz_translations_env)

        os.symlink(moz_translations_env, REPO_PATH)
        print(f"\n🔗 Create symlink: {REPO_PATH} -> {moz_translations_env}")

        return REPO_PATH
    else:
        print(
            "\n📄 MOZILLA_TRANSLATIONS_PATH not set. Cloning the repository as per moz.yaml."
        )

        with open(MOZ_YAML_PATH, "r", encoding="utf8") as file:
            moz_yaml = yaml.safe_load(file)

        repo_url = moz_yaml["origin"]["url"]
        revision = moz_yaml["origin"]["revision"]

        git_clone_update(
            name="translations",
            repo_url=repo_url,
            revision=revision,
        )


def create_command(allow_run_on_host: bool, task_args: list[str]):
    if allow_run_on_host:
        # Attempt to build the WASM artifacts on the host computer.
        command = ["task", "inference-build-wasm"]
    else:
        # Attempt to build the WASM artifacts within a Docker container.
        command = [
            "task",
            "docker-run",
            "--",
            "task",
            "inference-build-wasm",
            "--volume",
            f"{BUILD_PATH}:/inference/build_wasm",
        ]

        if platform.system() == "Linux":
            # Linux seems to have an issue with permissions involving `tar`
            # unless the UID and GID are set to the current user.
            command.append("--run-as-user")

    # Append task arguments if they exist
    if task_args:
        command.extend(task_args)

    return command


def build_bergamot(args: ArgNamespace):
    """
    Builds the inference engine by calling the 'inference-build-wasm' task.

    If the ALLOW_RUN_ON_HOST environment variable is set to 1, then the build
    will attempt to run locally on the host system.

    Otherwise, by default, the WASM artifacts will be built with a Docker container
    using the Docker image specified by the repository.
    """
    allow_run_on_host = os.getenv("ALLOW_RUN_ON_HOST", "0") == "1"

    task_args = []
    if args.clobber:
        task_args.append("--clobber")
    if args.debug:
        task_args.append("--debug")

    command = create_command(
        allow_run_on_host,
        task_args,
    )

    print("\n🛠️  Building inference engine WASM...\n")
    return subprocess.run(command, cwd=REPO_PATH, shell=False, check=True)


def write_final_bergamot_js_file():
    """
    Formats and writes the final JavaScript file for Bergamot by running ESLint on
    a temporary copy and moving it to the final destination.
    """
    with open(JS_PATH, "r", encoding="utf8") as file:
        print("\n📐 Formatting the final Bergamot file")

        # Create the file outside of this directory so it's not ignored by ESLint.
        temp_path = os.path.join(DIR_PATH, "../temp-bergamot.js")
        with open(temp_path, "w", encoding="utf8") as temp_file:
            temp_file.write(file.read())

        subprocess.run(
            f"./mach eslint --fix {temp_path} --rule 'curly:error'",
            cwd=ROOT_PATH,
            check=True,
            shell=True,
            capture_output=True,
        )

        print(f"\n💾 Writing out final Bergamot file: {FINAL_JS_PATH}")
        shutil.move(temp_path, FINAL_JS_PATH)


def main():
    args: ArgNamespace = parser.parse_args()

    if not os.path.exists(THIRD_PARTY_PATH):
        os.mkdir(THIRD_PARTY_PATH)

    fetch_bergamot_source()
    build_bergamot(args)
    write_final_bergamot_js_file()


if __name__ == "__main__":
    main()
