#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import hashlib
import sys
from pathlib import Path
from string import Template

# This script regenerates telemetry IDs for the TLS server auth trust
# anchors included in the build. This script must be run whenever new
# roots are added (typically when updating NSS). The values for
# previously-known roots are re-used, and new values are assigned to new
# roots. Given the IDs and a list of sha256 hashes of the roots, this
# script outputs both a human-readable list (KnownRootHashes.txt) and a
# C++ source file (RootHashes.inc) that is included in the build.
# Currently these IDs are used in the metrics
# cert.validation_success_by_ca_2, cert_pinning.failures_by_ca_2, and
# ssl.ct_policy_non_compliant_connections_by_ca_2.


class Attribute:
    """Helper class to keep track of attribute (name, data type,
    value) tuples."""

    def __init__(self, name, data_type, value):
        self.name = name
        self.data_type = data_type
        self.value = value


def maybe_read_attribute(stream):
    """Skipping any comments (lines starting with '#'), maybe
    read a (attribute name, data type, value) tuple from the
    stream. For example, 'CKA_CLASS CK_OBJECT_CLASS
    CKO_CERTIFICATE' has name 'CKA_CLASS', data type
    'CK_OBJECT_CLASS', and value 'CKO_CERTIFICATE'. If the data
    type is 'MULTILINE_OCTAL', the value begins on the next line
    and consists of a series of octal values until a line
    consisting solely of 'END' is encountered. If a blank line
    is encountered, there are no more attributes in the current
    object being read."""

    line = stream.readline()
    while line.startswith("#"):
        line = stream.readline()
    if not line.strip():
        return None
    (name, data_type_and_value) = line.strip().split(" ", maxsplit=1)
    if data_type_and_value == "MULTILINE_OCTAL":
        data_type = "MULTILINE_OCTAL"
        value = b""
        line = stream.readline()
        while line and line.strip() != "END":
            octets = [int(octal, base=8) for octal in line.strip().split("\\")[1:]]
            value += bytes(octets)
            line = stream.readline()
    else:
        (data_type, value) = data_type_and_value.split(" ", maxsplit=1)
    return Attribute(name, data_type, value)


class Object:
    """Helper class representing objects, each of which consist
    of a series of attributes."""

    def __init__(self, attributes):
        self.attributes = attributes

    def get_attribute_value(self, name):
        """Helper function to get the value of a particular
        attribute, if present. Returns None otherwise."""
        for attribute in self.attributes:
            if attribute.name == name:
                return attribute.value
        return None

    def clss(self):
        """Get the 'CKA_CLASS' attribute."""
        return self.get_attribute_value("CKA_CLASS")

    def label(self):
        """Get the 'CKA_LABEL' attribute, removing any leading
        and trailing '"'."""
        return self.get_attribute_value("CKA_LABEL").removeprefix('"').removesuffix('"')

    def sha1(self):
        """Get the 'CKA_CERT_SHA1_HASH' attribute. Calculates it
        based on the 'CKA_VALUE' attribute if it is not
        present."""
        digest = self.get_attribute_value("CKA_CERT_SHA1_HASH")
        if digest:
            return digest
        value = self.get_attribute_value("CKA_VALUE")
        if value:
            return hashlib.sha1(value).digest()
        return None

    def sha256(self):
        """Calculates and returns the sha256 hash of the
        'CKA_CLASS' attribute."""
        value = self.get_attribute_value("CKA_VALUE")
        if not value:
            return None
        return hashlib.sha256(value).digest()

    def sha256base64(self):
        """Calculates and returns the sha256 hash of the
        'CKA_CLASS' attribute, base64-encoded."""
        value = self.get_attribute_value("CKA_VALUE")
        if not value:
            return None
        return base64.b64encode(hashlib.sha256(value).digest()).decode("ascii")

    def trust_server_auth(self):
        """Get the 'CKA_TRUST_SERVER_AUTH' attribute."""
        return self.get_attribute_value("CKA_TRUST_SERVER_AUTH")


def maybe_read_object(stream):
    """Maybe read an object, which is a series of one or more
    attributes. Returns None if no more attributes are in the
    stream."""
    attributes = []
    while True:
        attribute = maybe_read_attribute(stream)
        if not attribute:
            break
        attributes.append(attribute)
    if attributes:
        return Object(attributes)
    return None


def read_certdata(path):
    """Read a certdata.txt file at the given path and return all
    certificate objects that are TLS server auth trust anchors,
    sorted by sha256 hash."""
    certdata = open(path, encoding="utf-8")
    line = certdata.readline()
    # Discard everything up until the "BEGINDATA" line.
    while line and line.strip() != "BEGINDATA":
        line = certdata.readline()
    objects = []
    while True:
        object = maybe_read_object(certdata)
        if not object:
            break
        objects.append(object)
    # Get all certificate objects.
    certificates = [o for o in objects if o.clss() == "CKO_CERTIFICATE"]
    # Get a map of all sha1 hashes of certificates to trust objects.
    trusts = {o.sha1(): o for o in objects if o.clss() == "CKO_NSS_TRUST"}
    # Get a list of certificates where the sha1 hash of each certificate
    # corresponds to a trust object indicating that that certificate is
    # a trust anchor.
    server_auth_trust_anchors = [
        c
        for c in certificates
        if c.sha1() in trusts
        and trusts[c.sha1()].trust_server_auth() == "CKT_NSS_TRUSTED_DELEGATOR"
    ]
    server_auth_trust_anchors.sort(key=Object.sha256)
    return server_auth_trust_anchors


