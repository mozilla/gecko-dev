import inspect


def _bridge_build_meta():
    import functools
    import sys

    from setuptools import build_meta

    self_module = sys.modules[__name__]

    for attr_name in build_meta.__all__:
        attr_value = getattr(build_meta, attr_name)
        if callable(attr_value):
            setattr(self_module, attr_name, functools.partial(_expose_config_settings, attr_value))


class ActiveConfigSettings:
    _current = {}

    def __init__(self, config_settings):
        self._config = config_settings

    def __enter__(self):
        type(self)._current = self._config

    def __exit__(self, exc_type, exc_val, exc_tb):
        type(self)._current = {}

    @classmethod
    def current(cls):
        return cls._current


def _expose_config_settings(real_method, *args, **kwargs):
    from contextlib import nullcontext
    import inspect

    sig = inspect.signature(real_method)
    boundargs = sig.bind(*args, **kwargs)

    config = boundargs.arguments.get('config_settings')

    ctx = ActiveConfigSettings(config) if config else nullcontext()

    with ctx:
        return real_method(*args, **kwargs)


_bridge_build_meta()

