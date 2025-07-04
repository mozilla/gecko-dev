# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import atexit
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Any

import appdirs
import yaml
from taskgraph.main import (
    FORMAT_METHODS,
    argument,
    command,
    commands,
    dump_output,
    generate_taskgraph,
)

from gecko_taskgraph import GECKO
from gecko_taskgraph.files_changed import get_locally_changed_files


def format_taskgraph_yaml(taskgraph):
    from taskgraph.util.readonlydict import ReadOnlyDict

    class TGDumper(yaml.SafeDumper):
        def ignore_aliases(self, data):
            return True

        def represent_ro_dict(self, data):
            return self.represent_dict(dict(data))

    TGDumper.add_representer(ReadOnlyDict, TGDumper.represent_ro_dict)

    return yaml.dump(taskgraph.to_json(), Dumper=TGDumper, default_flow_style=False)


FORMAT_METHODS["yaml"] = format_taskgraph_yaml


@command(
    "tasks",
    help="Show all tasks in the taskgraph.",
    defaults={"graph_attr": "full_task_set"},
)
@command(
    "full", help="Show the full taskgraph.", defaults={"graph_attr": "full_task_graph"}
)
@command(
    "target",
    help="Show the set of target tasks.",
    defaults={"graph_attr": "target_task_set"},
)
@command(
    "target-graph",
    help="Show the target graph.",
    defaults={"graph_attr": "target_task_graph"},
)
@command(
    "optimized",
    help="Show the optimized graph.",
    defaults={"graph_attr": "optimized_task_graph"},
)
@command(
    "morphed",
    help="Show the morphed graph.",
    defaults={"graph_attr": "morphed_task_graph"},
)
@argument("--root", "-r", help="root of the taskgraph definition relative to topsrcdir")
@argument("--quiet", "-q", action="store_true", help="suppress all logging output")
@argument(
    "--verbose", "-v", action="store_true", help="include debug-level logging output"
)
@argument(
    "--json",
    "-J",
    action="store_const",
    dest="format",
    const="json",
    help="Output task graph as a JSON object",
)
@argument(
    "--yaml",
    "-Y",
    action="store_const",
    dest="format",
    const="yaml",
    help="Output task graph as a YAML object",
)
@argument(
    "--labels",
    "-L",
    action="store_const",
    dest="format",
    const="labels",
    help="Output the label for each task in the task graph (default)",
)
@argument(
    "--parameters",
    "-p",
    default=None,
    action="append",
    help="Parameters to use for the generation. Can be a path to file (.yml or "
    ".json; see `taskcluster/docs/parameters.rst`), a directory (containing "
    "parameters files), a url, of the form `project=mozilla-central` to download "
    "latest parameters file for the specified project from CI, or of the form "
    "`task-id=<decision task id>` to download parameters from the specified "
    "decision task. Can be specified multiple times, in which case multiple "
    "generations will happen from the same invocation (one per parameters "
    "specified).",
)
@argument(
    "--force-local-files-changed",
    default=False,
    action="store_true",
    help="Compute the 'files-changed' parameter from local version control, "
    "even when explicitly using a parameter set that already has it defined. "
    "Note that this is already the default behaviour when no parameters are "
    "specified.",
)
@argument(
    "--no-optimize",
    dest="optimize",
    action="store_false",
    default="true",
    help="do not remove tasks from the graph that are found in the "
    "index (a.k.a. optimize the graph)",
)
@argument(
    "-o",
    "--output-file",
    default=None,
    help="file path to store generated output.",
)
@argument(
    "--tasks-regex",
    "--tasks",
    default=None,
    help="only return tasks with labels matching this regular " "expression.",
)
@argument(
    "--exclude-key",
    default=None,
    dest="exclude_keys",
    action="append",
    help="Exclude the specified key (using dot notation) from the final result. "
    "This is mainly useful with '--diff' to filter out expected differences.",
)
@argument(
    "-k",
    "--target-kind",
    dest="target_kinds",
    action="append",
    default=[],
    help="only return tasks that are of the given kind, or their dependencies.",
)
@argument(
    "-F",
    "--fast",
    default=False,
    action="store_true",
    help="enable fast task generation for local debugging.",
)
@argument(
    "--diff",
    const="default",
    nargs="?",
    default=None,
    help="Generate and diff the current taskgraph against another revision. "
    "Without args the base revision will be used. A revision specifier such as "
    "the hash or `.~1` (hg) or `HEAD~1` (git) can be used as well.",
)
@argument(
    "-j",
    "--max-workers",
    dest="max_workers",
    default=None,
    type=int,
    help="The maximum number of workers to use for parallel operations such as"
    "when multiple parameters files are passed.",
)
def show_taskgraph(options):
    from mozversioncontrol import get_repository_object as get_repository
    from taskgraph.parameters import Parameters, parameters_loader

    if options.pop("verbose", False):
        logging.root.setLevel(logging.DEBUG)

    repo = None
    cur_ref = None
    diffdir = None
    output_file = options["output_file"]

    if options["diff"]:
        # --root argument is taskgraph's config at <repo>/taskcluster
        repo_root = os.getcwd()
        if options["root"]:
            repo_root = f"{options['root']}/.."
        repo = get_repository(repo_root)

        if not repo.working_directory_clean():
            print(
                "abort: can't diff taskgraph with dirty working directory",
                file=sys.stderr,
            )
            return 1

        # We want to return the working directory to the current state
        # as best we can after we're done. In all known cases, using
        # branch or bookmark (which are both available on the VCS object)
        # as `branch` is preferable to a specific revision.
        cur_ref = repo.branch or repo.head_ref[:12]

        diffdir = tempfile.mkdtemp()
        atexit.register(
            shutil.rmtree, diffdir
        )  # make sure the directory gets cleaned up
        options["output_file"] = os.path.join(
            diffdir, f"{options['graph_attr']}_{cur_ref}"
        )
        print(f"Generating {options['graph_attr']} @ {cur_ref}", file=sys.stderr)

    overrides = {
        "target-kinds": options.get("target_kinds"),
    }
    parameters: list[Any[str, Parameters]] = options.pop("parameters")
    if not parameters:
        parameters = [
            parameters_loader(None, strict=False, overrides=overrides)
        ]  # will use default values

        # This is the default behaviour anyway, so no need to re-compute.
        options["force_local_files_changed"] = False

    elif options["force_local_files_changed"]:
        overrides["files-changed"] = sorted(get_locally_changed_files(GECKO))

    for param in parameters[:]:
        if isinstance(param, str) and os.path.isdir(param):
            parameters.remove(param)
            parameters.extend(
                [
                    p.as_posix()
                    for p in Path(param).iterdir()
                    if p.suffix in (".yml", ".json")
                ]
            )

    logdir = None
    if len(parameters) > 1:
        # Log to separate files for each process instead of stderr to
        # avoid interleaving.
        basename = os.path.basename(os.getcwd())
        logdir = os.path.join(appdirs.user_log_dir("taskgraph"), basename)
        if not os.path.isdir(logdir):
            os.makedirs(logdir)
    else:
        # Only setup logging if we have a single parameter spec. Otherwise
        # logging will go to files. This is also used as a hook for Gecko
        # to setup its `mach` based logging.
        setup_logging()

    generate_taskgraph(options, parameters, overrides, logdir)

    if options["diff"]:
        assert diffdir is not None
        assert repo is not None

        # Reload taskgraph modules to pick up changes and clear global state.
        for mod in sys.modules.copy():
            if (
                mod != __name__
                and mod != "taskgraph.main"
                and mod.split(".", 1)[0].endswith(("taskgraph", "mozbuild"))
            ):
                del sys.modules[mod]

        # Ensure gecko_taskgraph is ahead of taskcluster_taskgraph in sys.path.
        # Without this, we may end up validating some things against the wrong
        # schema.
        import gecko_taskgraph  # noqa

        if options["diff"] == "default":
            base_ref = repo.base_ref
        else:
            base_ref = options["diff"]

        try:
            repo.update(base_ref)
            base_ref = repo.head_ref[:12]
            options["output_file"] = os.path.join(
                diffdir, f"{options['graph_attr']}_{base_ref}"
            )
            print(f"Generating {options['graph_attr']} @ {base_ref}", file=sys.stderr)
            generate_taskgraph(options, parameters, overrides, logdir)
        finally:
            repo.update(cur_ref)

        # Generate diff(s)
        diffcmd = [
            "diff",
            "-U20",
            "--report-identical-files",
            f"--label={options['graph_attr']}@{base_ref}",
            f"--label={options['graph_attr']}@{cur_ref}",
        ]

        non_fatal_failures = []
        for spec in parameters:
            base_path = os.path.join(diffdir, f"{options['graph_attr']}_{base_ref}")
            cur_path = os.path.join(diffdir, f"{options['graph_attr']}_{cur_ref}")

            params_name = None
            if len(parameters) > 1:
                params_name = Parameters.format_spec(spec)
                base_path += f"_{params_name}"
                cur_path += f"_{params_name}"

            # If the base or cur files are missing it means that generation
            # failed. If one of them failed but not the other, the failure is
            # likely due to the patch making changes to taskgraph in modules
            # that don't get reloaded (safe to ignore). If both generations
            # failed, there's likely a real issue.
            base_missing = not os.path.isfile(base_path)
            cur_missing = not os.path.isfile(cur_path)
            if base_missing != cur_missing:  # != is equivalent to XOR for booleans
                non_fatal_failures.append(os.path.basename(base_path))
                continue

            try:
                # If the output file(s) are missing, this command will raise
                # CalledProcessError with a returncode > 1.
                proc = subprocess.run(
                    diffcmd + [base_path, cur_path],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                diff_output = proc.stdout
                returncode = 0
            except subprocess.CalledProcessError as e:
                # returncode 1 simply means diffs were found
                if e.returncode != 1:
                    print(e.stderr, file=sys.stderr)
                    raise
                diff_output = e.output
                returncode = e.returncode

            dump_output(
                diff_output,
                # Don't bother saving file if no diffs were found. Log to
                # console in this case instead.
                path=None if returncode == 0 else output_file,
                params_spec=spec if len(parameters) > 1 else None,
            )

        if non_fatal_failures:
            failstr = "\n  ".join(sorted(non_fatal_failures))
            print(
                "WARNING: Diff skipped for the following generation{s} "
                "due to failures:\n  {failstr}".format(
                    s="s" if len(non_fatal_failures) > 1 else "", failstr=failstr
                ),
                file=sys.stderr,
            )

        if options["format"] != "json":
            print(
                "If you were expecting differences in task bodies "
                'you should pass "-J"\n',
                file=sys.stderr,
            )

    if len(parameters) > 1:
        print(f"See '{logdir}' for logs", file=sys.stderr)


@command("build-image", help="Build a Docker image")
@argument("image_name", help="Name of the image to build")
@argument(
    "-t", "--tag", help="tag that the image should be built as.", metavar="name:tag"
)
@argument(
    "--context-only",
    help="File name the context tarball should be written to."
    "with this option it will only build the context.tar.",
    metavar="context.tar",
)
def build_image(args):
    from gecko_taskgraph.docker import build_context, build_image

    if args["context_only"] is None:
        build_image(args["image_name"], args["tag"], os.environ)
    else:
        build_context(args["image_name"], args["context_only"], os.environ)


@command(
    "load-image",
    help="Load a pre-built Docker image. Note that you need to "
    "have docker installed and running for this to work.",
)
@argument(
    "--task-id",
    help="Load the image at public/image.tar.zst in this task, "
    "rather than searching the index",
)
@argument(
    "-t",
    "--tag",
    help="tag that the image should be loaded as. If not "
    "image will be loaded with tag from the tarball",
    metavar="name:tag",
)
@argument(
    "image_name",
    nargs="?",
    help="Load the image of this name based on the current "
    "contents of the tree (as built for mozilla-central "
    "or mozilla-inbound)",
)
def load_image(args):
    from taskgraph.docker import load_image_by_name, load_image_by_task_id

    if not args.get("image_name") and not args.get("task_id"):
        print("Specify either IMAGE-NAME or TASK-ID")
        sys.exit(1)
    try:
        if args["task_id"]:
            ok = load_image_by_task_id(args["task_id"], args.get("tag"))
        else:
            ok = load_image_by_name(args["image_name"], args.get("tag"))
        if not ok:
            sys.exit(1)
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@command("image-digest", help="Print the digest of a docker image.")
@argument(
    "image_name",
    help="Print the digest of the image of this name based on the current "
    "contents of the tree.",
)
def image_digest(args):
    from taskgraph.docker import get_image_digest

    try:
        digest = get_image_digest(args["image_name"])
        print(digest)
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@command("decision", help="Run the decision task")
@argument("--root", "-r", help="root of the taskgraph definition relative to topsrcdir")
@argument(
    "--message",
    required=False,
    help=argparse.SUPPRESS,
)
@argument(
    "--project",
    required=True,
    help="Project to use for creating task graph. Example: --project=try",
)
@argument("--pushlog-id", dest="pushlog_id", required=True, default="0")
@argument("--pushdate", dest="pushdate", required=True, type=int, default=0)
@argument("--owner", required=True, help="email address of who owns this graph")
@argument("--level", required=True, help="SCM level of this repository")
@argument(
    "--target-tasks-method", help="method for selecting the target tasks to generate"
)
@argument(
    "--repository-type",
    required=True,
    help='Type of repository, either "hg" or "git"',
)
@argument("--base-repository", required=True, help='URL for "base" repository to clone')
@argument(
    "--base-ref", default="", help='Reference of the revision in the "base" repository'
)
@argument(
    "--base-rev",
    default="",
    help="Taskgraph decides what to do based on the revision range between "
    "`--base-rev` and `--head-rev`. Value is determined automatically if not provided",
)
@argument(
    "--head-repository",
    required=True,
    help='URL for "head" repository to fetch revision from',
)
@argument(
    "--head-ref", required=True, help="Reference (this is same as rev usually for hg)"
)
@argument(
    "--head-rev", required=True, help="Commit revision to use from head repository"
)
@argument("--head-tag", help="Tag attached to the revision", default="")
@argument(
    "--tasks-for", required=True, help="the tasks_for value used to generate this task"
)
@argument("--try-task-config-file", help="path to try task configuration file")
def decision(options):
    from gecko_taskgraph.decision import taskgraph_decision

    taskgraph_decision(options)


@command("action-callback", description="Run action callback used by action tasks")
@argument(
    "--root",
    "-r",
    default="taskcluster",
    help="root of the taskgraph definition relative to topsrcdir",
)
def action_callback(options):
    from taskgraph.util import json

    from gecko_taskgraph.actions import trigger_action_callback
    from gecko_taskgraph.actions.util import get_parameters

    try:
        # the target task for this action (or null if it's a group action)
        task_id = json.loads(os.environ.get("ACTION_TASK_ID", "null"))
        # the target task group for this action
        task_group_id = os.environ.get("ACTION_TASK_GROUP_ID", None)
        input = json.loads(os.environ.get("ACTION_INPUT", "null"))
        callback = os.environ.get("ACTION_CALLBACK", None)
        root = options["root"]

        parameters = get_parameters(task_group_id)

        return trigger_action_callback(
            task_group_id=task_group_id,
            task_id=task_id,
            input=input,
            callback=callback,
            parameters=parameters,
            root=root,
            test=False,
        )
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@command("test-action-callback", description="Run an action callback in a testing mode")
@argument(
    "--root",
    "-r",
    default="taskcluster",
    help="root of the taskgraph definition relative to topsrcdir",
)
@argument(
    "--parameters",
    "-p",
    default="",
    help="parameters file (.yml or .json; see " "`taskcluster/docs/parameters.rst`)`",
)
@argument("--task-id", default=None, help="TaskId to which the action applies")
@argument(
    "--task-group-id", default=None, help="TaskGroupId to which the action applies"
)
@argument("--input", default=None, help="Action input (.yml or .json)")
@argument("callback", default=None, help="Action callback name (Python function name)")
def test_action_callback(options):
    import taskgraph.parameters
    from taskgraph.config import load_graph_config
    from taskgraph.util import json, yaml

    import gecko_taskgraph.actions

    def load_data(filename):
        with open(filename) as f:
            if filename.endswith(".yml"):
                return yaml.load_stream(f)
            if filename.endswith(".json"):
                return json.load(f)
            raise Exception(f"unknown filename {filename}")

    try:
        task_id = options["task_id"]

        if options["input"]:
            input = load_data(options["input"])
        else:
            input = None

        root = options["root"]
        graph_config = load_graph_config(root)
        trust_domain = graph_config["trust-domain"]
        graph_config.register()

        parameters = taskgraph.parameters.load_parameters_file(
            options["parameters"], strict=False, trust_domain=trust_domain
        )
        parameters.check()

        return gecko_taskgraph.actions.trigger_action_callback(
            task_group_id=options["task_group_id"],
            task_id=task_id,
            input=input,
            callback=options["callback"],
            parameters=parameters,
            root=root,
            test=True,
        )
    except Exception:
        traceback.print_exc()
        sys.exit(1)


def create_parser():
    parser = argparse.ArgumentParser(description="Interact with taskgraph")
    subparsers = parser.add_subparsers()
    for _, (func, args, kwargs, defaults) in commands.items():
        subparser = subparsers.add_parser(*args, **kwargs)
        for arg in func.args:
            subparser.add_argument(*arg[0], **arg[1])
        subparser.set_defaults(command=func, **defaults)
    return parser


def setup_logging():
    logging.basicConfig(
        format="%(asctime)s - %(levelname)s - %(message)s", level=logging.INFO
    )


def main(args=sys.argv[1:]):
    setup_logging()
    parser = create_parser()
    args = parser.parse_args(args)
    try:
        args.command(vars(args))
    except Exception:
        traceback.print_exc()
        sys.exit(1)