class RootHash:
    """Helper class to keep track of (certificate sha256 digest,
    bin number, label) tuples."""

    def __init__(self, digest_b64, bin_number, label):
        self.digest_b64 = digest_b64
        self.digest = base64.b64decode(digest_b64)
        self.bin_number = bin_number
        self.label = label

    def digest(self):
        return self.digest

    def bin_number(self):
        return self.bin_number


def read_known_root_hashes(path):
    """Read the known (sha256 digest, bin number, label) tuples
    from the file at the given path."""
    known_root_hashes_data = open(path, encoding="utf-8")
    known_root_hashes = {}
    line = known_root_hashes_data.readline()
    while line:
        # Lines beginning with '#' are comments.
        if not line.startswith("#"):
            (digest_b64, bin_number, label) = line.strip().split(" ", maxsplit=2)
            known_root_hashes[digest_b64] = RootHash(digest_b64, int(bin_number), label)
        line = known_root_hashes_data.readline()
    return known_root_hashes


KNOWN_ROOT_HASHES_HEADER = """\
# This is a generated file.
"""


def write_known_root_hashes(path, known_root_hashes):
    """Write the known root hashes as a flat list of tuples to
    the given path."""
    with open(path, "w") as f:
        f.write(KNOWN_ROOT_HASHES_HEADER)
        for root_hash in known_root_hashes.values():
            f.write(
                f"{root_hash.digest_b64} {root_hash.bin_number} {root_hash.label}\n"
            )


ROOT_HASHES_HEADER = """\
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/
/* This is an automatically generated file. If you're not                    */
/* RootCertificateTelemetryUtils.cpp, you shouldn't be #including it.        */
/*****************************************************************************/

#define HASH_LEN 32
struct CertAuthorityHash {
  // See bug 1338873 about making these fields const.
  uint8_t hash[HASH_LEN];
  int32_t binNumber;
};

static const struct CertAuthorityHash ROOT_TABLE[] = {
"""


ROOT_HASHES_ENTRY_TEMPLATE = """\
  {
    /* $label */
    { $digest_half_1
      $digest_half_2 },
    $bin_number /* Bin Number */
  },
"""


ROOT_HASHES_FOOTER = """\
};
"""


def write_root_hashes(path, certdata, known_root_hashes):
    """Write the known root hashes C++ source file for inclusion
    in the build."""
    with open(root_hashes_path, "w") as f:
        f.write(ROOT_HASHES_HEADER)
        tmpl = Template(ROOT_HASHES_ENTRY_TEMPLATE)
        for root in certdata:
            root_hash = known_root_hashes[root.sha256base64()]
            digest_half_1 = "".join(
                [f"0x{c:02x}, " for c in root_hash.digest[: len(root_hash.digest) >> 1]]
            ).removesuffix(" ")
            digest_half_2 = "".join(
                [f"0x{c:02x}, " for c in root_hash.digest[len(root_hash.digest) >> 1 :]]
            ).removesuffix(", ")
            f.write(
                tmpl.substitute(
                    label=root_hash.label,
                    digest_half_1=digest_half_1,
                    digest_half_2=digest_half_2,
                    bin_number=root_hash.bin_number,
                )
            )
        f.write(ROOT_HASHES_FOOTER)


if __name__ == "__main__":
    # Read and parse the certdata.txt file that will be used to build
    # the builtin roots module.
    certdata_path = Path("security/nss/lib/ckfw/builtins/certdata.txt")
    if not certdata_path.exists():
        print("Could not find certdata.txt.")
        sys.exit(1)
    certdata = read_certdata(certdata_path)

    # Read the list of known root hashes.
    known_root_hashes_path = Path("security/manager/tools/KnownRootHashes.txt")
    if not known_root_hashes_path.exists():
        print("Could not read KnownRootHashes.txt.")
        sys.exit(1)
    known_root_hashes = read_known_root_hashes(known_root_hashes_path)

    # Assign bin numbers to any newly-added roots. If there are no known roots,
    # start at 4, because:
    # 0 is reserved for "unknown" (likely indicating an error or a non-Mozilla
    # builtin roots module).
    # 1 is reserved for "softoken/cert9.db".
    # 2 is reserved for "external PKCS#11 module".
    # 3 is reserved for "third-party root from OS".
    # Otherwise, start with one more than the largest currently-known.
    next_bin_number = (
        max(map(RootHash.bin_number, known_root_hashes.values()), default=3) + 1
    )
    for root in certdata:
        digest_b64 = root.sha256base64()
        if digest_b64 not in known_root_hashes:
            known_root_hashes[digest_b64] = RootHash(
                digest_b64, next_bin_number, root.label()
            )
            next_bin_number += 1
    # Save the (potentially-updated) list of known roots as a flat list
    # of tuples.
    write_known_root_hashes(known_root_hashes_path, known_root_hashes)

    # Write the array of root telemetry information as a C++ source file to
    # include in the build. Whereas the flat list of known root hashes
    # (KnownRootHashes.txt) contains all roots ever known (including removed
    # ones), this file only needs to include the roots currently in
    # certdata.txt.
    root_hashes_path = Path("security/manager/ssl/RootHashes.inc")
    write_root_hashes(root_hashes_path, certdata, known_root_hashes)
