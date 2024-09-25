# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import importlib
import inspect
import pathlib

from base_python_support import BasePythonSupport


def import_support_class(path):
    """This function returns a BasePythonSupport subclass from the given path.

    :param str path: The path pointing to the custom support subclass.
    :return: A subclass of BasePythonSupport.
    """
    file = pathlib.Path(path)

    if not file.exists():
        raise Exception(f"The support_class path {path} does not exist.")

    # Importing a source file directly
    spec = importlib.util.spec_from_file_location(name=file.name, location=path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    members = inspect.getmembers(
        module,
        lambda c: inspect.isclass(c)
        and c != BasePythonSupport
        and issubclass(c, BasePythonSupport),
    )

    if not members:
        raise Exception(
            f"The path {path} was found but it was not a valid support_class."
        )

    return members[0][-1]
