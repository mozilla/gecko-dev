# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from copy import deepcopy
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_build_attributes(config, jobs):
    """
    Set the build_platform and build_type attributes based on the job name.
    """
    for job in jobs:
        build_platform, build_type = job["name"].split("/")
        attributes = job.setdefault("attributes", {})
        attributes.update(
            {
                "build_platform": build_platform,
                "build_type": build_type,
            }
        )

        yield job


EXTRA_COMPILERS = {
    "clang-4": {"CC": "clang-4.0", "CCC": "clang++-4.0"},
    "clang-10": {"CC": "clang-10", "CCC": "clang++-10"},
    "clang-18": {"CC": "clang-18", "CCC": "clang++-18"},
    # gcc-4.6 introduced nullptr.
    "gcc-4.4": {"CC": "gcc-4.4", "CCC": "g++-4.4", "NSS_DISABLE_GTESTS": "1"},
    # gcc-4.8 has incomplete c++11 support
    "gcc-4.8": {"CC": "gcc-4.8", "CCC": "g++-4.8", "NSS_DISABLE_GTESTS": "1"},
    "gcc-5": {"CC": "gcc-5", "CCC": "g++-5"},
    "gcc-11": {"CC": "gcc-11", "CCC": "g++-11"},
}


@transforms.add
def add_variants(config, jobs):
    for job in jobs:
        attributes = job["attributes"]

        # nspr
        if not any(attributes.get(attr) for attr in ("make", "asan", "fuzz")):
            nspr_job = deepcopy(job)
            nspr_job["name"] = f"{attributes['build_platform']}-nspr/{attributes['build_type']}"
            nspr_job["description"]+= " (NSPR only)"
            nspr_job["attributes"]["nspr"] = True
            yield nspr_job

        # base build
        build_job = deepcopy(job)
        build_job["attributes"].setdefault("certs", True)
        yield build_job

        # fips
        if attributes.get("make"):
            fips_build = deepcopy(job)
            fips_build["name"] = f"{attributes['build_platform']}-fips/{attributes['build_type']}"
            fips_build["description"] += " w/ NSS_FORCE_FIPS"
            fips_build.setdefault("worker", {}).setdefault("env", {})["NSS_FORCE_FIPS"] = "1"
            fips_build["attributes"]["make-fips"] = True
            fips_build["attributes"]["certs"] = True
            yield fips_build

        if "linux" in attributes["build_platform"]:
            # more compilers
            if not attributes.get("asan") and not attributes.get("fuzz"):
                for cc in EXTRA_COMPILERS:
                    if cc == "gcc-4.4" and not (attributes.get("make") and job["attributes"]["build_platform"].startswith("linux64")):
                        # Use the old Makefile-based build system, GYP doesn't have a proper GCC
                        # version check for __int128 support. It's mainly meant to cover RHEL6.
                        continue
                    if cc == "gcc-4.8" and not attributes.get("make"):
                        # Use -Ddisable-intelhw_sha=1, GYP doesn't have a proper GCC version
                        # check for Intel SHA support.
                        continue
                    cc_job = deepcopy(job)
                    cc_job["name"] += f"-{cc}"
                    cc_job["description"] += f" w/ {cc}"
                    cc_job["attributes"]["cc"] = cc
                    cc_job.setdefault("worker", {}).setdefault("env", {}).update(EXTRA_COMPILERS[cc])
                    yield cc_job

            # modular
            if attributes.get("make"):
                modular_job = deepcopy(job)
                modular_job["attributes"]["modular"] = True
                modular_job.setdefault("worker", {}).setdefault("env", {})["NSS_BUILD_MODULAR"] = "1"
                modular_job["name"] += "-modular"
                modular_job["description"] += " w/ modular builds"
                yield modular_job

            # dbm
            if not attributes.get("make") and not attributes.get("fuzz"):
                dbm_job = deepcopy(job)
                dbm_job["attributes"]["dbm"] = True
                dbm_job["attributes"].setdefault("certs", True)
                dbm_job["name"] += "-dbm"
                dbm_job["description"] += " w/ legacy-db"
                yield dbm_job

            if attributes.get("fuzz"):
                tlsfuzz_job = deepcopy(job)
                tlsfuzz_job["attributes"]["tlsfuzz"] = True
                tlsfuzz_job["name"] = job["name"].replace("-fuzz", "-tlsfuzz")
                tlsfuzz_job["description"] = job["description"].replace("fuzz", "TLS fuzz")
                yield tlsfuzz_job


