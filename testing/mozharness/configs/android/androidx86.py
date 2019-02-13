import os

config = {
    "buildbot_json_path": "buildprops.json",
    "host_utils_url": "http://talos-remote.pvt.build.mozilla.org/tegra/tegra-host-utils.Linux.1109310.2.zip",
    "robocop_package_name": "org.mozilla.roboexample.test",
    "device_ip": "127.0.0.1",
    "default_sut_port1": "20701",
    "default_sut_port2": "20700", # does not prompt for commands
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86/releng.manifest",
    "tooltool_cache": "/builds/tooltool_cache",
    "emulator_manifest": """
        [
        {
        "size": 193383673,
        "digest": "6609e8b95db59c6a3ad60fc3dcfc358b2c8ec8b4dda4c2780eb439e1c5dcc5d550f2e47ce56ba14309363070078d09b5287e372f6e95686110ff8a2ef1838221",
        "algorithm": "sha512",
        "filename": "android-sdk18_0.r18moz1.orig.tar.gz",
        "unpack": "True"
        }
        ] """,
    "emulator_process_name": "emulator64-x86",
    "emulator_extra_args": "-debug init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket -qemu -m 1024 -enable-kvm",
    "device_manager": "sut",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk18/platform-tools/adb',
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk18/tools:%(abs_work_dir)s/android-sdk18/platform-tools",
    },
    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'setup-avds',
        'start-emulators',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
        'stop-emulators',
    ],
    "emulators": [
        {
            "name": "test-1",
            "device_id": "emulator-5554",
            "http_port": "8854", # starting http port to use for the mochitest server
            "ssl_port": "4454", # starting ssl port to use for the server
            "emulator_port": 5554,
            "sut_port1": 20701,
            "sut_port2": 20700
        },
        {
            "name": "test-2",
            "device_id": "emulator-5556",
            "http_port": "8856", # starting http port to use for the mochitest server
            "ssl_port": "4456", # starting ssl port to use for the server
            "emulator_port": 5556,
            "sut_port1": 20703,
            "sut_port2": 20702
        },
        {
            "name": "test-3",
            "device_id": "emulator-5558",
            "http_port": "8858", # starting http port to use for the mochitest server
            "ssl_port": "4458", # starting ssl port to use for the server
            "emulator_port": 5558,
            "sut_port1": 20705,
            "sut_port2": 20704
        },
        {
            "name": "test-4",
            "device_id": "emulator-5560",
            "http_port": "8860", # starting http port to use for the mochitest server
            "ssl_port": "4460", # starting ssl port to use for the server
            "emulator_port": 5560,
            "sut_port1": 20707,
            "sut_port2": 20706
        }
    ],
    "test_suite_definitions": {
        "jsreftest": {
            "category": "reftest",
            "extra_args": ["../jsreftest/tests/jstests.list",
                "--extra-profile-file=jsreftest/tests/user.js"]
        },
        "mochitest-1": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=2", "--this-chunk=1"],
        },
        "mochitest-2": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=2", "--this-chunk=2"],
        },
        "mochitest-gl": {
            "category": "mochitest",
            "extra_args": ["--subsuite=webgl"],
        },
        "reftest-1": {
            "category": "reftest",
            "extra_args": ["--total-chunks=3", "--this-chunk=1",
                "tests/layout/reftests/reftest.list"]
        },
        "reftest-2": {
            "category": "reftest",
            "extra_args": ["--total-chunks=3", "--this-chunk=2",
                "tests/layout/reftests/reftest.list"]
        },
        "reftest-3": {
            "category": "reftest",
            "extra_args": ["--total-chunks=3", "--this-chunk=3",
                "tests/layout/reftests/reftest.list"]
        },
        "crashtest": {
            "category": "reftest",
            "extra_args": ["tests/testing/crashtest/crashtests.list"]
        },
        "xpcshell": {
            "category": "xpcshell",
            # XXX --manifest is superceded by testing/config/mozharness/android_x86_config.py.
            # Remove when Gecko 35 no longer in tbpl.
            "extra_args": ["--manifest=tests/xpcshell_android.ini"]
        },
    }, # end of "test_definitions"
    # test harness options are located in the gecko tree
    "in_tree_config": "config/mozharness/android_x86_config.py",
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file" : os.path.join(os.getcwd(), "oauth.txt"),
}
