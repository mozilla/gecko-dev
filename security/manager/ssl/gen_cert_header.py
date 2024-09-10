# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import textwrap

from pyasn1_modules import pem


def read_certificate(filename):
    with open(filename, "r") as f:
        try:
            return pem.readPemFromFile(
                f, "-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----"
            )
        except UnicodeDecodeError:
            raise Exception(
                f"Could not decode {filename} (it should be a PEM-encoded certificate)"
            )


def write_header(output, array_name, certificates):
    certificate_names = []
    for index, certificate in enumerate(certificates):
        certificate_name = f"{array_name}{index}"
        certificate_names.append(
            f"mozilla::Span({certificate_name}, sizeof({certificate_name}))"
        )
        output.write(f"const uint8_t {certificate_name}[] = {{\n")
        certificate_bytes = read_certificate(certificate)
        hexified = ", ".join(["0x%02x" % byte for byte in certificate_bytes])
        wrapped = textwrap.wrap(hexified)
        for line in wrapped:
            output.write(f"    {line}\n")
        output.write("};\n")
    output.write(
        f'const mozilla::Span<const uint8_t> {array_name}[] = {{ {", ".join(certificate_names)} }};\n'
    )


def generate(output, *args):
    write_header(output, args[-1], args[0:-1])
