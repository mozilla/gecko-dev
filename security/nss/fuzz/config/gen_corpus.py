#!/usr/bin/env python3

import argparse
import hashlib
import itertools
import os
import re
import subprocess
import sys
import threading

EXTERNAL_PSK = "0x783666676F55306932745A32303354442B394A3271735A7A30714B464B645943"
ECH_CONFIGS = "AEX+DQBBcQAgACDh4IuiuhhInUcKZx5uYcehlG9PQ1ZlzhvVZyjJl7dscQAEAAEAAQASY2xvdWRmbGFyZS1lY2guY29tAAA="

TSTCLNT_ARGS = [
    "-o",  # Override bad server cert. Make it OK.
    "-D",  # Run without a cert database
    "-Q",  # Quit after handshake
    "-b",  # Load the default "builtins" root CA module
    "--enable-rfc8701-grease",
    "--enable-ch-extension-permutation",
    "--zlib-certificate-compression",
    "-z",
    EXTERNAL_PSK,
    "-N",
    ECH_CONFIGS,
]


def brrrrr(hosts, args):
    tstclnt_bin = os.path.join(args.nss, "bin/tstclnt")
    ld_libary_path = os.path.join(args.nss, "lib")

    for host in hosts:
        try:
            result = subprocess.run([
                "strace", "-f", "-x", "-s", "65535", "-e", "trace=network",
                tstclnt_bin, "-h", host
            ] + TSTCLNT_ARGS,
                                    env={
                                        "LD_LIBRARY_PATH": ld_libary_path,
                                    },
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE,
                                    timeout=1,
                                    text=True)
        except subprocess.TimeoutExpired:
            print("Getting handshake timed out for:", host, file=sys.stderr)
            continue

        client_data = bytearray()
        server_data = bytearray()

        lines = result.stderr.splitlines()
        for line in lines:
            sendto = line.startswith("sendto(")
            recvfrom = line.startswith("recvfrom(")

            if not (sendto or recvfrom):
                continue

            match = re.search(r"(\\x[a-f0-9]{2})+", line)
            if match is None:
                continue

            data = bytearray.fromhex(match.group(0).replace("\\x", ""))

            # After the initial "Client Hello", each sent/received data
            # block can be added accordingly.
            if not client_data:
                if len(data) > 5 and data[0] == 0x16 and data[5] == 0x01:
                    client_data = data

                continue

            if sendto:
                client_data += data
                continue

            assert recvfrom
            server_data += data

        if not (client_data and server_data):
            print("Failed to get handshake for:", host, file=sys.stderr)
            continue

        # The data sent by the client is used as the corpus for the TLS
        # server target as it simulates a server receiving client data.
        filename = hashlib.sha1(client_data).hexdigest()
        with open(os.path.join(args.output, "tls-server-corpus", filename),
                  "wb") as f:
            f.write(client_data)

        # The data sent by the server is used as the corpus for the TLS
        # client target as it simulates a client receiving server data.
        filename = hashlib.sha1(server_data).hexdigest()
        with open(os.path.join(args.output, "tls-client-corpus", filename),
                  "wb") as f:
            f.write(server_data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--nss",
                        required=True,
                        help="e.g. /path/to/dist/Debug")
    parser.add_argument("--hosts", required=True)
    parser.add_argument("--threads", required=True, type=int)
    parser.add_argument("--output", required=True)

    args = parser.parse_args()

    with open(args.hosts, "r") as f:
        hosts = f.read().splitlines()

    os.makedirs(os.path.join(args.output, "client"), exist_ok=True)
    os.makedirs(os.path.join(args.output, "server"), exist_ok=True)

    chunks = itertools.batched(hosts, len(hosts) // args.threads)
    threads = []

    while chunk := next(chunks, None):
        thread = threading.Thread(target=brrrrr, args=(
            chunk,
            args,
        ))
        thread.daemon = True
        thread.start()

        threads.append(thread)

    for thread in threads:
        thread.join()


if __name__ == "__main__":
    main()
