# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import tarfile
import tempfile

import mozfile
import mozpack.path as mozpath

from mozbuild.repackaging.utils import (
    copy_plain_config,
    get_build_variables,
    inject_desktop_entry_file,
    inject_distribution_folder,
    inject_prefs_file,
    load_application_ini_data,
    mv_manpage_files,
    render_templates,
)


class NoRpmPackageFound(Exception):
    """Raised when no .rpm is found after calling rpmbuild"""

    def __init__(self, file_path) -> None:
        super().__init__(f"No {file_path} package found after calling rpmbuild")


def repackage_rpm(
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

        application_ini_data = load_application_ini_data(infile, version, build_number)
        build_variables = _get_build_variables(
            application_ini_data,
            arch,
            version,
            release_product=release_product,
            build_number=build_number,
        )

        rpm_dir = mozpath.join(source_dir, "rpm")

        copy_plain_config(template_dir, rpm_dir)
        render_templates(
            template_dir,
            rpm_dir,
            build_variables,
            exclude_file_names=["package-prefs.js"],
        )

        app_name = application_ini_data["name"]
        with open(
            mozpath.join(source_dir, app_name.lower(), "is-packaged-app"), "w"
        ) as f:
            f.write("This is a packaged app.\n")

        inject_distribution_folder(source_dir, "rpm", app_name)
        inject_desktop_entry_file(
            log,
            rpm_dir,
            build_variables,
            release_product,
            release_type,
            fluent_localization,
            fluent_resource_loader,
        )
        mv_manpage_files(rpm_dir, build_variables)
        inject_prefs_file(source_dir, app_name, template_dir)
        _generate_rpm_archive(
            rpm_dir,
            infile,
            target_dir=tmpdir,
            output_file_path=output,
            build_variables=build_variables,
            arch=arch,
        )

    finally:
        shutil.rmtree(tmpdir)


def _create_temporary_directory(arch):
    if _is_chroot_available(arch):
        return tempfile.mkdtemp(dir=f"{_get_chroot_path(arch)}/tmp")
    return tempfile.mkdtemp()


def _generate_rpm_archive(
    source_dir, infile, target_dir, output_file_path, build_variables, arch
):
    shutil.copy(
        infile,
        mozpath.join(source_dir, f"{build_variables['PKG_NAME']}.tar.xz"),
    )

    command = _get_command(source_dir, target_dir, arch)
    subprocess.check_call(command, cwd=source_dir)

    file_path = mozpath.join(
        target_dir,
        arch,
        f"{build_variables['PKG_NAME']}-{build_variables['PKG_VERSION']}"
        f"-{build_variables['PKG_BUILD_NUMBER']}.{arch}.rpm",
    )

    if not os.path.exists(file_path):
        raise NoRpmPackageFound(file_path)

    shutil.move(file_path, output_file_path)


def _get_build_variables(
    application_ini_data,
    arch,
    version,
    release_product="",
    package_name_suffix="",
    description_suffix="",
    build_number="1",
):
    build_variables = get_build_variables(
        application_ini_data,
        arch,
        version,
        release_product=release_product,
        package_name_suffix=package_name_suffix,
        description_suffix=description_suffix,
        build_number=build_number,
    )

    # The format of the date must use the same format as “Wen Jan 22 2024”
    build_variables["CHANGELOG_DATE"] = application_ini_data["timestamp"].strftime(
        "%a %b %d %Y"
    )

    return build_variables


def _get_chroot_path(arch):
    return f"/srv/rpm-{arch}"


def _get_command(source_dir, target_dir, arch):
    command = [
        "rpmbuild",
        "-ba",
        mozpath.join(source_dir, "firefox.spec"),
        "--define",
        f"_builddir {mozpath.join(target_dir, 'build')}",
        "--define",
        f"_rpmdir {target_dir}",  # Build the rpm file in target_dir/{arch}
        "--define",
        f"_sourcedir {source_dir}",  # Retrieve the sources from this directory
        "--define",
        f"_srcrpmdir {target_dir}",  # Store the generated src.rpm in target_dir
    ]

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


def _is_chroot_available(arch):
    return os.path.isdir(_get_chroot_path(arch))
