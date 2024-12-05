# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import atexit
import os
import re
import shutil
import sys

from run_operations import run_git, run_hg, run_shell

# This script saves the mozilla patch stack and no-op commit tracking
# files.  This makes our fast-forward process much more resilient by
# saving the intermediate state after each upstream commit is processed.

error_help = None
script_name = os.path.basename(__file__)


@atexit.register
def early_exit_handler():
    print(f"*** ERROR *** {script_name} did not complete successfully")
    if error_help is not None:
        print(error_help)


def save_patch_stack(
    github_path,
    github_branch,
    patch_directory,
    state_directory,
    target_branch_head,
    bug_number,
):
    # remove the current patch files
    files_to_remove = os.listdir(patch_directory)
    for file in files_to_remove:
        os.remove(os.path.join(patch_directory, file))

    # find the base of the patch stack
    cmd = f"git merge-base {github_branch} {target_branch_head}"
    stdout_lines = run_git(cmd, github_path)
    merge_base = stdout_lines[0]

    # grab patch stack
    cmd = f"git format-patch --keep-subject --no-signature --output-directory {patch_directory} {merge_base}..{github_branch}"
    run_git(cmd, github_path)

    # remove the commit summary from the file name
    patches_to_rename = os.listdir(patch_directory)
    for file in patches_to_rename:
        shortened_name = re.sub(r"^(\d\d\d\d)-.*\.patch", "\\1.patch", file)
        os.rename(
            os.path.join(patch_directory, file),
            os.path.join(patch_directory, shortened_name),
        )

    # remove the unhelpful first line of the patch files that only
    # causes diff churn.  For reasons why we can't skip creating backup
    # files during the in-place editing, see:
    # https://stackoverflow.com/questions/5694228/sed-in-place-flag-that-works-both-on-mac-bsd-and-linux
    run_shell(f"sed -i'.bak' -e '1d' {patch_directory}/*.patch")
    run_shell(f"rm {patch_directory}/*.patch.bak")

    # it is also helpful to save the no-op-cherry-pick-msg files from
    # the state directory so that if we're restoring a patch-stack we
    # also restore the possibly consumed no-op tracking files.
    no_op_files = [
        path
        for path in os.listdir(state_directory)
        if re.findall(".*no-op-cherry-pick-msg$", path)
    ]
    for file in no_op_files:
        shutil.copy(os.path.join(state_directory, file), patch_directory)

    # get missing files (that should be marked removed)
    cmd = f"hg status --no-status --deleted {patch_directory}"
    stdout_lines = run_hg(cmd)
    if len(stdout_lines) != 0:
        cmd = f"hg rm {' '.join(stdout_lines)}"
        run_hg(cmd)

    # get unknown files (that should be marked added)
    cmd = f"hg status --no-status --unknown {patch_directory}"
    stdout_lines = run_hg(cmd)
    if len(stdout_lines) != 0:
        cmd = f"hg add {' '.join(stdout_lines)}"
        run_hg(cmd)

    # if any files are marked for add/remove/modify, commit them
    cmd = f"hg status --added --removed --modified {patch_directory}"
    stdout_lines = run_hg(cmd)
    if (len(stdout_lines)) != 0:
        print(f"Updating {len(stdout_lines)} files in {patch_directory}")
        if bug_number is None:
            run_hg("hg amend")
        else:
            run_shell(
                f"hg commit --message 'Bug {bug_number} - updated libwebrtc patch stack'"
            )


if __name__ == "__main__":
    default_patch_dir = "third_party/libwebrtc/moz-patch-stack"
    default_script_dir = "dom/media/webrtc/third_party_build"
    default_state_dir = ".moz-fast-forward"

    parser = argparse.ArgumentParser(
        description="Save moz-libwebrtc github patch stack"
    )
    parser.add_argument(
        "--repo-path",
        required=True,
        help="path to libwebrtc repo",
    )
    parser.add_argument(
        "--branch",
        default="mozpatches",
        help="moz-libwebrtc branch (defaults to mozpatches)",
    )
    parser.add_argument(
        "--patch-path",
        default=default_patch_dir,
        help=f"path to save patches (defaults to {default_patch_dir})",
    )
    parser.add_argument(
        "--state-path",
        default=default_state_dir,
        help=f"path to state directory (defaults to {default_state_dir})",
    )
    parser.add_argument(
        "--target-branch-head",
        required=True,
        help="target branch head for fast-forward, should match MOZ_TARGET_UPSTREAM_BRANCH_HEAD in config_env",
    )
    parser.add_argument(
        "--script-path",
        default=default_script_dir,
        help=f"path to script directory (defaults to {default_script_dir})",
    )
    parser.add_argument(
        "--separate-commit-bug-number",
        type=int,
        help="integer Bugzilla number (example: 1800920), if provided will write patch stack as separate commit",
    )
    parser.add_argument(
        "--skip-startup-sanity",
        action="store_true",
        default=False,
        help="skip checking for clean repo and doing the initial verify vendoring",
    )
    args = parser.parse_args()

    if not args.skip_startup_sanity:
        # make sure the mercurial repo is clean before beginning
        error_help = (
            "There are modified or untracked files in the mercurial repo.\n"
            f"Please start with a clean repo before running {script_name}"
        )
        stdout_lines = run_hg("hg status")
        if len(stdout_lines) != 0:
            sys.exit(1)

        # make sure the github repo exists
        error_help = (
            f"No moz-libwebrtc github repo found at {args.repo_path}\n"
            f"Please run restore_patch_stack.py before running {script_name}"
        )
        if not os.path.exists(args.repo_path):
            sys.exit(1)
        error_help = None

        print("Verifying vendoring before saving patch-stack...")
        run_shell(f"bash {args.script_path}/verify_vendoring.sh", False)

    save_patch_stack(
        args.repo_path,
        args.branch,
        os.path.abspath(args.patch_path),
        args.state_path,
        args.target_branch_head,
        args.separate_commit_bug_number,
    )

    # unregister the exit handler so the normal exit doesn't falsely
    # report as an error.
    atexit.unregister(early_exit_handler)
