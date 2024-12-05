# -*- coding: utf-8 -*-"
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import json
import os
from pathlib import Path

import pytest
from mozunit import main
from skipfails import Kind, Skipfails

DATA_PATH = Path(__file__).with_name("data")


def test_get_revision():
    """Test get_revision"""

    sf = Skipfails()
    with pytest.raises(ValueError) as e_info:
        sf.get_revision("")
    assert str(e_info.value) == "try_url scheme not https"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://foo.bar")
    assert str(e_info.value) == "try_url server not treeherder.mozilla.org"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://treeherder.mozilla.org")
    assert str(e_info.value) == "try_url query missing"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://treeherder.mozilla.org?a=1")
    assert str(e_info.value) == "try_url query missing revision"

    revision, repo = sf.get_revision(
        "https://treeherder.mozilla.org/jobs?repo=try&revision=5b1738d0af571777199ff3c694b1590ff574946b"
    )
    assert revision == "5b1738d0af571777199ff3c694b1590ff574946b"
    assert repo == "try"


def test_get_tasks():
    """Test get_tasks import of mozci"""

    from mozci.push import Push

    revision = "5b1738d0af571777199ff3c694b1590ff574946b"
    repo = "try"
    push = Push(revision, repo)
    assert push is not None


def get_failures(
    tasks_name, exp_f_name, task_details=None, error_summary=None, implicit_vars=False
):
    """Runs Skipfails.get_failures on tasks to compare with failures"""
    sf = Skipfails(implicit_vars=implicit_vars)
    assert sf.implicit_vars == implicit_vars
    if task_details is not None:  # preload task details cache, if needed
        if isinstance(task_details, str):  # read file
            task_details = sf.read_json(DATA_PATH.joinpath(task_details))
        sf.tasks = task_details
    if error_summary is not None:  # preload task details cache, if needed
        if isinstance(error_summary, str):  # read file
            error_summary = sf.read_json(DATA_PATH.joinpath(error_summary))
        sf.error_summary = error_summary
    tasks = sf.read_tasks(DATA_PATH.joinpath(tasks_name))
    exp_f = sf.read_failures(DATA_PATH.joinpath(exp_f_name))
    expected_failures = json.dumps(exp_f, indent=2, sort_keys=True).strip()
    failures = sf.get_failures(tasks)
    actual_failures = json.dumps(failures, indent=2, sort_keys=True).strip()
    assert actual_failures == expected_failures


def test_get_failures_1():
    """Test get_failures 1"""
    task_details = {"dwOJ8M9ERSmk6oI2KXg6hg": {}}
    get_failures("wayland-tasks-1.json", "wayland-failures-1.json", task_details)


def test_get_failures_2():
    """Test get_failures 2"""
    task_details = {
        "Y7r1q2xWSu-2bRAofEfeBw": {},
        "Z7r1q2xWSu-2bRAofEfeBw": {},
        "X7r1q2xWSu-2bRAofEfeBw": {},
    }
    get_failures("wayland-tasks-2.json", "wayland-failures-2.json", task_details)


def test_get_failures_3():
    """Test get_failures 3"""
    task_details = {
        "b7_ahjGtQ_-ZMNBG_hUZUw": {},
        "WVczuxkuSRKZg_jMiGyQsA": {},
        "UOZUIVAaTZKmRwArq5WkDw": {},
    }
    get_failures("wayland-tasks-3.json", "wayland-failures-3.json", task_details)


def test_get_failures_4():
    """Test get_failures 4"""
    task_details = {
        "bxMVPbPMTru_bfAivc1sPA": {},
        "EDql3NKPR3W6OEU3mLeKbg": {},
        "FDql3NKPR3W6OEU3mLeKbg": {},
    }
    get_failures("wayland-tasks-4.json", "wayland-failures-4.json", task_details)


def test_get_failures_5():
    """Test get_failures 5"""

    task_details = {
        "Bgc6We1sSjakIo3V9crldw": {
            "expires": "2024-01-09T16:05:56.825Z",
            "extra": {
                "suite": "mochitest-browser",
                "test-setting": {
                    "build": {"type": "opt"},
                    "platform": {
                        "arch": "64",
                        "os": {"name": "linux", "version": "22.04"},
                        "display": "wayland",
                    },
                    "runtime": {},
                },
            },
        }
    }
    get_failures("wayland-tasks-5.json", "wayland-failures-5.json", task_details)


