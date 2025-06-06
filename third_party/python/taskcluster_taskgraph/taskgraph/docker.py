# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import shlex
import subprocess
import tarfile
import tempfile
from io import BytesIO
from textwrap import dedent
from typing import List, Optional

try:
    import zstandard as zstd
except ImportError as e:
    zstd = e

from taskgraph.util import docker, json
from taskgraph.util.taskcluster import (
    get_artifact_url,
    get_root_url,
    get_session,
    get_task_definition,
)

DEPLOY_WARNING = """
*****************************************************************
WARNING: Image is not suitable for deploying/pushing.

To automatically tag the image the following files are required:
- {image_dir}/REGISTRY
- {image_dir}/VERSION

The REGISTRY file contains the Docker registry hosting the image.
A default REGISTRY file may also be defined in the parent docker
directory.

The VERSION file contains the version of the image.
*****************************************************************
"""


def get_image_digest(image_name):
    from taskgraph.generator import load_tasks_for_kind
    from taskgraph.parameters import Parameters

    params = Parameters(
        level=os.environ.get("MOZ_SCM_LEVEL", "3"),
        strict=False,
    )
    tasks = load_tasks_for_kind(params, "docker-image")
    task = tasks[f"docker-image-{image_name}"]
    return task.attributes["cached_task"]["digest"]


def load_image_by_name(image_name, tag=None):
    from taskgraph.generator import load_tasks_for_kind
    from taskgraph.optimize.strategies import IndexSearch
    from taskgraph.parameters import Parameters

    params = Parameters(
        level=os.environ.get("MOZ_SCM_LEVEL", "3"),
        strict=False,
    )
    tasks = load_tasks_for_kind(params, "docker-image")
    task = tasks[f"docker-image-{image_name}"]

    indexes = task.optimization.get("index-search", [])
    task_id = IndexSearch().should_replace_task(task, {}, None, indexes)

    if task_id in (True, False):
        print(
            "Could not find artifacts for a docker image "
            "named `{image_name}`. Local commits and other changes "
            "in your checkout may cause this error. Try "
            "updating to a fresh checkout of {project} "
            "to download image.".format(
                image_name=image_name, project=params["project"]
            )
        )
        return False

    return load_image_by_task_id(task_id, tag)


def load_image_by_task_id(task_id, tag=None):
    artifact_url = get_artifact_url(task_id, "public/image.tar.zst")
    result = load_image(artifact_url, tag)
    print("Found docker image: {}:{}".format(result["image"], result["tag"]))
    if tag:
        print(f"Re-tagged as: {tag}")
    else:
        tag = "{}:{}".format(result["image"], result["tag"])
    print(f"Try: docker run -ti --rm {tag} bash")
    return tag


def build_context(name, outputFile, args=None):
    """Build a context.tar for image with specified name."""
    if not name:
        raise ValueError("must provide a Docker image name")
    if not outputFile:
        raise ValueError("must provide a outputFile")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception(f"image directory does not exist: {image_dir}")

    docker.create_context_tar(".", image_dir, outputFile, args)


def build_image(name, tag, args=None):
    """Build a Docker image of specified name.

    Output from image building process will be printed to stdout.
    """
    if not name:
        raise ValueError("must provide a Docker image name")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception(f"image directory does not exist: {image_dir}")

    tag = tag or docker.docker_image(name, by_tag=True)

    buf = BytesIO()
    docker.stream_context_tar(".", image_dir, buf, args)
    cmdargs = ["docker", "image", "build", "--no-cache", "-"]
    if tag:
        cmdargs.insert(-1, f"-t={tag}")
    subprocess.run(cmdargs, input=buf.getvalue(), check=True)

    msg = f"Successfully built {name}"
    if tag:
        msg += f" and tagged with {tag}"
    print(msg)

    if not tag or tag.endswith(":latest"):
        print(DEPLOY_WARNING.format(image_dir=os.path.relpath(image_dir), image=name))


