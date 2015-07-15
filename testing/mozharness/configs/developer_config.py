"""
This config file can be appended to any other mozharness job
running under treeherder. The purpose of this config is to
override values that are specific to Release Engineering machines
that can reach specific hosts within their network.
In other words, this config allows you to run any job
outside of the Release Engineering network

Using this config file should be accompanied with using
--test-url and --installer-url where appropiate
"""

import os
LOCAL_WORKDIR = os.path.expanduser("~/.mozilla/releng")

config = {
    # Developer mode values
    "developer_mode": True,
    "local_workdir": LOCAL_WORKDIR,

    # General local variable overwrite
    "exes": {},
    "find_links": ["http://pypi.pub.build.mozilla.org/pub"],
    "replace_urls": [
        ("http://pvtbuilds.pvt.build",
         "https://pvtbuilds"),
        ("http://tooltool.pvt.build.mozilla.org/build",
         "https://secure.pub.build.mozilla.org/tooltool/pvt/build")
    ],

    # Talos related
    "python_webserver": True,
    "virtualenv_path": '%s/build/venv' % os.getcwd(),

    # Tooltool related
    "tooltool_servers": [
        "https://secure.pub.build.mozilla.org/tooltool/pvt/build"
    ],
    "tooltool_cache": os.path.join(LOCAL_WORKDIR, "builds/tooltool_cache"),
    "tooltool_cache_path": os.path.join(LOCAL_WORKDIR,
                                        "builds/tooltool_cache"),
    # Android related
    "host_utils_url":
        "https://secure.pub.build.mozilla.org/tooltool" +
        "/pvt/build/sha512/372c89f9dccaf5ee3b9d35fd1cfeb089e1e5db3ff1c04e35" +
        "aa3adc8800bc61a2ae10e321f37ae7bab20b56e60941f91bb003bcb22035902a73" +
        "d70872e7bd3282",
}
