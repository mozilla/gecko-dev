# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from ..fixtures import *  # noqa: F403


def pytest_generate_tests(metafunc):
    """Generate tests based on markers."""

    if "session" not in metafunc.fixturenames:
        return

    marks = [mark.name for mark in metafunc.function.pytestmark]

    otherargs = {}
    argvalues = []
    ids = []

    if "only_firefox_versions" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "only_firefox_versions":
                otherargs["only_firefox_versions"] = mark.args

    if "only_platforms" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "only_platforms":
                otherargs["only_platforms"] = mark.args

    if "skip_platforms" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "skip_platforms":
                otherargs["skip_platforms"] = mark.args

    if "only_channels" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "only_channels":
                otherargs["only_channels"] = mark.args

    if "skip_channels" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "skip_channels":
                otherargs["skip_channels"] = mark.args

    if "with_interventions" in marks:
        argvalues.append([dict({"interventions": True}, **otherargs)])
        ids.append("with_interventions")

    if "without_interventions" in marks:
        argvalues.append([dict({"interventions": False}, **otherargs)])
        ids.append("without_interventions")

    if "need_visible_scrollbars" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "need_visible_scrollbars":
                otherargs["need_visible_scrollbars"] = mark.args

    if "actual_platform_required" in marks:
        otherargs["actual_platform_required"] = True

    if "no_overlay_scrollbars" in marks:
        otherargs["no_overlay_scrollbars"] = True

    if "disable_moztransform" in marks:
        otherargs["disable_moztransform"] = True

    metafunc.parametrize(["session"], argvalues, ids=ids, indirect=True)


@pytest.fixture(scope="function")  # noqa: F405
async def test_config(request, driver):
    params = request.node.callspec.params.get("session")

    use_interventions = params.get("interventions")
    print(f"use_interventions {use_interventions}")
    if use_interventions is None:
        raise ValueError(
            f"Missing intervention marker in {request.fspath}:{request.function.__name__}"
        )

    return {
        "actual_platform_required": params.get("actual_platform_required", False),
        "enable_moztransform": params.get("enable_moztransform", False),
        "disable_moztransform": params.get("disable_moztransform", False),
        "need_visible_scrollbars": params.get("need_visible_scrollbars", False),
        "no_overlay_scrollbars": params.get("no_overlay_scrollbars", False),
        "use_interventions": use_interventions,
        "use_pbm": params.get("with_private_browsing", False),
        "use_strict_etp": params.get("with_strict_etp", False),
        "without_tcp": params.get("without_tcp", False),
    }