def test_get_failures_6():
    """Test get_failures 6"""

    task_details = {
        "AKYqxtoWStigj_5yHVqAeg": {
            "expires": "2024-03-19T03:29:11.050Z",
            "extra": {
                "suite": "web-platform-tests",
                "test-setting": {
                    "build": {
                        "type": "opt",
                        "shippable": True,
                    },
                    "platform": {
                        "arch": "32",
                        "os": {"name": "linux", "version": "1804"},
                    },
                    "runtime": {},
                },
            },
        }
    }
    get_failures("wpt-tasks-1.json", "wpt-failures-1.json", task_details)


def test_get_failures_7():
    """Test get_failures 7"""

    get_failures(
        "reftest-tasks-1.json",
        "reftest-failures-1.json",
        "reftest-extra-1.json",
        "reftest-summary-1.json",
        True,
    )


def test_get_bug_by_id():
    """Test get_bug_by_id"""

    sf = Skipfails()
    id = 1682371
    # preload bug cache
    bugs_filename = f"bug-{id}.json"
    sf.bugs = sf.read_bugs(DATA_PATH.joinpath(bugs_filename))
    # function under test
    bug = sf.get_bug_by_id(id)
    assert bug is not None
    assert bug.id == id
    assert bug.product == "Testing"
    assert bug.component == "General"
    assert (
        bug.summary
        == "create tool to quickly parse and identify all failures from a try push and ideally annotate manifests"
    )


def test_get_bugs_by_summary():
    """Test get_bugs_by_summary"""

    sf = Skipfails()
    id = 1682371
    # preload bug cache
    bugs_filename = f"bug-{id}.json"
    sf.bugs = sf.read_bugs(DATA_PATH.joinpath(bugs_filename))
    # function under test
    summary = "create tool to quickly parse and identify all failures from a try push and ideally annotate manifests"
    bugs = sf.get_bugs_by_summary(summary)
    assert len(bugs) == 1
    assert bugs[0].id == id
    assert bugs[0].product == "Testing"
    assert bugs[0].component == "General"
    assert bugs[0].summary == summary


def test_get_variants():
    """Test get_variants"""

    sf = Skipfails()
    variants = sf.get_variants()
    assert "1proc" in variants
    assert variants["1proc"] == "e10s"
    assert "webrender-sw" in variants
    assert variants["webrender-sw"] == "swgl"
    assert "aab" in variants
    assert variants["aab"] == "aab"


