# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from email.utils import format_datetime
from pathlib import Path
from string import Template

import mozfile
import mozpack.path as mozpath
from mozilla_version.gecko import GeckoVersion

from mozbuild.repackaging.application_ini import get_application_ini_values
from mozbuild.repackaging.desktop_file import generate_browser_desktop_entry_file_text


class NoDebPackageFound(Exception):
    """Raised when no .deb is found after calling dpkg-buildpackage"""

    def __init__(self, deb_file_path) -> None:
        super().__init__(
            f"No {deb_file_path} package found after calling dpkg-buildpackage"
        )


class HgServerError(Exception):
    """Raised when Hg responds with an error code that is not 404 (i.e. when there is an outage)"""

    def __init__(self, msg) -> None:
        super().__init__(msg)


# Maps our CI/release pipeline's architecture names (e.g., "x86_64")
# into architectures ("amd64") compatible with Debian's dpkg-buildpackage tool.
# This is the target architecture we are building the .deb package for.
_DEB_ARCH = {
    "all": "all",
    "x86": "i386",
    "x86_64": "amd64",
    "aarch64": "arm64",
}

# Defines the sysroot (build host's) architecture for each target architecture in the pipeline.
# It defines the architecture dpkg-buildpackage runs on.
_DEB_SYSROOT_ARCH = {
    "all": "amd64",
    "x86": "i386",
    "x86_64": "amd64",
    "aarch64": "amd64",
}

# Assigns the Debian distribution version for the sysroot based on the target architecture.
# It defines the Debian distribution dpkg-buildpackage runs on.
_DEB_SYSROOT_DIST = {
    "all": "jessie",
    "x86": "jessie",
    "x86_64": "jessie",
    "aarch64": "buster",
}


def repackage_deb(
    log,
    infile,
    output,
    template_dir,
    arch,
    version,
    build_number,
    release_product,
    release_type,
    fluent_localization,
    fluent_resource_loader,
):
    if not tarfile.is_tarfile(infile):
        raise Exception("Input file %s is not a valid tarfile." % infile)

    tmpdir = _create_temporary_directory(arch)
    source_dir = os.path.join(tmpdir, "source")
    try:
        mozfile.extract_tarball(infile, source_dir)
        application_ini_data = _load_application_ini_data(infile, version, build_number)
        build_variables = _get_build_variables(
            application_ini_data,
            arch,
            depends="${shlibs:Depends},",
            release_product=release_product,
        )

        _copy_plain_deb_config(template_dir, source_dir)
        _render_deb_templates(
            template_dir,
            source_dir,
            build_variables,
            exclude_file_names=["package-prefs.js"],
        )

        app_name = application_ini_data["name"]
        with open(
            mozpath.join(source_dir, app_name.lower(), "is-packaged-app"), "w"
        ) as f:
            f.write("This is a packaged app.\n")

        _inject_deb_distribution_folder(source_dir, app_name)
        _inject_deb_desktop_entry_file(
            log,
            source_dir,
            build_variables,
            release_product,
            release_type,
            fluent_localization,
            fluent_resource_loader,
        )
        _mv_manpage_files(source_dir, build_variables)
        _inject_deb_prefs_file(source_dir, app_name, template_dir)
        _generate_deb_archive(
            source_dir,
            target_dir=tmpdir,
            output_file_path=output,
            build_variables=build_variables,
            arch=arch,
        )

    finally:
        shutil.rmtree(tmpdir)