def load_image(url, imageName=None, imageTag=None):
    """
    Load docker image from URL as imageName:tag, if no imageName or tag is given
    it will use whatever is inside the zstd compressed tarball.

    Returns an object with properties 'image', 'tag' and 'layer'.
    """
    if isinstance(zstd, ImportError):
        raise ImportError(
            dedent(
                """
                zstandard is not installed! Use `pip install taskcluster-taskgraph[load-image]`
                to use this feature.
                """
            )
        ) from zstd

    # If imageName is given and we don't have an imageTag
    # we parse out the imageTag from imageName, or default it to 'latest'
    # if no imageName and no imageTag is given, 'repositories' won't be rewritten
    if imageName and not imageTag:
        if ":" in imageName:
            imageName, imageTag = imageName.split(":", 1)
        else:
            imageTag = "latest"

    info = {}

    def download_and_modify_image():
        # This function downloads and edits the downloaded tar file on the fly.
        # It emits chunked buffers of the edited tar file, as a generator.
        print(f"Downloading from {url}")
        # get_session() gets us a requests.Session set to retry several times.
        req = get_session().get(url, stream=True)
        req.raise_for_status()

        with zstd.ZstdDecompressor().stream_reader(req.raw) as ifh:  # type: ignore
            tarin = tarfile.open(
                mode="r|",
                fileobj=ifh,
                bufsize=zstd.DECOMPRESSION_RECOMMENDED_OUTPUT_SIZE,  # type: ignore
            )

            # Stream through each member of the downloaded tar file individually.
            for member in tarin:
                # Non-file members only need a tar header. Emit one.
                if not member.isfile():
                    yield member.tobuf(tarfile.GNU_FORMAT)
                    continue

                # Open stream reader for the member
                reader = tarin.extractfile(member)

                # If member is `repositories`, we parse and possibly rewrite the
                # image tags.
                if member.name == "repositories":
                    # Read and parse repositories
                    repos = json.loads(reader.read())  # type: ignore
                    reader.close()  # type: ignore

                    # If there is more than one image or tag, we can't handle it
                    # here.
                    if len(repos.keys()) > 1:
                        raise Exception("file contains more than one image")
                    info["image"] = image = list(repos.keys())[0]
                    if len(repos[image].keys()) > 1:
                        raise Exception("file contains more than one tag")
                    info["tag"] = tag = list(repos[image].keys())[0]
                    info["layer"] = layer = repos[image][tag]

                    # Rewrite the repositories file
                    data = json.dumps({imageName or image: {imageTag or tag: layer}})
                    reader = BytesIO(data.encode("utf-8"))
                    member.size = len(data)

                # Emit the tar header for this member.
                yield member.tobuf(tarfile.GNU_FORMAT)
                # Then emit its content.
                remaining = member.size
                while remaining:
                    length = min(remaining, zstd.DECOMPRESSION_RECOMMENDED_OUTPUT_SIZE)  # type: ignore
                    buf = reader.read(length)  # type: ignore
                    remaining -= len(buf)
                    yield buf
                # Pad to fill a 512 bytes block, per tar format.
                remainder = member.size % 512
                if remainder:
                    yield ("\0" * (512 - remainder)).encode("utf-8")

                reader.close()  # type: ignore

    subprocess.run(
        ["docker", "image", "load"], input=b"".join(download_and_modify_image())
    )

    # Check that we found a repositories file
    if not info.get("image") or not info.get("tag") or not info.get("layer"):
        raise Exception("No repositories file found!")

    return info


def _index(l: List, s: str) -> Optional[int]:
    try:
        return l.index(s)
    except ValueError:
        pass


def load_task(task_id, remove=True, user=None):
    user = user or "worker"
    task_def = get_task_definition(task_id)

    if (
        impl := task_def.get("tags", {}).get("worker-implementation")
    ) != "docker-worker":
        print(f"Tasks with worker-implementation '{impl}' are not supported!")
        return 1

    command = task_def["payload"].get("command")
    if not command or not command[0].endswith("run-task"):
        print("Only tasks using `run-task` are supported!")
        return 1

    # Remove the payload section of the task's command. This way run-task will
    # set up the task (clone repos, download fetches, etc) but won't actually
    # start the core of the task. Instead we'll drop the user into an interactive
    # shell and provide the ability to resume the task command.
    task_command = None
    if index := _index(command, "--"):
        task_command = shlex.join(command[index + 1 :])
        # I attempted to run the interactive bash shell here, but for some
        # reason when executed through `run-task`, the interactive shell
        # doesn't work well. There's no shell prompt on newlines and tab
        # completion doesn't work. That's why it is executed outside of
        # `run-task` below, and why we need to parse `--task-cwd`.
        command[index + 1 :] = [
            "echo",
            "Task setup complete!\nRun `exec-task` to execute the task's command.",
        ]

    # Parse `--task-cwd` so we know where to execute the task's command later.
    if index := _index(command, "--task-cwd"):
        task_cwd = command[index + 1]
    else:
        for arg in command:
            if arg.startswith("--task-cwd="):
                task_cwd = arg.split("=", 1)[1]
                break
        else:
            task_cwd = "$TASK_WORKDIR"

    image_task_id = task_def["payload"]["image"]["taskId"]
    image_tag = load_image_by_task_id(image_task_id)

    # Set some env vars the worker would normally set.
    env = {
        "RUN_ID": "0",
        "TASK_GROUP_ID": task_def.get("taskGroupId", ""),
        "TASK_ID": task_id,
        "TASKCLUSTER_ROOT_URL": get_root_url(False),
    }
    # Add the task's environment variables.
    env.update(task_def["payload"].get("env", {}))

    envfile = None
    initfile = None
    try:
        command = [
            "docker",
            "run",
            "-it",
            image_tag,
            "bash",
            "-c",
            f"{shlex.join(command)} && cd $TASK_WORKDIR && su -p {user}",
        ]

        if remove:
            command.insert(2, "--rm")

        if env:
            envfile = tempfile.NamedTemporaryFile("w+", delete=False)
            envfile.write("\n".join([f"{k}={v}" for k, v in env.items()]))
            envfile.close()

            command.insert(2, f"--env-file={envfile.name}")

        if task_command:
            initfile = tempfile.NamedTemporaryFile("w+", delete=False)
            initfile.write(
                dedent(
                    f"""
            function exec-task() {{
                echo "Starting task: {task_command}";
                pushd {task_cwd};
                {task_command};
                popd
            }}
            """
                ).lstrip()
            )
            initfile.close()

            command[2:2] = ["-v", f"{initfile.name}:/builds/worker/.bashrc"]

        proc = subprocess.run(command)
    finally:
        if envfile:
            os.remove(envfile.name)

        if initfile:
            os.remove(initfile.name)

    return proc.returncode