def test_task_to_skip_if():
    """Test task_to_skip_if"""

    # Windows 2009 task
    sf = Skipfails()
    task_id = "UP-t3xrGSDWvUNjFGIt_aQ"
    task_details = {
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "mochitest-plain",
            "test-setting": {
                "build": {"type": "debug"},
                "platform": {
                    "arch": "32",
                    "os": {"build": "2009", "name": "windows", "version": "11"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {
            "win": {
                "11.2009": {"x86": {"debug": ["no_variant"], "opt": ["no_variant"]}}
            }
        }
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert skip_if == "win11_2009 && processor == 'x86' && debug"

    # Failed task on specific runtime on x86_64
    sf = Skipfails()
    task_id = "I3iXyGDATDSDyzGh4YfNJw"
    task_details = {
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "web-platform-tests-crashtest",
            "test-setting": {
                "build": {"type": "debug"},
                "platform": {
                    "arch": "64",
                    "os": {"name": "macosx", "version": "1015"},
                },
                "runtime": {"webrender-sw": True},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {
            "mac": {
                "10.15": {
                    "x86_64": {"debug": ["swgl", "no_variant"], "opt": ["no_variant"]}
                }
            }
        }
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if
        == "os == 'mac' && os_version == '10.15' && processor == 'x86_64' && debug && swgl"
    )

    # Failed task on specific runtime on aarch64
    sf = Skipfails()
    task_id = "bAkMaQIVQp6oeEIW6fzBDw"
    task_details = {
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "mochitest-media",
            "test-setting": {
                "build": {"type": "debug"},
                "platform": {
                    "arch": "aarch64",
                    "os": {"name": "macosx", "version": "1100"},
                },
                "runtime": {"webrender-sw": True},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {
            "mac": {
                "11.00": {
                    "aarch64": {"debug": ["swgl", "no_variant"], "opt": ["no_variant"]}
                }
            }
        }
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if
        == "os == 'mac' && os_version == '11.00' && processor == 'aarch64' && debug && swgl"
    )

    # Do not include build type or test variant if everything failed
    sf = Skipfails()
    task_id = "AKYqxtoWStigj_5yHVqAeg"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {"linux": {"18.04": {"x86": {"opt": ["no_variant"]}}}}
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert skip_if == "os == 'linux' && os_version == '18.04' && processor == 'x86'"

    sf = Skipfails()
    task_id = "QFo2jGFvTKGVcoqHCBpMGw"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {"xorigin": True},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {"linux": {"18.04": {"x86": {"opt": ["xorigin"]}}}}
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert skip_if == "os == 'linux' && os_version == '18.04' && processor == 'x86'"

    # Only the test without variant failed
    sf = Skipfails()
    task_id = "Xvdt2gbEQ3iDVAZPddY9PQ"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {
            "linux": {"18.04": {"x86": {"opt": ["no_variant", "xorigin"]}}}
        }
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if
        == "os == 'linux' && os_version == '18.04' && processor == 'x86' && opt && !xorigin"
    )

    # Missing platform permutation for the task
    sf = Skipfails()
    task_id = "czj2mQwqQv6PwON5aijPJg"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {}
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert skip_if == "os == 'linux' && os_version == '18.04' && processor == 'x86'"

    # Full fail with everal tasks
    sf = Skipfails()
    sf.platform_permutations = {
        "test-manifest": {
            "linux": {
                "18.04": {
                    "x86": {"opt": ["no_variant", "xorigin"], "debug": ["no_variant"]}
                }
            }
        }
    }
    task_id = "PPWic3zuRIyGdzUXC-XCvw"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {"xorigin": True},
            },
        },
    }
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if
        == "os == 'linux' && os_version == '18.04' && processor == 'x86' && opt && xorigin"
    )

    task_id = "c_OXt3mESB-G-aElu0hoxg"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if == "os == 'linux' && os_version == '18.04' && processor == 'x86' && opt"
    )

    task_id = "ShPeY1F8SY6Gm-1VSjsyUA"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "debug",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert skip_if == "os == 'linux' && os_version == '18.04' && processor == 'x86'"

    # Multiple failed tasks allowing for optimized skip if
    sf = Skipfails()
    sf.platform_permutations = {
        "test-manifest": {
            "linux": {
                "18.04": {
                    "x86": {"opt": ["no_variant"], "debug": ["no_variant", "xorigin"]}
                }
            }
        }
    }
    task_id = "Zc17K1IQRXOsGoSgegA_kA"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "debug",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {"xorigin": True},
            },
        },
    }
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if
        == "os == 'linux' && os_version == '18.04' && processor == 'x86' && debug && xorigin"
    )

    task_id = "ChOXnndsQQODAGpDqscbMg"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "debug",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert (
        skip_if
        == "os == 'linux' && os_version == '18.04' && processor == 'x86' && debug"
    )

    task_id = "caDMGUmnT7muCqNWj6w3nQ"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.TOML, "test-path")
    assert skip_if == "os == 'linux' && os_version == '18.04' && processor == 'x86'"


def test_task_to_skip_if_wpt():
    """Test task_to_skip_if_wpt"""

    # preload task cache
    task_id = "AKYqxtoWStigj_5yHVqAeg"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "web-platform-tests",
            "test-setting": {
                "build": {
                    "type": "opt",
                    "shippable": True,
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf = Skipfails()
    sf.tasks[task_id] = task_details
    sf.platform_permutations = {
        "test-manifest": {
            "linux": {
                "18.04": {"x86": {"debug": ["no_variant"], "opt": ["no_variant"]}}
            }
        }
    }
    # function under test
    skip_if = sf.task_to_skip_if("test-manifest", task_id, Kind.WPT, "test-path")
    assert (
        skip_if
        == 'os == "linux" and os_version == "18.04" and processor == "x86" and opt'
    )


def test_task_to_skip_if_reftest():
    """Test task_to_skip_if_reftest"""

    # preload task cache
    task_id = "AKYqxtoWStigj_5yHVqAeg"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "reftest",
            "test-setting": {
                "build": {
                    "type": "opt",
                    "shippable": True,
                },
                "platform": {
                    "arch": "32",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {},
            },
        },
    }
    sf = Skipfails(implicit_vars=True)
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("", task_id, Kind.LIST, "test-path")
    assert skip_if == "gtkWidget&&optimized&&!is64Bit"


def test_task_to_skip_if_reftest2():
    """Test task_to_skip_if_reftest2"""

    # preload task cache
    task_id = "ajp7DRgGQbyfnIAklKA7Tw"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "reftest",
            "test-setting": {
                "build": {"tsan": True, "type": "opt"},
                "runtime": {"webrender-sw": True},
                "platform": {"os": {"name": "linux", "version": "1804"}, "arch": "64"},
            },
        },
    }
    sf = Skipfails(implicit_vars=True)
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("", task_id, Kind.LIST, "test-path")
    assert skip_if == "gtkWidget&&ThreadSanitizer&&swgl"


