from base64 import b64encode, urlsafe_b64encode
import calendar
import hashlib
import hmac
import logging
import math
import os
import pprint
import re
import sys
import time

import six

from .exc import (
    BadHeaderValue,
    HawkFail,
    InvalidCredentials)


HAWK_VER = 1
HAWK_HEADER_RE = re.compile(r'(?P<key>\w+)=\"(?P<value>[^\"\\]*)\"\s*(?:,\s*|$)')
MAX_LENGTH = 4096
log = logging.getLogger(__name__)
allowable_header_keys = set(['id', 'ts', 'tsm', 'nonce', 'hash',
                             'error', 'ext', 'mac', 'app', 'dlg'])


def validate_credentials(creds):
    if not hasattr(creds, '__getitem__'):
        raise InvalidCredentials('credentials must be a dict-like object')
    try:
        creds['id']
        creds['key']
        creds['algorithm']
    except KeyError:
        etype, val, tb = sys.exc_info()
        raise InvalidCredentials('{etype}: {val}'
                                 .format(etype=etype, val=val))


def random_string(length):
    """Generates a random string for a given length."""
    # this conservatively gets 8*length bits and then returns 6*length of
    # them. Grabbing (6/8)*length bits could lose some entropy off the ends.
    return urlsafe_b64encode(os.urandom(length))[:length]


def calculate_payload_hash(payload, algorithm, content_type, block_size=1024):
    """Calculates a hash for a given payload."""
    p_hash = hashlib.new(algorithm)

    parts = []
    parts.append('hawk.' + str(HAWK_VER) + '.payload\n')
    parts.append(parse_content_type(content_type) + '\n')
    parts.append(payload or '')
    parts.append('\n')

    for i, p in enumerate(parts):
        # Make sure we are about to hash binary strings.
        if hasattr(p, "read"):
            log.debug("part %i being handled as a file object", i)
            while True:
                block = p.read(block_size)
                if not block:
                    break
                p_hash.update(block)
        elif not isinstance(p, six.binary_type):
            p = p.encode('utf8')
            p_hash.update(p)
        else:
            p_hash.update(p)
        parts[i] = p

    log.debug('calculating payload hash from:\n{parts}'
              .format(parts=pprint.pformat(parts)))

    return b64encode(p_hash.digest())


def calculate_mac(mac_type, resource, content_hash):
    """Calculates a message authorization code (MAC)."""
    normalized = normalize_string(mac_type, resource, content_hash)
    log.debug(u'normalized resource for mac calc: {norm}'
              .format(norm=normalized))
    digestmod = getattr(hashlib, resource.credentials['algorithm'])

    # Make sure we are about to hash binary strings.

    if not isinstance(normalized, six.binary_type):
        normalized = normalized.encode('utf8')
    key = resource.credentials['key']
    if not isinstance(key, six.binary_type):
        key = key.encode('ascii')

    result = hmac.new(key, normalized, digestmod)
    return b64encode(result.digest())


def calculate_ts_mac(ts, credentials):
    """Calculates a message authorization code (MAC) for a timestamp."""
    normalized = ('hawk.{hawk_ver}.ts\n{ts}\n'
                  .format(hawk_ver=HAWK_VER, ts=ts))
    log.debug(u'normalized resource for ts mac calc: {norm}'
              .format(norm=normalized))
    digestmod = getattr(hashlib, credentials['algorithm'])

    if not isinstance(normalized, six.binary_type):
        normalized = normalized.encode('utf8')
    key = credentials['key']
    if not isinstance(key, six.binary_type):
        key = key.encode('ascii')

    result = hmac.new(key, normalized, digestmod)
    return b64encode(result.digest())


