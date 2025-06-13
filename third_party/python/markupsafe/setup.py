import os
import platform
import sys

from setuptools import Extension
from setuptools import setup
from setuptools.command.build_ext import build_ext
from setuptools.errors import CCompilerError
from setuptools.errors import ExecError
from setuptools.errors import PlatformError

ext_modules = [Extension("markupsafe._speedups", ["src/markupsafe/_speedups.c"])]


class BuildFailed(Exception):
    pass


class ve_build_ext(build_ext):
    """This class allows C extension building to fail."""

    def run(self):
        try:
            super().run()
        except PlatformError as e:
            raise BuildFailed() from e

    def build_extension(self, ext):
        try:
            super().build_extension(ext)
        except (CCompilerError, ExecError, PlatformError) as e:
            raise BuildFailed() from e
        except ValueError as e:
            # this can happen on Windows 64 bit, see Python issue 7511
            if "'path'" in str(sys.exc_info()[1]):  # works with Python 2 and 3
                raise BuildFailed() from e
            raise


def run_setup(with_binary):
    setup(
        cmdclass={"build_ext": ve_build_ext},
        ext_modules=ext_modules if with_binary else [],
    )


def show_message(*lines):
    print("=" * 74)
    for line in lines:
        print(line)
    print("=" * 74)


supports_speedups = platform.python_implementation() not in {
    "PyPy",
    "Jython",
    "GraalVM",
}

if os.environ.get("CIBUILDWHEEL", "0") == "1" and supports_speedups:
    run_setup(True)
elif supports_speedups:
    try:
        run_setup(True)
    except BuildFailed:
        show_message(
            "WARNING: The C extension could not be compiled, speedups"
            " are not enabled.",
            "Failure information, if any, is above.",
            "Retrying the build without the C extension now.",
        )
        run_setup(False)
        show_message(
            "WARNING: The C extension could not be compiled, speedups"
            " are not enabled.",
            "Plain-Python build succeeded.",
        )
else:
    run_setup(False)
    show_message(
        "WARNING: C extensions are not supported on this Python"
        " platform, speedups are not enabled.",
        "Plain-Python build succeeded.",
    )