def test_task_to_skip_if_reftest3():
    """Test task_to_skip_if_reftest3"""

    # preload task cache
    task_id = "UP-t3xrGSDWvUNjFGIt_aQ"
    task_details = {
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "mochitest-plain",
            "test-setting": {
                "build": {"type": "debug"},
                "platform": {
                    "arch": "32",
                    "os": {"build": "2009", "name": "windows", "version": "11"},
                },
                "runtime": {},
            },
        },
    }
    sf = Skipfails(implicit_vars=False)
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("", task_id, Kind.LIST, "test-path")
    assert skip_if == "winWidget&&isDebugBuild&&fission&&!is64Bit&&!swgl"


def test_task_to_skip_if_reftest4():
    """Test task_to_skip_if_reftest4"""

    # preload task cache
    task_id = "ajp7DRgGQbyfnIAklKA7Tw"
    task_details = {
        "expires": "2024-03-19T03:29:11.050Z",
        "extra": {
            "suite": "reftest",
            "test-setting": {
                "build": {"tsan": True, "type": "opt"},
                "runtime": {},
                "platform": {"os": {"name": "linux", "version": "1804"}, "arch": "64"},
            },
        },
    }
    sf = Skipfails(implicit_vars=False)
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if("", task_id, Kind.LIST, "test-path")
    assert skip_if == "gtkWidget&&ThreadSanitizer&&fission&&!swgl"


def test_wpt_add_skip_if():
    """Test wpt_add_skip_if"""

    sf = Skipfails()
    manifest_before1 = ""
    anyjs = {}
    filename = "myfile.html"
    anyjs[filename] = False
    skip_if = 'os == "linux" and processor == "x86" and not debug'
    bug_reference = "Bug 123"
    disabled = "  disabled:\n"
    condition = "    if " + skip_if + ": " + bug_reference + "\n"
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before1, anyjs, skip_if, bug_reference
    )
    manifest_expected1 = "[myfile.html]\n" + disabled + condition + "\n"
    assert manifest_str == manifest_expected1
    manifest_before2 = """[myfile.html]
  expected:
    if fission: [OK, FAIL]
  [< [Test 5\\] 1 out of 2 assertions were failed.]
    expected: FAIL
"""
    manifest_expected2 = (
        """[myfile.html]
  expected:
    if fission: [OK, FAIL]
"""
        + disabled
        + condition
        + """
  [< [Test 5\\] 1 out of 2 assertions were failed.]
    expected: FAIL
"""
    )
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before2, anyjs, skip_if, bug_reference
    )
    assert manifest_str == manifest_expected2
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_expected2, anyjs, skip_if, bug_reference
    )
    assert manifest_str == manifest_expected2
    manifest_before4 = """;https: //bugzilla.mozilla.org/show_bug.cgi?id=1838684
expected: [FAIL, PASS]
[custom-highlight-painting-overlapping-highlights-002.html]
  expected:
    [PASS, FAIL]
"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before4, anyjs, skip_if, bug_reference
    )
    manifest_expected4 = manifest_before4 + "\n" + manifest_expected1
    assert manifest_str == manifest_expected4
    manifest_before5 = """[myfile.html]
  disabled:
    if win11_2009 && processor == '32' && debug: Bug 456
"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before5, anyjs, skip_if, bug_reference
    )
    manifest_expected5 = manifest_before5 + condition + "\n"
    assert manifest_str == manifest_expected5
    manifest_before6 = """[myfile.html]
  [Window Size]
    expected:
      if product == "firefox_android": FAIL
"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before6, anyjs, skip_if, bug_reference
    )
    manifest_expected6 = (
        """[myfile.html]
  disabled:
