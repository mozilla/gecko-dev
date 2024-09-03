#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import pathlib
from unittest import mock

import mozunit
import pytest

from mozperftest.environment import TEST
from mozperftest.tests.support import get_running_env
from mozperftest.utils import silence

HERE = pathlib.Path(__file__).parent
MOCK_DATA_DIR = HERE / "data" / "alert-mock-data"


def get_alert_layer(layers):
    for layer in layers:
        if layer.__class__.__name__ == "AlertTestRunner":
            return layer
    return None


@pytest.mark.parametrize(
    "alert_json, expected_command",
    (
        ("alert-summary-awsy.json", ["./mach", "awsy-test", "--base"]),
        ("alert-summary-talos.json", ["./mach", "talos-test", "-a", "canvas2dvideo"]),
        ("alert-summary-raptor-android.json", ["./mach", "raptor", "-t", "reddit"]),
        ("alert-summary-raptor-desktop.json", ["./mach", "raptor", "-t", "yahoo-mail"]),
    ),
)
def test_alert_basic_command(alert_json, expected_command):
    args = {"flavor": "alert", "tests": ["9000"]}

    mach_cmd, metadata, env = get_running_env(**args)
    test = env.layers[TEST]
    alert_layer = get_alert_layer(test.layers)
    line_handler = alert_layer.create_line_handler("a-test")

    with mock.patch(
        "mozperftest.test.alert.requests.get"
    ) as mocked_request, mock.patch(
        "mozperftest.test.alert.mozprocess"
    ) as mocked_mozprocess, (
        MOCK_DATA_DIR / alert_json
    ).open() as f:
        mocked_response = mock.MagicMock()
        mocked_response.configure_mock(status_code=200)
        mocked_response.json.return_value = json.load(f)
        mocked_request.return_value = mocked_response

        def _add_perfherder(*args, **kwargs):
            line_handler(mock.MagicMock(), b'PERFHERDER_DATA: {"name": "test"}')

        mocked_mozprocess.run_and_wait.side_effect = _add_perfherder

        with test as layer, silence(test):
            layer(metadata)

        mocked_mozprocess.run_and_wait.assert_called_once()
        assert mocked_mozprocess.run_and_wait.call_args[0][0] == expected_command
        assert len(alert_layer.perfherder_data) == 1


def test_alert_basic_command_failed():
    alert_json = "alert-summary-awsy.json"
    expected_command = ["./mach", "awsy-test", "--base"]
    args = {"flavor": "alert", "tests": ["9000"]}

    mach_cmd, metadata, env = get_running_env(**args)
    test = env.layers[TEST]
    alert_layer = get_alert_layer(test.layers)
    alert_layer.create_line_handler("a-test")

    with mock.patch(
        "mozperftest.test.alert.requests.get"
    ) as mocked_request, mock.patch(
        "mozperftest.test.alert.mozprocess"
    ) as mocked_mozprocess, (
        MOCK_DATA_DIR / alert_json
    ).open() as f:
        mocked_response = mock.MagicMock()
        mocked_response.configure_mock(status_code=200)
        mocked_response.json.return_value = json.load(f)
        mocked_request.return_value = mocked_response

        with test as layer, silence(test):
            layer(metadata)

        mocked_mozprocess.run_and_wait.assert_called_once()
        assert mocked_mozprocess.run_and_wait.call_args[0][0] == expected_command
        assert len(alert_layer.perfherder_data) == 0