def normalize_string(mac_type, resource, content_hash):
    """Serializes mac_type and resource into a HAWK string."""

    normalized = [
        'hawk.' + str(HAWK_VER) + '.' + mac_type,
        normalize_header_attr(resource.timestamp),
        normalize_header_attr(resource.nonce),
        normalize_header_attr(resource.method or ''),
        normalize_header_attr(resource.name or ''),
        normalize_header_attr(resource.host),
        normalize_header_attr(resource.port),
        normalize_header_attr(content_hash or '')
    ]

    # The blank lines are important. They follow what the Node Hawk lib does.

    normalized.append(normalize_header_attr(resource.ext or ''))

    if resource.app:
        normalized.append(normalize_header_attr(resource.app))
        normalized.append(normalize_header_attr(resource.dlg or ''))

    # Add trailing new line.
    normalized.append('')

    normalized = '\n'.join(normalized)

    return normalized


def parse_content_type(content_type):
    """Cleans up content_type."""
    if content_type:
        return content_type.split(';')[0].strip().lower()
    else:
        return ''


def parse_authorization_header(auth_header):
    """
    Example Authorization header:

        'Hawk id="dh37fgj492je", ts="1367076201", nonce="NPHgnG", ext="and
        welcome!", mac="CeWHy4d9kbLGhDlkyw2Nh3PJ7SDOdZDa267KH4ZaNMY="'
    """
    if len(auth_header) > MAX_LENGTH:
        raise BadHeaderValue('Header exceeds maximum length of {max_length}'.format(
            max_length=MAX_LENGTH))

    # Make sure we have a unicode object for consistency.
    if isinstance(auth_header, six.binary_type):
        auth_header = auth_header.decode('utf8')

    scheme, attributes_string = auth_header.split(' ', 1)

    if scheme.lower() != 'hawk':
        raise HawkFail("Unknown scheme '{scheme}' when parsing header"
                       .format(scheme=scheme))


    attributes = {}

    def replace_attribute(match):
        """Extract the next key="value"-pair in the header."""
        key = match.group('key')
        value = match.group('value')
        if key not in allowable_header_keys:
            raise HawkFail("Unknown Hawk key '{key}' when parsing header"
                           .format(key=key))
        validate_header_attr(value, name=key)
        if key in attributes:
            raise BadHeaderValue('Duplicate key in header: {key}'.format(key=key))
        attributes[key] = value

    # Iterate over all the key="value"-pairs in the header, replace them with
    # an empty string, and store the extracted attribute in the attributes
    # dict. Correctly formed headers will then leave nothing unparsed ('').
    unparsed_header = HAWK_HEADER_RE.sub(replace_attribute, attributes_string)
    if unparsed_header != '':
        raise BadHeaderValue("Couldn't parse Hawk header", unparsed_header)

    log.debug('parsed Hawk header: {header} into: \n{parsed}'
              .format(header=auth_header, parsed=pprint.pformat(attributes)))
    return attributes


def strings_match(a, b):
    # Constant time string comparision, mitigates side channel attacks.
    if len(a) != len(b):
        return False
    result = 0

    def byte_ints(buf):
        for ch in buf:
            # In Python 3, if we have a bytes object, iterating it will
            # already get the integer value. In older pythons, we need
            # to use ord().
            if not isinstance(ch, int):
                ch = ord(ch)
            yield ch

    for x, y in zip(byte_ints(a), byte_ints(b)):
        result |= x ^ y
    return result == 0


def utc_now(offset_in_seconds=0.0):
    # TODO: add support for SNTP server? See ntplib module.
    return int(math.floor(calendar.timegm(time.gmtime()) +
                          float(offset_in_seconds)))


# Allowed value characters:
# !#$%&'()*+,-./:;<=>?@[]^_`{|}~ and space, a-z, A-Z, 0-9, \, "
_header_attribute_chars = re.compile(
    r"^[ a-zA-Z0-9_\!#\$%&'\(\)\*\+,\-\./\:;<\=>\?@\[\]\^`\{\|\}~]*$")


def validate_header_attr(val, name=None):
    if not _header_attribute_chars.match(val):
        raise BadHeaderValue('header value name={name} value={val} '
                             'contained an illegal character'
                             .format(name=name or '?', val=repr(val)))


def prepare_header_val(val):
    if isinstance(val, six.binary_type):
        val = val.decode('utf-8')
    validate_header_attr(val)
    return val


def normalize_header_attr(val):
    if isinstance(val, six.binary_type):
        return val.decode('utf-8')
    return val
