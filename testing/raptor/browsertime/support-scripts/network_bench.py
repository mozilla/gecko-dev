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

import filters
from base_python_support import BasePythonSupport
from logger.logger import RaptorLogger

LOG = RaptorLogger(component="raptor-browsertime")


class NetworkBench(BasePythonSupport):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
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
        # loopback
        self.interface = "lo"
        self.network_type = "unthrottled"
        self.packet_loss_rate = None
        self.cleanup = []

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

        utils_path = Path(__file__).parent / ".." / ".." / "browsertime" / "utils"

        if not utils_path.exists():
            return None

        key_path = utils_path / "http2-cert.key"
        LOG.info("key_path: %s" % key_path)
        if not key_path.exists():
            return None

        pem_path = utils_path / "http2-cert.pem"
        LOG.info("pem_path: %s" % pem_path)
        if not pem_path.exists():
            return None

        port_str = f":{self.caddy_port}"
        if self.transfer_type == "upload":
            upstream = f"localhost:{self.backend_port}"
            routes = [
                {
                    "handle": [
                        {
                            "handler": "reverse_proxy",
                            "upstreams": [{"dial": upstream}],
                        }
                    ]
                }
            ]
        elif self.transfer_type == "download":
            fetches_dir = Path(os.environ.get("MOZ_FETCHES_DIR", "None"))
            if not fetches_dir.exists():
                return None
            routes = [
                {
                    "match": [{"path": ["/download_test.html"]}],
                    "handle": [
                        {
                            "handler": "file_server",
                            "root": str(utils_path),
                            "browse": {},
                        }
                    ],
                },
                {
                    "match": [{"path": ["/upload-test-32MB.dat"]}],
                    "handle": [
                        {
                            "handler": "headers",
                            "response": {
                                "set": {
                                    "Cache-Control": [
                                        "no-store",
                                        "no-cache",
                                        "must-revalidate",
                                        "proxy-revalidate",
                                        "max-age=0",
                                    ],
                                    "Pragma": ["no-cache"],
                                    "Expires": ["0"],
                                }
                            },
                        },
                        {
                            "handler": "file_server",
                            "root": str(fetches_dir),
                            "browse": {},
                        },
                    ],
                },
            ]
        caddyfile_content = {
            "admin": {"disabled": True},
            "apps": {
                "http": {
                    "servers": {
                        "server1": {
                            "listen": [port_str],
                            "protocols": ["h1", "h2", "h3"],
                            "routes": routes,
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

    def check_tc_command(self):
        try:
            result = subprocess.run(
                ["sudo", "tc", "-help"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            if result.returncode == 0:
                LOG.info("tc can be executed as root")
                return True
            else:
                LOG.error("tc is not available")
        except Exception as e:
            LOG.error(f"Error executing tc: {str(e)}")
        return False

    def run_command(self, command):
        try:
            subprocess.run(
                command,
                shell=True,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            LOG.info(command)
            return True
        except subprocess.CalledProcessError as e:
            LOG.info(f"Error executing command: {command}")
            LOG.info(f"Error: {e.stderr.decode()}")
            return False

    def apply_network_throttling(self, interface, network_type, loss, port):
        def calculate_bdp(bandwidth_mbit, delay_ms):
            bandwidth_bps = bandwidth_mbit * 1_000_000
            delay_sec = delay_ms / 1000
            bdp_bits = bandwidth_bps * delay_sec
            bdp_bytes = bdp_bits / 8
            return int(bdp_bytes)

        def network_type_to_bandwidth_delay(network_type):
            # Define the mapping of network types
            network_mapping = {
                "slow3G": {"bandwidth": "1Mbit", "delay_ms": 200},
                "fast5G": {"bandwidth": "300Mbit", "delay_ms": 20},
                "busy5G": {"bandwidth": "300Mbit", "delay_ms": 200},
            }

            # Look up the network type in the mapping
            result = network_mapping.get(network_type)

            if result is None:
                raise Exception(f"Unknown network type: {network_type}")
            return result["bandwidth"], result["delay_ms"]

        bandwidth_str, delay_ms = network_type_to_bandwidth_delay(network_type)
        bandwidth_mbit = float(bandwidth_str.replace("Mbit", ""))
        bdp_bytes = calculate_bdp(bandwidth_mbit, delay_ms)

        LOG.info(
            f"apply_network_throttling: bandwidth={bandwidth_str} delay={delay_ms}ms loss={loss}"
        )

        self.run_command(f"sudo tc qdisc del dev {interface} root")
        if not self.run_command(
            f"sudo tc qdisc add dev {interface} root handle 1: htb default 12"
        ):
            return False
        else:
            LOG.info("Register cleanup function")
            self.cleanup.append(
                lambda: self.run_command(f"sudo tc qdisc del dev {self.interface} root")
            )

        if not self.run_command(
            f"sudo tc class add dev {interface} parent 1: classid 1:1 htb rate {bandwidth_str}"
        ):
            return False

        delay_str = f"{delay_ms}ms"
        if not loss or loss == "0":
            if not self.run_command(
                f"sudo tc qdisc add dev {interface} parent 1:1 handle 10: netem delay {delay_str} limit {bdp_bytes}"
            ):
                return False
        elif not self.run_command(
            f"sudo tc qdisc add dev {interface} parent 1:1 handle 10: netem delay {delay_str} loss {loss}% limit {bdp_bytes}"
        ):
            return False
        # Add a filter to match UDP traffic on the specified port for IPv4
        if not self.run_command(
            f"sudo tc filter add dev {interface} protocol ip parent 1:0 u32 "
            f"match ip protocol 17 0xff "
            f"match ip dport {port} 0xffff "
            f"flowid 1:1"
        ):
            return False
        if not self.run_command(
            f"sudo tc filter add dev {interface} protocol ip parent 1:0 u32 "
            f"match ip protocol 17 0xff "
            f"match ip sport {port} 0xffff "
            f"flowid 1:1"
        ):
            return False
        # Add a filter to match UDP traffic on the specified port for IPv6
        if not self.run_command(
            f"sudo tc filter add dev {interface} parent 1:0 protocol ipv6 u32 "
            f"match ip6 protocol 17 0xff "
            f"match ip6 dport {port} 0xffff "
            f"flowid 1:1"
        ):
            return False
        if not self.run_command(
            f"sudo tc filter add dev {interface} parent 1:0 protocol ipv6 u32 "
            f"match ip6 protocol 17 0xff "
            f"match ip6 sport {port} 0xffff "
            f"flowid 1:1"
        ):
            return False
        return True

    def get_network_conditions(self, cmd):
        try:
            i = 0
            while i < len(cmd):
                if cmd[i] == "--network_type":
                    self.network_type = cmd[i + 1]
                    i += 2
                elif cmd[i] == "--pkt_loss_rate":
                    self.packet_loss_rate = cmd[i + 1]
                    i += 2
                else:
                    i += 1
        except Exception:
            raise Exception("failed to get network condition")

    def modify_command(self, cmd, test):
        if not self._is_chrome:
            cmd += [
                "--firefox.acceptInsecureCerts",
                "true",
            ]
        if self.http_version == "h3":
            self.caddy_port = self.find_free_port(socket.SOCK_DGRAM)
            if not self._is_chrome:
                cmd += [
                    "--firefox.preference",
                    f"network.http.http3.alt-svc-mapping-for-testing:localhost;h3=:{self.caddy_port}",
                    "--firefox.preference",
                    "network.http.http3.force-use-alt-svc-mapping-for-testing:true",
                ]
            else:
                spki = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8="
                cmd += [
                    f"--chrome.args=--origin-to-force-quic-on=localhost:{self.caddy_port}",
                    f"--chrome.args=--ignore-certificate-errors-spki-list={spki}",
                ]
        else:
            self.caddy_port = self.find_free_port(socket.SOCK_STREAM)

        if self.transfer_type == "upload":
            cmd += [
                "--browsertime.server_url",
                f"https://localhost:{self.caddy_port}",
            ]
        elif self.transfer_type == "download":
            cmd += [
                "--browsertime.server_url",
                f"https://localhost:{self.caddy_port}/download_test.html",
            ]

        self.get_network_conditions(cmd)

        LOG.info("modify_command: %s" % cmd)

        # We know that cmd[0] is the path to nodejs.
        self.browsertime_node = Path(cmd[0])
        self.backend_server = self.start_backend_server("benchmark_backend_server.js")
        if self.backend_server:
            self.caddy_server = self.start_caddy()
        if self.caddy_server is None:
            raise Exception("Failed to start test servers")

        if self.network_type != "unthrottled":
            if not self.check_tc_command():
                raise Exception("tc is not available")
            if not self.apply_network_throttling(
                self.interface,
                self.network_type,
                self.packet_loss_rate,
                self.caddy_port,
            ):
                raise Exception("apply_network_throttling failed")

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
        bandwidth_key = (
            "upload-bandwidth"
            if self.transfer_type == "upload"
            else "download-bandwidth"
        )

        def get_bandwidth(data):
            try:
                extras = data.get("extras", [])
                if extras and isinstance(extras, list):
                    custom_data = extras[0].get("custom_data", {})
                    if bandwidth_key in custom_data:
                        return custom_data[bandwidth_key]
                return None  # Return None if any key or index is missing
            except Exception:
                return None

        bandwidth = get_bandwidth(raw_result)
        if not bandwidth:
            return

        LOG.info(f"Bandwidth: [{' '.join(map(str, bandwidth))}]")
        bt_result["measurements"].setdefault(bandwidth_key, []).append(bandwidth)

    def _build_subtest(self, measurement_name, replicates, test):
        unit = test.get("unit", "Mbit/s")
        if test.get("subtest_unit"):
            unit = test.get("subtest_unit")

        return {
            "name": measurement_name,
            "lowerIsBetter": test.get("lower_is_better", False),
            "alertThreshold": float(test.get("alert_threshold", 2.0)),
            "unit": unit,
            "replicates": replicates,
            "value": round(filters.geometric_mean(replicates[0]), 3),
        }

    def summarize_test(self, test, suite, **kwargs):
        suite["type"] = "benchmark"
        if suite["subtests"] == {}:
            suite["subtests"] = []
        for measurement_name, replicates in test["measurements"].items():
            if not replicates:
                continue
            suite["subtests"].append(
                self._build_subtest(measurement_name, replicates, test)
            )
        suite["subtests"].sort(key=lambda subtest: subtest["name"])

    def summarize_suites(self, suites):
        for index, item in enumerate(suites):
            if "extraOptions" in item:
                item["extraOptions"].append(self.network_type)
                loss_str = (
                    f"loss-{self.packet_loss_rate}"
                    if self.packet_loss_rate
                    else "loss-0"
                )
                item["extraOptions"].append(loss_str)

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
        while self.cleanup:
            func = self.cleanup.pop()
            func()
