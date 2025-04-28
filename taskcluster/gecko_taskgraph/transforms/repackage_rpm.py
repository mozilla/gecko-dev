from collections import defaultdict

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


def _extract_locale_from_langpack_artifact_path(langpack_artifact_path):
    return langpack_artifact_path.split("/")[2]


@transforms.add
def repackage_rpm(config, tasks):
    kind_deps = config.kind_dependencies_tasks
    l10n_signing_kind = (
        "devedition-l10n-signing"
        if config.params["release_product"] == "devedition"
        else "shippable-l10n-signing"
    )
    # langpacks are platform independent, so we just pick one.
    l10n_signing_build_platform = (
        "linux64-devedition"
        if config.params["release_product"] == "devedition"
        else "linux64-shippable"
    )
    for task in tasks:
        # depend on the shippable l10n tasks so we can use the xpi artifacts
        # in the creation of a unified RPM package.
        l10n_signing_deps = {
            dep: dep
            for dep in kind_deps
            if all(
                (
                    kind_deps[dep].kind == l10n_signing_kind,
                    kind_deps[dep].attributes.get("build_platform")
                    == l10n_signing_build_platform,
                )
            )
        }
        fetches = defaultdict(
            list,
            **{
                "build-signing": [
                    {
                        "artifact": "public/build/target.tar.xz",
                        "extract": False,
                        "dest": "/builds/worker/fetches/target.tar.xz",
                    }
                ]
            },
        )
        for l10n_signing_dep in l10n_signing_deps:
            langpack_artifact_paths = [
                release_artifact
                for release_artifact in kind_deps[l10n_signing_dep].attributes[
                    "release_artifacts"
                ]
                if "langpack" in release_artifact
            ]
            for langpack_artifact_path in langpack_artifact_paths:
                locale = _extract_locale_from_langpack_artifact_path(
                    langpack_artifact_path
                )
                fetches[l10n_signing_dep].append(
                    {
                        "artifact": langpack_artifact_path,
                        "extract": False,
                        "dest": f"/builds/worker/fetches/{locale}.langpack.xpi",
                    }
                )
        task["dependencies"] = {**task["dependencies"], **l10n_signing_deps}
        task["fetches"] = dict(fetches)
        yield task
