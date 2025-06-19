"""Provides compatibility between different Python versions."""

# std imports
import sys

__all__ = 'PY2', 'unicode_chr', 'StringIO', 'TextType', 'StringType'

# isort: off

# Python 3
if sys.version_info[0] >= 3:
    PY2 = False
    unicode_chr = chr
    from io import StringIO
    TextType = str
    StringType = str

# Python 2
else:
    PY2 = True
    unicode_chr = unichr  # pylint: disable=undefined-variable  # noqa: F821
    from StringIO import StringIO
    TextType = unicode  # pylint: disable=undefined-variable  # noqa: F821
    StringType = basestring  # pylint: disable=undefined-variable  # noqa: F821
