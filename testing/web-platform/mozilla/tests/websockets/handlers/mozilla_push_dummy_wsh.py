#!/usr/bin/python

"""
A dummy push server that emits dummy push subscription such that Firefox can
understand it, but without any data storage nor validation.

How it works:
1. Firefox will establish a websocket connection with the dummy server via
   PushServiceWebSocket.sys.mjs.
   The initial connection will also start up an HTTPS server that works
   as a push endpoint.
2. A push subscription from a test via Push API will send messageType=register,
   which will generate a URL that points to the HTTPS server. It exposes
   channel ID as a URL parameter, and the server will pass through that parameter
   to Firefox so that it can map it to each push subscription.
3. message=unregister will be sent from Firefox when unsubscription happens,
   but the dummy server doesn't process that as it doesn't even remember any
   subscription (it just passes through the channel ID and that's it).


Caveat:
* It sends CORS headers but that behavior is only observed in Firefox's push server
(autopush-rs).
* The dummy server doesn't throw any error on an invalid VAPID.
* It assumes that Firefox will use websocket to connect to the push server, which is
  not true on Firefox for Android as it uses Google FCM.
  (See https://firefox-source-docs.mozilla.org/mobile/android/fenix/Firebase-Cloud-Messaging-for-WebPush.html)
"""

import base64
import json
import pathlib
import ssl
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

from pywebsocket3 import msgutil

from tools import localpaths

UAID = "8e1c93a9-139b-419c-b200-e715bb1e8ce8"


class DummyEndpointHandler(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.end_headers()

    def do_POST(self):
        url = urlparse(self.path)
        query = parse_qs(url.query)

        content_len = int(self.headers.get("Content-Length"))
        post_body = self.rfile.read(content_len)
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.end_headers()

        headers = {
            "encoding": self.headers.get("Content-Encoding"),
        }
        msgutil.send_message(
            self.server.websocket_request,
            json.dumps(
                {
                    "messageType": "notification",
                    "channelID": query["channelID"][0],
                    "data": base64.urlsafe_b64encode(post_body).decode(),
                    "headers": headers if len(post_body) > 0 else None,
                }
            ),
        )


def web_socket_do_extra_handshake(request):
    request.ws_protocol = "push-notification"


def start_endpoint(server):
    def impl():
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)

        certs = pathlib.Path(localpaths.repo_root) / "tools/certs"

        context.load_cert_chain(
            certfile=str(certs / "web-platform.test.pem"),
            keyfile=str(certs / "web-platform.test.key"),
        )

        server.socket = context.wrap_socket(server.socket, server_side=True)

        server.serve_forever()

    return impl


def web_socket_transfer_data(request):
    server = HTTPServer(("localhost", 0), DummyEndpointHandler)
    server.websocket_request = request
    port = server.server_port

    endpoint_thread = threading.Thread(target=start_endpoint(server), daemon=True)
    endpoint_thread.start()

    while True:
        line = request.ws_stream.receive_message()
        if line is None:
            server.shutdown()
            return

        message = json.loads(line)
        messageType = message["messageType"]

        if messageType == "hello":
            msgutil.send_message(
                request,
                json.dumps(
                    {
                        "messageType": "hello",
                        "uaid": UAID,
                        "status": 200,
                        "use_webpush": True,
                    }
                ),
            )
        elif messageType == "register":
            channelID = message["channelID"]
            msgutil.send_message(
                request,
                json.dumps(
                    {
                        "messageType": "register",
                        "uaid": UAID,
                        "channelID": channelID,
                        "status": 200,
                        "pushEndpoint": f"https://web-platform.test:{port}/push_endpoint?channelID={channelID}",
                    }
                ),
            )
        elif messageType == "unregister":
            msgutil.send_message(
                request,
                json.dumps(
                    {
                        "messageType": "unregister",
                        "channelID": message["channelID"],
                        "status": 200,
                    }
                ),
            )
