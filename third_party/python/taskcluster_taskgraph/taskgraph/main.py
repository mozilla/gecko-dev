# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import atexit
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
from collections import namedtuple
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from textwrap import dedent
from typing import Any, List
from urllib.parse import urlparse

import appdirs
import yaml

Command = namedtuple("Command", ["func", "args", "kwargs", "defaults"])
commands = {}


def command(*args, **kwargs):
    defaults = kwargs.pop("defaults", {})

    def decorator(func):
        commands[args[0]] = Command(func, args, kwargs, defaults)
        return func

    return decorator


def argument(*args, **kwargs):
    def decorator(func):
        if not hasattr(func, "args"):
            func.args = []
        func.args.append((args, kwargs))
        return func

    return decorator


def format_taskgraph_labels(taskgraph):
    return "\n".join(
        sorted(
            taskgraph.tasks[index].label for index in taskgraph.graph.visit_postorder()
        )
    )


def format_taskgraph_json(taskgraph):
    from taskgraph.util import json

    return json.dumps(taskgraph.to_json(), sort_keys=True, indent=2)


def format_taskgraph_yaml(taskgraph):
    return yaml.safe_dump(taskgraph.to_json(), default_flow_style=False)


def get_filtered_taskgraph(taskgraph, tasksregex, exclude_keys):
    """
    Filter all the tasks on basis of a regular expression
    and returns a new TaskGraph object
    """
    from taskgraph.graph import Graph
    from taskgraph.task import Task
    from taskgraph.taskgraph import TaskGraph

    if tasksregex:
        named_links_dict = taskgraph.graph.named_links_dict()
        filteredtasks = {}
        filterededges = set()
        regexprogram = re.compile(tasksregex)

        for key in taskgraph.graph.visit_postorder():
            task = taskgraph.tasks[key]
            if regexprogram.match(task.label):
                filteredtasks[key] = task
                for depname, dep in named_links_dict[key].items():
                    if regexprogram.match(dep):
                        filterededges.add((key, dep, depname))

        taskgraph = TaskGraph(
            filteredtasks, Graph(frozenset(filteredtasks), frozenset(filterededges))
        )

    if exclude_keys:
        for label, task in taskgraph.tasks.items():
            task = task.to_json()
            for key in exclude_keys:
                obj = task
                attrs = key.split(".")
                while obj and attrs[0] in obj:
                    if len(attrs) == 1:
                        del obj[attrs[0]]
                        break
                    obj = obj[attrs[0]]
                    attrs = attrs[1:]
            taskgraph.tasks[label] = Task.from_json(task)

    return taskgraph


FORMAT_METHODS = {
    "labels": format_taskgraph_labels,
    "json": format_taskgraph_json,
    "yaml": format_taskgraph_yaml,
}


def get_taskgraph_generator(root, parameters):
    """Helper function to make testing a little easier."""
    from taskgraph.generator import TaskGraphGenerator

    return TaskGraphGenerator(root_dir=root, parameters=parameters)


def format_taskgraph(options, parameters, overrides, logfile=None):
    import taskgraph
    from taskgraph.parameters import parameters_loader

    if logfile:
        handler = logging.FileHandler(logfile, mode="w")
        if logging.root.handlers:
            oldhandler = logging.root.handlers[-1]
            logging.root.removeHandler(oldhandler)
            handler.setFormatter(oldhandler.formatter)
        logging.root.addHandler(handler)

    if options["fast"]:
        taskgraph.fast = True

    if isinstance(parameters, str):
        parameters = parameters_loader(
            parameters,
            overrides=overrides,
            strict=False,
        )

    tgg = get_taskgraph_generator(options.get("root"), parameters)

    tg = getattr(tgg, options["graph_attr"])
    tg = get_filtered_taskgraph(tg, options["tasks_regex"], options["exclude_keys"])
    format_method = FORMAT_METHODS[options["format"] or "labels"]
    return format_method(tg)