def repackage_deb_l10n(
    input_xpi_file,
    input_tar_file,
    output,
    template_dir,
    version,
    build_number,
    release_product,
):
    arch = "all"

    tmpdir = _create_temporary_directory(arch)
    source_dir = os.path.join(tmpdir, "source")
    try:
        langpack_metadata = _extract_langpack_metadata(input_xpi_file)
        langpack_dir = mozpath.join(source_dir, "firefox", "distribution", "extensions")
        application_ini_data = _load_application_ini_data(
            input_tar_file, version, build_number
        )
        langpack_id = langpack_metadata["langpack_id"]
        if release_product == "devedition":
            depends = (
                f"firefox-devedition (= {application_ini_data['deb_pkg_version']})"
            )
        else:
            depends = f"{application_ini_data['remoting_name']} (= {application_ini_data['deb_pkg_version']})"
        build_variables = _get_build_variables(
            application_ini_data,
            arch,
            depends=depends,
            # Debian package names are only lowercase
            package_name_suffix=f"-l10n-{langpack_id.lower()}",
            description_suffix=f" - {langpack_metadata['description']}",
            release_product=release_product,
        )
        _copy_plain_deb_config(template_dir, source_dir)
        _render_deb_templates(template_dir, source_dir, build_variables)

        os.makedirs(langpack_dir, exist_ok=True)
        shutil.copy(
            input_xpi_file,
            mozpath.join(
                langpack_dir,
                f"{langpack_metadata['browser_specific_settings']['gecko']['id']}.xpi",
            ),
        )
        _generate_deb_archive(
            source_dir=source_dir,
            target_dir=tmpdir,
            output_file_path=output,
            build_variables=build_variables,
            arch=arch,
        )
    finally:
        shutil.rmtree(tmpdir)


def _extract_application_ini_data(input_tar_file):
    with tempfile.TemporaryDirectory() as d:
        with tarfile.open(input_tar_file) as tar:
            application_ini_files = [
                tar_info
                for tar_info in tar.getmembers()
                if tar_info.name.endswith("/application.ini")
            ]
            if len(application_ini_files) == 0:
                raise ValueError(
                    f"Cannot find any application.ini file in archive {input_tar_file}"
                )
            if len(application_ini_files) > 1:
                raise ValueError(
                    f"Too many application.ini files found in archive {input_tar_file}. "
                    f"Found: {application_ini_files}"
                )

            tar.extract(application_ini_files[0], path=d)

        application_ini_data = _extract_application_ini_data_from_directory(d)

        return application_ini_data


def _load_application_ini_data(infile, version, build_number):
    extracted_application_ini_data = _extract_application_ini_data(infile)
    parsed_application_ini_data = _parse_application_ini_data(
        extracted_application_ini_data, version, build_number
    )
    return parsed_application_ini_data


def _parse_application_ini_data(application_ini_data, version, build_number):
    application_ini_data["timestamp"] = datetime.datetime.strptime(
        application_ini_data["build_id"], "%Y%m%d%H%M%S"
    )

    application_ini_data["remoting_name"] = application_ini_data[
        "remoting_name"
    ].lower()

    application_ini_data["deb_pkg_version"] = _get_deb_pkg_version(
        version, application_ini_data["build_id"], build_number
    )

    return application_ini_data


def _get_deb_pkg_version(version, build_id, build_number):
    gecko_version = GeckoVersion.parse(version)
    deb_pkg_version = (
        f"{gecko_version}~{build_id}"
        if gecko_version.is_nightly
        else f"{gecko_version}~build{build_number}"
    )
    return deb_pkg_version


def _extract_application_ini_data_from_directory(application_directory):
    values = get_application_ini_values(
        application_directory,
        dict(section="App", value="Name"),
        dict(section="App", value="CodeName", fallback="Name"),
        dict(section="App", value="Vendor"),
        dict(section="App", value="RemotingName"),
        dict(section="App", value="BuildID"),
    )

    data = {
        "name": next(values),
        "display_name": next(values),
        "vendor": next(values),
        "remoting_name": next(values),
        "build_id": next(values),
    }

    return data