"""
        + condition
        + """
  [Window Size]
    expected:
      if product == "firefox_android": FAIL
"""
    )
    assert manifest_str == manifest_expected6
    manifest_before7 = """[myfile.html]
  fuzzy:
    if os == "android": maxDifference=0-255;totalPixels=0-105 # bug 1392254
    if os == "linux": maxDifference=0-255;totalPixels=0-136 # bug 1599638
    maxDifference=0-1;totalPixels=0-80
  disabled:
    if os == "win": https://bugzilla.mozilla.org/show_bug.cgi?id=1314684
"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before7, anyjs, skip_if, bug_reference
    )
    manifest_expected7 = manifest_before7 + condition + "\n"
    assert manifest_str == manifest_expected7
    manifest_before8 = """[myfile.html]
  disabled:
    if os == "linux" and os_version == "22.04" and processor == "x86_64" and debug and display == "wayland": Bug TBD
  expected:
    if swgl and (os == "linux") and debug and not fission: [PASS, FAIL]

"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before8, anyjs, skip_if, bug_reference
    )
    manifest_expected8 = (
        """[myfile.html]
  disabled:
    if os == "linux" and os_version == "22.04" and processor == "x86_64" and debug and display == "wayland": Bug TBD
"""
        + condition
        + """  expected:
    if swgl and (os == "linux") and debug and not fission: [PASS, FAIL]

"""
    )
    assert manifest_str == manifest_expected8
    manifest_before9 = """[myfile.html]
  disabled:
    if os == "linux" and os_version == "22.04" and processor == "x86_64" and debug and display == "wayland": Bug TBD

"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before9, anyjs, skip_if, bug_reference
    )
    manifest_expected9 = (
        """[myfile.html]
  disabled:
    if os == "linux" and os_version == "22.04" and processor == "x86_64" and debug and display == "wayland": Bug TBD
"""
        + condition
        + "\n"
    )
    assert manifest_str == manifest_expected9
    manifest_before10 = """[myfile.html]
  disabled:
    if os == "linux" and os_version == "22.04" and processor == "x86_64" and not debug and display == "wayland": Bug TBD

  [3P fetch: Cross site window setting HTTP cookies]
"""
    anyjs[filename] = False
    manifest_str, additional_comment_ = sf.wpt_add_skip_if(
        manifest_before10, anyjs, skip_if, bug_reference
    )
    manifest_expected10 = (
        """[myfile.html]
  disabled:
    if os == "linux" and os_version == "22.04" and processor == "x86_64" and not debug and display == "wayland": Bug TBD
"""
        + condition
        + """\n  [3P fetch: Cross site window setting HTTP cookies]
"""
    )
    assert manifest_str == manifest_expected10


def test_get_filename_in_manifest():
    """Test get_filename_in_manifest"""

    sf = Skipfails()

    assert (
        sf.get_filename_in_manifest(
            "browser/components/sessionstore/test/browser.toml",
            "browser/components/sessionstore/test/browser_closed_tabs_windows.js",
        )
        == "browser_closed_tabs_windows.js"
    )
    assert (
        sf.get_filename_in_manifest(
            "browser/base/content/test/webrtc/gracePeriod/browser.toml",
            "browser/base/content/test/webrtc/browser_devices_get_user_media_grace.js",
        )
        == "../browser_devices_get_user_media_grace.js"
    )
    assert (
        sf.get_filename_in_manifest(
            "dom/animation/test/mochitest.toml",
            "dom/animation/test/document-timeline/test_document-timeline.html",
        )
        == "document-timeline/test_document-timeline.html"
    )


def test_label_to_platform_testname():
    """Test label_to_platform_testname"""

    sf = Skipfails()
    label = "test-linux2204-64-wayland/opt-mochitest-browser-chrome-swr-13"
    platform, testname = sf.label_to_platform_testname(label)
    assert platform == "test-linux2204-64-wayland/opt"
    assert testname == "mochitest-browser-chrome"


