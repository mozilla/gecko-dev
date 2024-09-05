# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import re
import signal
import socket
import subprocess
import tempfile
import threading
from pathlib import Path
from subprocess import PIPE, Popen

from base_python_support import BasePythonSupport
from logger.logger import RaptorLogger

LOG = RaptorLogger(component="raptor-browsertime")


class NetworkBench(BasePythonSupport):
    def __init__(self, **kwargs):
        self._is_chrome = False
        self.browsertime_node = None
        self.backend_server = None
        self.backend_port = None
        self.caddy_port = None
        self.caddy_server = None
        self.caddy_stdout = None
        self.caddy_stderr = None
        self.http_version = "h1"
        self.transfer_type = "download"

    def setup_test(self, test, args):
        from cmdline import CHROME_ANDROID_APPS, CHROMIUM_DISTROS

        LOG.info("setup_test: '%s'" % test)

        self._is_chrome = (
            args.app in CHROMIUM_DISTROS or args.app in CHROME_ANDROID_APPS
        )

        test_name = test.get("name").split("-", 2)
        self.http_version = test_name[0] if test_name[0] in ["h3", "h2"] else "unknown"
        self.transfer_type = (
            test_name[1] if test_name[1] in ["download", "upload"] else "unknown"
        )
        LOG.info(f"http_version: '{self.http_version}', type: '{self.transfer_type}'")

        if self.http_version == "unknown" or self.transfer_type == "unknown":
            raise Exception("Unsupported test")

    def check_caddy_installed(self):
        try:
            result = subprocess.run(
                ["caddy", "version"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            if result.returncode == 0:
                LOG.info("Caddy is installed. Version: %s" % result.stdout.strip())
                return True
            else:
                LOG.error("Caddy is not installed.")
        except FileNotFoundError:
            LOG.error("Caddy is not installed.")
        return False

    def start_backend_server(self, path):
        if self.browsertime_node is None or not self.browsertime_node.exists():
            return None

        LOG.info("node bin: %s" % self.browsertime_node)

        server_path = (
            Path(__file__).parent / ".." / ".." / "browsertime" / "utils" / path
        )
        LOG.info("server_path: %s" % server_path)

        if not server_path.exists():
            return None

        process = Popen(
            [self.browsertime_node, server_path],
            stdin=PIPE,
            stdout=PIPE,
            stderr=PIPE,
            universal_newlines=True,
            start_new_session=True,
        )
        msg = process.stdout.readline()
        LOG.info("server msg: %s" % msg)
        match = re.search(r"Server is running on http://[^:]+:(\d+)", msg)
        if match:
            self.backend_port = match.group(1)
            LOG.info("backend port: %s" % self.backend_port)
            return process
        return None

    def find_free_port(self, socket_type):
        with socket.socket(socket.AF_INET, socket_type) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(("localhost", 0))
            return s.getsockname()[1]

    def start_caddy(self):
        if not self.check_caddy_installed():
            return None
        if self.caddy_port is None or not (1 <= self.caddy_port <= 65535):
            return None

        cert_path = Path(__file__).parent / ".." / ".." / "browsertime" / "utils"

        if not cert_path.exists():
            return None

        key_path = cert_path / "http2-cert.key"
        LOG.info("key_path: %s" % key_path)
        if not key_path.exists():
            return None

        pem_path = cert_path / "http2-cert.pem"
        LOG.info("pem_path: %s" % pem_path)
        if not pem_path.exists():
            return None

        upstream = f"localhost:{self.backend_port}"
        port_str = f":{self.caddy_port}"
        caddyfile_content = {
            "admin": {"disabled": True},
            "apps": {
                "http": {
                    "servers": {
                        "server1": {
                            "listen": [port_str],
                            "protocols": ["h1", "h2", "h3"],
                            "routes": [
                                {
                                    "handle": [
                                        {
                                            "handler": "reverse_proxy",
                                            "upstreams": [{"dial": upstream}],
                                        }
                                    ]
                                }
                            ],
                            "tls_connection_policies": [
                                {"certificate_selection": {"any_tag": ["cert1"]}}
                            ],
                            "automatic_https": {"disable": True},
                        }
                    },
                },
                "tls": {
                    "certificates": {
                        "load_files": [
                            {
                                "certificate": str(pem_path),
                                "key": str(key_path),
                                "tags": ["cert1"],
                            }
                        ]
                    }
                },
            },
        }

        LOG.info("caddyfile_content: %s" % caddyfile_content)

        with tempfile.NamedTemporaryFile(
            mode="w", delete=False, suffix=".json"
        ) as temp_json_file:
            json.dump(caddyfile_content, temp_json_file, indent=2)
            temp_json_file_path = temp_json_file.name

        LOG.info("temp_json_file_path: %s" % temp_json_file_path)
        command = ["caddy", "run", "--config", temp_json_file_path]

        def read_output(pipe, log_func):
            for line in iter(pipe.readline, ""):
                log_func(line)

        process = Popen(
            command,
            stdin=PIPE,
            stdout=PIPE,
            stderr=PIPE,
            universal_newlines=True,
            start_new_session=True,
        )
        self.caddy_stdout = threading.Thread(
            target=read_output, args=(process.stdout, LOG.info)
        )
        self.caddy_stderr = threading.Thread(
            target=read_output, args=(process.stderr, LOG.info)
        )
        self.caddy_stdout.start()
        self.caddy_stderr.start()
        return process

    def modify_command(self, cmd, test):
        if not self._is_chrome:
            cmd += [
                "--firefox.acceptInsecureCerts",
                "true",
            ]
        if self.http_version == "h3":
            self.caddy_port = self.find_free_port(socket.SOCK_DGRAM)
            cmd += [
                "--firefox.preference",
                f"network.http.http3.alt-svc-mapping-for-testing:localhost;h3=:{self.caddy_port}",
            ]
        else:
            self.caddy_port = self.find_free_port(socket.SOCK_STREAM)

        cmd += [
            "--browsertime.server_url",
            f"https://localhost:{self.caddy_port}",
        ]

        LOG.info("modify_command: %s" % cmd)

        # We know that cmd[0] is the path to nodejs.
        self.browsertime_node = Path(cmd[0])
        self.backend_server = self.start_backend_server("benchmark_backend_server.js")
        if self.backend_server:
            self.caddy_server = self.start_caddy()
        if self.caddy_server is None:
            raise Exception("Failed to start test servers")

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
        # TODO
        LOG.info("handle_result: %s" % raw_result)

    def _build_subtest(self, measurement_name, replicates, test):
        # TODO
        LOG.info("_build_subtest")

    def summarize_test(self, test, suite, **kwargs):
        # TODO
        LOG.info("summarize_test")

    def shutdown_server(self, name, proc):
        LOG.info("%s server shutting down ..." % name)
        if proc.poll() is not None:
            LOG.info("server already dead %s" % proc.poll())
        else:
            LOG.info("server pid is %s" % str(proc.pid))
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except Exception as e:
                LOG.error("Failed during kill: " + str(e))

    def clean_up(self):
        if self.caddy_server:
            self.shutdown_server("Caddy", self.caddy_server)
        if self.backend_server:
            self.shutdown_server("Backend", self.backend_server)
        if self.caddy_stdout:
            self.caddy_stdout.join()
        if self.caddy_stderr:
            self.caddy_stderr.join()