def dump_output(out, path=None, params_spec=None):
    from taskgraph.parameters import Parameters

    params_name = Parameters.format_spec(params_spec)
    fh = None
    if path:
        # Substitute params name into file path if necessary
        if params_spec and "{params}" not in path:
            name, ext = os.path.splitext(path)
            name += "_{params}"
            path = name + ext

        path = path.format(params=params_name)
        fh = open(path, "w")
    else:
        print(
            f"Dumping result with parameters from {params_name}:",
            file=sys.stderr,
        )
    print(out + "\n", file=fh)


def generate_taskgraph(options, parameters, overrides, logdir):
    from taskgraph.parameters import Parameters

    def logfile(spec):
        """Determine logfile given a parameters specification."""
        if logdir is None:
            return None
        return os.path.join(
            logdir,
            "{}_{}.log".format(options["graph_attr"], Parameters.format_spec(spec)),
        )

    # Don't bother using futures if there's only one parameter. This can make
    # tracebacks a little more readable and avoids additional process overhead.
    if len(parameters) == 1:
        spec = parameters[0]
        out = format_taskgraph(options, spec, overrides, logfile(spec))
        dump_output(out, options["output_file"])
        return 0

    futures = {}
    with ProcessPoolExecutor(max_workers=options["max_workers"]) as executor:
        for spec in parameters:
            f = executor.submit(
                format_taskgraph, options, spec, overrides, logfile(spec)
            )
            futures[f] = spec

    returncode = 0
    for future in as_completed(futures):
        output_file = options["output_file"]
        spec = futures[future]
        e = future.exception()
        if e:
            returncode = 1
            out = "".join(traceback.format_exception(type(e), e, e.__traceback__))
            if options["diff"]:
                # Dump to console so we don't accidentally diff the tracebacks.
                output_file = None
        else:
            out = future.result()

        dump_output(
            out,
            path=output_file,
            params_spec=spec if len(parameters) > 1 else None,
        )

    return returncode


