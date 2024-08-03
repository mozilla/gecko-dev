# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import shutil

import mozpack.path as mozpath
from mach.decorators import Command, CommandArgument, SubCommand

targets = ["dom", "nsresult", "services", "xpcom"]


@Command("ts", category="misc", description="Run TypeScript and related commands.")
def ts(ctx):
    """
    TypeScript related commands to build/update typelibs and type-check js.

    Example:
      # Use tsc to check types in extensions framework code:
      $ ./mach ts check toolkit/components/extensions
    """
    ctx._sub_mach(["help", "ts"])
    return 1


@SubCommand("ts", "build", description="Build typelibs.")
@CommandArgument("lib", choices=targets, nargs="?")
def build(ctx, lib):
    """Command to build the target typelibs."""

    if lib is None:
        for t in targets:
            if rv := build(ctx, t):
                return rv
        return 0

    types_dir = mozpath.join(ctx.distdir, "@types")
    lib_dts = mozpath.join(types_dir, f"lib.gecko.{lib}.d.ts")
    if not os.path.exists(types_dir):
        os.makedirs(types_dir)

    if lib == "nsresult":
        xpc_msg = mozpath.join(ctx.topsrcdir, "js/xpconnect/src/xpc.msg")
        errors_json = mozpath.join(ctx.topsrcdir, "tools/ts/config/error_list.json")
        return node(ctx, "build_nsresult", lib_dts, xpc_msg, errors_json)

    if lib == "services":
        services_json = mozpath.join(ctx.topobjdir, "xpcom/components/services.json")
        if not os.path.isfile(services_json):
            return build_required(lib, services_json)
        return node(ctx, "build_services", lib_dts, services_json)

    if lib == "xpcom":
        # When we hook this up to be part of the build, we'll have
        # an explicit list of files. For now, just get all of them.
        dir = mozpath.join(ctx.topobjdir, "config/makefiles/xpidl")
        if not os.path.isdir(dir):
            return build_required(lib, dir)

        files = [f for f in os.listdir(dir) if f.endswith(".d.json")]
        if not len(files):
            return build_required(lib, f"*.d.json files in {dir}")

        return node(ctx, "build_xpcom", lib_dts, dir, *files)

    if lib == "dom":
        # Same as above, get all *.webidl files for now.
        dir = mozpath.join(ctx.topsrcdir, "dom")
        files = []
        for subdir in ["webidl", "chrome-webidl"]:
            for file in os.listdir(mozpath.join(dir, subdir)):
                if file.endswith(".webidl"):
                    files.append(subdir + "/" + file)

        return node(ctx, "build_dom", lib_dts, dir, *files)

    raise ValueError(f"Unknown typelib: {lib}")


@SubCommand("ts", "check", description="Check types in a project using tsc.")
@CommandArgument("paths", nargs="+", help="Path to a (dir with) tsconfig.json.")
def check(ctx, paths):
    for p in paths:
        rv = node(ctx, "node_modules/typescript/bin/tsc", "--project", p)
        if rv:
            return rv
    return 0


@SubCommand("ts", "setup", description="Install TypeScript and other dependencies.")
def setup(ctx):
    # Install locally under tools/ts/node_modules, to avoid any conflicts for now.
    os.chdir(mozpath.dirname(__file__))
    return ctx._sub_mach(["npm", "ci"])


@SubCommand("ts", "update", description="Update tools/@types libraries.")
def update(ctx):
    typelib_dir = mozpath.join(ctx.topsrcdir, "tools/@types")

    for lib in targets:
        file = f"lib.gecko.{lib}.d.ts"
        path = mozpath.join(ctx.distdir, "@types", file)
        if not os.path.exists(path):
            print(f"[ERROR] {path} not found. Did you run `mach ts build`?")
            return 1

        # This command inherently goes in a confusing direction, we're copying:
        # from `<topobjdir>/dist/@types` files generated with `mach ts build`,
        # into `<topsrcdir>/tools/@types` typelib dir back in your source tree.
        print(f"[INFO] Updating {typelib_dir}/{file}")
        shutil.copy(path, typelib_dir)

    print("[WARNING] Your source tree was updated, you should commit the changes.")


def node(ctx, script, *args):
    maybe_setup(ctx)
    path = mozpath.join(mozpath.dirname(__file__), script)
    return ctx._sub_mach(["node", path, *args])


def maybe_setup(ctx):
    """Check if npm modules are installed, and run setup if needed."""
    dir = mozpath.dirname(__file__)
    package_json = json.load(open(mozpath.join(dir, "package.json")))
    needs_setup = False

    # TODO: Use proper version checking from tools/lint/eslint/setup_helper.py.
    for module in package_json.get("devDependencies", {}):
        path = mozpath.join(dir, "node_modules", module, "package.json")
        if not os.path.isfile(path):
            print(f"Missing node module {path}.")
            needs_setup = True
            break

    if needs_setup:
        print("Running npm install.")
        setup(ctx)


def build_required(lib, item):
    print(f"Missing {item}.")
    print(f"Building {lib} typelib requires a full Firefox build.")
    return 1
