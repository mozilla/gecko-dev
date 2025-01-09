# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import atexit
import os
import re

from filter_git_changes import filter_git_changes
from run_operations import get_last_line, run_hg, run_shell, update_resume_state

# This script vendors moz-libwebrtc, handles add/deletes/renames and
# commits the newly vendored code with the provided commit message.

script_name = os.path.basename(__file__)


class ErrorHelp:
    def __init__(self, help_string):
        self.help_string = help_string

    def set_help(self, help_string):
        self.help_string = help_string

    def show_help(self):
        if self.help_string is not None:
            print(self.help_string)
            print(f"Please resolve the error and then continue running {script_name}")


error_help = ErrorHelp(None)


def early_exit_handler():
    print(f"*** ERROR *** {script_name} did not complete successfully")
    error_help.show_help()


def log_output_lines(lines, log_dir, filename):
    if len(lines) == 0:
        return

    with open(os.path.join(log_dir, filename), "w") as ofile:
        for line in lines:
            ofile.write(line)
            ofile.write("\n")


def vendor_current_stack(github_branch, github_path, script_dir):
    print("-------")
    print(f"------- Vendor {github_branch} from {github_path}")
    print("-------")
    cmd = [
        "./mach",
        "python",
        f"{script_dir}/vendor-libwebrtc.py",
        "--from-local",
        github_path,
        "--commit",
        github_branch,
        "libwebrtc",
    ]
    cmd = " ".join(cmd)
    run_shell(cmd)


def restore_mozbuild_files(target_dir, log_dir):
    print("-------")
    print("------- Restore moz.build files from repo")
    print("-------")
    cmd = f'hg revert --include "{target_dir}/**moz.build" {target_dir}'
    stdout_lines = run_shell(cmd)  # run_shell to allow file wildcard
    log_output_lines(stdout_lines, log_dir, "log-regen-mozbuild-files.txt")


def remove_deleted_upstream_files(
    github_path, github_sha, target_dir, log_dir, handle_noop_commit
):
    if handle_noop_commit:
        return
    deleted_paths = filter_git_changes(github_path, github_sha, "D")
    deleted_paths = [re.sub("^.\t", "", x) for x in deleted_paths]
    deleted_paths = [os.path.join(target_dir, x) for x in deleted_paths]
    if len(deleted_paths) != 0:
        print("-------")
        print("------- Remove deleted upstream files")
        print("-------")
        cmd = f"hg rm {' '.join(deleted_paths)}"
        stdout_lines = run_hg(cmd)
        log_output_lines(stdout_lines, log_dir, "log-deleted-upstream-files.txt")


def add_new_upstream_files(
    github_path, github_sha, target_dir, log_dir, handle_noop_commit
):
    if handle_noop_commit:
        return
    added_paths = filter_git_changes(github_path, github_sha, "A")
    added_paths = [re.sub("^.\t", "", x) for x in added_paths]
    added_paths = [os.path.join(target_dir, x) for x in added_paths]
    if len(added_paths) != 0:
        print("-------")
        print("------- Add new upstream files")
        print("-------")
        cmd = f"hg add {' '.join(added_paths)}"
        stdout_lines = run_hg(cmd)
        log_output_lines(stdout_lines, log_dir, "log-new-upstream-files.txt")


def handle_renamed_upstream_files(
    github_path, github_sha, target_dir, log_dir, handle_noop_commit
):
    if handle_noop_commit:
        return
    renamed_paths = filter_git_changes(github_path, github_sha, "R")
    renamed_paths = [re.sub("^.[0-9]+\t", "", x) for x in renamed_paths]
    renamed_paths = [x.split("\t") for x in renamed_paths]
    renamed_paths = [
        " ".join([os.path.join(target_dir, file) for file in rename_pair])
        for rename_pair in renamed_paths
    ]
    if len(renamed_paths) != 0:
        print("-------")
        print("------- Handle renamed upstream files")
        print("-------")
        for x in renamed_paths:
            cmd = f"hg rename --after {x}"
            stdout_lines = run_hg(cmd)
            log_output_lines(stdout_lines, log_dir, "log-renamed-upstream-files.txt")


def commit_all_changes(github_sha, commit_msg_filename, target_dir):
    print("-------")
    print(f"------- Commit vendored changes from {github_sha}")
    print("-------")
    cmd = f"hg commit -l {commit_msg_filename} {target_dir}"
    run_hg(cmd)


