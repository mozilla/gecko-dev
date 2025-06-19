"""Type hints for Windows version of :class:`Terminal`."""

# std imports
from typing import Optional, ContextManager

# local
from .terminal import Terminal as _Terminal

# pylint: disable=missing-class-docstring

class Terminal(_Terminal):
    def getch(self) -> str: ...
    def kbhit(self, timeout: Optional[float] = ...) -> bool: ...
    def cbreak(self) -> ContextManager[None]: ...
    def raw(self) -> ContextManager[None]: ...