@pytest.mark.parametrize(
    "alert_json, task_info_json, expected_commands, mozprocess_call_count",
    (
        (
            "alert-summary-awsy.json",
            "task-info-awsy.json",
            [
                [
                    "./mach",
                    "awsy-test",
                    "--base",
                    "--setpref=media.peerconnection.mtransport_process=false",
                    "--setpref=network.process.enabled=false",
                ]
            ],
            1,
        ),
        (
            "alert-summary-talos.json",
            "task-info-talos.json",
            [
                [
                    "./mach",
                    "talos-test",
                    "-a",
                    "canvas2dvideo",
                    "--setpref",
                    "webgl.out-of-process=true",
                    "--setpref",
                    "security.sandbox.content.headless=true",
                    "--setpref",
                    "media.peerconnection.mtransport_process=false",
                    "--setpref",
                    "network.process.enabled=false",
                    "--project",
                    "autoland",
                ]
            ],
            1,
        ),
        (
            "alert-summary-raptor-android.json",
            "task-info-raptor-android.json",
            [
                [
                    "./mach",
                    "raptor",
                    "-t",
                    "reddit",
                    "--app",
                    "fenix",
                    "-a",
                    "org.mozilla.fenix.IntentReceiverActivity",
                    "--chimera",
                    "--project",
                    "autoland",
                    "--device-name",
                    "a51",
                    "--disable-fission",
                    "--conditioned-profile",
                    "settled",
                    "--browsertime-video",
                    "--browsertime-visualmetrics",
                    "--browsertime-no-ffwindowrecorder",
                ]
            ],
            1,
        ),
        (
            "alert-summary-raptor-desktop.json",
            "task-info-raptor-desktop.json",
            [
                [
                    "./mach",
                    "raptor",
                    "-t",
                    "yahoo-mail",
                    "--chimera",
                    "--extra-profiler-run",
                    "--project",
                    "autoland",
                    "--conditioned-profile",
                    "settled",
                    "--browsertime-video",
                    "--browsertime-visualmetrics",
                ]
            ],
            1,
        ),
        (
            "alert-summary-mpt-android.json",
            "task-info-mpt-android.json",
            [
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mobile-browser",
                    "testing/performance/perftest_android_startup.js",
                    "--AndroidStartUp",
                    "--AndroidStartUp-test-name",
                    "cold_main_first_frame",
                    "--AndroidStartUp-product",
                    "fenix",
                    "--browsertime-cycles",
                    "0",
                    "--perfherder",
                ]
            ],
            1,
        ),
        (
            "alert-summary-mpt-desktop.json",
            "task-info-mpt-desktop.json",
            [
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_caching.html",
                ],
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_fetch.html",
                ],
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_registration.html",
                ],
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_update.html",
                ],
            ],
            4,
        ),
        (
            "alert-summary-mpt-desktop-windows.json",
            "task-info-mpt-desktop-windows.json",
            [
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_caching.html",
                ],
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_fetch.html",
                ],
                [
                    "./mach",
                    "perftest",
                    "--flavor",
                    "mochitest",
                    "dom/serviceworkers/test/performance/test_registration.html",
                ],
            ],
            3,
        ),
    ),
)
def test_alert_exact_command(
    alert_json, task_info_json, expected_commands, mozprocess_call_count
):
    args = {"flavor": "alert", "tests": ["9000"], "alert-exact": True}

    mach_cmd, metadata, env = get_running_env(**args)
    test = env.layers[TEST]

    with mock.patch(
        "mozperftest.test.alert.requests.get"
    ) as mocked_request, mock.patch(
        "mozperftest.test.alert.mozprocess"
    ) as mocked_mozprocess, (
        MOCK_DATA_DIR / alert_json
    ).open() as alert_file, (
        MOCK_DATA_DIR / task_info_json
    ).open() as task_file:
        mocked_alert_response = mock.MagicMock()
        mocked_alert_response.configure_mock(status_code=200)
        mocked_alert_response.json.return_value = json.load(alert_file)

        mocked_task_response = mock.MagicMock()
        mocked_task_response.configure_mock(status_code=200)
        mocked_task_response.json.return_value = json.load(task_file)

        mocked_request.side_effect = [mocked_alert_response, mocked_task_response]

        with test as layer, silence(test):
            layer(metadata)

        assert mocked_mozprocess.run_and_wait.call_count == mozprocess_call_count
        for i, expected_command in enumerate(expected_commands):
            assert expected_command in [
                call[0][0] for call in mocked_mozprocess.run_and_wait.call_args_list
            ]


def test_alert_info_failure():
    args = {"flavor": "alert", "tests": ["9000"]}

    mach_cmd, metadata, env = get_running_env(**args)
    test = env.layers[TEST]
    alert_layer = get_alert_layer(test.layers)

    with mock.patch("mozperftest.test.alert.requests.get") as mocked_request:
        mocked_alert_response = mock.MagicMock()
        mocked_alert_response.configure_mock(status_code=400)
        mocked_request.return_value = mocked_alert_response

        alert_layer._get_alert("9000")
        mocked_alert_response.raise_for_status.assert_called_once()

        mocked_task_response = mock.MagicMock()
        mocked_task_response.configure_mock(status_code=400)
        mocked_request.return_value = mocked_task_response

        alert_layer._get_task_info("9000")
        mocked_task_response.raise_for_status.assert_called_once()


def test_alert_line_handler():
    args = {"flavor": "alert", "tests": ["9000"]}

    mach_cmd, metadata, env = get_running_env(**args)
    test = env.layers[TEST]
    alert_layer = get_alert_layer(test.layers)

    line_handler = alert_layer.create_line_handler("a-test-name")

    proc_mock = mock.MagicMock()
    line_handler(proc_mock, b'PERFHERDER_DATA: {"name": "test"}')

    # Check for a single test, and single test run
    assert len(alert_layer.perfherder_data) == 1
    assert "a-test-name" in alert_layer.perfherder_data
    assert alert_layer.perfherder_data["a-test-name"] == [{"name": "test"}]

    # Check for a single test, and multiple runs
    line_handler(proc_mock, b'PERFHERDER_DATA: {"name": "test"}')
    assert len(alert_layer.perfherder_data["a-test-name"]) == 2

    # Check for multiple tests
    line_handler = alert_layer.create_line_handler("another-test-name")
    line_handler(proc_mock, b'PERFHERDER_DATA: {"name": "testing"}')

    assert len(alert_layer.perfherder_data) == 2
    assert "another-test-name" in alert_layer.perfherder_data
    assert alert_layer.perfherder_data["another-test-name"] == [{"name": "testing"}]


if __name__ == "__main__":
    mozunit.main()
