#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import argparse
import hashlib
import os
import threading

import frida

ARGUMENTS = {}
SOCKETS = {}
TERMINATE = threading.Condition()


def store_for_target(target, data):
    filename = hashlib.sha1(data).hexdigest()
    directory = os.path.join(ARGUMENTS["output"], target)

    os.makedirs(directory, exist_ok=True)

    with open(os.path.join(directory, filename), "wb") as f:
        f.write(data)


# --- asn1 ---


def on_SEC_ASN1DecodeItem_Util(payload):
    if not "data" in payload:
        return

    store_for_target("asn1", bytes(payload["data"].values()))


# --- certDN ---


def on_CERT_AsciiToName(payload):
    if not "data" in payload:
        return

    store_for_target("certDN", payload["data"].encode())


# --- pkcs7 ---


def on_CERT_DecodeCertPackage(payload):
    if not "data" in payload:
        return

    store_for_target("pkcs7", bytes(payload["data"].values()))


# --- pkcs8 ---


def on_PK11_ImportDERPrivateKeyInfoAndReturnKey(payload):
    if not "data" in payload:
        return

    store_for_target("pkcs8", bytes(payload["data"].values()))


# --- pkcs12 ---


def on_SEC_PKCS12DecoderUpdate(payload):
    if not "data" in payload:
        return

    store_for_target("pkcs12", bytes(payload["data"].values()))


# --- quickder ---


def on_SEC_QuickDERDecodeItem_Util(payload):
    if not "data" in payload:
        return

    store_for_target("quickder", bytes(payload["data"].values()))


# --- smime ---


def on_NSS_CMSDecoder_Update(payload):
    if not "data" in payload:
        return

    store_for_target("smime", bytes(payload["data"].values()))


# --- TLS ---


def on_ssl_DefClose(payload):
    ss = payload["ss"]
    if not ss in SOCKETS:
        return

    # There is no way for us to determine (in a clean and future-proof)
    # way the variant (DTLS/TLS) and origin (client/server) of the
    # received data.
    # Since you want to minimize the corpus anyway, we just say it belongs
    # to all possible targets.
    data = SOCKETS[ss].lstrip(b"\x00")
    store_for_target("tls-client", data)

    base_output_path = os.path.abspath(ARGUMENTS["output"])
    for target in ["dtls-client", "dtls-server", "tls-client", "tls-server"]:
        if not os.path.exists(os.path.join(base_output_path, target)):
            os.symlink(os.path.join(base_output_path, "tls-client"),
                       os.path.join(base_output_path, target),
                       target_is_directory=True)

    del SOCKETS[ss]


def on_ssl_DefRecv(payload):
    if not "data" in payload:
        return

    ss = payload["ss"]
    if not ss in SOCKETS:
        SOCKETS[ss] = bytes()

    SOCKETS[ss] += bytes(payload["data"].values())


def on_ssl_DefRead(payload):
    if not "data" in payload:
        return

    ss = payload["ss"]
    if not ss in SOCKETS:
        SOCKETS[ss] = bytes()

    SOCKETS[ss] += bytes(payload["data"].values())


def script_on_message(message, _data):
    if message["type"] != "send":
        print(message)
        return

    assert message["type"] == "send"

    payload = message["payload"]
    func = "on_" + payload["func"]

    assert func in globals()
    globals()[func](payload)


def session_on_detached():
    with TERMINATE:
        TERMINATE.notify_all()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--script", required=True, type=str)
    parser.add_argument("--nss-build",
                        required=True,
                        type=str,
                        help="e.g. /path/to/dist/Debug")
    parser.add_argument("--program", required=True, type=str)
    parser.add_argument("--output", required=True, type=str)

    args, programargs = parser.parse_known_args()

    global ARGUMENTS
    ARGUMENTS = vars(args)

    with open(args.script, "r") as f:
        script = f.read()

    pid = frida.spawn(program=args.program,
                      argv=programargs,
                      env={
                          **os.environ, "LD_LIBRARY_PATH":
                          os.path.join(args.nss_build, "lib")
                      })
    session = frida.attach(pid)

    script = session.create_script(script)
    script.load()

    script.on("message", script_on_message)

    session.resume()
    frida.resume(pid)

    session.on("detached", session_on_detached)

    with TERMINATE:
        TERMINATE.wait()


if __name__ == "__main__":
    main()
