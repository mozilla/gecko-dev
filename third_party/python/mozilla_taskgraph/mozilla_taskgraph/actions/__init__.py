from typing import Any

from taskgraph.actions.registry import register_callback_action
from taskgraph.util.python_path import import_sibling_modules

available_actions = {}


def make_action_available(**kwargs: Any):
    """Decorator to make the action available to consumers.

    Consumers of `mozilla-taskgraph` may not want to implicitly enable all
    actions that `mozilla-taskgraph` defines. This decorator simply adds the
    action to the list of `available_actions`. Consumers can then explicitly
    call `enable_action` on it if desired.

    Args:
        kwargs (Dict): Key-word args to forward to
            `taskgraph.actions.registry.register_callback_action`.
    """

    def inner(func):
        available_actions[kwargs["name"]] = (func, kwargs)

    return inner


# trigger decorators
import_sibling_modules()


def enable_action(name: str, **overrides: Any):
    """Enables an available action.

    This function would typically be called from a consumer's `register`
    function. E.g:

    .. code-block:: python

       from mozilla_taskgraph.actions import enable_action

       def register(graph_config):
           enable_action("release-promotion")

    Args:
        name (str): Name of the action to enable.
        overrides (Dict): Key-word arguments to override defaults from.
            A subset of the options
    """
    func, kwargs = available_actions[name]
    kwargs.update(overrides)
    register_callback_action(**kwargs)(func)
