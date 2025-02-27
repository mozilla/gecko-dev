# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import glob
import json
import logging
import os
import shlex
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
from string import Template

# When updating this, please make sure to keep in sync the script for symbol
# scraping at
# https://github.com/mozilla/symbol-scrapers/blob/master/firefox-flatpak/script.sh
FREEDESKTOP_VERSION = "24.08"
# The base app is shared by firefox and thunderbird
FIREFOX_BASEAPP = "org.mozilla.firefox.BaseApp"
FIREFOX_BASEAPP_CHANNEL = FREEDESKTOP_VERSION


def run_command(log, *args, **kwargs):
    log(
        logging.INFO,
        "flatpak",
        {"command": shlex.join(args[0]), "cwd": str(kwargs.get("cwd", os.getcwd()))},
        "Running: {command} (in {cwd})",
    )
    return subprocess.run(*args, **kwargs)


def _inject_flatpak_distribution_ini(log, target):
    with tempfile.TemporaryDirectory() as git_clone_dir:
        run_command(
            log,
            [
                "git",
                "clone",
                "https://github.com/mozilla-partners/flatpak.git",
                git_clone_dir,
            ],
            check=True,
        )
        shutil.copyfile(
            os.path.join(
                git_clone_dir, "desktop/flatpak/distribution/distribution.ini"
            ),
            target,
        )


def _langpack_manifest(xpi):
    with zipfile.ZipFile(xpi) as f:
        return json.load(f.open("manifest.json"))


def _render_template(source, dest, variables):
    if source.endswith(".in"):
        with open(source) as f:
            template = Template(f.read())
        with open(dest[:-3], "w") as f:
            f.write(template.substitute(variables))
    else:
        shutil.copy(source, dest)


def _render_flatpak_templates(template_dir, build_dir, variables):
    for root, dirs, files in os.walk(template_dir):
        relative = os.path.relpath(root, template_dir)
        for d in dirs:
            os.makedirs(build_dir / relative / d, exist_ok=True)
        for f in files:
            _render_template(
                os.path.join(root, f), os.path.join(build_dir, relative, f), variables
            )


