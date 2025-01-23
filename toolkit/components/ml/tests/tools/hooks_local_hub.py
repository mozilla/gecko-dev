# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import http.server
import os
import socket
import socketserver
import threading
from pathlib import Path

_THREAD = None


class CustomHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    hub_root = ""

    def translate_path(self, path):
        # Remove front slash and query args to match the files
        return str(self.hub_root / Path(path.lstrip("/").split("?")[0]))

    def send_head(self):
        path = Path(self.translate_path(self.path))
        if path.is_dir():
            return super().send_head()

        # when dealing with a file, we set the ETag header using the file size.
        if path.is_file():
            file_size = path.stat().st_size
            etag = f'"{file_size}"'

            # Handle conditional GET requests
            if_match = self.headers.get("If-None-Match")
            if if_match == etag:
                self.send_response(304)
                self.end_headers()
                return None

            self.send_response(200)
            self.send_header("Content-type", self.guess_type(str(path)))
            self.send_header("Content-Length", str(file_size))
            self.send_header("ETag", etag)
            self.end_headers()
            return path.open("rb")

        self.send_error(404, "File not found")


def serve_directory(directory, port):
    """Serves the directory at the given port."""
    CustomHTTPRequestHandler.hub_root = directory

    with socketserver.TCPServer(("", port), CustomHTTPRequestHandler) as httpd:
        print(f"Serving {directory} at http://localhost:{port}")
        httpd.serve_forever()


def start_hub(root_directory):
    """Starts a local hub server and returns the port and thread."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("", 0))
        port = s.getsockname()[1]

    server_thread = threading.Thread(
        target=serve_directory, args=(root_directory, port), daemon=True
    )
    server_thread.start()
    return port, server_thread


def before_runs(env):
    """Runs before all performance tests.

    We grab MOZ_FETCHES_DIR. If set we serve MOZ_FETCHES_DIR/onnx-models as our local hub.
    """
    global _THREAD
    fetches_dir = os.environ.get("MOZ_FETCHES_DIR")
    if fetches_dir is None:
        return
    hub_dir = Path(fetches_dir) / "onnx-models"
    if not hub_dir.is_dir():
        return
    port, server_thread = start_hub(hub_dir)
    os.environ["MOZ_MODELS_HUB"] = f"http://localhost:{port}"
    _THREAD = server_thread


def after_runs(env):
    global _THREAD
    if _THREAD is not None:
        print("Shutting down")
        _THREAD.join(timeout=0)
        _THREAD = None
