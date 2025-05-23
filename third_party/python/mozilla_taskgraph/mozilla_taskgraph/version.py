import os

from taskgraph.util.vcs import get_repository


def default_parser(params):
    repo_root = get_repository(os.getcwd()).path

    with open(os.path.join(repo_root, "version.txt")) as f:
        return f.read().strip()
