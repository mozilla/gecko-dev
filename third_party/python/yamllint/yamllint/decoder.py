# Copyright (C) 2023–2025 Jason Yundt
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import codecs
import os
import warnings


def detect_encoding(stream_data):
    """
    Return stream_data’s character encoding

    Specifically, this function will take a bytes object and return a string
    that contains the name of one of Python’s built-in codecs [1].

    The YAML spec says that streams must begin with a BOM or an ASCII
    character. If stream_data doesn’t begin with either of those, then this
    function might return the wrong encoding. See chapter 5.2 of the YAML spec
    for details [2].

    Before this function was added, yamllint would sometimes decode text files
    using a non-standard character encoding. It’s possible that there are users
    out there who still want to use yamllint with non-standard character
    encodings, so this function includes an override switch for those users. If
    the YAMLLINT_FILE_ENCODING environment variable is set to "example_codec",
    then this function will always return "example_codec".

    [1]: <https://docs.python.org/3/library/codecs.html#standard-encodings>
    [2]: <https://yaml.org/spec/1.2.2/#52-character-encodings>
    """
    if 'YAMLLINT_FILE_ENCODING' in os.environ:
        warnings.warn("YAMLLINT_FILE_ENCODING is meant for temporary "
                      "workarounds. It may be removed in a future version of "
                      "yamllint.")
        return os.environ['YAMLLINT_FILE_ENCODING']
    elif stream_data.startswith(codecs.BOM_UTF32_BE):
        return 'utf_32'
    elif stream_data.startswith(b'\x00\x00\x00') and len(stream_data) >= 4:
        return 'utf_32_be'
    elif stream_data.startswith(codecs.BOM_UTF32_LE):
        return 'utf_32'
    elif stream_data[1:4] == b'\x00\x00\x00':
        return 'utf_32_le'
    elif stream_data.startswith(codecs.BOM_UTF16_BE):
        return 'utf_16'
    elif stream_data.startswith(b'\x00') and len(stream_data) >= 2:
        return 'utf_16_be'
    elif stream_data.startswith(codecs.BOM_UTF16_LE):
        return 'utf_16'
    elif stream_data[1:2] == b'\x00':
        return 'utf_16_le'
    elif stream_data.startswith(codecs.BOM_UTF8):
        return 'utf_8_sig'
    else:
        return 'utf_8'


def auto_decode(stream_data):
    return stream_data.decode(encoding=detect_encoding(stream_data))


def lines_in_files(paths):
    """Autodecodes files and yields their lines."""
    for path in paths:
        with open(path, 'rb') as file:
            text = auto_decode(file.read())
        yield from text.splitlines()