def _get_build_variables(
    application_ini_data,
    arch,
    depends,
    package_name_suffix="",
    description_suffix="",
    release_product="",
):
    if release_product == "devedition":
        deb_pkg_install_path = "usr/lib/firefox-devedition"
        deb_pkg_name = f"firefox-devedition{package_name_suffix}"
    else:
        deb_pkg_install_path = f"usr/lib/{application_ini_data['remoting_name']}"
        deb_pkg_name = f"{application_ini_data['remoting_name']}{package_name_suffix}"
    return {
        "DEB_DESCRIPTION": f"{application_ini_data['vendor']} {application_ini_data['display_name']}"
        f"{description_suffix}",
        "DEB_PKG_INSTALL_PATH": deb_pkg_install_path,
        "DEB_PKG_NAME": deb_pkg_name,
        "DEB_PRODUCT_NAME": application_ini_data["name"],
        "DEB_DISPLAY_NAME": application_ini_data["display_name"],
        "DEB_PKG_VERSION": application_ini_data["deb_pkg_version"],
        "DEB_CHANGELOG_DATE": format_datetime(application_ini_data["timestamp"]),
        "DEB_MANPAGE_DATE": application_ini_data["timestamp"].strftime("%B %d, %Y"),
        "DEB_ARCH_NAME": _DEB_ARCH[arch],
        "DEB_DEPENDS": depends,
        "Icon": deb_pkg_name,
    }


def _copy_plain_deb_config(input_template_dir, source_dir):
    template_dir_filenames = os.listdir(input_template_dir)
    plain_filenames = [
        mozpath.basename(filename)
        for filename in template_dir_filenames
        if not filename.endswith(".in") and not filename.endswith(".js")
    ]
    os.makedirs(mozpath.join(source_dir, "debian"), exist_ok=True)

    for filename in plain_filenames:
        shutil.copy(
            mozpath.join(input_template_dir, filename),
            mozpath.join(source_dir, "debian", filename),
        )


def _render_deb_templates(
    input_template_dir, source_dir, build_variables, exclude_file_names=None
):
    exclude_file_names = [] if exclude_file_names is None else exclude_file_names

    template_dir_filenames = os.listdir(input_template_dir)
    template_filenames = [
        mozpath.basename(filename)
        for filename in template_dir_filenames
        if filename.endswith(".in") and filename not in exclude_file_names
    ]
    os.makedirs(mozpath.join(source_dir, "debian"), exist_ok=True)

    for file_name in template_filenames:
        with open(mozpath.join(input_template_dir, file_name)) as f:
            template = Template(f.read())
        with open(mozpath.join(source_dir, "debian", Path(file_name).stem), "w") as f:
            f.write(template.substitute(build_variables))


def _inject_deb_distribution_folder(source_dir, app_name):
    distribution_ini_path = mozpath.join(source_dir, "debian", "distribution.ini")

    # Check to see if a distribution.ini file is already supplied in the debian templates directory
    # If not, continue to download default Firefox distribution.ini from GitHub
    if os.path.exists(distribution_ini_path):
        os.makedirs(
            mozpath.join(source_dir, app_name.lower(), "distribution"), exist_ok=True
        )
        shutil.move(
            distribution_ini_path,
            mozpath.join(source_dir, app_name.lower(), "distribution"),
        )

        return

    with tempfile.TemporaryDirectory() as git_clone_dir:
        subprocess.check_call(
            [
                "git",
                "clone",
                "https://github.com/mozilla-partners/deb.git",
                git_clone_dir,
            ],
        )
        shutil.copytree(
            mozpath.join(git_clone_dir, "desktop/deb/distribution"),
            mozpath.join(source_dir, app_name.lower(), "distribution"),
        )


def _inject_deb_prefs_file(source_dir, app_name, template_dir):
    src = mozpath.join(template_dir, "package-prefs.js")
    dst = mozpath.join(source_dir, app_name.lower(), "defaults/pref")
    shutil.copy(src, dst)


def _mv_manpage_files(source_dir, build_variables):
    src = mozpath.join(source_dir, "debian", "manpage.1")
    dst = mozpath.join(source_dir, "debian", f"{build_variables['DEB_PKG_NAME']}.1")
    shutil.move(src, dst)
    src = mozpath.join(source_dir, "debian", "manpages")
    dst = mozpath.join(
        source_dir, "debian", f"{build_variables['DEB_PKG_NAME']}.manpages"
    )
    shutil.move(src, dst)