def vendor_and_commit(
    script_dir,
    github_path,
    github_branch,
    github_sha,
    target_dir,
    state_dir,
    log_dir,
    commit_msg_filename,
):
    # register the exit handler after the arg parser completes so '--help' doesn't exit with
    # an error.
    atexit.register(early_exit_handler)

    print(f"script_dir: {script_dir}")
    print(f"github_path: {github_path}")
    print(f"github_branch: {github_branch}")
    print(f"github_sha: {github_sha}")
    print(f"target_dir: {target_dir}")
    print(f"state_dir: {state_dir}")
    print(f"log_dir: {log_dir}")
    print(f"commit_msg_filename: {commit_msg_filename}")

    resume_state_filename = os.path.join(state_dir, "vendor_and_commit.resume")

    noop_commit_path = os.path.join(state_dir, f"{github_sha}.no-op-cherry-pick-msg")
    handle_noop_commit = os.path.exists(noop_commit_path)
    print(f"noop_commit_path: {noop_commit_path}")
    print(f"handle_noop_commit: {handle_noop_commit}")
    if handle_noop_commit:
        print("***")
        print("*** Detected special commit msg, setting handle_noop_commit.")
        print("*** This commit is flagged as having been handled by a")
        print("*** previous commit, meaning the changed file count between")
        print("*** upstream and our vendored commit will not match. The")
        print("*** commit message is annotated with info on the previous")
        print("*** commit.")
        print("***")

    resume_state = ""
    if os.path.exists(resume_state_filename):
        resume_state = get_last_line(resume_state_filename).strip()
    print(f"resume_state: '{resume_state}'")

    if len(resume_state) == 0:
        update_resume_state("resume2", resume_state_filename)
        error_help.set_help(
            (
                f"Running script '{script_dir}/vendor-libwebrtc.py' failed.\n"
                f"Please manually confirm that all changes from git ({github_path})\n"
                "are reflected in the output of 'hg diff'"
            )
        )
        vendor_current_stack(github_branch, github_path, script_dir)
        error_help.set_help(None)

    if len(resume_state) == 0 or resume_state == "resume2":
        resume_state = ""
        update_resume_state("resume3", resume_state_filename)
        error_help.set_help(
            "An error occurred while restoring moz.build files after vendoring.\n"
            "Verify no moz.build files are modified, missing, or changed."
        )
        restore_mozbuild_files(target_dir, log_dir)
        error_help.set_help(None)

    if len(resume_state) == 0 or resume_state == "resume3":
        resume_state = ""
        update_resume_state("resume4", resume_state_filename)
        error_help.set_help(
            "An error occurred while removing deleted upstream files.\n"
            "Verify files deleted in the newest upstream commit are also\n"
            "shown as deleted in the output of 'hg status'"
        )
        remove_deleted_upstream_files(
            github_path, github_sha, target_dir, log_dir, handle_noop_commit
        )
        error_help.set_help(None)

    if len(resume_state) == 0 or resume_state == "resume4":
        resume_state = ""
        update_resume_state("resume5", resume_state_filename)
        error_help.set_help(
            "An error occurred while adding new upstream files.\n"
            "Verify files added in the newest upstream commit are also\n"
            "shown as added in the output of 'hg status'"
        )
        add_new_upstream_files(
            github_path, github_sha, target_dir, log_dir, handle_noop_commit
        )
        error_help.set_help(None)

    if len(resume_state) == 0 or resume_state == "resume5":
        resume_state = ""
        update_resume_state("resume6", resume_state_filename)
        error_help.set_help(
            "An error occurred while adding handling renamed upstream files.\n"
            "Verify files renamed in the newest upstream commit are also\n"
            "shown as renamed/moved in the output of 'hg status'"
        )
        handle_renamed_upstream_files(
            github_path, github_sha, target_dir, log_dir, handle_noop_commit
        )
        error_help.set_help(None)

    if len(resume_state) == 0 or resume_state == "resume6":
        resume_state = ""
        update_resume_state("", resume_state_filename)
        error_help.set_help(
            "An error occurred while committing the vendored changes to Mercurial.\n"
        )
        # write the base of latest commit sha for use next time
        with open(os.path.join(target_dir, "README.mozilla.last-vendor"), "a") as f:
            # write the the command line used
            f.write("# base of lastest vendoring\n")
            f.write(f"{github_sha}\n")
        commit_all_changes(github_sha, commit_msg_filename, target_dir)
        error_help.set_help(None)

    # unregister the exit handler so the normal exit doesn't falsely
    # report as an error.
    atexit.unregister(early_exit_handler)


if __name__ == "__main__":
    default_target_dir = "third_party/libwebrtc"
    default_state_dir = ".moz-fast-forward"
    default_log_dir = ".moz-fast-forward/logs"

    parser = argparse.ArgumentParser(
        description="Vendor from local copy of moz-libwebrtc and commit"
    )
    parser.add_argument(
        "--repo-path",
        required=True,
        help="path to libwebrtc repo",
    )
    parser.add_argument(
        "--script-path",
        required=True,
        help="path to script directory",
    )
    parser.add_argument(
        "--commit-sha",
        required=True,
        help="sha of commit to examine",
    )
    parser.add_argument(
        "--branch",
        default="mozpatches",
        help="moz-libwebrtc branch (defaults to mozpatches)",
    )
    parser.add_argument(
        "--target-path",
        default=default_target_dir,
        help=f"target path for vendoring (defaults to {default_target_dir})",
    )
    parser.add_argument(
        "--state-path",
        default=default_state_dir,
        help=f"path to state directory (defaults to {default_state_dir})",
    )
    parser.add_argument(
        "--log-path",
        default=default_log_dir,
        help=f"path to log directory (defaults to {default_log_dir})",
    )
    parser.add_argument(
        "--commit-msg-path",
        required=True,
        help="path to file containing commit message",
    )
    args = parser.parse_args()

    vendor_and_commit(
        args.script_path,
        args.repo_path,
        args.branch,
        args.commit_sha,
        args.target_path,  # os.path.abspath(args.target_path),
        args.state_path,
        args.log_path,
        args.commit_msg_path,
    )