@transforms.add
def set_attributes_defaults(config, jobs):
    for job in jobs:
        attributes = job["attributes"]
        for attr in ("make", "asan", "make-fips", "fuzz", "certs", "nspr", "cc", "dbm", "modular", "tlsfuzz"):
            attributes.setdefault(attr, False)
        yield job


@transforms.add
def set_make_command(config, jobs):
    for job in jobs:
        if not job["attributes"].get("make"):
            yield job
            continue
        platform = job["attributes"]["build_platform"]
        if "win" in platform:
            script = "${VCS_PATH}/nss/automation/taskcluster/windows/build.sh"
        else:
            script = "${VCS_PATH}/nss/automation/taskcluster/scripts/build.sh"
        if "64" in job["attributes"]["build_platform"]:
            job["worker"].setdefault("env", {})["USE_64"] = "1"
        job["run"]["command"] = script
        yield job


@transforms.add
def set_gyp_command(config, jobs):
    for job in jobs:
        attributes = job["attributes"]
        if attributes.get("make"):
            yield job
            continue
        platform = attributes["build_platform"]
        if "win" in platform:
            script = "${VCS_PATH}/nss/automation/taskcluster/windows/build_gyp.sh"
        else:
            script = "${VCS_PATH}/nss/automation/taskcluster/scripts/build_gyp.sh"
        command = script + " --python=python3"
        if "64" not in platform:
            command += " -t ia32"
        if attributes["build_type"] in ("opt", "opt-static"):
            command += " --opt"
        if "fips" in attributes["build_type"]:
            command += " --enable-fips"
        if attributes.get("asan"):
            command += " --ubsan --asan"
        if attributes.get("nspr"):
            command += " --nspr-only --nspr-test-build --nspr-test-run"
        if attributes.get("static"):
            command += " --static -Ddisable_libpkix=1"
        if attributes.get("fuzz"):
            command += " --disable-tests -Ddisable_libpkix=1 --fuzz"
            job.setdefault("worker", {}).setdefault("env", {}).update({
                "ASAN_OPTIONS": "allocator_may_return_null=1:detect_stack_use_after_return=1",
                "UBSAN_OPTIONS": "print_stacktrace=1",
                "NSS_DISABLE_ARENA_FREE_LIST": "1",
                "NSS_DISABLE_UNLOAD": "1",
                "CC": "clang",
                "CCC": "clang++",
            })
            if attributes.get("tlsfuzz"):
                command += "=tls"
            else:
                command += " && nss/automation/taskcluster/scripts/build_cryptofuzz.sh"
                if "64" not in platform:
                    command += " --i386"
        job["run"]["command"] = command
        yield job

@transforms.add
def set_docker_image(config, jobs):
    for job in jobs:
        if "linux" not in job["attributes"]["build_platform"]:
            yield job
            continue

        if job["attributes"].get("cc") == "gcc-4.4":
            image = "gcc-4.4"
        elif job["attributes"].get("cc"):
            image = "builds"
        elif job["attributes"].get("fuzz"):
            image = "fuzz"
        else:
            image = "base"
        job["worker"]["docker-image"] = {"in-tree": image}
        yield job


def get_job_symbol(job):
    if job["attributes"].get("nspr"):
        return "NSPR"
    if job["attributes"].get("cc"):
        return f"Builds({job['attributes']['cc']})"
    if job["attributes"].get("tlsfuzz"):
        return "TLS(B)"
    if job["attributes"].get("dbm"):
        return "DBM(B)"
    if job["attributes"].get("make-fips"):
        return "FIPS(B)"
    if job["attributes"].get("modular"):
        return "Builds(modular)"
    return "B"

@transforms.add
def set_treeherder_symbol(config, jobs):
    for job in jobs:
        job["treeherder"].update({
            "symbol": get_job_symbol(job),
            "platform": f'{job["attributes"]["build_platform"]}/{job["attributes"]["build_type"]}',
        })
        yield job
