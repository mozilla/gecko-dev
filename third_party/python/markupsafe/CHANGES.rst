Version 3.0.2
-------------

Released 2024-10-18

-   Fix compatibility when ``__str__`` returns a ``str`` subclass. :issue:`472`
-   Build requires setuptools >= 70.1. :issue:`475`


Version 3.0.1
-------------

Released 2024-10-08

-   Address compiler warnings that became errors in GCC 14. :issue:`466`
-   Fix compatibility with proxy objects. :issue:`467`


Version 3.0.0
-------------

Released 2024-10-07

-   Support Python 3.13 and its experimental free-threaded build. :pr:`461`
-   Drop support for Python 3.7 and 3.8.
-   Use modern packaging metadata with ``pyproject.toml`` instead of ``setup.cfg``.
    :pr:`348`
-   Change ``distutils`` imports to ``setuptools``. :pr:`399`
-   Use deferred evaluation of annotations. :pr:`400`
-   Update signatures for ``Markup`` methods to match ``str`` signatures. Use
    positional-only arguments. :pr:`400`
-   Some ``str`` methods on ``Markup`` no longer escape their argument:
    ``strip``, ``lstrip``, ``rstrip``, ``removeprefix``, ``removesuffix``,
    ``partition``, and ``rpartition``; ``replace`` only escapes its ``new``
    argument. These methods are conceptually linked to search methods such as
    ``in``, ``find``, and ``index``, which already do not escape their argument.
    :issue:`401`
-   The ``__version__`` attribute is deprecated. Use feature detection, or
    ``importlib.metadata.version("markupsafe")``, instead. :pr:`402`
-   Speed up escaping plain strings by 40%. :pr:`434`
-   Simplify speedups implementation. :pr:`437`


Version 2.1.5
-------------

Released 2024-02-02

-   Fix ``striptags`` not collapsing spaces. :issue:`417`


Version 2.1.4
-------------

Released 2024-01-19

-   Don't use regular expressions for ``striptags``, avoiding a performance
    issue. :pr:`413`


Version 2.1.3
-------------

Released 2023-06-02

-   Implement ``format_map``, ``casefold``, ``removeprefix``, and ``removesuffix``
    methods. :issue:`370`
-   Fix static typing for basic ``str`` methods on ``Markup``. :issue:`358`
-   Use ``Self`` for annotating return types. :pr:`379`


Version 2.1.2
-------------

Released 2023-01-17

-   Fix ``striptags`` not stripping tags containing newlines.
    :issue:`310`


Version 2.1.1
-------------

Released 2022-03-14

-   Avoid ambiguous regex matches in ``striptags``. :pr:`293`


Version 2.1.0
-------------

Released 2022-02-17

-   Drop support for Python 3.6. :pr:`262`
-   Remove ``soft_unicode``, which was previously deprecated. Use
    ``soft_str`` instead. :pr:`261`
-   Raise error on missing single placeholder during string
    interpolation. :issue:`225`
-   Disable speedups module for GraalPython. :issue:`277`


Version 2.0.1
-------------

Released 2021-05-18

-   Mark top-level names as exported so type checking understands
    imports in user projects. :pr:`215`
-   Fix some types that weren't available in Python 3.6.0. :pr:`215`


Version 2.0.0
-------------

Released 2021-05-11

-   Drop Python 2.7, 3.4, and 3.5 support.
-   ``Markup.unescape`` uses :func:`html.unescape` to support HTML5
    character references. :pr:`117`
-   Add type annotations for static typing tools. :pr:`149`


Version 1.1.1
-------------

Released 2019-02-23

-   Fix segfault when ``__html__`` method raises an exception when using
    the C speedups. The exception is now propagated correctly. :pr:`109`


Version 1.1.0
-------------

Released 2018-11-05

-   Drop support for Python 2.6 and 3.3.
-   Build wheels for Linux, Mac, and Windows, allowing systems without
    a compiler to take advantage of the C extension speedups. :pr:`104`
-   Use newer CPython API on Python 3, resulting in a 1.5x speedup.
    :pr:`64`
-   ``escape`` wraps ``__html__`` result in ``Markup``, consistent with
    documented behavior. :pr:`69`


Version 1.0
-----------

Released 2017-03-07

-   Fixed custom types not invoking ``__unicode__`` when used with
    ``format()``.
-   Added ``__version__`` module attribute.
-   Improve unescape code to leave lone ampersands alone.


Version 0.18
------------

Released 2013-05-22

-   Fixed ``__mul__`` and string splitting on Python 3.


Version 0.17
------------

Released 2013-05-21

-   Fixed a bug with broken interpolation on tuples.


Version 0.16
------------

Released 2013-05-20

-   Improved Python 3 Support and removed 2to3.
-   Removed support for Python 3.2 and 2.5.


Version 0.15
------------

Released 2011-07-20

-   Fixed a typo that caused the library to fail to install on pypy and
    jython.


Version 0.14
------------

Released 2011-07-20

-   Release fix for 0.13.


Version 0.13
------------

Released 2011-07-20

-   Do not attempt to compile extension for PyPy or Jython.
-   Work around some 64bit Windows issues.


Version 0.12
------------

Released 2011-02-17

-   Improved PyPy compatibility.