def repackage_flatpak(
    log,
    infile,
    output,
    arch,
    version,
    release_product,
    release_type,
    flatpak_name,
    flatpak_branch,
    template_dir,
    langpack_pattern,
):
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        build_dir = tmpdir / "build"
        app_dir = build_dir / "files"
        lib_dir = app_dir / "lib"

        # Fetch and install the base app
        run_command(
            log,
            [
                "flatpak",
                "remote-add",
                "--user",
                "--if-not-exists",
                "--from",
                "flathub",
                "https://dl.flathub.org/repo/flathub.flatpakrepo",
            ],
            check=True,
        )
        run_command(
            log,
            [
                "flatpak",
                "install",
                "--user",
                "-y",
                "flathub",
                f"{FIREFOX_BASEAPP}/{arch}/{FIREFOX_BASEAPP_CHANNEL}",
                "--no-deps",
            ],
            check=True,
        )
        # Copy files from the base app to our build dir
        base = (
            Path.home()
            / f".local/share/flatpak/app/{FIREFOX_BASEAPP}/{arch}/{FIREFOX_BASEAPP_CHANNEL}/active/files"
        )
        shutil.copytree(base, app_dir, symlinks=True)

        # Extract our build to the app dir
        lib_dir.mkdir(exist_ok=True)
        run_command(
            log, ["tar", "xf", os.path.abspath(infile)], cwd=lib_dir, check=True
        )

        if release_product == "firefox":
            distribution_ini = lib_dir / "firefox" / "distribution" / "distribution.ini"
            distribution_ini.parent.mkdir(parents=True)
            _inject_flatpak_distribution_ini(log, distribution_ini)

        date = datetime.date.today().strftime("%Y-%m-%d")
        variables = {
            "ARCH": arch,
            "FREEDESKTOP_VERSION": FREEDESKTOP_VERSION,
            "FIREFOX_BASEAPP_CHANNEL": FIREFOX_BASEAPP_CHANNEL,
            "FLATPAK_BRANCH": flatpak_branch,
            "VERSION": version,
            "DATE": date,
            "DEB_PKG_NAME": release_product,
            "DBusActivatable": "false",
            "Icon": flatpak_name,
            "StartupWMClass": release_product,
        }
        _render_flatpak_templates(template_dir, build_dir, variables)

        from fluent.runtime.fallback import FluentLocalization, FluentResourceLoader

        from mozbuild.repackaging.desktop_file import generate_browser_desktop_entry

        desktop = generate_browser_desktop_entry(
            log,
            variables,
            release_product,
            release_type,
            FluentLocalization,
            FluentResourceLoader,
        )
        desktop_dir = app_dir / "share" / "applications"
        desktop_dir.mkdir(parents=True, exist_ok=True)
        desktop_file_name = desktop_dir / f"{flatpak_name}.desktop"
        with desktop_file_name.open("w") as f:
            for line in desktop:
                print(line, file=f)

        if release_product == "firefox":
            icon_path = "lib/firefox/browser/chrome/icons/default"
        elif release_product == "thunderbird":
            icon_path = "lib/thunderbird/chrome/icons/default"
        else:
            raise NotImplementedError()

        for size in (16, 32, 48, 64, 128):
            os.makedirs(
                app_dir / f"share/icons/hicolor/{size}x{size}/apps", exist_ok=True
            )
            shutil.copy(
                app_dir / icon_path / f"default{size}.png",
                app_dir / f"share/icons/hicolor/{size}x{size}/apps/{flatpak_name}.png",
            )

        run_command(
            log,
            [
                "appstream-compose",
                f"--prefix={app_dir}",
                "--origin=flatpak",
                f"--basename={flatpak_name}",
                flatpak_name,
            ],
            check=True,
        )
        run_command(
            log,
            [
                "appstream-util",
                "mirror-screenshots",
                f"{app_dir}/share/app-info/xmls/{flatpak_name}.xml.gz",
                f"https://dl.flathub.org/repo/screenshots/{flatpak_name}-{flatpak_branch}",
                "build/screenshots",
                f"build/screenshots/{flatpak_name}-{flatpak_branch}",
            ],
            check=True,
            cwd=tmpdir,
        )

        os.makedirs(
            app_dir / f"lib/{release_product}/distribution/extensions", exist_ok=True
        )
        for langpack in glob.iglob(langpack_pattern):
            manifest = _langpack_manifest(langpack)
            locale = manifest["langpack_id"]
            name = manifest["browser_specific_settings"]["gecko"]["id"]

            lang = locale.split("-", 1)[0]
            os.makedirs(app_dir / "share/runtime/langpack" / lang, exist_ok=True)
            shutil.copy(
                langpack, app_dir / "share/runtime/langpack" / lang / f"{name}.xpi"
            )
            os.symlink(
                f"/app/share/runtime/langpack/{lang}/{name}.xpi",
                app_dir / f"lib/{release_product}/distribution/extensions/{name}.xpi",
            )

        run_command(
            log,
            [
                "flatpak",
                "build-finish",
                "build",
                "--allow=devel",
                "--share=ipc",
                "--share=network",
                "--socket=pulseaudio",
                "--socket=wayland",
                "--socket=fallback-x11",
                "--socket=pcsc",
                "--socket=cups",
                "--require-version=0.11.1",
                "--persist=.mozilla",
                "--env=DICPATH=/usr/share/hunspell",
                "--filesystem=xdg-download:rw",
                "--filesystem=/run/.heim_org.h5l.kcm-socket",
                "--filesystem=xdg-run/speech-dispatcher:ro",
                "--device=all",
                "--talk-name=org.freedesktop.FileManager1",
                "--system-talk-name=org.freedesktop.NetworkManager",
                "--talk-name=org.a11y.Bus",
                "--talk-name=org.gtk.vfs.*",
                "--own-name=org.mpris.MediaPlayer2.firefox.*",
                "--own-name=org.mozilla.firefox.*",
                "--own-name=org.mozilla.firefox_beta.*",
                "--command=firefox",
            ],
            check=True,
            cwd=tmpdir,
        )

        run_command(
            log,
            ["find", "build"],
            check=True,
            cwd=tmpdir,
        )

        run_command(
            log,
            [
                "flatpak",
                "build-export",
                f"--arch={arch}",
                "--disable-sandbox",
                "--no-update-summary",
                "--exclude=/share/runtime/langpack/*/*",
                "repo",
                "build",
                flatpak_branch,
            ],
            check=True,
            cwd=tmpdir,
        )
        run_command(
            log,
            [
                "flatpak",
                "build-export",
                f"--arch={arch}",
                "--disable-sandbox",
                "--no-update-summary",
                "--metadata=metadata.locale",
                "--files=files/share/runtime/langpack",
                "repo",
                "build",
                flatpak_branch,
            ],
            check=True,
            cwd=tmpdir,
        )
        run_command(
            log,
            [
                "ostree",
                "commit",
                "--repo=repo",
                "--canonical-permissions",
                f"--branch=screenshots/{arch}",
                "build/screenshots",
            ],
            check=True,
            cwd=tmpdir,
        )
        run_command(
            log,
            ["flatpak", "build-update-repo", "--generate-static-deltas", "repo"],
            check=True,
            cwd=tmpdir,
        )
        env = os.environ.copy()
        env["XZ_OPT"] = "-e9"
        run_command(
            log,
            ["tar", "cvfJ", os.path.abspath(output), "repo"],
            check=True,
            env=env,
            cwd=tmpdir,
        )
