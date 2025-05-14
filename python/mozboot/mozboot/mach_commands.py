# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import sys
from pathlib import Path

from mach.decorators import Command, CommandArgument

from mozboot.bootstrap import APPLICATIONS


@Command(
    "bootstrap",
    category="devenv",
    description="Install required system packages for building.",
)
@CommandArgument(
    "--application-choice",
    choices=list(APPLICATIONS.keys()) + list(APPLICATIONS.values()),
    default=None,
    help="Pass in an application choice instead of using the default "
    "interactive prompt.",
)
@CommandArgument(
    "--no-system-changes",
    dest="no_system_changes",
    action="store_true",
    help="Only execute actions that leave the system configuration alone.",
)
@CommandArgument(
    "--exclude",
    nargs="+",
    help="A list of bootstrappable elements not to bootstrap.",
)
def bootstrap(
    command_context, application_choice=None, no_system_changes=False, exclude=[]
):
    """Bootstrap system and mach for optimal development experience."""
    from mozboot.bootstrap import Bootstrapper

    bootstrapper = Bootstrapper(
        choice=application_choice,
        no_interactive=not command_context._mach_context.is_interactive,
        no_system_changes=no_system_changes,
        exclude=exclude,
        mach_context=command_context._mach_context,
    )
    bootstrapper.bootstrap(command_context.settings)


@Command(
    "vcs-setup",
    category="devenv",
    description="Help configure a VCS for optimal development.",
)
@CommandArgument(
    "-u",
    "--update-only",
    action="store_true",
    help="Only update recommended extensions, don't run the wizard.",
)
@CommandArgument(
    "--vcs",
    choices=("hg", "git", "jj"),
    help="Force a specific VCS backend instead of auto-detecting.",
)
def vcs_setup(command_context, update_only=False, vcs=None):
    """Ensure a Version Control System (Mercurial, Git, or
    Git + Jujutsu) is optimally configured.

    This command will inspect your VCS configuration and
    guide you through an interactive wizard helping you configure the
    VCS for optimal use on Mozilla projects.

    If "--update-only" is used, the interactive wizard is disabled
    and this command only ensures that remote repositories providing
    VCS extensions are up-to-date.
    """
    import mozversioncontrol
    from mach.util import to_optional_path
    from mozfile import which

    import mozboot.bootstrap as bootstrap

    topsrcdir = Path(command_context._mach_context.topdir)
    state_dir = Path(command_context._mach_context.state_dir)

    repo = mozversioncontrol.get_repository_object(topsrcdir)

    if not vcs:
        vcs = repo.name
        print(f"Automatically detected a {vcs} repository.")

    tool = "hg"
    if repo.name == "git" or repo.name == "jj":
        tool = "git"

    # "hg" is an executable script with a shebang, which will be found by
    # which. We need to pass a win32 executable to the function because we
    # spawn a process from it.
    if sys.platform in ("win32", "msys"):
        tool += ".exe"

    vcs_exe = to_optional_path(which(tool))
    if not vcs_exe:
        raise OSError(errno.ENOENT, f"Could not find {tool} on $PATH")

    if vcs == "git":
        bootstrap.configure_git(vcs_exe, state_dir, topsrcdir, update_only=update_only)
    elif vcs == "hg":
        bootstrap.configure_mercurial(vcs_exe, state_dir, update_only=update_only)
    elif vcs == "jj":
        from mozversioncontrol.factory import USING_JJ_WARNING

        print(USING_JJ_WARNING, file=sys.stderr)
        print(
            "\nOur jj support currently relies on Git; checks will run for both jj and Git.\n"
        )
        jj_exe = to_optional_path(which("jj"))
        if not jj_exe:
            raise OSError(errno.ENOENT, f"Could not find {jj_exe} on $PATH")
        bootstrap.configure_git(vcs_exe, state_dir, topsrcdir, update_only=update_only)
        bootstrap.configure_jujutsu(jj_exe, topsrcdir, update_only=update_only)
    else:
        raise ValueError(f"Unsupported VCS: {repo.name}")
