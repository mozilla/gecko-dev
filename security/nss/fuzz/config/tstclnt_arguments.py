#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import random
import string

ECH_CONFIGS ="AEX+DQBBcQAgACDh4IuiuhhInUcKZx5uYcehlG9PQ1ZlzhvVZyjJl7dscQAEAAEAAQASY2xvdWRmbGFyZS1lY2guY29tAAA="


def main():
    # Use Encrypted Client Hello with the given Base64-encoded ECHConfigs.
    if random.randint(0, 1):
        print(f"-N {ECH_CONFIGS}")

    # Configure a TLS 1.3 External PSK with the given hex string for a key.
    if random.randint(0, 1):
        print(f"-z 0x{''.join(random.choices(string.hexdigits, k=16))}")

    # Enable the session ticket extension.
    if random.randint(0, 1):
        print("-u")

    # Enable the cert_status extension (OCSP stapling).
    if random.randint(0, 1):
        print("-T")

    # Enable the signed_certificate_timestamp extension.
    if random.randint(0, 1):
        print("-U")

    # Enable the delegated credentials extension.
    if random.randint(0, 1):
        print("-B")

    # Enable the extended master secret extension [RFC7627).
    if random.randint(0, 1):
        print("-G")

    # Allow 0-RTT data (TLS 1.3 only).
    if random.randint(0, 1):
        print("-Z")

    # Enable Encrypted Client Hello GREASEing with the given padding size (0-255).
    if random.randint(0, 1):
        print(f"-i {random.randint(0, 255)}")

    # Enable middlebox compatibility mode (TLS 1.3 only).
    if random.randint(0, 1):
        print("-e")

    if random.randint(0, 1):
        print("--enable-rfc8701-grease")

    if random.randint(0, 1):
        print("--enable-ch-extension-permutation")

    if random.randint(0, 1):
        print("--zlib-certificate-compression")


if __name__ =="__main__":
    main()