@command(
    "tasks",
    help="Show the full task set in the task graph. The full task set includes all tasks defined by any kind, without edges (dependencies) between them.",
    defaults={"graph_attr": "full_task_set"},
)
@command(
    "full",
    help="Show the full task graph. The full task graph consists of the full task set, with edges (dependencies) between tasks.",
    defaults={"graph_attr": "full_task_graph"},
)
@command(
    "target",
    help="Show the target task set in the task graph. The target task set includes the tasks which have indicated they should be run, without edges (dependencies) between them.",
    defaults={"graph_attr": "target_task_set"},
)
@command(
    "target-graph",
    help="Show the target task graph. The target task graph consists of the target task set, with edges (dependencies) between tasks.",
    defaults={"graph_attr": "target_task_graph"},
)
@command(
    "optimized",
    help="Show the optimized task graph, which is the target task set with tasks optimized out (filtered, omitted, or replaced) and edges representing dependencies.",
    defaults={"graph_attr": "optimized_task_graph"},
)
@command(
    "morphed",
    help="Show the morphed graph, which is the optimized task graph with additional morphs applied. It retains the same meaning as the optimized task graph but in a form more palatable to TaskCluster.",
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
    help="only return tasks with labels matching this regular expression.",
)
@argument(
    "--exclude-key",
    default=None,
    dest="exclude_keys",
    action="append",
    help="Exclude the specified key (using dot notation) from the final result. "
    "This is mainly useful with '--diff' to filter out expected differences. Can be "
    "used multiple times.",
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
    from taskgraph.parameters import Parameters, parameters_loader
    from taskgraph.util.vcs import get_repository

    if options.pop("verbose", False):
        logging.root.setLevel(logging.DEBUG)

    repo = None
    cur_rev = None
    diffdir = None
    output_file = options["output_file"]

    if options["diff"] or options["force_local_files_changed"]:
        repo = get_repository(os.getcwd())

    if options["diff"]:
        assert repo is not None
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
        cur_rev = repo.branch or repo.head_rev[:12]
        cur_rev_file = cur_rev.replace("/", "_")

        diffdir = tempfile.mkdtemp()
        atexit.register(
            shutil.rmtree, diffdir
        )  # make sure the directory gets cleaned up
        options["output_file"] = os.path.join(
            diffdir, f"{options['graph_attr']}_{cur_rev_file}"
        )
        print(f"Generating {options['graph_attr']} @ {cur_rev}", file=sys.stderr)

    overrides = {
        "target-kinds": options.get("target_kinds"),
    }
    parameters: List[Any[str, Parameters]] = options.pop("parameters")
    if not parameters:
        parameters = [
            parameters_loader(None, strict=False, overrides=overrides)
        ]  # will use default values

        # This is the default behaviour anyway, so no need to re-compute.
        options["force_local_files_changed"] = False

    elif options["force_local_files_changed"]:
        assert repo is not None
        overrides["files-changed"] = sorted(repo.get_changed_files("AM"))

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

    ret = generate_taskgraph(options, parameters, overrides, logdir)

    if options["diff"]:
        assert diffdir is not None
        assert repo is not None

        # Reload taskgraph modules to pick up changes and clear global state.
        for mod in sys.modules.copy():
            if mod != __name__ and mod.split(".", 1)[0].endswith("taskgraph"):
                del sys.modules[mod]

        if options["diff"] == "default":
            base_rev = repo.base_rev
        else:
            base_rev = options["diff"]
        base_rev_file = base_rev.replace("/", "_")

        try:
            repo.update(base_rev)
            base_rev = repo.head_rev[:12]
            options["output_file"] = os.path.join(
                diffdir, f"{options['graph_attr']}_{base_rev_file}"
            )
            print(f"Generating {options['graph_attr']} @ {base_rev}", file=sys.stderr)
            ret |= generate_taskgraph(options, parameters, overrides, logdir)
        finally:
            assert cur_rev
            repo.update(cur_rev)

        # Generate diff(s)
        diffcmd = [
            "diff",
            "-U20",
            "--report-identical-files",
            f"--label={options['graph_attr']}@{base_rev}",
            f"--label={options['graph_attr']}@{cur_rev}",
        ]

        non_fatal_failures = []

        for spec in parameters:
            base_path = os.path.join(
                diffdir, f"{options['graph_attr']}_{base_rev_file}"
            )
            cur_path = os.path.join(diffdir, f"{options['graph_attr']}_{cur_rev_file}")  # type: ignore

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

    return ret


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
    from taskgraph.docker import build_context, build_image

    validate_docker()
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
    validate_docker()
    try:
        if args["task_id"]:
            tag = load_image_by_task_id(args["task_id"], args.get("tag"))
        else:
            tag = load_image_by_name(args["image_name"], args.get("tag"))
        if not tag:
            sys.exit(1)
    except Exception:
        traceback.print_exc()
        sys.exit(1)


def validate_docker():
    p = subprocess.run(["docker", "ps"], capture_output=True)
    if p.returncode != 0:
        print("Error connecting to Docker:", p.stderr)
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


@command(
    "load-task",
    help="Loads a pre-built Docker image and drops you into a container with "
    "the same environment variables and run-task setup as the specified task. "
    "The task's payload.command will be replaced with 'bash'. You need to have "
    "docker installed and running for this to work.",
)
@argument("task_id", help="The task id to load into a docker container.")
@argument(
    "--keep",
    dest="remove",
    action="store_false",
    default=True,
    help="Keep the docker container after exiting.",
)
@argument("--user", default=None, help="Container user to start shell with.")
def load_task(args):
    from taskgraph.docker import load_task

    validate_docker()
    return load_task(args["task_id"], remove=args["remove"], user=args["user"])


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
@argument(
    "--verbose", "-v", action="store_true", help="include debug-level logging output"
)
def decision(options):
    from taskgraph.decision import taskgraph_decision

    taskgraph_decision(options)


@command("actions", help="Print the rendered actions.json")
@argument(
    "--root",
    "-r",
    help="root of the taskgraph definition relative to topsrcdir",
    default="taskcluster",
)
@argument(
    "--verbose",
    "-v",
    action="store_true",
    help="include debug-level logging output",
)
@argument(
    "--parameters",
    "-p",
    default="",
    help="parameters file (.yml or .json; see `taskcluster/docs/parameters.rst`)`",
)
def actions(args):
    from taskgraph.actions import render_actions_json
    from taskgraph.generator import TaskGraphGenerator
    from taskgraph.parameters import parameters_loader
    from taskgraph.util import json

    if args.pop("verbose", False):
        logging.root.setLevel(logging.DEBUG)

    try:
        parameters = parameters_loader(args["parameters"], strict=False)
        tgg = TaskGraphGenerator(root_dir=args.get("root"), parameters=parameters)

        actions = render_actions_json(tgg.parameters, tgg.graph_config, "DECISION-TASK")
        print(json.dumps(actions, sort_keys=True, indent=2))
    except Exception:
        traceback.print_exc()
        sys.exit(1)

    return 0


@command("action-callback", description="Run action callback used by action tasks")
@argument(
    "--root",
    "-r",
    default="taskcluster",
    help="root of the taskgraph definition relative to topsrcdir",
)
def action_callback(options):
    from taskgraph.actions import trigger_action_callback
    from taskgraph.actions.util import get_parameters
    from taskgraph.util import json

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
    help="parameters file (.yml or .json; see `taskcluster/docs/parameters.rst`)`",
)
@argument("--task-id", default=None, help="TaskId to which the action applies")
@argument(
    "--task-group-id", default=None, help="TaskGroupId to which the action applies"
)
@argument("--input", default=None, help="Action input (.yml or .json)")
@argument("callback", default=None, help="Action callback name (Python function name)")
def test_action_callback(options):
    import taskgraph.actions
    import taskgraph.parameters
    from taskgraph.config import load_graph_config
    from taskgraph.util import json, yaml

    def load_data(filename):
        with open(filename) as f:
            if filename.endswith(".yml"):
                return yaml.load_stream(f)
            elif filename.endswith(".json"):
                return json.load(f)
            else:
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

        return taskgraph.actions.trigger_action_callback(
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


@command(
    "init", description="Initialize a new Taskgraph setup in a new or existing project."
)
@argument(
    "-f",
    "--force",
    action="store_true",
    default=False,
    help="Bypass safety checks.",
)
@argument(
    "--prompt",
    dest="no_input",
    action="store_false",
    default=True,
    help="Prompt for input rather than using default values (advanced).",
)
@argument(
    "--template",
    default="gh:taskcluster/taskgraph",
    help=argparse.SUPPRESS,  # used for testing
)
def init_taskgraph(options):
    from cookiecutter.main import cookiecutter

    import taskgraph
    from taskgraph.util.vcs import get_repository

    repo = get_repository(os.getcwd())
    root = Path(repo.path)

    # Clean up existing installations if necessary.
    tc_yml = root.joinpath(".taskcluster.yml")
    if tc_yml.is_file():
        if not options["force"]:
            proceed = input(
                "A Taskcluster setup already exists in this repository, "
                "would you like to overwrite it? [y/N]: "
            ).lower()
            while proceed not in ("y", "yes", "n", "no"):
                proceed = input(f"Invalid option '{proceed}'! Try again: ")

            if proceed[0] == "n":
                sys.exit(1)

        tc_yml.unlink()
        tg_dir = root.joinpath("taskcluster")
        if tg_dir.is_dir():
            shutil.rmtree(tg_dir)

    # Populate some defaults from the current repository.
    context = {"project_name": root.name, "taskgraph_version": taskgraph.__version__}

    try:
        repo_url = repo.get_url(remote=repo.remote_name)
    except RuntimeError:
        repo_url = ""

    if repo.tool == "git" and "github.com" in repo_url:
        context["repo_host"] = "github"
    else:
        print(
            dedent(
                """\
            Repository not supported!

            The `taskgraph init` command only supports repositories hosted on
            Github. Ensure you use a remote that points to a Github repository.
            """
            ),
            file=sys.stderr,
        )
        return 1

    context["repo_name"] = urlparse(repo_url).path.rsplit("/", 1)[-1]
    if context["repo_name"].endswith(".git"):
        context["repo_name"] = context["repo_name"][: -len(".git")]

    # Generate the project.
    cookiecutter(
        options["template"],
        checkout=taskgraph.__version__,
        directory="template",
        extra_context=context,
        no_input=options["no_input"],
        output_dir=str(root.parent),
        overwrite_if_exists=True,
    )


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

    if not args:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args(args)
    try:
        return args.command(vars(args))
    except Exception:
        traceback.print_exc()
        sys.exit(1)