def test_reftest_add_fuzzy_if():
    """Test reftest_add_fuzzy_if"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-1-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-1-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(0-11,0-7155) == blur-inside-clipPath.svg blur-inside-clipPath-ref.svg",
        "gtkWidget",
        [10, 12],
        [7000, 8000],
        2,
        False,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if2():
    """Test reftest_add_fuzzy_if2"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-2-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-2-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(0-11,0-7155) HTTP(..) == blur-inside-clipPath.svg  blur-inside-clipPath-ref.svg",
        "gtkWidget",
        [10, 12],
        [7000, 8000],
        2,
        False,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if3():
    """Test reftest_add_fuzzy_if3"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-2-after.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-3-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(0-11,0-7155) fuzzy-if(gtkWidget,10-12,7000-8400) HTTP(..) == blur-inside-clipPath.svg blur-inside-clipPath-ref.svg",
        "gtkWidget",
        [9, 13],
        [7500, 7900],
        2,
        False,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if4():
    """Test reftest_add_fuzzy_if4"""

    # Now with the relaxed merge policy the additional condition has been merged
    # instead of added....
    # fuzzy-if(winWidget,0-177,0-1) fuzzy-if(winWidget&&!is64Bit&&isDebugBuild,0-152,0-1) == 23605-5.html 23605-5-ref.html # Bug TBD

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-4-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-4-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy-if(winWidget,0-177,0-1) == 23605-5.html 23605-5-ref.html",
        "winWidget&&!is64Bit&&isDebugBuild",
        [139, 145],
        [1, 1],
        2,
        True,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str
    # assert (
    #     additional_comment
    #     == "NOTE: more than one fuzzy-if for the OS = winWidget ==> may require manual review"
    # )


def test_reftest_add_fuzzy_if5():
    """Test reftest_add_fuzzy_if5"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-2-after.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-5-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(0-11,0-7155) fuzzy-if(gtkWidget,10-12,7000-8400) HTTP(..) == blur-inside-clipPath.svg blur-inside-clipPath-ref.svg",
        "gtkWidget&&!is64Bit",
        [5, 6],
        [3000, 4000],
        2,
        False,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if6():
    """Test reftest_add_fuzzy_if6"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-6-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-6-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(16-51,5234-5622) fuzzy-if(swgl,32-38,1600-91746) fuzzy-if(useDrawSnapshot,16-16,11600-11600) fuzzy-if(cocoaWidget,16-73,5212-5622) == ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.av1.webm ../reftest_img.html?src=color_quads/720p.png",
        "Android&&isDebugBuild&&!fission&&swgl",
        [1, 32],
        [1, 1760],
        2,
        True,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if7():
    """Test reftest_add_fuzzy_if7"""

    sf = Skipfails(implicit_vars=False)
    manifest_path = DATA_PATH.joinpath("reftest-7-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-7-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "skip-if(winWidget&&isCoverageBuild) fuzzy(0-16,75-1941) fuzzy-if(Android,28-255,273680-359920) fuzzy-if(appleSilicon,30-48,1835-187409) fuzzy-if(cocoaWidget,30-32,187326-187407) == ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.h264.mp4 ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.av1.webm",
        "cocoaWidget",
        [],
        [],
        2,
        False,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if8():
    """Test reftest_add_fuzzy_if8"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-6-after.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-8-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(16-51,5234-5622) fuzzy-if(swgl,32-38,1600-91746) fuzzy-if(useDrawSnapshot,16-16,11600-11600) fuzzy-if(Android&&isDebugBuild&&!fission&&swgl,0-33,0-1848) fuzzy-if(cocoaWidget,16-73,5212-5622) == ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.av1.webm ../reftest_img.html?src=color_quads/720p.png",
        "cocoaWidget",
        [0, 0],
        [0, 0],
        2,
        True,
        "Bug TBD",
    )
    assert additional_comment == ""
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if9():
    """Test reftest_add_fuzzy_if9"""

    sf = Skipfails(implicit_vars=True)
    manifest_path = DATA_PATH.joinpath("reftest-9-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-9-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "fuzzy(0-201,0-1486) fuzzy-if(Android&&(fission||!fission),201-267,42-1560) fuzzy-if(Android&&!fission&&(swgl||!swgl),201-267,40-1560) fuzzy-if(Android&&isDebugBuild&&!fission&&swgl,198-207,1439-1510) fuzzy-if(cocoaWidget&&isDebugBuild&&swgl,198-267,40-1510) fuzzy-if(gtkWidget&&swgl,198-267,40-1510) fuzzy-if(gtkWidget&&!fission&&swgl,0-267,0-1510) fuzzy-if(gtkWidget&&!fission&&(swgl||!swgl),201-267,40-1560) == element-paint-transform-02.html element-paint-transform-02-ref.html",
        "gtkWidget&&isDebugBuild&&!fission&&swgl",
        [198],
        [1439],
        2,
        True,
        "Bug TBD",
    )
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if10():
    """Test reftest_add_fuzzy_if10"""

    sf = Skipfails(implicit_vars=False)
    manifest_path = DATA_PATH.joinpath("reftest-10-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    after_path = DATA_PATH.joinpath("reftest-10-after.list")
    after_str = io.open(after_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "skip-if(winWidget&&isCoverageBuild) fuzzy(0-16,75-1941) fuzzy-if(Android,28-255,273680-359920) fuzzy-if(appleSilicon,30-48,1835-187409) fuzzy-if(cocoaWidget,0-0,0-0) == ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.h264.mp4 ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.av1.webm",
        "cocoaWidget",
        [],
        [],
        2,
        True,
        "Bug TBD",
    )
    assert additional_comment == "fuzzy-if removed as calculated range is 0-0,0-0"
    assert manifest_str == after_str


def test_reftest_add_fuzzy_if11():
    """Test reftest_add_fuzzy_if11"""

    sf = Skipfails(implicit_vars=False)
    manifest_path = DATA_PATH.joinpath("reftest-10-before.list")
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    manifest_str, additional_comment = sf.reftest_add_fuzzy_if(
        manifest_str,
        "skip-if(winWidget&&isCoverageBuild) fuzzy(0-16,75-1941) fuzzy-if(Android,28-255,273680-359920) fuzzy-if(cocoaWidget,30-32,187326-187407) fuzzy-if(appleSilicon,30-48,1835-187409) == ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.h264.mp4 ../reftest_video.html?src=color_quads/720p.png.bt709.bt709.tv.yuv420p.av1.webm",
        "winWidget",
        [],
        [],
        2,
        True,
        "Bug TBD",
    )
    assert additional_comment == "fuzzy-if not added as calculated range is 0-0,0-0"
    assert manifest_str == ""


def test_reftest_get_lineno():
    """Test reftest_get_lineno"""

    sf = Skipfails()
    mods = [
        "pref(gfx.font_rendering.colr_v1.enabled,true)",
        "fuzzy(0-8,0-10100)",
        "==",
        "colrv1-01.html#C",
        "colrv1-01-ref.html#C",
    ]
    allmods = " ".join(mods)
    lineno = sf.reftest_find_lineno(
        "testing/test/data/fontface_reftest.list", mods, allmods
    )
    assert lineno == 171


def test_reftest_get_lineno2():
    """Test reftest_get_lineno2"""

    sf = Skipfails()
    mods = [
        "pref(image.downscale-during-decode.enabled,true)",
        "fuzzy(0-53,0-6391)",
        "fuzzy-if(appleSilicon,0-20,0-11605)",
        "fuzzy-if(gtkWidget,18-19,5502-5568)",
        "skip-if(Android)",
        "==",
        "downscale-moz-icon-1.html",
        "downscale-moz-icon-1-ref.html",
    ]
    allmods = " ".join(mods)
    lineno = sf.reftest_find_lineno(
        "testing/test/data/downscaling_reftest.list", mods, allmods
    )
    assert lineno == 183


def test_reftest_get_lineno3():
    """Test reftest_get_lineno3"""

    sf = Skipfails()
    mods = [
        "pref(webgl.force-enabled,true)",
        "skip-if(Android)",
        "fuzzy(0-235,0-3104)",
        "==",
        "1177726-text-stroke-bounds.html",
        "1177726-text-stroke-bounds-ref.html",
    ]
    allmods = " ".join(mods)
    lineno = sf.reftest_find_lineno(
        "testing/test/data/dom_canvas_reftest.list", mods, allmods
    )
    assert lineno == 233


def test_reftest_skip_failure_win_32(capsys):
    """Test reftest_skip_failure_win_32"""

    sf = Skipfails(verbose=True, bugzilla="disable", implicit_vars=True)
    manifest = "layout/reftests/svg/reftest.list"
    kind = Kind.LIST
    path = "fuzzy(0-1,0-5) fuzzy-if(winWidget,0-96,0-21713) skip-if(winWidget&&isCoverageBuild) fuzzy-if(Android&&device,0-4,0-946) == radialGradient-basic-03.svg radialGradient-basic-03-ref.html"
    anyjs = None
    differences = [68]
    pixels = [21668]
    lineno = 419
    status = "FAIL"
    label = "test-windows11-32-2009-qr/debug-reftest-1"
    classification = "disable_recommended"
    task_id = "BpoP8I2CRZekXUKoSIZjUQ"
    try_url = "https://treeherder.mozilla.org/jobs?repo=try&tier=1%2C2%2C3&revision=3e54b0b81de7d6a3e6a2c3408892ffd6430bc137&selectedTaskRun=BpoP8I2CRZekXUKoSIZjUQ.0"
    revision = "3e54b0b81de7d6a3e6a2c3408892ffd6430bc137"
    repo = "try"
    meta_bug_id = None
    task_details = {  # pre-cache task details
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "reftest",
            "test-setting": {
                "build": {"type": "debug"},
                "platform": {
                    "arch": "32",
                    "os": {"build": "2009", "name": "windows", "version": "11"},
                },
                "runtime": {},
            },
        },
    }
    sf.tasks[task_id] = task_details

    path_to_task = {}
    path_to_task[path] = task_id
    sf.skip_failure(
        manifest,
        kind,
        path_to_task,
        anyjs,
        differences,
        pixels,
        lineno,
        status,
        label,
        classification,
        task_id,
        try_url,
        revision,
        repo,
        meta_bug_id,
    )
    capsys.readouterr()
    # assert (
    #     captured.err.find(
    #         "Skipping failures for Windows 32-bit are temporarily disabled"
    #     )
    #     > 0
    # )