def _inject_deb_desktop_entry_file(
    log,
    source_dir,
    build_variables,
    release_product,
    release_type,
    fluent_localization,
    fluent_resource_loader,
):
    desktop_entry_template_path = mozpath.join(
        source_dir, "debian", f"{build_variables['DEB_PRODUCT_NAME'].lower()}.desktop"
    )
    desktop_entry_file_filename = f"{build_variables['DEB_PKG_NAME']}.desktop"

    # Check to see if a .desktop file is already supplied in the debian templates directory
    # If not, continue to generate default Firefox .desktop file
    if os.path.exists(desktop_entry_template_path):
        shutil.move(
            desktop_entry_template_path,
            mozpath.join(source_dir, "debian", desktop_entry_file_filename),
        )

        return

    desktop_entry_file_text = generate_browser_desktop_entry_file_text(
        log,
        build_variables,
        release_product,
        release_type,
        fluent_localization,
        fluent_resource_loader,
    )
    os.makedirs(mozpath.join(source_dir, "debian"), exist_ok=True)
    with open(
        mozpath.join(source_dir, "debian", desktop_entry_file_filename), "w"
    ) as f:
        f.write(desktop_entry_file_text)


def _generate_deb_archive(
    source_dir, target_dir, output_file_path, build_variables, arch
):
    command = _get_command(arch)
    subprocess.check_call(command, cwd=source_dir)
    deb_arch = _DEB_ARCH[arch]
    deb_file_name = f"{build_variables['DEB_PKG_NAME']}_{build_variables['DEB_PKG_VERSION']}_{deb_arch}.deb"
    deb_file_path = mozpath.join(target_dir, deb_file_name)

    if not os.path.exists(deb_file_path):
        raise NoDebPackageFound(deb_file_path)

    subprocess.check_call(["dpkg-deb", "--info", deb_file_path])
    shutil.move(deb_file_path, output_file_path)


def _get_command(arch):
    deb_arch = _DEB_ARCH[arch]
    command = [
        "dpkg-buildpackage",
        # TODO: Use long options once we stop supporting Debian Jesse. They're more
        # explicit.
        #
        # Long options were added in dpkg 1.18.8 which is part of Debian Stretch.
        #
        # https://git.dpkg.org/cgit/dpkg/dpkg.git/commit/?h=1.18.x&id=293bd243a19149165fc4fd8830b16a51d471a5e9
        # https://packages.debian.org/stretch/dpkg-dev
        "-us",  # --unsigned-source
        "-uc",  # --unsigned-changes
        "-b",  # --build=binary
    ]

    if deb_arch != "all":
        command.append(f"--host-arch={deb_arch}")

    if _is_chroot_available(arch):
        flattened_command = " ".join(command)
        command = [
            "chroot",
            _get_chroot_path(arch),
            "bash",
            "-c",
            f"cd /tmp/*/source; {flattened_command}",
        ]

    return command


def _create_temporary_directory(arch):
    if _is_chroot_available(arch):
        return tempfile.mkdtemp(dir=f"{_get_chroot_path(arch)}/tmp")
    else:
        return tempfile.mkdtemp()


def _is_chroot_available(arch):
    return os.path.isdir(_get_chroot_path(arch))


def _get_chroot_path(arch):
    # At the moment the Firefox build baseline for i386 and amd64 is jessie and the baseline for arm64 is buster.
    # These baselines are defined in taskcluster/scripts/misc/build-sysroot.sh
    # The debian-repackage image defined in taskcluster/docker/debian-repackage/Dockerfile
    # bootstraps /srv/jessie-i386, /srv/jessie-amd64, and /srv/buster-amd64 roots.
    # We use these roots to run the repackage step and generate shared
    # library dependencies that match the Firefox build baseline.
    deb_sysroot_dist = _DEB_SYSROOT_DIST[arch]
    deb_sysroot_arch = _DEB_SYSROOT_ARCH[arch]
    return f"/srv/{deb_sysroot_dist}-{deb_sysroot_arch}"


_MANIFEST_FILE_NAME = "manifest.json"


def _extract_langpack_metadata(input_xpi_file):
    with tempfile.TemporaryDirectory() as d:
        with zipfile.ZipFile(input_xpi_file) as zip:
            zip.extract(_MANIFEST_FILE_NAME, path=d)

        with open(mozpath.join(d, _MANIFEST_FILE_NAME)) as f:
            return json.load(f)