def test_reftest_skip_failure_reorder(capsys):
    """Test reftest_skip_failure_reorder"""

    manifest_before_path = DATA_PATH.joinpath("reftest-reorder-before.list")
    manifest_before = io.open(manifest_before_path, "r", encoding="utf-8").read()
    manifest_path = DATA_PATH.joinpath("reftest-reorder.list")
    manifest_fp = io.open(manifest_path, "w", encoding="utf-8")
    manifest_fp.write(manifest_before)
    manifest_fp.close()
    manifest_after_path = DATA_PATH.joinpath("reftest-reorder-after.list")
    manifest_after = io.open(manifest_after_path, "r", encoding="utf-8").read()
    sf = Skipfails(verbose=True, bugzilla="disable", implicit_vars=True)
    manifest = "testing/test/data/reftest-reorder.list"
    kind = Kind.LIST
    path = "fuzzy-if(cocoaWidget,0-80,0-76800) fuzzy-if(appleSilicon,0-80,0-76800) skip-if(Android) fuzzy-if(winWidget,0-63,0-76799) fuzzy-if(gtkWidget,0-70,0-2032) HTTP(..) == short.mp4.firstframe.html short.mp4.firstframe-ref.html"
    anyjs = None
    differences = [59, 61]
    pixels = [2032, 2133]
    lineno = 2
    status = "FAIL"
    label = "test-linux1804-64-asan-qr/opt-reftest-nofis-7"
    classification = "disable_failure"
    task_id = "a9Pz5yFJTxOL_uM-BEvGoQ"
    try_url = "https://treeherder.mozilla.org/jobs?repo=try&tier=1%2C2%2C3&revision=3e54b0b81de7d6a3e6a2c3408892ffd6430bc137&selectedTaskRun=BpoP8I2CRZekXUKoSIZjUQ.0"
    revision = "3e54b0b81de7d6a3e6a2c3408892ffd6430bc137"
    repo = "try"
    meta_bug_id = None
    task_details = {  # pre-cache task details
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "reftest",
            "test-setting": {
                "build": {"asan": True, "type": "opt"},
                "platform": {
                    "arch": "64",
                    "os": {"name": "linux", "version": "1804"},
                },
                "runtime": {"no-fission": True},
            },
        },
    }
    sf.tasks[task_id] = task_details

    path_to_task = {}
    path_to_task[path] = task_id
    sf.skip_failure(
        manifest,
        kind,
        path_to_task,
        anyjs,
        differences,
        pixels,
        lineno,
        status,
        label,
        classification,
        task_id,
        try_url,
        revision,
        repo,
        meta_bug_id,
    )
    assert os.path.exists(manifest_path)
    manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
    assert manifest_str == manifest_after
    os.remove(manifest_path)
    # captured = capsys.readouterr()
    # assert (
    #     captured.err.find(
    #         "NOTE: more than one fuzzy-if for the OS = gtkWidget ==> may require manual review"
    #     )
    #     > 0
    # )


if __name__ == "__main__":
    main()
